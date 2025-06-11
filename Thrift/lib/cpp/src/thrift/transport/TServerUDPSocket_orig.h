#ifndef _THRIFT_TRANSPORT_TSERVERUDPSOCKET_H_
#define _THRIFT_TRANSPORT_TSERVERUDPSOCKET_H_ 1

#include <functional>

#include <thrift/concurrency/Mutex.h>
#include <thrift/transport/PlatformSocket.h>
#include <thrift/transport/TServerTransport.h>

#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

namespace apache {
namespace thrift {
namespace transport {

class TUDPSocket;

/**
 * Server socket implementation of TServerTransport. Wrapper around a UDP socket
 * for receiving datagrams.
 */
class TServerUDPSocket : public TServerTransport {
public:
  typedef std::function<void(THRIFT_SOCKET fd)> socket_func_t;

  const static int DEFAULT_BACKLOG = 1024;

  /**
   * Constructor.
   *
   * @param port    Port number to bind to
   */
  TServerUDPSocket(int port);

  /**
   * Constructor.
   *
   * @param port        Port number to bind to
   * @param sendTimeout Socket send timeout
   * @param recvTimeout Socket receive timeout
   */
  TServerUDPSocket(int port, int sendTimeout, int recvTimeout);

  /**
   * Constructor.
   *
   * @param address Address to bind to
   * @param port    Port number to bind to
   */
  TServerUDPSocket(const std::string& address, int port); 

  /**
   * Constructor for Unix domain socket.
   * 
   * @param path Pathname for unix socket 
   */
  TServerUDPSocket(const std::string& path);

  ~TServerUDPSocket() override;

  bool isOpen() const override;

  void setSendTimeout(int sendTimeout);
  void setRecvTimeout(int recvTimeout);

  void setTcpSendBuffer(int tcpSendBuffer);
  void setTcpRecvBuffer(int tcpRecvBuffer);

  // listenCallback gets called just before listen
  void setListenCallback(const socket_func_t& listenCallback) { listenCallback_ = listenCallback; }

  THRIFT_SOCKET getSocketFD() override { return serverSocket_; }

  int getPort() const;
  std::string getPath() const;
  bool isUnixDomainSocket() const;

  void listen() override;
  void interrupt() override;
  void close() override;

protected:
  std::shared_ptr<TTransport> acceptImpl() override;
  virtual std::shared_ptr<TUDPSocket> createSocket(THRIFT_SOCKET client);

private:
  void notify(THRIFT_SOCKET notifySock);
  void _setup_sockopts();
  void _setup_unixdomain_sockopts();

  int port_;
  std::string address_;
  std::string path_;
  THRIFT_SOCKET serverSocket_;
  int sendTimeout_;
  int recvTimeout_;
  int tcpSendBuffer_;
  int tcpRecvBuffer_;
  bool listening_;

  concurrency::Mutex rwMutex_;                     // thread-safe interrupt
  THRIFT_SOCKET interruptSockWriter_;              // is notified on interrupt()
  THRIFT_SOCKET interruptSockReader_;              // is used in select/poll with serverSocket_

  socket_func_t listenCallback_;
};

}
}
} // apache::thrift::transport

#endif // #ifndef _THRIFT_TRANSPORT_TSERVERUDPSOCKET_H_
