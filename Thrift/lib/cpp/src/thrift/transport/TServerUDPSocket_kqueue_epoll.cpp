#include <thrift/transport/TServerUDPSocket.h>
#include <thrift/transport/TUDPSocket.h>
#include <thrift/transport/TSocketUtils.h>
#include <thrift/transport/SocketCommon.h>

#include <cstring>
#include <memory>

#ifndef SOCKOPT_CAST_T
#ifndef _WIN32
#define SOCKOPT_CAST_T void
#else
#define SOCKOPT_CAST_T char
#endif // _WIN32
#endif

template <class T>
inline const SOCKOPT_CAST_T* const_cast_sockopt(const T* v) {
  return reinterpret_cast<const SOCKOPT_CAST_T*>(v);
}

template <class T>
inline SOCKOPT_CAST_T* cast_sockopt(T* v) {
  return reinterpret_cast<SOCKOPT_CAST_T*>(v);
}

using std::shared_ptr;
using std::string;

namespace apache {
namespace thrift {
namespace transport {

TServerUDPSocket::TServerUDPSocket(int port, bool usingKq)
  : usingKqueue_(usingKq),
    event_({0, 0, 0, 0, 0, 0, {0}}),
    triggerEvent_({0, 0, 0, 0, 0, 0, {0}}),
    port_(port),
    serverSocket_(THRIFT_INVALID_SOCKET),
    sendTimeout_(0),
    recvTimeout_(0),
    udpRecvBuffer_(MAX_UDP_PACKET),
    udpSendBuffer_(0),
    retryLimit_(0),
    retryDelay_(0),
    listening_(false),
    interruptSockWriter_(THRIFT_INVALID_SOCKET),
    interruptSockReader_(THRIFT_INVALID_SOCKET) {
  std::memset(events_, 0, sizeof(events_));
  std::memset(&ev_epoll_, 0, sizeof(ev_epoll_));
  std::memset(events_epoll_, 0, sizeof(events_epoll_));
  if(usingKqueue_){
    kq_ = ff_kqueue();
  } else {
    epfd_ = ff_epoll_create(1);
  }
}

TServerUDPSocket::TServerUDPSocket(int port, int sendTimeout, int recvTimeout, bool usingKq)
  : usingKqueue_(usingKq),
    event_({0, 0, 0, 0, 0, 0, {0}}),
    triggerEvent_({0, 0, 0, 0, 0, 0, {0}}),
    port_(port),
    serverSocket_(THRIFT_INVALID_SOCKET),
    sendTimeout_(sendTimeout),
    recvTimeout_(recvTimeout),
    udpRecvBuffer_(MAX_UDP_PACKET),
    udpSendBuffer_(0),
    retryLimit_(0),
    retryDelay_(0),
    listening_(false),
    interruptSockWriter_(THRIFT_INVALID_SOCKET),
    interruptSockReader_(THRIFT_INVALID_SOCKET) {
  std::memset(events_, 0, sizeof(events_));
  std::memset(&ev_epoll_, 0, sizeof(ev_epoll_));
  std::memset(events_epoll_, 0, sizeof(events_epoll_));
  if(usingKqueue_){
    kq_ = ff_kqueue();
  }
  else {
    epfd_ = ff_epoll_create(1);
  }
}

TServerUDPSocket::TServerUDPSocket(const string& address, int port, bool usingKq)
  : usingKqueue_(usingKq),
    event_({0, 0, 0, 0, 0, 0, {0}}),
    triggerEvent_({0, 0, 0, 0, 0, 0, {0}}),
    port_(port),
    address_(address),
    serverSocket_(THRIFT_INVALID_SOCKET),
    sendTimeout_(0),
    recvTimeout_(0),
    udpRecvBuffer_(MAX_UDP_PACKET),
    udpSendBuffer_(0),
    retryLimit_(0),
    retryDelay_(0),
    listening_(false),
    interruptSockWriter_(THRIFT_INVALID_SOCKET),
    interruptSockReader_(THRIFT_INVALID_SOCKET) {
  std::memset(events_, 0, sizeof(events_));
  std::memset(&ev_epoll_, 0, sizeof(ev_epoll_));
  std::memset(events_epoll_, 0, sizeof(events_epoll_));
  if(usingKqueue_){
    kq_ = ff_kqueue();
  }
  else {
    epfd_ = ff_epoll_create(1);
  }
}

TServerUDPSocket::~TServerUDPSocket() {
  close();
}

void TServerUDPSocket::_setup_sockopts() {
  // Set UDP buffer sizes if specified
  if (udpRecvBuffer_ > 0) {
    if (-1 == ff_setsockopt(serverSocket_, 
                           SOL_SOCKET, 
                           SO_RCVBUF, 
                           cast_sockopt(&udpRecvBuffer_), 
                           sizeof(udpRecvBuffer_))) {
      int errno_copy = THRIFT_GET_SOCKET_ERROR;
      GlobalOutput.perror("TServerUDPSocket::_setup_sockopts() SO_RCVBUF ", errno_copy);
      close();
      throw TTransportException(TTransportException::NOT_OPEN,
                               "Could not set SO_RCVBUF",
                               errno_copy);
    }
  }

  if (udpSendBuffer_ > 0) {
    if (-1 == ff_setsockopt(serverSocket_, 
                           SOL_SOCKET, 
                           SO_SNDBUF, 
                           cast_sockopt(&udpSendBuffer_), 
                           sizeof(udpSendBuffer_))) {
      int errno_copy = THRIFT_GET_SOCKET_ERROR;
      GlobalOutput.perror("TServerUDPSocket::_setup_sockopts() SO_SNDBUF ", errno_copy);
      close();
      throw TTransportException(TTransportException::NOT_OPEN,
                               "Could not set SO_SNDBUF",
                               errno_copy);
    }
  }

  // Set non-blocking mode
  int flags = 1;
  if (ff_ioctl(serverSocket_, FIONBIO, &flags) == -1) {
    int errno_copy = THRIFT_GET_SOCKET_ERROR;
    GlobalOutput.perror("TServerUDPSocket::_setup_sockopts() FIONBIO ", errno_copy);
    close();
    throw TTransportException(TTransportException::NOT_OPEN,
                             "Could not set FIONBIO",
                             errno_copy);
  }
}

void TServerUDPSocket::listen() {
  // Validate port
  if (port_ < 0 || port_ > 0xFFFF) {
    throw TTransportException(TTransportException::BAD_ARGS, "Specified port is invalid");
  }

  // Resolve host:port strings into an iterable of struct addrinfo*
  AddressResolutionHelper resolved_addresses;
  try {
    resolved_addresses.resolve(address_, std::to_string(port_), SOCK_DGRAM,
#ifdef ANDROID
                             AI_PASSIVE | AI_ADDRCONFIG);
#else
                             AI_PASSIVE | AI_V4MAPPED);
#endif
  } catch (const std::system_error& e) {
    GlobalOutput.printf("getaddrinfo() -> %d; %s", e.code().value(), e.what());
    close();
    throw TTransportException(TTransportException::NOT_OPEN,
                             "Could not resolve host for server socket.");
  }

