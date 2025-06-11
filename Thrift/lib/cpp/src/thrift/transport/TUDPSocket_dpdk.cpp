// TUDPSocket.cpp
#include <thrift/transport/TUDPSocket.h>
#include <thrift/transport/TTransportException.h>

#include <rte_udp.h>
#include <rte_ip.h>
#include <rte_ether.h>

namespace apache {
namespace thrift {
namespace transport {

// Constants for header sizes
static const size_t ETHER_HDR_LEN = sizeof(struct rte_ether_hdr);
static const size_t IP_HDR_LEN = sizeof(struct rte_ipv4_hdr);
static const size_t UDP_HDR_LEN = sizeof(struct rte_udp_hdr);
static const size_t HEADERS_LEN = ETHER_HDR_LEN + IP_HDR_LEN + UDP_HDR_LEN;



TUDPSocket::TUDPSocket(std::shared_ptr<TConfiguration> config)
  : TVirtualTransport(config)
  , port_(0)
  , isInitialized_(false)
  , localIpAddress_(0)
  , sendTimeout_(0)
  , recvTimeout_(0) {
}

TUDPSocket::TUDPSocket(const std::string& host, int port, std::shared_ptr<TConfiguration> config)
  : TVirtualTransport(config)
  , host_(host)
  , port_(port)
  , isInitialized_(false)
  , localIpAddress_(0)
  , sendTimeout_(0)
  , recvTimeout_(0) {
}

TUDPSocket::TUDPSocket(std::shared_ptr<DPDKResources> dpdkResources,
                       std::shared_ptr<TConfiguration> config)
    : TVirtualTransport(config)
    , port_(0)
    , localIpAddress_(0)
    , dpdkResources_(dpdkResources)
    , sendTimeout_(0)
    , recvTimeout_(0) {
    memset(&peerAddr_, 0, sizeof(peerAddr_));
    setLocalIpAddress("192.168.1.1");
}

TUDPSocket::~TUDPSocket() {
  close();
}

bool TUDPSocket::initDPDK() {
  // Initialize EAL
  int argc = 0;
  char **argv = nullptr;
  int ret = rte_eal_init(argc, argv);
  if (ret < 0) {
    return false;
  }

  // Create mempool for packet buffers
  dpdkResources_->mbufPool  = createMempool();
  if (!dpdkResources_->mbufPool ) {
    return false;
  }

  // Setup network port
  if (!setupDPDKPort()) {
    return false;
  }

  isInitialized_ = true;
  return true;
}

struct rte_mempool* TUDPSocket::createMempool() {
  // Create mempool for packet buffers
  unsigned nb_ports = rte_eth_dev_count_avail();
  if (nb_ports == 0) {
    return nullptr;
  }

  char pool_name[64];
  snprintf(pool_name, sizeof(pool_name), "mbuf_pool_%d", dpdkResources_->portId);

