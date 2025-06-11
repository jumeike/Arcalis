#include "DPDKHandler.h"

bool DPDKHandler::init(uint16_t port_id) {
    // Get shared memory handle first
    auto& mgr = SharedMemoryManager::getInstance();
    shared_mem_ = mgr.initSharedMemory();
    if (!shared_mem_) {
        fprintf(stderr, "Failed to get shared memory\n");
        return false;
    }
    portId_ = port_id;
    // Minimal EAL arguments
    const char* argv[] = {
        "thrift-server",           // Program name
        "-l", "0-1",              // Use CPU cores 0-1
        "-n", "4",                // Number of memory channels
        "--proc-type=auto",       // Process type
        "--log-level", "8",       // Debug log level
        NULL
    };
    int argc = sizeof(argv) / sizeof(argv[0]) - 1;  // -1 for NULL terminator
    // Initialize EAL
    int ret = rte_eal_init(argc, (char**)argv);
    if (ret < 0) {
        fprintf(stderr, "Failed to initialize EAL: %s\n", rte_strerror(rte_errno));
        return false;
    }

    // Create mempool
    mbufPool_ = createMempool();
    if (!mbufPool_) {
        fprintf(stderr, "Failed to create mbuf pool: %s\n", rte_strerror(rte_errno));
        return false;
    }

    // Setup port
    if (!setupDPDKPort()) {
        fprintf(stderr, "Failed to setup port: %s\n", rte_strerror(rte_errno));
        return false;
    }
    
    return true;
}

struct rte_mempool* DPDKHandler::createMempool() {
  unsigned nb_ports = rte_eth_dev_count_avail();
  if (nb_ports == 0) {
    return nullptr;
  }
  char pool_name[64];
  int socket_id = rte_socket_id();

  snprintf(pool_name, sizeof(pool_name), "mbuf_pool_%d", portId_);

  return rte_pktmbuf_pool_create(pool_name,
                                NUM_MBUFS * nb_ports,
                                MBUF_CACHE_SIZE,
                                0,
                                RTE_MBUF_DEFAULT_BUF_SIZE,
                                socket_id);
}

bool DPDKHandler::setupDPDKPort() {
  // Get port info
  rte_eth_dev_info_get(portId_, &devInfo_);

  // Configure port
  memset(&portConf_, 0, sizeof(portConf_));
  // Set proper packet type flags
  portConf_.rxmode.max_lro_pkt_size = RTE_ETHER_MAX_LEN;
  portConf_.rxmode.mq_mode = RTE_ETH_MQ_RX_NONE;
  portConf_.rxmode.offloads = RTE_ETH_RX_OFFLOAD_CHECKSUM;

  // Accept all packet types
  portConf_.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_IPV4_CKSUM;
  portConf_.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_UDP_CKSUM;

  portConf_.txmode.mq_mode = RTE_ETH_MQ_TX_NONE;
  portConf_.txmode.offloads = RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
                                            RTE_ETH_TX_OFFLOAD_UDP_CKSUM;

  // Configure device
  int ret = rte_eth_dev_configure(portId_, 1, 1, &portConf_);
  if (ret != 0) {
    fprintf(stderr, "Failed to configure port %d (err=%d)\n", portId_, ret);
    return false;
  }

  // Get the port MAC address
  struct rte_ether_addr addr;
  ret = rte_eth_macaddr_get(portId_, &addr);
  if (ret != 0) {
    return false;
  }

  // Setup RX queue
  ret = rte_eth_rx_queue_setup(portId_,
                              0,
                              RX_RING_SIZE,
                              rte_eth_dev_socket_id(portId_),
                              nullptr,
                              mbufPool_ );
  if (ret < 0) {
    return false;
  }

  // Setup TX queue
  ret = rte_eth_tx_queue_setup(portId_,
                              0,
                              TX_RING_SIZE,
                              rte_eth_dev_socket_id(portId_),
                              nullptr);
  if (ret < 0) {
    return false;
  }

  // Enable promiscuous mode
  ret = rte_eth_promiscuous_enable(portId_);
  if (ret != 0) {
    return false;
  }

  // Start device
  ret = rte_eth_dev_start(portId_);
  if (ret < 0) {
    return false;
  }

  // Check link status
  struct rte_eth_link link;
  ret = rte_eth_link_get(portId_, &link);
  if (ret < 0 || !link.link_status) {
    return false;
  }

  return true;
}