  // Try each resolved address until we find one that works
  int retries = 0;
  int errno_copy = 0;
  auto addr_iter = AddressResolutionHelper::Iter{};

  do {
    if (!addr_iter) {
      addr_iter = resolved_addresses.iterate();
    }
    auto trybind = *addr_iter++;

    serverSocket_ = ff_socket(trybind->ai_family, SOCK_DGRAM, IPPROTO_UDP);
    if (serverSocket_ == -1) {
      errno_copy = THRIFT_GET_SOCKET_ERROR;
      continue;
    }

    _setup_sockopts();

#ifdef IPV6_V6ONLY
    if (trybind->ai_family == AF_INET6) {
      int zero = 0;
      if (-1 == ff_setsockopt(serverSocket_, 
                             IPPROTO_IPV6, 
                             IPV6_V6ONLY, 
                             cast_sockopt(&zero), 
                             sizeof(zero))) {
        GlobalOutput.perror("TServerUDPSocket::listen() IPV6_V6ONLY ", THRIFT_GET_SOCKET_ERROR);
      }
    }
#endif

    if (bindCallback_) {
      bindCallback_(serverSocket_);
    }

    if (0 == ff_bind(serverSocket_, 
                     reinterpret_cast<const linux_sockaddr*>(trybind->ai_addr), 
                     static_cast<int>(trybind->ai_addrlen))) {
      break;
    }

    errno_copy = THRIFT_GET_SOCKET_ERROR;
    ff_close(serverSocket_);
    serverSocket_ = THRIFT_INVALID_SOCKET;

  } while ((retries++ < retryLimit_) && (THRIFT_SLEEP_SEC(retryDelay_) == 0));