  return rte_pktmbuf_pool_create(pool_name,
                                NUM_MBUFS * nb_ports,
                                MBUF_CACHE_SIZE,
                                0,
                                RTE_MBUF_DEFAULT_BUF_SIZE,
                                rte_socket_id());
}

bool TUDPSocket::setupDPDKPort() {
  // Get port info
  rte_eth_dev_info_get(dpdkResources_->portId, &dpdkResources_->devInfo);

  // Configure port
  memset(&dpdkResources_->portConf, 0, sizeof(dpdkResources_->portConf));
  dpdkResources_->portConf.rxmode.max_lro_pkt_size = RTE_ETHER_MAX_LEN;
  dpdkResources_->portConf.rxmode.mq_mode = RTE_ETH_MQ_RX_NONE;
  dpdkResources_->portConf.txmode.mq_mode = RTE_ETH_MQ_TX_NONE;

  // Configure device
  int ret = rte_eth_dev_configure(dpdkResources_->portId, 1, 1, &dpdkResources_->portConf);
  if (ret != 0) {
    return false;
  }

  // Setup RX queue
  ret = rte_eth_rx_queue_setup(dpdkResources_->portId,
                              0,
                              RX_RING_SIZE,
                              rte_eth_dev_socket_id(dpdkResources_->portId),
                              nullptr,
                              dpdkResources_->mbufPool );
  if (ret < 0) {
    return false;
  }

  // Setup TX queue
  ret = rte_eth_tx_queue_setup(dpdkResources_->portId,
                              0,
                              TX_RING_SIZE,
                              rte_eth_dev_socket_id(dpdkResources_->portId),
                              nullptr);
  if (ret < 0) {
    return false;
  }

  // Start device
  ret = rte_eth_dev_start(dpdkResources_->portId);
  if (ret < 0) {
    return false;
  }

  return true;
}

bool TUDPSocket::isOpen() const {
  return dpdkResources_->isInitialized;
}

bool TUDPSocket::peek() {
  if (!dpdkResources_->isInitialized) {
    return false;
  }

  struct rte_mbuf* pkts_burst[1];
  const uint16_t nb_rx = rte_eth_rx_burst(dpdkResources_->portId, 0, pkts_burst, 1);
  
  if (nb_rx > 0) {
    rte_pktmbuf_free(pkts_burst[0]);
    return true;
  }
  
  return false;
}

void TUDPSocket::open() {
  if (dpdkResources_->isInitialized) {
    return;
  }

  if (!initDPDK()) {
    throw TTransportException(TTransportException::NOT_OPEN,
                             "Could not initialize DPDK");
  }
}

void TUDPSocket::close() {
  if (!dpdkResources_->isInitialized) {
    return;
  }

  rte_eth_dev_stop(dpdkResources_->portId);
  rte_eth_dev_close(dpdkResources_->portId);
  
  if (dpdkResources_->mbufPool ) {
    rte_mempool_free(dpdkResources_->mbufPool );
    dpdkResources_->mbufPool  = nullptr;
  }

  dpdkResources_->isInitialized = false;
}

void TUDPSocket::setLocalIpAddress(const std::string& ipStr) {
        struct in_addr addr;
        inet_aton(ipStr.c_str(), &addr);
        localIpAddress_ = addr.s_addr;
}

void TUDPSocket::handleArpPacket(struct rte_mbuf* m) {
    struct rte_ether_hdr* eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr*);
    struct rte_arp_hdr* arp_hdr = (struct rte_arp_hdr*)(eth_hdr + 1);

