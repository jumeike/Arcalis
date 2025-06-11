#ifndef _THRIFT_TRANSPORT_TSERVERUDPSOCKET_H_
#define _THRIFT_TRANSPORT_TSERVERUDPSOCKET_H_ 1

#include <functional>
#include <thrift/concurrency/Mutex.h>
#include <thrift/transport/PlatformSocket.h>
#include <thrift/transport/TServerTransport.h>

#include <ff_api.h>
#include <ff_epoll.h>
#include <sys/ioctl.h>

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
 * UDP Server socket implementation of TServerTransport. 
 * Wrapper around UDP socket functionality.
 */
class TServerUDPSocket : public TServerTransport {
public:
  typedef std::function<void(THRIFT_SOCKET fd)> socket_func_t;

  const static int DEFAULT_BACKLOG = 1024;
  const static int MAX_UDP_PACKET = 65507;  // Maximum UDP packet size

  /**
   * Constructor.
   * @param port Port number to bind to
   */
  TServerUDPSocket(int port, bool usingKq);

  /**
   * Constructor.
   * @param port Port number to bind to
   * @param sendTimeout Socket send timeout
   * @param recvTimeout Socket receive timeout
   */
  TServerUDPSocket(int port, int sendTimeout, int recvTimeout, bool usingKq);

  /**
   * Constructor.
   * @param address Address to bind to
   * @param port Port number to bind to
   */
  TServerUDPSocket(const std::string& address, int port, bool usingKq);

  ~TServerUDPSocket() override;

  bool isOpen() const override;

  void setSendTimeout(int sendTimeout);
  void setRecvTimeout(int recvTimeout);

  void setUDPRecvBuffer(int recvBuffer); 
  void setUDPSendBuffer(int sendBuffer);

  // Callback gets called just before bind, after socket options are set
  void setBindCallback(const socket_func_t& bindCallback) { bindCallback_ = bindCallback; }

  THRIFT_SOCKET getSocketFD() override { return serverSocket_; }

  int getPort() const;

  void listen() override;
  void interrupt() override;
  void close() override;

  // Access to kqueue/epoll descriptors and events
  int getKqueueDescriptor() const { return kq_; }
  int getEpfdDescriptor() const { return epfd_; }

  const struct kevent& getEvent() const { return event_; }
  const struct kevent& getTriggerEvent() const { return triggerEvent_; }

  const struct kevent* getEvents() const { return events_; }
  struct epoll_event* getEpollEvents() { return events_epoll_; }

protected:
  std::shared_ptr<TTransport> acceptImpl() override;
  virtual std::shared_ptr<TUDPSocket> createSocket(THRIFT_SOCKET socket);

  // Event handling members
  int kq_;
  bool usingKqueue_;
  struct kevent event_;
  struct kevent triggerEvent_;
  struct kevent events_[DEFAULT_BACKLOG];
  int epfd_;
  struct epoll_event ev_epoll_;
  struct epoll_event events_epoll_[DEFAULT_BACKLOG];

private:
  void notify(THRIFT_SOCKET notifySock);
  void _setup_sockopts();

  int port_;
  std::string address_;
  THRIFT_SOCKET serverSocket_;
  int sendTimeout_;
  int recvTimeout_;
  int udpRecvBuffer_;
  int udpSendBuffer_;
  bool listening_;
  int retryLimit_;
  int retryDelay_;

  concurrency::Mutex rwMutex_;          // thread-safe interrupt
  THRIFT_SOCKET interruptSockWriter_;   // notified on interrupt()
  THRIFT_SOCKET interruptSockReader_;   // used in select/poll with serverSocket_

  socket_func_t bindCallback_;
};

}}} // apache::thrift::transport

#endif // #ifndef _THRIFT_TRANSPORT_TSERVERUDPSOCKET_H_