  // Get the bound port if it was assigned by the system
  if (port_ == 0 && retries <= retryLimit_) {
    struct sockaddr_storage sa;
    socklen_t len = sizeof(sa);
    std::memset(&sa, 0, len);
    if (ff_getsockname(serverSocket_, reinterpret_cast<linux_sockaddr*>(&sa), &len) < 0) {
      errno_copy = THRIFT_GET_SOCKET_ERROR;
      GlobalOutput.perror("TServerUDPSocket::listen() getsockname() ", errno_copy);
    } else {
      if (sa.ss_family == AF_INET6) {
        const auto* sin = reinterpret_cast<const struct sockaddr_in6*>(&sa);
        port_ = ntohs(sin->sin6_port);
      } else {
        const auto* sin = reinterpret_cast<const struct sockaddr_in*>(&sa);
        port_ = ntohs(sin->sin_port);
      }
    }
  }

  // Check if we successfully bound to any address
  if (serverSocket_ == THRIFT_INVALID_SOCKET) {
    GlobalOutput.perror("TServerUDPSocket::listen() socket() ", errno_copy);
    throw TTransportException(TTransportException::NOT_OPEN,
                             "Could not create server socket.",
                             errno_copy);
  }

  // Set up event monitoring (kqueue/epoll)
  if (kq_ == -1 && epfd_ == -1) {
    int errno_copy = THRIFT_GET_SOCKET_ERROR;
    GlobalOutput.perror("TServerUDPSocket::listen() event system init failed ", errno_copy);
    throw TTransportException(TTransportException::UNKNOWN, "Event system init failed", errno_copy);
  }

  if (usingKqueue_) {
    EV_SET(&event_, serverSocket_, EVFILT_READ, EV_ADD, 0, 0, NULL);
    if (ff_kevent(kq_, &event_, 1, NULL, 0, NULL) == -1) {
      int errno_copy = THRIFT_GET_SOCKET_ERROR;
      GlobalOutput.perror("TServerUDPSocket::listen() kevent() ", errno_copy);
      close();
      throw TTransportException(TTransportException::UNKNOWN, "kevent()", errno_copy);
    }
  } else {
    ev_epoll_.events = EPOLLIN;
    ev_epoll_.data.fd = serverSocket_;
    if (ff_epoll_ctl(epfd_, EPOLL_CTL_ADD, serverSocket_, &ev_epoll_) == -1) {
      int errno_copy = THRIFT_GET_SOCKET_ERROR;
      GlobalOutput.perror("TServerUDPSocket::listen() epoll_ctl() ", errno_copy);
      close();
      throw TTransportException(TTransportException::UNKNOWN, "epoll_ctl()", errno_copy);
    }
  }
  