    if (rte_be_to_cpu_16(arp_hdr->arp_opcode) == RTE_ARP_OP_REQUEST &&
        arp_hdr->arp_data.arp_tip == localIpAddress_) {

        struct rte_mbuf* reply = rte_pktmbuf_alloc(dpdkResources_->mbufPool);
        if (reply) {
            char* pkt = rte_pktmbuf_append(reply,
                sizeof(struct rte_ether_hdr) + sizeof(struct rte_arp_hdr));

            if (pkt) {
                struct rte_ether_hdr* reply_eth =
                    rte_pktmbuf_mtod(reply, struct rte_ether_hdr*);
                struct rte_arp_hdr* reply_arp =
                    (struct rte_arp_hdr*)(reply_eth + 1);

                // Set ethernet header
                reply_eth->dst_addr = eth_hdr->src_addr;
                rte_eth_macaddr_get(dpdkResources_->portId, &reply_eth->src_addr);
                reply_eth->ether_type = eth_hdr->ether_type;

                // Set ARP header
                reply_arp->arp_hardware = arp_hdr->arp_hardware;
                reply_arp->arp_protocol = arp_hdr->arp_protocol;
                reply_arp->arp_hlen = arp_hdr->arp_hlen;
                reply_arp->arp_plen = arp_hdr->arp_plen;
                reply_arp->arp_opcode = rte_cpu_to_be_16(RTE_ARP_OP_REPLY);

                // Set ARP data
                rte_eth_macaddr_get(dpdkResources_->portId,
                    &reply_arp->arp_data.arp_sha);
                reply_arp->arp_data.arp_sip = localIpAddress_;
                reply_arp->arp_data.arp_tha = arp_hdr->arp_data.arp_sha;
                reply_arp->arp_data.arp_tip = arp_hdr->arp_data.arp_sip;

                uint16_t nb_tx = rte_eth_tx_burst(dpdkResources_->portId,
                    0, &reply, 1);
                if (nb_tx == 0) {
                    rte_pktmbuf_free(reply);
                }
            } else {
                rte_pktmbuf_free(reply);
            }
        }
    }
}

uint32_t TUDPSocket::read(uint8_t* buf, uint32_t len) {
    if (!dpdkResources_ || !dpdkResources_->isInitialized) {
        throw TTransportException(TTransportException::NOT_OPEN, 
                                "Called read on non-open socket");
    }

    uint32_t total_rx = 0;
    struct rte_mbuf* pkt = nullptr;
    
    //printf("Called read()\n"); 
    
    // Poll for packets with timeout handling
    uint64_t start_time = rte_get_timer_cycles();
    uint64_t timeout_cycles = (uint64_t)recvTimeout_ * rte_get_timer_hz() / 1000;

    while (total_rx == 0) {
        // Check for timeout
        if (recvTimeout_ > 0) {
            uint64_t elapsed = rte_get_timer_cycles() - start_time;
            if (elapsed > timeout_cycles) {
                throw TTransportException(TTransportException::TIMED_OUT,
                                        "UDP read timeout");
            }
        }

        // Use optimized single packet receive
        uint16_t nb_rx = rte_eth_rx_burst(dpdkResources_->portId, 0, &pkt, 1);
        if (nb_rx == 0) continue;
        //if (nb_rx > 0 ) printf("nb_rx: %d\n", nb_rx);
        // Get packet headers
        struct rte_ether_hdr* eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr*);
        uint16_t ether_type = rte_be_to_cpu_16(eth_hdr->ether_type);

        // Handle ARP packets
        if (ether_type == RTE_ETHER_TYPE_ARP) {
            //fprintf(stderr, "Debug: Received ARP request\n");
            handleArpPacket(pkt);
            rte_pktmbuf_free(pkt);
            continue;
        }

        // Skip non-IPv4 packets
        if (ether_type != RTE_ETHER_TYPE_IPV4) {
            fprintf(stderr, "Debug: found non-IPv4 packets\n");
            rte_pktmbuf_free(pkt);
            continue;
        }

        //struct rte_ipv4_hdr* ip_hdr = (struct rte_ipv4_hdr*)(eth_hdr + 1);
        struct rte_ipv4_hdr* ip_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_ipv4_hdr*,
                                                             sizeof(struct rte_ether_hdr));
        struct rte_udp_hdr* udp_hdr = (struct rte_udp_hdr*)(ip_hdr + 1);

        // Skip non-UDP packets
        if (ip_hdr->next_proto_id != IPPROTO_UDP) {
            fprintf(stderr, "Debug: found non-UDP packet\n");
            rte_pktmbuf_free(pkt);
            continue;
        }

        // Validate port if set
        if (port_ != 0 && rte_be_to_cpu_16(udp_hdr->dst_port) != port_) {
            rte_pktmbuf_free(pkt);
            continue;
        }

        // Get and copy payload
        uint8_t* payload = rte_pktmbuf_mtod_offset(pkt, uint8_t*, HEADERS_LEN);
        if (!payload) {
            printf("Error: Invalid payload\n");  // Debug`
            rte_pktmbuf_free(pkt);
            continue;
        }
        uint32_t payload_len = static_cast<uint32_t>(pkt->data_len - HEADERS_LEN);
        uint32_t copy_len = std::min(payload_len, len); 
        rte_memcpy(buf, payload, copy_len);
        total_rx = copy_len;

        // Cache peer address
        peerAddr_.sin_family = AF_INET;
        peerAddr_.sin_port = udp_hdr->src_port;
        peerAddr_.sin_addr.s_addr = ip_hdr->src_addr;
        
        rte_pktmbuf_free(pkt);
        break;
    }
    return total_rx;
}

void TUDPSocket::write(const uint8_t* buf, uint32_t len) {
  write_partial(buf, len);
}