void DPDKHandler::setLocalIpAddress(const std::string& ipStr) {
        struct in_addr addr;
        inet_aton(ipStr.c_str(), &addr);
        localIpAddress_ = addr.s_addr;
}

void DPDKHandler::startPolling() {
    running_ = true;
    setLocalIpAddress("192.168.1.1");
    pollThread_ = std::thread(&DPDKHandler::pollLoop, this);
}

void DPDKHandler::stopPolling() {
    running_ = false;
    if (pollThread_.joinable()) {
        pollThread_.join();
    }
}

void DPDKHandler::handleArpPacket(struct rte_mbuf* m) {
    struct rte_ether_hdr* eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr*);
    struct rte_arp_hdr* arp_hdr = (struct rte_arp_hdr*)(eth_hdr + 1);

    if (rte_be_to_cpu_16(arp_hdr->arp_opcode) == RTE_ARP_OP_REQUEST &&
        arp_hdr->arp_data.arp_tip == localIpAddress_) {

        struct rte_mbuf* reply = rte_pktmbuf_alloc(mbufPool_);
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
                rte_eth_macaddr_get(portId_, &reply_eth->src_addr);
                reply_eth->ether_type = eth_hdr->ether_type;

                // Set ARP header
                reply_arp->arp_hardware = arp_hdr->arp_hardware;
                reply_arp->arp_protocol = arp_hdr->arp_protocol;
                reply_arp->arp_hlen = arp_hdr->arp_hlen;
                reply_arp->arp_plen = arp_hdr->arp_plen;
                reply_arp->arp_opcode = rte_cpu_to_be_16(RTE_ARP_OP_REPLY);

                // Set ARP data
                rte_eth_macaddr_get(portId_,
                    &reply_arp->arp_data.arp_sha);
                reply_arp->arp_data.arp_sip = localIpAddress_;
                reply_arp->arp_data.arp_tha = arp_hdr->arp_data.arp_sha;
                reply_arp->arp_data.arp_tip = arp_hdr->arp_data.arp_sip;

                uint16_t nb_tx = rte_eth_tx_burst(portId_,
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

void DPDKHandler::pollLoop() {
    auto& logger = PacketLogger::getInstance();
    while (running_) {
        struct rte_mbuf* pkts[1];
        const uint16_t nb_rx = rte_eth_rx_burst(portId_, 0, pkts, 1);
        
        for (uint16_t i = 0; i < nb_rx; i++) {
            struct rte_mbuf* pkt = pkts[i];
            
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

            struct rte_ipv4_hdr* ip_hdr = (struct rte_ipv4_hdr*)(eth_hdr + 1);
            struct rte_udp_hdr* udp_hdr = (struct rte_udp_hdr*)(ip_hdr + 1);

            // Skip non-UDP packets
            if (ip_hdr->next_proto_id != IPPROTO_UDP) {
                fprintf(stderr, "Debug: found non-UDP packet\n");
                rte_pktmbuf_free(pkt);
                continue;
            }
            
            // Get payload
            uint8_t* payload = rte_pktmbuf_mtod_offset(pkt, uint8_t*, 
                sizeof(struct rte_ether_hdr) + 
                sizeof(struct rte_ipv4_hdr) + 
                sizeof(struct rte_udp_hdr));
                
            uint32_t payload_len = pkt->data_len - (sizeof(struct rte_ether_hdr) + 
                                                sizeof(struct rte_ipv4_hdr) + 
                                                sizeof(struct rte_udp_hdr));
            // Validate destination port if specific port is set
            if (port_ != 0 && rte_be_to_cpu_16(udp_hdr->dst_port) != port_) {
                fprintf(stderr, "Debug: port mismatch. Expected: %d, Got: %d\n", 
                        port_, rte_be_to_cpu_16(udp_hdr->dst_port));
                rte_pktmbuf_free(pkt);
                continue;
            }             
            
            // Call callback with payload
//            if (packetCallback_) {
//                packetCallback_(payload, payload_len);
//            }

            // Copy to shared memory
            pthread_mutex_lock(&shared_mem_->rx_buffer.mutex);
            memcpy(shared_mem_->rx_buffer.data, payload, payload_len);
            shared_mem_->rx_buffer.size = payload_len;
            shared_mem_->rx_buffer.has_data = true;
            //printf("rx_buffer.size: %ld\n", shared_mem_->rx_buffer.size);
            
            // if (payload_len > 0) {
            //     logger.logDPDKToRPC(payload, payload_len);
            // } 
            
            // Signal RPC layer
            sem_post(&shared_mem_->rx_buffer.sem);
            pthread_mutex_unlock(&shared_mem_->rx_buffer.mutex);
             
            // Cache peer address for responses
            peerAddr_.sin_family = AF_INET;
            peerAddr_.sin_port = udp_hdr->src_port;
            peerAddr_.sin_addr.s_addr = ip_hdr->src_addr;

            rte_pktmbuf_free(pkt);
            break;
        }

        // Check if RPC has data to send
        if (sem_trywait(&shared_mem_->tx_buffer.sem) == 0) {
            pthread_mutex_lock(&shared_mem_->tx_buffer.mutex);

            if (shared_mem_->tx_buffer.has_data) {
                //printf("tx_buffer.size: %ld\n", shared_mem_->tx_buffer.size);
                
                // logger.logRPCToDPDK(shared_mem_->tx_buffer.data,
                //                shared_mem_->tx_buffer.size);
                
                sendData(shared_mem_->tx_buffer.data,
                        shared_mem_->tx_buffer.size);

                // Reset buffer state
                shared_mem_->tx_buffer.has_data = false;
                shared_mem_->tx_buffer.size = 0;
            }

            pthread_mutex_unlock(&shared_mem_->tx_buffer.mutex);
        }
    }
}

bool DPDKHandler::sendData(const uint8_t* data, uint32_t len) {

    struct rte_mbuf* m = rte_pktmbuf_alloc(mbufPool_);
    if (!m) return false;

    // Reserve space for headers + data
    uint32_t total_len = sizeof(struct rte_ether_hdr) + 
                        sizeof(struct rte_ipv4_hdr) +
                        sizeof(struct rte_udp_hdr) + len;
                        
    char* pkt = rte_pktmbuf_append(m, total_len);
    if (!pkt) {
        rte_pktmbuf_free(m);
        return false; 
    }

    // Set up headers...
    struct rte_ether_hdr* eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr*);
    struct rte_ipv4_hdr* ip_hdr = (struct rte_ipv4_hdr*)(eth_hdr + 1);
    struct rte_udp_hdr* udp_hdr = (struct rte_udp_hdr*)(ip_hdr + 1);
    uint8_t* payload = (uint8_t*)(udp_hdr + 1);
    
    // Ethernet header
    rte_eth_macaddr_get(portId_, &eth_hdr->src_addr);
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
//    printf("port_: %d\n", port_);
//    printf("dst_port: %d\n", udp_hdr->dst_port);
//    printf("src_port: %d\n", udp_hdr->src_port);
    udp_hdr->dgram_len = rte_cpu_to_be_16(sizeof(*udp_hdr) + len);
    udp_hdr->dgram_cksum = 0;

    // Copy payload
    rte_memcpy(payload, data, total_len);

    // Send packet
    uint16_t nb_tx = rte_eth_tx_burst(portId_, 0, &m, 1);
    if (nb_tx == 0) {
        rte_pktmbuf_free(m);
        return false;
    }
    return true;
}

// DPDKHandler.cpp
void DPDKHandler::cleanup() {
    // Stop polling if running
    if (running_) {
        running_ = false;
        if (pollThread_.joinable()) {
            pollThread_.join();
        }
    }

    // Cleanup DPDK resources
    if (portId_ != RTE_MAX_ETHPORTS) {
        rte_eth_dev_stop(portId_);
        rte_eth_dev_close(portId_);
    }

    if (mbufPool_) {
        rte_mempool_free(mbufPool_);
        mbufPool_ = nullptr;
    }

    // Cleanup EAL
    rte_eal_cleanup();
}