  listening_ = true;
}

shared_ptr<TTransport> TServerUDPSocket::acceptImpl() {
  if (!isOpen()) {
    throw TTransportException(TTransportException::NOT_OPEN, "UDP Server not listening");
  }

  // For UDP, just create a new TUDPSocket with the server socket
  // No need to wait for events here - that happens in read()
  shared_ptr<TUDPSocket> client = createSocket(serverSocket_);
  client->isUDP_ = true;
  client->usingKqueue_ = usingKqueue_;

  if (sendTimeout_ > 0) {
    client->setSendTimeout(sendTimeout_);
  }
  if (recvTimeout_ > 0) {
    client->setRecvTimeout(recvTimeout_);
  }

  return client;
}

shared_ptr<TUDPSocket> TServerUDPSocket::createSocket(THRIFT_SOCKET socket) {
  bool isUDP = true;
  return std::make_shared<TUDPSocket>(socket, this, isUDP, usingKqueue_);
}

void TServerUDPSocket::interrupt() {
  concurrency::Guard g(rwMutex_);
  if (interruptSockWriter_ != THRIFT_INVALID_SOCKET) {
    notify(interruptSockWriter_);
  }
}

void TServerUDPSocket::close() {
  concurrency::Guard g(rwMutex_);
  
  if (serverSocket_ != THRIFT_INVALID_SOCKET) {
    ff_shutdown(serverSocket_, SHUT_RDWR);
    ff_close(serverSocket_);
    serverSocket_ = THRIFT_INVALID_SOCKET;
  }

  if (interruptSockWriter_ != THRIFT_INVALID_SOCKET) {
    ff_close(interruptSockWriter_);
    interruptSockWriter_ = THRIFT_INVALID_SOCKET;
  }

  if (interruptSockReader_ != THRIFT_INVALID_SOCKET) {
    ff_close(interruptSockReader_);
    interruptSockReader_ = THRIFT_INVALID_SOCKET;
  }

  if (kq_ != -1) {
    ff_close(kq_);
    kq_ = -1;
  }

  if (epfd_ != -1) {
    ff_close(epfd_);
    epfd_ = -1;
  }

  listening_ = false;
}

void TServerUDPSocket::notify(THRIFT_SOCKET notifySock) {
  if (notifySock != THRIFT_INVALID_SOCKET) {
    int8_t byte = 0;
    if (-1 == send(notifySock, cast_sockopt(&byte), sizeof(int8_t), 0)) {
      GlobalOutput.perror("TServerUDPSocket::notify() send() ", THRIFT_GET_SOCKET_ERROR);
    }
  }
}

bool TServerUDPSocket::isOpen() const {
  return (serverSocket_ != THRIFT_INVALID_SOCKET) && listening_;
}

int TServerUDPSocket::getPort() const {
  return port_;
}

void TServerUDPSocket::setSendTimeout(int sendTimeout) {
  sendTimeout_ = sendTimeout;
}

void TServerUDPSocket::setRecvTimeout(int recvTimeout) {
  recvTimeout_ = recvTimeout;
}

void TServerUDPSocket::setUDPRecvBuffer(int recvBuffer) {
  udpRecvBuffer_ = recvBuffer;
  
  if (serverSocket_ != THRIFT_INVALID_SOCKET) {
    if (-1 == ff_setsockopt(serverSocket_,
                           SOL_SOCKET,
                           SO_RCVBUF,
                           cast_sockopt(&recvBuffer),
                           sizeof(recvBuffer))) {
      int errno_copy = THRIFT_GET_SOCKET_ERROR;
      GlobalOutput.perror("TServerUDPSocket::setUDPRecvBuffer() SO_RCVBUF ", errno_copy);
    }
  }
}

void TServerUDPSocket::setUDPSendBuffer(int sendBuffer) {
  udpSendBuffer_ = sendBuffer;
  
  if (serverSocket_ != THRIFT_INVALID_SOCKET) {
    if (-1 == ff_setsockopt(serverSocket_,
                           SOL_SOCKET,
                           SO_SNDBUF,
                           cast_sockopt(&sendBuffer),
                           sizeof(sendBuffer))) {
      int errno_copy = THRIFT_GET_SOCKET_ERROR;
      GlobalOutput.perror("TServerUDPSocket::setUDPSendBuffer() SO_SNDBUF ", errno_copy);
    }
  }
}

}}} // apache::thrift::transport
