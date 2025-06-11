// TUDPSocket.h
#ifndef _THRIFT_TRANSPORT_TUDPSOCKET_H_
#define _THRIFT_TRANSPORT_TUDPSOCKET_H_ 1

#include <string>
#include <sstream> 
#include <netinet/in.h>  
#include <arpa/inet.h>  
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include "DPDKResources.h"

#include <thrift/transport/TTransport.h>
#include <thrift/transport/TVirtualTransport.h>

namespace apache {
namespace thrift {
namespace transport {

class TUDPSocket : public TVirtualTransport<TUDPSocket> {
public:
  static const uint16_t RX_RING_SIZE = 1024;
  static const uint16_t TX_RING_SIZE = 1024;
  static const uint16_t NUM_MBUFS = 8191;
  static const uint16_t MBUF_CACHE_SIZE = 250;
  static const uint16_t BURST_SIZE = 32;

  TUDPSocket(std::shared_ptr<TConfiguration> config = nullptr);
  TUDPSocket(const std::string& host, int port, std::shared_ptr<TConfiguration> config = nullptr);
  TUDPSocket(std::shared_ptr<DPDKResources> dpdkResources, 
               std::shared_ptr<TConfiguration> config = nullptr);
  ~TUDPSocket() override;

  bool isOpen() const override;
  bool peek() override;
  void open() override;
  void close() override;
  
  uint32_t read(uint8_t* buf, uint32_t len);
  void write(const uint8_t* buf, uint32_t len);
  uint32_t write_partial(const uint8_t* buf, uint32_t len);

  void setRecvTimeout(int ms);
  void setSendTimeout(int ms);
  
  std::string getHost() const { return host_; }
  int getPort() const { return port_; }
  const std::string getOrigin() const override;
  uint32_t GetLocalIPAddress(uint16_t port_id);
  void setLocalIpAddress(const std::string& ipStr);

protected:
  bool initDPDK();
  bool setupDPDKPort();
  struct rte_mempool* createMempool();
  
private:
  std::string host_;
  int port_;
  bool isInitialized_;
  uint32_t localIpAddress_;
  
  // DPDK specific members
  std::shared_ptr<DPDKResources> dpdkResources_;
  struct sockaddr_in peerAddr_;  // Keep this for UDP addressing
  void handleArpPacket(struct rte_mbuf* m);

  int sendTimeout_;
  int recvTimeout_;
};

}}} // apache::thrift::transport

#endif 
