#include <thrift/thrift-config.h>

#include <cstring>
#include <sstream>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif
#include <sys/types.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>

#include <thrift/concurrency/Monitor.h>
#include <thrift/transport/TUDPSocket.h>
#include <thrift/transport/TTransportException.h>
#include <thrift/transport/PlatformSocket.h>
#include <thrift/transport/SocketCommon.h>

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

using std::string;

namespace apache {
namespace thrift {
namespace transport {

TUDPSocket::TUDPSocket(const string& host, int port, std::shared_ptr<TConfiguration> config)
  : TVirtualTransport(config),
    host_(host),
    port_(port),
    socket_(THRIFT_INVALID_SOCKET),
    peerPort_(0),
    sendTimeout_(0),
    recvTimeout_(0),
    maxRecvRetries_(5) {
  cachedPeerAddr_.ipv4.sin_family = AF_UNSPEC;
}

TUDPSocket::TUDPSocket(const string& path, std::shared_ptr<TConfiguration> config)
  : TVirtualTransport(config),
    port_(0),
    path_(path),
    socket_(THRIFT_INVALID_SOCKET),
    peerPort_(0),
    sendTimeout_(0),
    recvTimeout_(0),
    maxRecvRetries_(5) {
  cachedPeerAddr_.ipv4.sin_family = AF_UNSPEC;
}

TUDPSocket::TUDPSocket(THRIFT_SOCKET socket, std::shared_ptr<TConfiguration> config)
  : TVirtualTransport(config),
    port_(0),
    socket_(socket),
    peerPort_(0),
    sendTimeout_(0),
    recvTimeout_(0),
    maxRecvRetries_(5) {
  cachedPeerAddr_.ipv4.sin_family = AF_UNSPEC;
  peerAddrLen_ = 0;
}

TUDPSocket::TUDPSocket(std::shared_ptr<TConfiguration> config)
  : TVirtualTransport(config),
    port_(0),
    socket_(THRIFT_INVALID_SOCKET),
    peerPort_(0),
    sendTimeout_(0),
    recvTimeout_(0),
    maxRecvRetries_(5) {
  cachedPeerAddr_.ipv4.sin_family = AF_UNSPEC;
}

TUDPSocket::~TUDPSocket() {
  close();
}

bool TUDPSocket::isOpen() const {
  return socket_ != THRIFT_INVALID_SOCKET;
}

bool TUDPSocket::peek() {
  if (!isOpen()) {
    return false;
  }

  struct pollfd fds[1];
  std::memset(fds, 0, sizeof(fds));
  fds[0].fd = socket_;
  fds[0].events = POLLIN;
  
  int ret = THRIFT_POLL(fds, 1, 0);
  return (ret > 0 && (fds[0].revents & POLLIN));
}

void TUDPSocket::openConnection(struct addrinfo* res) {
  if (isOpen()) {
    return;
  }

  if (isUnixDomainSocket()) {
    socket_ = socket(PF_UNIX, SOCK_DGRAM, IPPROTO_IP);
  } else {
    socket_ = socket(res->ai_family, SOCK_DGRAM, res->ai_protocol);
  }

  if (socket_ == THRIFT_INVALID_SOCKET) {
    int errno_copy = THRIFT_GET_SOCKET_ERROR;
    GlobalOutput.perror("TUDPSocket::open() socket() " + getSocketInfo(), errno_copy);
    throw TTransportException(TTransportException::NOT_OPEN, "socket()", errno_copy);
  }

  // Send timeout
  if (sendTimeout_ > 0) {
    setSendTimeout(sendTimeout_);
  }

  // Recv timeout
  if (recvTimeout_ > 0) {
    setRecvTimeout(recvTimeout_);
  }

  if (!isUnixDomainSocket()) {
    // Allow broadcast
    int one = 1;
    if (setsockopt(socket_, SOL_SOCKET, SO_BROADCAST, 
                   cast_sockopt(&one), sizeof(one)) == -1) {
      int errno_copy = THRIFT_GET_SOCKET_ERROR;
      GlobalOutput.perror("TUDPSocket::open() setsockopt() SO_BROADCAST " + getSocketInfo(), 
                         errno_copy);
    }

    // Connect if we have a specific destination
    if (!host_.empty()) {
      if (connect(socket_, res->ai_addr, static_cast<int>(res->ai_addrlen)) == -1) {
        int errno_copy = THRIFT_GET_SOCKET_ERROR;
        GlobalOutput.perror("TUDPSocket::open() connect() " + getSocketInfo(), errno_copy);
        throw TTransportException(TTransportException::NOT_OPEN, "connect() failed", errno_copy);
      }
    }

    setCachedAddress(res->ai_addr, static_cast<socklen_t>(res->ai_addrlen));
  } else {
#if (!defined(_WIN32) || defined(HAVE_AF_UNIX_H))
    struct sockaddr_un address;
    socklen_t structlen = fillUnixSocketAddr(address, path_);

    if (connect(socket_, (struct sockaddr*)&address, structlen) == -1) {
      int errno_copy = THRIFT_GET_SOCKET_ERROR;
      GlobalOutput.perror("TUDPSocket::open() connect() " + getSocketInfo(), errno_copy);
      throw TTransportException(TTransportException::NOT_OPEN, "connect() failed", errno_copy);
    }
#else
    GlobalOutput.perror("TUDPSocket::open() Unix Domain socket path not supported on this version of Windows", -99);
    throw TTransportException(TTransportException::NOT_OPEN,
                            "Unix Domain socket path not supported");
#endif
  }
}

void TUDPSocket::open() {
  if (isOpen()) {
    return;
  }
  if (isUnixDomainSocket()) {
    unix_open();
  } else {
    local_open();
  }
}

void TUDPSocket::unix_open() {
  if (isUnixDomainSocket()) {
    // Unix Domain Socket does not need addrinfo struct, so we pass NULL
    openConnection(nullptr);
  }
}

void TUDPSocket::local_open() {

#ifdef _WIN32
  TWinsockSingleton::create();
#endif // _WIN32

  if (isOpen()) {
    return;
  }

  // Validate port number
  if (port_ < 0 || port_ > 0xFFFF) {
    throw TTransportException(TTransportException::BAD_ARGS, "Specified port is invalid");
  }

  struct addrinfo hints, *res, *res0;
  res = nullptr;
  res0 = nullptr;
  int error;
  char port[sizeof("65535")];
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
  sprintf(port, "%d", port_);

  error = getaddrinfo(host_.empty() ? nullptr : host_.c_str(), port, &hints, &res0);

  if (error) {
    string errStr = "TUDPSocket::open() getaddrinfo() " + getSocketInfo() + string(THRIFT_GAI_STRERROR(error));
    GlobalOutput(errStr.c_str());
    close();
    throw TTransportException(TTransportException::NOT_OPEN, "Could not resolve host for client socket.");
  }

  // Cycle through all the returned addresses until one works
  for (res = res0; res; res = res->ai_next) {
    try {
      openConnection(res);
      break;
    } catch (TTransportException&) {
      if (res->ai_next) {
        close();
      } else {
        close();
        freeaddrinfo(res0);
        throw;
      }
    }
  }

  // Free address structure memory
  freeaddrinfo(res0);
}

void TUDPSocket::close() {
  if (socket_ != THRIFT_INVALID_SOCKET) {
    ::THRIFT_CLOSESOCKET(socket_);
  }
  socket_ = THRIFT_INVALID_SOCKET;
}

uint32_t TUDPSocket::read(uint8_t* buf, uint32_t len) {
  if (socket_ == THRIFT_INVALID_SOCKET) {
    throw TTransportException(TTransportException::NOT_OPEN, "Called read on non-open socket");
  }

  int32_t retries = 0;

  // THRIFT_EAGAIN can be signaled both when a timeout has occurred and when
  // the system is out of resources (an awesome undocumented feature).
  // The following is an approximation of the time interval under which
  // THRIFT_EAGAIN is taken to indicate an out of resources error.
  uint32_t eagainThresholdMicros = 0;
  if (recvTimeout_) {
    // if a readTimeout is specified along with a max number of recv retries, then
    // the threshold will ensure that the read timeout is not exceeded even in the
    // case of resource errors
    eagainThresholdMicros = (recvTimeout_ * 1000) / ((maxRecvRetries_ > 0) ? maxRecvRetries_ : 2);
  }

try_again:
  // Read from the socket
  struct timeval begin;
  if (recvTimeout_ > 0) {
    THRIFT_GETTIMEOFDAY(&begin, nullptr);
  }

  sockaddr_storage peer_addr;
  socklen_t peer_addr_len = sizeof(peer_addr);
  int got = static_cast<int>(recvfrom(socket_, 
                                     cast_sockopt(buf), 
                                     len,
                                     0,
                                     (struct sockaddr*)&peer_addr,
                                     &peer_addr_len));

  if (got > 0) {
            memcpy(&peerAddr_, &peer_addr, peer_addr_len);
            peerAddrLen_ = peer_addr_len;
            //fprintf(stderr, "Debug: read() operation was successful, returning got value: %d\n", got);
            return got;
  }

  // Check for error on read
  if (got < 0) {
    int errno_copy = THRIFT_GET_SOCKET_ERROR;

    if (errno_copy == THRIFT_EAGAIN) {
      // if no timeout we can assume that resource exhaustion has occurred.
      if (recvTimeout_ == 0) {
        throw TTransportException(TTransportException::TIMED_OUT,
                                "THRIFT_EAGAIN (unavailable resources)");
      }
      // check if this is the lack of resources or timeout case
      struct timeval end;
      THRIFT_GETTIMEOFDAY(&end, nullptr);
      auto readElapsedMicros = static_cast<uint32_t>(((end.tv_sec - begin.tv_sec) * 1000 * 1000)
                                                    + (end.tv_usec - begin.tv_usec));

      if (!eagainThresholdMicros || (readElapsedMicros < eagainThresholdMicros)) {
        if (retries++ < maxRecvRetries_) {
          THRIFT_SLEEP_USEC(50);
          goto try_again;
        } else {
          throw TTransportException(TTransportException::TIMED_OUT,
                                  "THRIFT_EAGAIN (unavailable resources)");
        }
      } else {
        // infer that timeout has been hit
        throw TTransportException(TTransportException::TIMED_OUT, "THRIFT_EAGAIN (timed out)");
      }
    }

    // If interrupted, try again
    if (errno_copy == THRIFT_EINTR && retries++ < maxRecvRetries_) {
      goto try_again;
    }

    // Now it's not a try again case, but a real problem
    GlobalOutput.perror("TUDPSocket::read() recvfrom() " + getSocketInfo(), errno_copy);
    throw TTransportException(TTransportException::UNKNOWN, "Unknown", errno_copy);
  }
   
  //fprintf(stderr, "Debug: read() operation was successful, returning got value: %d\n", got);
  return got;
}

// uint32_t TUDPSocket::read(uint8_t* buf, uint32_t len) {
//   if (socket_ == THRIFT_INVALID_SOCKET) {
//     throw TTransportException(TTransportException::NOT_OPEN, "Called read on non-open socket");
//   }

//   int32_t retries = 0;

//   // THRIFT_EAGAIN can be signaled both when a timeout has occurred and when
//   // the system is out of resources (an awesome undocumented feature).
//   // The following is an approximation of the time interval under which
//   // THRIFT_EAGAIN is taken to indicate an out of resources error.
//   uint32_t eagainThresholdMicros = 0;
//   if (recvTimeout_) {
//     // if a readTimeout is specified along with a max number of recv retries, then
//     // the threshold will ensure that the read timeout is not exceeded even in the
//     // case of resource errors
//     eagainThresholdMicros = (recvTimeout_ * 1000) / ((maxRecvRetries_ > 0) ? maxRecvRetries_ : 2);
//   }

// try_again:
//   // Read from the socket
//   struct timeval begin;
//   if (recvTimeout_ > 0) {
//     THRIFT_GETTIMEOFDAY(&begin, nullptr);
//   }

//   // Wait for data to be available with optional timeout
//   struct THRIFT_POLLFD fds[2];
//   std::memset(fds, 0, sizeof(fds));
//   fds[0].fd = socket_;
//   fds[0].events = THRIFT_POLLIN;
//   if (interruptListener_) {
//     fds[1].fd = *(interruptListener_.get());
//     fds[1].events = THRIFT_POLLIN;
//   }

//   int ret = THRIFT_POLL(fds, interruptListener_ ? 2 : 1, 
//                         (recvTimeout_ == 0) ? -1 : recvTimeout_);
  
//   if (ret < 0) {
//     // error cases
//     if (THRIFT_GET_SOCKET_ERROR == THRIFT_EINTR && (retries++ < maxRecvRetries_)) {
//       goto try_again;
//     }
//     int errno_copy = THRIFT_GET_SOCKET_ERROR;
//     GlobalOutput.perror("TUDPSocket::read() THRIFT_POLL() ", errno_copy);
//     throw TTransportException(TTransportException::UNKNOWN, "Unknown", errno_copy);
//   } else if (ret > 0) {
//     // Check for interrupt
//     if (interruptListener_ && (fds[1].revents & THRIFT_POLLIN)) {
//       int8_t buf;
//       if (-1 == recv(*(interruptListener_.get()), cast_sockopt(&buf), sizeof(int8_t), 0)) {
//         GlobalOutput.perror("TUDPSocket::read() recv() interrupt ", THRIFT_GET_SOCKET_ERROR);
//       }
//       throw TTransportException(TTransportException::INTERRUPTED);
//     }
    
//     // Check for actual data
//     if (fds[0].revents & THRIFT_POLLIN) {
//       sockaddr_storage peer_addr;
//       socklen_t peer_addr_len = sizeof(peer_addr);
      
//       int got = static_cast<int>(recvfrom(socket_,
//                                          cast_sockopt(buf),
//                                          len,
//                                          0,
//                                          (struct sockaddr*)&peer_addr,
//                                          &peer_addr_len));

//       if (got > 0) {
//         memcpy(&peerAddr_, &peer_addr, peer_addr_len);
//         peerAddrLen_ = peer_addr_len;
//         return got;
//       }

//       if (got == 0) {
//         return 0;
//       }

//       // Got an error
//       int errno_copy = THRIFT_GET_SOCKET_ERROR;
      
//       if (errno_copy == THRIFT_EAGAIN) {
//         if (recvTimeout_ == 0) {
//           throw TTransportException(TTransportException::TIMED_OUT,
//                                   "THRIFT_EAGAIN (unavailable resources)");
//         }
        
//         struct timeval end;
//         THRIFT_GETTIMEOFDAY(&end, nullptr);
//         uint32_t readElapsedMicros = static_cast<uint32_t>(
//             ((end.tv_sec - begin.tv_sec) * 1000 * 1000) + (end.tv_usec - begin.tv_usec));

//         if (!eagainThresholdMicros || (readElapsedMicros < eagainThresholdMicros)) {
//           if (retries++ < maxRecvRetries_) {
//             THRIFT_SLEEP_USEC(50);
//             goto try_again;
//           }
//         }
//         throw TTransportException(TTransportException::TIMED_OUT,
//                                 "THRIFT_EAGAIN (timed out)");
//       }

//       if (errno_copy == THRIFT_EINTR && retries++ < maxRecvRetries_) {
//         goto try_again;
//       }

//       GlobalOutput.perror("TUDPSocket::read() recvfrom() " + getSocketInfo(), errno_copy);
//       throw TTransportException(TTransportException::UNKNOWN, "Unknown", errno_copy);
//     }
//   } else {
//     // Timed out
//     throw TTransportException(TTransportException::TIMED_OUT,
//                             "THRIFT_POLL (timed out)");
//   }

//   throw TTransportException(TTransportException::UNKNOWN,
//                           "Unknown error");
// }

void TUDPSocket::write(const uint8_t* buf, uint32_t len) {
  // UDP is message oriented - no partial writes
  write_partial(buf, len);
}

uint32_t TUDPSocket::write_partial(const uint8_t* buf, uint32_t len) {
  if (socket_ == THRIFT_INVALID_SOCKET) {
    throw TTransportException(TTransportException::NOT_OPEN, "Called write on non-open socket");
  }

  uint32_t sent;

  sent = static_cast<uint32_t>(sendto(socket_,
                                      const_cast_sockopt(buf),
                                      len,
                                      0,
                                      (struct sockaddr*)&peerAddr_,
                                      peerAddrLen_));
  

  if (sent == static_cast<uint32_t>(-1)) {
    int errno_copy = THRIFT_GET_SOCKET_ERROR;
    GlobalOutput.perror("TUDPSocket::write_partial() send/sendto() " + getSocketInfo(), errno_copy);
    throw TTransportException(TTransportException::UNKNOWN, "send/sendto()", errno_copy);
  }

  //fprintf(stderr, "Debug: write() operation was successful, returning sent value: %d\n", sent);
  return sent;
}

string TUDPSocket::getHost() const {
  return host_;
}

int TUDPSocket::getPort() const {
  return port_;
}

string TUDPSocket::getPath() const {
  return path_;
}

bool TUDPSocket::isUnixDomainSocket() const {
  return !path_.empty();
}

void TUDPSocket::setHost(string host) {
  host_ = host;
}

void TUDPSocket::setPort(int port) {
  port_ = port;
}

void TUDPSocket::setPath(string path) {
  path_ = path;
}

void TUDPSocket::setRecvTimeout(int ms) {
  if (ms < 0) {
    char errBuf[512];
    sprintf(errBuf, "TUDPSocket::setRecvTimeout with negative input: %d\n", ms);
    GlobalOutput(errBuf);
    return;
  }
  recvTimeout_ = ms;

  if (socket_ == THRIFT_INVALID_SOCKET) {
    return;
  }

#ifdef _WIN32
  DWORD timeout = static_cast<DWORD>(ms);
#else
  struct timeval timeout = {(int)(ms / 1000), (int)((ms % 1000) * 1000)};
#endif

  if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, cast_sockopt(&timeout), sizeof(timeout)) == -1) {
    int errno_copy = THRIFT_GET_SOCKET_ERROR;
    GlobalOutput.perror("TUDPSocket::setRecvTimeout() setsockopt() SO_RCVTIMEO ", errno_copy);
  }
}

void TUDPSocket::setSendTimeout(int ms) {
  if (ms < 0) {
    char errBuf[512];
    sprintf(errBuf, "TUDPSocket::setSendTimeout with negative input: %d\n", ms);
    GlobalOutput(errBuf);
    return;
  }
  sendTimeout_ = ms;

  if (socket_ == THRIFT_INVALID_SOCKET) {
    return;
  }

#ifdef _WIN32
  DWORD timeout = static_cast<DWORD>(ms);
#else
  struct timeval timeout = {(int)(ms / 1000), (int)((ms % 1000) * 1000)};
#endif

  if (setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, cast_sockopt(&timeout), sizeof(timeout)) == -1) {
    int errno_copy = THRIFT_GET_SOCKET_ERROR;
    GlobalOutput.perror("TUDPSocket::setSendTimeout() setsockopt() SO_SNDTIMEO ", errno_copy);
  }
}