uint32_t TUDPSocket::write_partial(const uint8_t* buf, uint32_t len) {
  if (!dpdkResources_ || !dpdkResources_->isInitialized) {
    throw TTransportException(TTransportException::NOT_OPEN,
                             "Called write on non-open socket");
  }

  // Allocate new mbuf
  struct rte_mbuf* m = rte_pktmbuf_alloc(dpdkResources_->mbufPool);
  if (!m) {
      throw TTransportException(TTransportException::UNKNOWN,
                              "Failed to allocate mbuf");
  }

  // Reserve space for headers
  char* pkt = rte_pktmbuf_append(m, HEADERS_LEN + len);
  if (!pkt) {
    rte_pktmbuf_free(m);
    throw TTransportException(TTransportException::UNKNOWN,
                             "Failed to reserve space in mbuf");
  }

  // Set up headers
  struct rte_ether_hdr* eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr*);
  struct rte_ipv4_hdr* ip_hdr = (struct rte_ipv4_hdr*)(eth_hdr + 1);
  struct rte_udp_hdr* udp_hdr = (struct rte_udp_hdr*)(ip_hdr + 1);
  uint8_t* payload = (uint8_t*)(udp_hdr + 1);

  // Ethernet header
  rte_eth_macaddr_get(dpdkResources_->portId, &eth_hdr->src_addr);
  // Destination MAC should be set based on ARP or known destination
  // For now using broadcast
  // memset(&eth_hdr->dst_addr, 0xff, RTE_ETHER_ADDR_LEN); 
  // Set destination MAC directly
  // Converting 0c:42:a1:cc:83:82 to bytes
  eth_hdr->dst_addr.addr_bytes[0] = 0x0c;
  eth_hdr->dst_addr.addr_bytes[1] = 0x42;
  eth_hdr->dst_addr.addr_bytes[2] = 0xa1;
  eth_hdr->dst_addr.addr_bytes[3] = 0xcc;
  eth_hdr->dst_addr.addr_bytes[4] = 0x83;
  eth_hdr->dst_addr.addr_bytes[5] = 0x82;

  eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

  // IP header
  memset(ip_hdr, 0, sizeof(*ip_hdr));
  ip_hdr->version_ihl = (4 << 4) | (sizeof(*ip_hdr) >> 2);
  ip_hdr->type_of_service = 0;
  ip_hdr->total_length = rte_cpu_to_be_16(sizeof(*ip_hdr) + sizeof(*udp_hdr) + len);
  ip_hdr->packet_id = 0;
  ip_hdr->fragment_offset = 0;
  ip_hdr->time_to_live = 64;
  ip_hdr->next_proto_id = IPPROTO_UDP;
  // Source IP should be interface IP
  ip_hdr->src_addr = localIpAddress_; //use configured Ip
  // Destination IP from peer address
  ip_hdr->dst_addr = peerAddr_.sin_addr.s_addr;
  ip_hdr->hdr_checksum = 0;
  ip_hdr->hdr_checksum = rte_ipv4_cksum(ip_hdr);

  // UDP header
  udp_hdr->src_port = rte_cpu_to_be_16(port_);
  udp_hdr->dst_port = peerAddr_.sin_port;
  udp_hdr->dgram_len = rte_cpu_to_be_16(sizeof(*udp_hdr) + len);
  udp_hdr->dgram_cksum = 0;

  // Copy payload
  rte_memcpy(payload, buf, len);

  // Calculate UDP checksum
  udp_hdr->dgram_cksum = rte_ipv4_udptcp_cksum(ip_hdr, udp_hdr);

  // Send packet
  uint16_t nb_tx = rte_eth_tx_burst(dpdkResources_->portId, 0, &m, 1);
  if (nb_tx == 0) {
    rte_pktmbuf_free(m);
    throw TTransportException(TTransportException::UNKNOWN,
                             "Failed to send packet");
  }

  return len;
}

// Helper function to get local IP address
uint32_t TUDPSocket::GetLocalIPAddress(uint16_t port_id __attribute__((unused))) {
  // This should be implemented to return the IP address assigned to the DPDK port
  // For example, you could store this during initialization
  // For now returning a placeholder
  return rte_cpu_to_be_32(0x0A000001); // 10.0.0.1
}

void TUDPSocket::setRecvTimeout(int ms) {
  recvTimeout_ = ms;
}

void TUDPSocket::setSendTimeout(int ms) {
  sendTimeout_ = ms;
}

const std::string TUDPSocket::getOrigin() const {
  std::ostringstream oss;
  oss << host_ << ":" << port_;
  return oss.str();
}

}}} // apache::thrift::transport
