// TServerUDPSocket.h
#ifndef _THRIFT_TRANSPORT_TSERVERUDPSOCKET_H_
#define _THRIFT_TRANSPORT_TSERVERUDPSOCKET_H_ 1

#include <functional>
#include <memory>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include "DPDKResources.h"

#include <thrift/transport/TServerTransport.h>
#include <thrift/concurrency/Mutex.h>

namespace apache {
namespace thrift {
namespace transport {

class TUDPSocket;

class TServerUDPSocket : public TServerTransport {
public:
  typedef std::function<void(uint16_t portId)> port_func_t;

  static const uint16_t RX_RING_SIZE = 1024;
  static const uint16_t TX_RING_SIZE = 1024;
  static const uint16_t NUM_MBUFS = 8191;
  static const uint16_t MBUF_CACHE_SIZE = 250;
  
  TServerUDPSocket(int port);
  TServerUDPSocket(int port, int sendTimeout, int recvTimeout);
  ~TServerUDPSocket() override;

  bool isOpen() const override;
  void listen() override;
  void interrupt() override;
  void close() override;

  void setSendTimeout(int sendTimeout);
  void setRecvTimeout(int recvTimeout);
  void setListenCallback(const port_func_t& listenCallback) {
    listenCallback_ = listenCallback;
  }

  int getPort() const { return port_; }

  std::shared_ptr<DPDKResources> getDPDKResources() const {
        return dpdkResources_;
    }

protected:
  std::shared_ptr<TTransport> acceptImpl() override;
  virtual std::shared_ptr<TUDPSocket> createSocket(uint16_t portId);
  
  bool initDPDK();
  bool setupDPDKPort();
  struct rte_mempool* createMempool();

private:
  int port_;
  bool isInitialized_;  
  
  // DPDK specific members
  std::shared_ptr<DPDKResources> dpdkResources_;
  
  int sendTimeout_;
  int recvTimeout_;
  
  concurrency::Mutex mutex_;
  port_func_t listenCallback_;
};

}}} // apache::thrift::transport

#endif