void TUDPSocket::setMaxRecvRetries(int maxRecvRetries) {
  maxRecvRetries_ = maxRecvRetries;
}

string TUDPSocket::getSocketInfo() const {
  std::ostringstream oss;
  if (!isUnixDomainSocket()) {
    if (host_.empty() || port_ == 0) {
      oss << "<Host: " << getPeerAddress();
      oss << " Port: " << getPeerPort() << ">";
    } else {
      oss << "<Host: " << host_ << " Port: " << port_ << ">";
    }
  } else {
    oss << "<Path: " << path_ << ">";
  }
  return oss.str();
}

void TUDPSocket::setCachedAddress(const sockaddr* addr, socklen_t len) {
  if (isUnixDomainSocket()) {
    return;
  }

  switch (addr->sa_family) {
    case AF_INET:
      if (len == sizeof(sockaddr_in)) {
        memcpy(&cachedPeerAddr_.ipv4, addr, len);
      }
      break;

    case AF_INET6:
      if (len == sizeof(sockaddr_in6)) {
        memcpy(&cachedPeerAddr_.ipv6, addr, len);
      }
      break;
  }
}

sockaddr* TUDPSocket::getCachedAddress(socklen_t* len) const {
  switch (cachedPeerAddr_.ipv4.sin_family) {
    case AF_INET:
      *len = sizeof(sockaddr_in);
      return (sockaddr*)&cachedPeerAddr_.ipv4;

    case AF_INET6:
      *len = sizeof(sockaddr_in6);
      return (sockaddr*)&cachedPeerAddr_.ipv6;

    default:
      return nullptr;
  }
}

