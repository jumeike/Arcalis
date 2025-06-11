//TServerUDPSocket.cpp
#include <thrift/transport/TServerUDPSocket.h>
#include <thrift/transport/TUDPSocket.h>
#include <thrift/transport/TTransportException.h>

namespace apache {
namespace thrift {
namespace transport {

TServerUDPSocket::TServerUDPSocket(int port)
  : port_(port)
  , isInitialized_(false)
  , sendTimeout_(0)
  , recvTimeout_(0) {
}

TServerUDPSocket::TServerUDPSocket(int port, int sendTimeout, int recvTimeout)
  : port_(port)
  , isInitialized_(false)
  , sendTimeout_(sendTimeout)
  , recvTimeout_(recvTimeout) {
}

TServerUDPSocket::~TServerUDPSocket() {
  close();
}

bool TServerUDPSocket::initDPDK() {
    dpdkResources_ = std::make_shared<DPDKResources>();
    
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
        return false;
    }

    // Create mempool
    dpdkResources_->mbufPool = createMempool();
    if (!dpdkResources_->mbufPool) {
        return false;
    }

    // Setup port
    dpdkResources_->portId = 0;  // Or however you select the port
    if (!setupDPDKPort()) {
        return false;
    }

    dpdkResources_->isInitialized = true;
    return true;
}

struct rte_mempool* TServerUDPSocket::createMempool() {
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

bool TServerUDPSocket::setupDPDKPort() {
  // Get port info
  rte_eth_dev_info_get(dpdkResources_->portId, &dpdkResources_->devInfo);

  // Configure port
  memset(&dpdkResources_->portConf, 0, sizeof(dpdkResources_->portConf));
  // Set proper packet type flags
  dpdkResources_->portConf.rxmode.max_lro_pkt_size = RTE_ETHER_MAX_LEN;
  dpdkResources_->portConf.rxmode.mq_mode = RTE_ETH_MQ_RX_NONE;
  dpdkResources_->portConf.rxmode.offloads = RTE_ETH_RX_OFFLOAD_CHECKSUM;
  
  // Accept all packet types
  dpdkResources_->portConf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_IPV4_CKSUM;
  dpdkResources_->portConf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_UDP_CKSUM;
  
  dpdkResources_->portConf.txmode.mq_mode = RTE_ETH_MQ_TX_NONE;
  dpdkResources_->portConf.txmode.offloads = RTE_ETH_TX_OFFLOAD_IPV4_CKSUM | 
                                            RTE_ETH_TX_OFFLOAD_UDP_CKSUM; 

  // Configure device
  int ret = rte_eth_dev_configure(dpdkResources_->portId, 1, 1, &dpdkResources_->portConf);
  if (ret != 0) {
    return false;
  }

  // Get the port MAC address
  struct rte_ether_addr addr;
  ret = rte_eth_macaddr_get(dpdkResources_->portId, &addr);
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

  // Enable promiscuous mode
  ret = rte_eth_promiscuous_enable(dpdkResources_->portId);
  if (ret != 0) {
    return false;
  }

  // Start device
  ret = rte_eth_dev_start(dpdkResources_->portId);
  if (ret < 0) {
    return false;
  }

  // Check link status
  struct rte_eth_link link;
  ret = rte_eth_link_get(dpdkResources_->portId, &link);
  if (ret < 0 || !link.link_status) {
    return false;
  }

  return true;
}

void TServerUDPSocket::listen() {
  if (isInitialized_) {
    return;
  }

  if (port_ < 0 || port_ > 0xFFFF) {
    throw TTransportException(TTransportException::BAD_ARGS,
                             "Invalid port number");
  }

  if (!initDPDK()) {
    throw TTransportException(TTransportException::NOT_OPEN,
                             "Failed to initialize DPDK");
  }

  if (listenCallback_) {
    listenCallback_(dpdkResources_->portId);
  }
}

std::shared_ptr<TTransport> TServerUDPSocket::acceptImpl() {
  if (!dpdkResources_->isInitialized) {
    throw TTransportException(TTransportException::NOT_OPEN,
                             "Server socket not initialized");
  }

  // Create new UDP socket for client communication
  std::shared_ptr<TUDPSocket> client = createSocket(dpdkResources_->portId);
  
  if (sendTimeout_ > 0) {
    client->setSendTimeout(sendTimeout_);
  }
  if (recvTimeout_ > 0) {
    client->setRecvTimeout(recvTimeout_);
  }
  fprintf(stderr, "DPDK Initialized, returning client!\n");
  return client;
}

std::shared_ptr<TUDPSocket> TServerUDPSocket::createSocket(uint16_t portId) {
    // Create socket with shared DPDK resources
    auto socket = std::make_shared<TUDPSocket>(dpdkResources_);
    
    // Set socket-specific parameters
    if (sendTimeout_ > 0) {
        socket->setSendTimeout(sendTimeout_);
    }
    if (recvTimeout_ > 0) {
        socket->setRecvTimeout(recvTimeout_);
    }
    
    return socket;
}
void TServerUDPSocket::interrupt() {
  concurrency::Guard g(mutex_);
  if (dpdkResources_->isInitialized) {
    // Send an interrupt packet to wake up polling
    struct rte_mbuf* m = rte_pktmbuf_alloc(dpdkResources_->mbufPool );
    if (m) {
      uint16_t nb_tx = rte_eth_tx_burst(dpdkResources_->portId, 0, &m, 1);
      if (nb_tx == 0) {
        rte_pktmbuf_free(m);
      }
    }
  }
}

void TServerUDPSocket::close() {
  concurrency::Guard g(mutex_);
  
  if (!dpdkResources_->isInitialized) {
    return;
  }

  rte_eth_dev_stop(dpdkResources_->portId);
  rte_eth_dev_close(dpdkResources_->portId);
  
  if (dpdkResources_->mbufPool ) {
    rte_mempool_free(dpdkResources_->mbufPool );
    dpdkResources_->mbufPool  = nullptr;
  }

  isInitialized_ = false;
  dpdkResources_->isInitialized = false;
}

bool TServerUDPSocket::isOpen() const {
  return isInitialized_ || dpdkResources_->isInitialized;
}

void TServerUDPSocket::setSendTimeout(int sendTimeout) {
  sendTimeout_ = sendTimeout;
}

void TServerUDPSocket::setRecvTimeout(int recvTimeout) {
  recvTimeout_ = recvTimeout;
}

}}} // apache::thrift::transport