string TUDPSocket::getPeerHost() const {
  if (peerHost_.empty() && !isUnixDomainSocket()) {
    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);

    if (socket_ == THRIFT_INVALID_SOCKET) {
      return host_;
    }

    if (getpeername(socket_, (sockaddr*)&addr, &addrLen) != 0) {
      return peerHost_;
    }

    char clienthost[NI_MAXHOST];
    char clientservice[NI_MAXSERV];

    getnameinfo((sockaddr*)&addr,
                addrLen,
                clienthost,
                sizeof(clienthost),
                clientservice,
                sizeof(clientservice),
                0);

    peerHost_ = clienthost;
  }
  return peerHost_;
}

string TUDPSocket::getPeerAddress() const {
  if (peerAddress_.empty() && !isUnixDomainSocket()) {
    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);

    if (socket_ == THRIFT_INVALID_SOCKET) {
      return peerAddress_;
    }

    if (getpeername(socket_, (sockaddr*)&addr, &addrLen) != 0) {
      return peerAddress_;
    }

    char clienthost[NI_MAXHOST];
    char clientservice[NI_MAXSERV];

    getnameinfo((sockaddr*)&addr,
                addrLen,
                clienthost,
                sizeof(clienthost),
                clientservice,
                sizeof(clientservice),
                NI_NUMERICHOST | NI_NUMERICSERV);

    peerAddress_ = clienthost;
    peerPort_ = std::atoi(clientservice);
  }
  return peerAddress_;
}

int TUDPSocket::getPeerPort() const {
  if (peerPort_ == 0 && !isUnixDomainSocket()) {
    getPeerAddress();
  }
  return peerPort_;
}

const string TUDPSocket::getOrigin() const {
  std::ostringstream oss;
  if (!isUnixDomainSocket()) {
    oss << getPeerHost() << ":" << getPeerPort();
  } else {
    oss << path_;
  }
  return oss.str();
}

}
}
} // apache::thrift::transport
