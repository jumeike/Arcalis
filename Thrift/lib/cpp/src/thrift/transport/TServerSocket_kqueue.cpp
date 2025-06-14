/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <thrift/thrift-config.h>

#include <cstring>
#include <memory>
#include <stdexcept>
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include <thrift/transport/PlatformSocket.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TSocketUtils.h>
#include <thrift/transport/SocketCommon.h>

// Include FF-Stack header
#include <ff_api.h>
#include <sys/ioctl.h>

#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif

#ifndef SOCKOPT_CAST_T
#ifndef _WIN32
#define SOCKOPT_CAST_T void
#else
#define SOCKOPT_CAST_T char
#endif // _WIN32
#endif

#ifdef _WIN32
// Including Windows.h can conflict with Winsock2 usage, and also
// adds problematic macros like min() and max(). Try to work around:
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef NOMINMAX
#undef WIN32_LEAN_AND_MEAN
#endif

template <class T>
inline const SOCKOPT_CAST_T* const_cast_sockopt(const T* v) {
  return reinterpret_cast<const SOCKOPT_CAST_T*>(v);
}

template <class T>
inline SOCKOPT_CAST_T* cast_sockopt(T* v) {
  return reinterpret_cast<SOCKOPT_CAST_T*>(v);
}

void destroyer_of_fine_sockets(THRIFT_SOCKET* ssock) {
  ::THRIFT_CLOSESOCKET(*ssock);
  delete ssock;
}

using std::shared_ptr;
using std::string;

namespace apache {
namespace thrift {
namespace transport {


TServerSocket::TServerSocket(int port)
  : interruptableChildren_(true),
    kq_(ff_kqueue()),
    usingKqueue_(true),
    usingEpoll_(false),
    event_({0, 0, 0, 0, 0, 0, {0}}),
    triggerEvent_({0, 0, 0, 0, 0, 0, {0}}),
    port_(port),
    serverSocket_(THRIFT_INVALID_SOCKET),
    acceptBacklog_(DEFAULT_BACKLOG),
    sendTimeout_(0),
    recvTimeout_(0),
    accTimeout_(-1),
    retryLimit_(0),
    retryDelay_(0),
    tcpSendBuffer_(0),
    tcpRecvBuffer_(0),
    keepAlive_(false),
    listening_(false),
    interruptSockWriter_(THRIFT_INVALID_SOCKET),
    interruptSockReader_(THRIFT_INVALID_SOCKET),
    childInterruptSockWriter_(THRIFT_INVALID_SOCKET) {
      std::memset(events_, 0, sizeof(events_));
}

TServerSocket::TServerSocket(int port, int sendTimeout, int recvTimeout)
  : interruptableChildren_(true),
    kq_(ff_kqueue()),
    usingKqueue_(true),
    usingEpoll_(false),
    event_({0, 0, 0, 0, 0, 0, {0}}),
    triggerEvent_({0, 0, 0, 0, 0, 0, {0}}),
    port_(port),
    serverSocket_(THRIFT_INVALID_SOCKET),
    acceptBacklog_(DEFAULT_BACKLOG),
    sendTimeout_(sendTimeout),
    recvTimeout_(recvTimeout),
    accTimeout_(-1),
    retryLimit_(0),
    retryDelay_(0),
    tcpSendBuffer_(0),
    tcpRecvBuffer_(0),
    keepAlive_(false),
    listening_(false),
    interruptSockWriter_(THRIFT_INVALID_SOCKET),
    interruptSockReader_(THRIFT_INVALID_SOCKET),
    childInterruptSockWriter_(THRIFT_INVALID_SOCKET) {
      std::memset(events_, 0, sizeof(events_));
}

TServerSocket::TServerSocket(const string& address, int port)
  : interruptableChildren_(true),
    kq_(ff_kqueue()),
    usingKqueue_(true),
    usingEpoll_(false),
    event_({0, 0, 0, 0, 0, 0, {0}}),
    triggerEvent_({0, 0, 0, 0, 0, 0, {0}}),
    port_(port),
    address_(address),
    serverSocket_(THRIFT_INVALID_SOCKET),
    acceptBacklog_(DEFAULT_BACKLOG),
    sendTimeout_(0),
    recvTimeout_(0),
    accTimeout_(-1),
    retryLimit_(0),
    retryDelay_(0),
    tcpSendBuffer_(0),
    tcpRecvBuffer_(0),
    keepAlive_(false),
    listening_(false),
    interruptSockWriter_(THRIFT_INVALID_SOCKET),
    interruptSockReader_(THRIFT_INVALID_SOCKET),
    childInterruptSockWriter_(THRIFT_INVALID_SOCKET) {
      std::memset(events_, 0, sizeof(events_));
}

TServerSocket::TServerSocket(const string& path)
  : interruptableChildren_(true),
    kq_(ff_kqueue()),
    usingKqueue_(true),
    usingEpoll_(false),
    event_({0, 0, 0, 0, 0, 0, {0}}),
    triggerEvent_({0, 0, 0, 0, 0, 0, {0}}),
    port_(0),
    path_(path),
    serverSocket_(THRIFT_INVALID_SOCKET),
    acceptBacklog_(DEFAULT_BACKLOG),
    sendTimeout_(0),
    recvTimeout_(0),
    accTimeout_(-1),
    retryLimit_(0),
    retryDelay_(0),
    tcpSendBuffer_(0),
    tcpRecvBuffer_(0),
    keepAlive_(false),
    listening_(false),
    interruptSockWriter_(THRIFT_INVALID_SOCKET),
    interruptSockReader_(THRIFT_INVALID_SOCKET),
    childInterruptSockWriter_(THRIFT_INVALID_SOCKET) {
      std::memset(events_, 0, sizeof(events_));
}

TServerSocket::~TServerSocket() {
  close();
}

bool TServerSocket::isOpen() const {
  if (serverSocket_ == THRIFT_INVALID_SOCKET)
    return false;

  if (!listening_)
    return false;

  if (isUnixDomainSocket() && (path_[0] != '\0')) {
    // On some platforms the domain socket file may not be instantly
    // available yet, i.e. the Windows file system can be slow. Therefore
    // we should check that the domain socket file actually exists.
#ifdef _MSC_VER
    // Currently there is a bug in ClangCl on Windows so the stat() call
    // does not work. Workaround is a Windows-specific call if file exists:
    DWORD const f_attrib = GetFileAttributesA(path_.c_str());
    if (f_attrib == INVALID_FILE_ATTRIBUTES) {
#else
    struct THRIFT_STAT path_info;
    if (::THRIFT_STAT(path_.c_str(), &path_info) < 0) {
#endif
      const std::string vError = "TServerSocket::isOpen(): The domain socket path '" + path_ + "' does not exist (yet).";
      GlobalOutput.perror(vError.c_str(), THRIFT_GET_SOCKET_ERROR);
      return false;
    }
  }

  return true;
}

void TServerSocket::setSendTimeout(int sendTimeout) {
  sendTimeout_ = sendTimeout;
}

void TServerSocket::setRecvTimeout(int recvTimeout) {
  recvTimeout_ = recvTimeout;
}

void TServerSocket::setAcceptTimeout(int accTimeout) {
  accTimeout_ = accTimeout;
}

void TServerSocket::setAcceptBacklog(int accBacklog) {
  acceptBacklog_ = accBacklog;
}

void TServerSocket::setRetryLimit(int retryLimit) {
  retryLimit_ = retryLimit;
}

void TServerSocket::setRetryDelay(int retryDelay) {
  retryDelay_ = retryDelay;
}

void TServerSocket::setTcpSendBuffer(int tcpSendBuffer) {
  tcpSendBuffer_ = tcpSendBuffer;
}

void TServerSocket::setTcpRecvBuffer(int tcpRecvBuffer) {
  tcpRecvBuffer_ = tcpRecvBuffer;
}

void TServerSocket::setInterruptableChildren(bool enable) {
  if (listening_) {
    throw std::logic_error("setInterruptableChildren cannot be called after listen()");
  }
  interruptableChildren_ = enable;
}

void TServerSocket::_setup_sockopts() {
  int one = 1;
  if (!isUnixDomainSocket()) {
    // Set THRIFT_NO_SOCKET_CACHING to prevent 2MSL delay on accept.
    // This does not work with Domain sockets on most platforms. And
    // on Windows it completely breaks the socket. Therefore do not
    // use this on Domain sockets.
    // if (-1 == setsockopt(serverSocket_,
    //                     SOL_SOCKET,
    //                     THRIFT_NO_SOCKET_CACHING,
    //                     cast_sockopt(&one),
    //                     sizeof(one))) {
    if (-1 == ff_setsockopt(serverSocket_,
                        SOL_SOCKET,
                        THRIFT_NO_SOCKET_CACHING,
                        cast_sockopt(&one),
                        sizeof(one))) {
      // NOTE: SO_EXCLUSIVEADDRUSE socket option can only be used by members
      // of the Administrators security group on Windows XP and earlier. But
      // we do not target WinXP anymore so no special checks required.
      int errno_copy = THRIFT_GET_SOCKET_ERROR;
      // GlobalOutput.perror("TServerSocket::listen() setsockopt() THRIFT_NO_SOCKET_CACHING ",
      //                     errno_copy);
      GlobalOutput.perror("TServerSocket::listen() ff_setsockopt() THRIFT_NO_SOCKET_CACHING ",
                          errno_copy);
      close();
      throw TTransportException(TTransportException::NOT_OPEN,
                                "Could not set THRIFT_NO_SOCKET_CACHING",
                                errno_copy);
    }
  }

  // Set TCP buffer sizes
  if (tcpSendBuffer_ > 0) {
    // if (-1 == setsockopt(serverSocket_,
    //                      SOL_SOCKET,
    //                      SO_SNDBUF,
    //                      cast_sockopt(&tcpSendBuffer_),
    //                      sizeof(tcpSendBuffer_))) {
    if (-1 == ff_setsockopt(serverSocket_,
                         SOL_SOCKET,
                         SO_SNDBUF,
                         cast_sockopt(&tcpSendBuffer_),
                         sizeof(tcpSendBuffer_))) {
      int errno_copy = THRIFT_GET_SOCKET_ERROR;
      // GlobalOutput.perror("TServerSocket::listen() setsockopt() SO_SNDBUF ", errno_copy);
      GlobalOutput.perror("TServerSocket::listen() ff_setsockopt() SO_SNDBUF ", errno_copy);
      close();
      throw TTransportException(TTransportException::NOT_OPEN,
                                "Could not set SO_SNDBUF",
                                errno_copy);
    }
  }

  if (tcpRecvBuffer_ > 0) {
    // if (-1 == setsockopt(serverSocket_,
    //                      SOL_SOCKET,
    //                      SO_RCVBUF,
    //                      cast_sockopt(&tcpRecvBuffer_),
    //                      sizeof(tcpRecvBuffer_))) {
    if (-1 == ff_setsockopt(serverSocket_,
                         SOL_SOCKET,
                         SO_RCVBUF,
                         cast_sockopt(&tcpRecvBuffer_),
                         sizeof(tcpRecvBuffer_))) {
      int errno_copy = THRIFT_GET_SOCKET_ERROR;
      // GlobalOutput.perror("TServerSocket::listen() setsockopt() SO_RCVBUF ", errno_copy);
      GlobalOutput.perror("TServerSocket::listen() ff_setsockopt() SO_RCVBUF ", errno_copy);
      close();
      throw TTransportException(TTransportException::NOT_OPEN,
                                "Could not set SO_RCVBUF",
                                errno_copy);
    }
  }

  // Turn linger off, don't want to block on calls to close
  struct linger ling = {0, 0};
  // if (-1 == setsockopt(serverSocket_, SOL_SOCKET, SO_LINGER, cast_sockopt(&ling), sizeof(ling))) {
  if (-1 == ff_setsockopt(serverSocket_, SOL_SOCKET, SO_LINGER, cast_sockopt(&ling), sizeof(ling))) {
    int errno_copy = THRIFT_GET_SOCKET_ERROR;
    // GlobalOutput.perror("TServerSocket::listen() setsockopt() SO_LINGER ", errno_copy);
    GlobalOutput.perror("TServerSocket::listen() ff_setsockopt() SO_LINGER ", errno_copy);
    close();
    throw TTransportException(TTransportException::NOT_OPEN, "Could not set SO_LINGER", errno_copy);
  }

#ifdef SO_NOSIGPIPE
  // if (-1 == setsockopt(serverSocket_, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one))) {
  if (-1 == ff_setsockopt(serverSocket_, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one))) {  
    int errno_copy = THRIFT_GET_SOCKET_ERROR;
    GlobalOutput.perror("TServerSocket::listen() setsockopt() SO_NOSIGPIPE", errno_copy);
    close();
    throw TTransportException(TTransportException::NOT_OPEN,
                              "Could not set SO_NOSIGPIPE",
                              errno_copy);
  }
#endif

  // Set NONBLOCK on the accept socket
  // int flags = THRIFT_FCNTL(serverSocket_, THRIFT_F_GETFL, 0);
  // if (flags == -1) {
  // int flags = 0;
  // if (ff_ioctl(serverSocket_, FIONBIO, &flags) == -1) {
  //   int errno_copy = THRIFT_GET_SOCKET_ERROR;
  //   // GlobalOutput.perror("TServerSocket::listen() THRIFT_FCNTL() THRIFT_F_GETFL ", errno_copy);
  //   // close();
  //   // throw TTransportException(TTransportException::NOT_OPEN,
  //   //                           "THRIFT_FCNTL() THRIFT_F_GETFL failed",
  //   //                           errno_copy);
  //   GlobalOutput.perror("TSocket::_setup_sockopts() ff_ioctl() FIONBIO ", errno_copy);
  //   close();
  //   throw TTransportException(TTransportException::NOT_OPEN,
  //                             "ff_ioctl() FIONBIO failed",
  //                             errno_copy);
  // }
  int flags = 1;  // Set to non-blocking mode
  // if (-1 == THRIFT_FCNTL(serverSocket_, THRIFT_F_SETFL, flags | THRIFT_O_NONBLOCK)) {
  if (ff_ioctl(serverSocket_, FIONBIO, &flags) == -1) {
    int errno_copy = THRIFT_GET_SOCKET_ERROR;
    // GlobalOutput.perror("TServerSocket::listen() THRIFT_FCNTL() THRIFT_O_NONBLOCK ", errno_copy);
    // close();
    // throw TTransportException(TTransportException::NOT_OPEN,
    //                           "THRIFT_FCNTL() THRIFT_F_SETFL THRIFT_O_NONBLOCK failed",
    //                           errno_copy);
    GlobalOutput.perror("TSocket::_setup_sockopts() ff_ioctl() FIONBIO ", errno_copy);
    close();
    throw TTransportException(TTransportException::NOT_OPEN,
                              "ff_ioctl() FIONBIO failed",
                              errno_copy);
  }
}

void TServerSocket::_setup_unixdomain_sockopts() {
}

void TServerSocket::_setup_tcp_sockopts() {
  int one = 1;

  // Defer accept
#ifdef TCP_DEFER_ACCEPT
  if (!isUnixDomainSocket()) {
    // if (-1 == setsockopt(serverSocket_, IPPROTO_TCP, TCP_DEFER_ACCEPT, &one, sizeof(one))) {
    if (-1 == ff_setsockopt(serverSocket_, IPPROTO_TCP, TCP_DEFER_ACCEPT, &one, sizeof(one))) {
      // Check if the error is due to an unsupported option
      if (errno == ENOPROTOOPT || errno == EINVAL) {
        // The TCP_DEFER_ACCEPT option is not supported, log a warning and continue
        GlobalOutput.printf("TSocket::_setup_tcp_sockopts() warning: TCP_DEFER_ACCEPT not supported");
      } else {
        // It's a different error, so we'll still throw an exception
        int errno_copy = THRIFT_GET_SOCKET_ERROR;
        // GlobalOutput.perror("TServerSocket::listen() setsockopt() TCP_DEFER_ACCEPT ", errno_copy);
        GlobalOutput.perror("TSocket::_setup_tcp_sockopts() ff_setsockopt() TCP_DEFER_ACCEPT ", errno_copy);
        close();
        throw TTransportException(TTransportException::NOT_OPEN, "Could not set TCP_DEFER_ACCEPT",
                                  errno_copy);
      }
    }
  }
#endif // #ifdef TCP_DEFER_ACCEPT

  // TCP Nodelay, speed over bandwidth
  // if (-1
  //     == setsockopt(serverSocket_, IPPROTO_TCP, TCP_NODELAY, cast_sockopt(&one), sizeof(one))) {
  if (-1 == ff_setsockopt(serverSocket_, IPPROTO_TCP, TCP_NODELAY, cast_sockopt(&one), sizeof(one))) {
    int errno_copy = THRIFT_GET_SOCKET_ERROR;
    // GlobalOutput.perror("TServerSocket::listen() setsockopt() TCP_NODELAY ", errno_copy);
    GlobalOutput.perror("TSocket::_setup_tcp_sockopts() ff_setsockopt() TCP_NODELAY ", errno_copy);
    close();
    throw TTransportException(TTransportException::NOT_OPEN,
                              "Could not set TCP_NODELAY",
                              errno_copy);
  }
}

void TServerSocket::listen() {
#ifdef _WIN32
  TWinsockSingleton::create();
#endif // _WIN32

  // Initialize FF-Stack
  // ff_init(0, nullptr);

  THRIFT_SOCKET sv[2];
  // Create the socket pair used to interrupt
  if (-1 == THRIFT_SOCKETPAIR(AF_LOCAL, SOCK_STREAM, 0, sv)) {
    GlobalOutput.perror("TServerSocket::listen() socketpair() interrupt",
                        THRIFT_GET_SOCKET_ERROR);
    interruptSockWriter_ = THRIFT_INVALID_SOCKET;
    interruptSockReader_ = THRIFT_INVALID_SOCKET;
  } else {
    interruptSockWriter_ = sv[1];
    interruptSockReader_ = sv[0];
  }

  // Create the socket pair used to interrupt all clients
  if (-1 == THRIFT_SOCKETPAIR(AF_LOCAL, SOCK_STREAM, 0, sv)) {
    GlobalOutput.perror("TServerSocket::listen() socketpair() childInterrupt",
                        THRIFT_GET_SOCKET_ERROR);
    childInterruptSockWriter_ = THRIFT_INVALID_SOCKET;
    pChildInterruptSockReader_.reset();
  } else {
    childInterruptSockWriter_ = sv[1];
    pChildInterruptSockReader_
        = std::shared_ptr<THRIFT_SOCKET>(new THRIFT_SOCKET(sv[0]), destroyer_of_fine_sockets);
  }


  // Validate port number
  if (port_ < 0 || port_ > 0xFFFF) {
    throw TTransportException(TTransportException::BAD_ARGS, "Specified port is invalid");
  }

  // Resolve host:port strings into an iterable of struct addrinfo*
  AddressResolutionHelper resolved_addresses;
  if (!isUnixDomainSocket()) {
    try {

      resolved_addresses.resolve(address_, std::to_string(port_), SOCK_STREAM,
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
  }

  // we may want to try to bind more than once, since THRIFT_NO_SOCKET_CACHING doesn't
  // always seem to work. The client can configure the retry variables.
  int retries = 0;
  int errno_copy = 0;

  if (isUnixDomainSocket()) {
    // -- Unix Domain Socket -- //

    serverSocket_ = socket(PF_UNIX, SOCK_STREAM, IPPROTO_IP);

    if (serverSocket_ == THRIFT_INVALID_SOCKET) {
      int errno_copy = THRIFT_GET_SOCKET_ERROR;
      GlobalOutput.perror("TServerSocket::listen() socket() ", errno_copy);
      close();
      throw TTransportException(TTransportException::NOT_OPEN,
                                "Could not create server socket.",
                                errno_copy);
    }

    _setup_sockopts();
    _setup_unixdomain_sockopts();

    // Windows supports Unix domain sockets since it ships the header
    // HAVE_AF_UNIX_H (see https://devblogs.microsoft.com/commandline/af_unix-comes-to-windows/)
#if (!defined(_WIN32) || defined(HAVE_AF_UNIX_H))
    struct sockaddr_un address;
    socklen_t structlen = fillUnixSocketAddr(address, path_);

    do {
      if (0 == ::bind(serverSocket_, (struct sockaddr*)&address, structlen)) {
        break;
      }
      errno_copy = THRIFT_GET_SOCKET_ERROR;
      // use short circuit evaluation here to only sleep if we need to
    } while ((retries++ < retryLimit_) && (THRIFT_SLEEP_SEC(retryDelay_) == 0));
#else
    GlobalOutput.perror("TServerSocket::open() Unix Domain socket path not supported on this version of Windows", -99);
    throw TTransportException(TTransportException::NOT_OPEN,
                              " Unix Domain socket path not supported");
#endif
  } else {
    // -- TCP socket -- //

    auto addr_iter = AddressResolutionHelper::Iter{};

    // Via DNS or somehow else, single hostname can resolve into many addresses.
    // Results may contain perhaps a mix of IPv4 and IPv6.  Here, we iterate
    // over what system gave us, picking the first address that works.
    do {
      if (!addr_iter) {
        // init + recycle over many retries
        addr_iter = resolved_addresses.iterate();
      }
      auto trybind = *addr_iter++;

      // serverSocket_ = socket(trybind->ai_family, trybind->ai_socktype, trybind->ai_protocol);
      serverSocket_ = ff_socket(trybind->ai_family, trybind->ai_socktype, trybind->ai_protocol);
      if (serverSocket_ == -1) {
        errno_copy = THRIFT_GET_SOCKET_ERROR;
        continue;
      }

      _setup_sockopts();
      _setup_tcp_sockopts();

#ifdef IPV6_V6ONLY
      if (trybind->ai_family == AF_INET6) {
        int zero = 0;
        // if (-1 == setsockopt(serverSocket_,
                            //  IPPROTO_IPV6,
                            //  IPV6_V6ONLY,
                            //  cast_sockopt(&zero),
                            //  sizeof(zero))) {
        if (-1 == ff_setsockopt(serverSocket_, 
                                IPPROTO_IPV6, 
                                IPV6_V6ONLY, 
                                cast_sockopt(&zero), 
                                sizeof(zero))) {
          GlobalOutput.perror("TServerSocket::listen() IPV6_V6ONLY ", THRIFT_GET_SOCKET_ERROR);
        }
      }
#endif // #ifdef IPV6_V6ONLY

      // if (0 == ::bind(serverSocket_, trybind->ai_addr, static_cast<int>(trybind->ai_addrlen))) {
      if (0 == ff_bind(serverSocket_, reinterpret_cast<const linux_sockaddr*>(trybind->ai_addr), static_cast<int>(trybind->ai_addrlen))) {
        break;
      }
      errno_copy = THRIFT_GET_SOCKET_ERROR;

      // use short circuit evaluation here to only sleep if we need to
    } while ((retries++ < retryLimit_) && (THRIFT_SLEEP_SEC(retryDelay_) == 0));

    // retrieve bind info
    if (port_ == 0 && retries <= retryLimit_) {
      struct sockaddr_storage sa;
      socklen_t len = sizeof(sa);
      std::memset(&sa, 0, len);
      // if (::getsockname(serverSocket_, reinterpret_cast<struct sockaddr*>(&sa), &len) < 0) {
      if (ff_getsockname(serverSocket_, reinterpret_cast<linux_sockaddr*>(&sa), &len) < 0) {
        errno_copy = THRIFT_GET_SOCKET_ERROR;
        GlobalOutput.perror("TServerSocket::getPort() getsockname() ", errno_copy);
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
  } // TCP socket //

  // throw error if socket still wasn't created successfully
  if (serverSocket_ == THRIFT_INVALID_SOCKET) {
    GlobalOutput.perror("TServerSocket::listen() socket() ", errno_copy);
    close();
    throw TTransportException(TTransportException::NOT_OPEN,
                              "Could not create server socket.",
                              errno_copy);
  }

  // throw an error if we failed to bind properly
  if (retries > retryLimit_) {
    char errbuf[1024];
    if (isUnixDomainSocket()) {
#ifdef _WIN32
      THRIFT_SNPRINTF(errbuf, sizeof(errbuf), "TServerSocket::listen() Could not bind to domain socket path %s, error %d", path_.c_str(), WSAGetLastError());
#else
      // Fixme: This does not currently handle abstract domain sockets:
      THRIFT_SNPRINTF(errbuf, sizeof(errbuf), "TServerSocket::listen() Could not bind to domain socket path %s", path_.c_str());
#endif
    } else {
      THRIFT_SNPRINTF(errbuf, sizeof(errbuf), "TServerSocket::listen() Could not bind to port %d", port_);
    }
    GlobalOutput(errbuf);
    close();
    throw TTransportException(TTransportException::NOT_OPEN,
                              "Could not bind",
                              errno_copy);
  }

  if (listenCallback_)
    listenCallback_(serverSocket_);

  // Call listen
  // if (-1 == ::listen(serverSocket_, acceptBacklog_)) {
  if (-1 == ff_listen(serverSocket_, acceptBacklog_)) {
    errno_copy = THRIFT_GET_SOCKET_ERROR;
    GlobalOutput.perror("TServerSocket::listen() listen() ", errno_copy);
    close();
    throw TTransportException(TTransportException::NOT_OPEN, "Could not listen", errno_copy);
  }

  if (kq_ == -1) {
    int errno_copy = THRIFT_GET_SOCKET_ERROR;
    GlobalOutput.perror("TServerSocket::listen() ff_kqueue() ", errno_copy);
    throw TTransportException(TTransportException::UNKNOWN, "ff_kqueue()", errno_copy);
  }

  EV_SET(&event_, serverSocket_, EVFILT_READ, EV_ADD, 0, 0, NULL);
  
  if (ff_kevent(kq_, &event_, 1, NULL, 0, NULL) == -1) {
    int errno_copy = THRIFT_GET_SOCKET_ERROR;
    GlobalOutput.perror("TServerSocket::listen() ff_kevent() ", errno_copy);
    ff_close(kq_);
    throw TTransportException(TTransportException::UNKNOWN, "ff_kevent()", errno_copy);
  }
  //fprintf(stderr, "kq_ value: %d, serverSocket_:%d in listen()\n", kq_, serverSocket_);

  // The socket is now listening!
  listening_ = true;
}

int TServerSocket::getPort() const {
  return port_;
}

std::string TServerSocket::getPath() const {
    return path_;
}

bool TServerSocket::isUnixDomainSocket() const {
    return !path_.empty();
}

shared_ptr<TTransport> TServerSocket::acceptImpl() {
  if (serverSocket_ == THRIFT_INVALID_SOCKET) {
    throw TTransportException(TTransportException::NOT_OPEN, "TServerSocket not listening");
  }
  
  // Wait for events to happen
  int nevents = ff_kevent(kq_, NULL, 0, events_, DEFAULT_BACKLOG, NULL);
  int i; 
  if (nevents < 0) {
    int errno_copy = THRIFT_GET_SOCKET_ERROR;
    GlobalOutput.perror("TServerSocket::acceptImpl() ff_kevent() error", errno_copy);
    //fprintf(stderr, "ff_kevent error: %s\n", strerror(errno));
      //fprintf(stderr, "nevents: %d, kq_: %d, events_ addr: %p, DEFAULT_BACKLOG: %d\n", 
              //nevents, kq_, (void*)events_, DEFAULT_BACKLOG);
    throw TTransportException(TTransportException::UNKNOWN, "ff_kevent()", errno_copy);
  }

  for (i = 0; i < nevents; ++i) {
    //fprintf(stderr, "nevents: %d, kq_: %d, events_ addr: %p, DEFAULT_BACKLOG: %d\n", 
              //nevents, kq_, (void*)events_, DEFAULT_BACKLOG);
    event_ = events_[i];
    int clientfd = static_cast<int>(event_.ident);

    // Handle disconnect
    if (event_.flags & EV_EOF) {
      // fprintf(stderr, "Debug: handling a client disconnect.\n EV_EOF value: 0x%X\n", EV_EOF);
      if (errno == EINTR) {
      continue;
      }
      int errno_copy = THRIFT_GET_SOCKET_ERROR;
      GlobalOutput.perror("TServerSocket::acceptImpl() ff_kevent() disconnect", errno_copy);
      //fprintf(stderr, "ff_kevent error: %s\n", strerror(errno));
      //fprintf(stderr, "nevents: %d, kq_: %d, events_ addr: %p, DEFAULT_BACKLOG: %d\n", 
              //nevents, kq_, (void*)events_, DEFAULT_BACKLOG);
      ff_close(kq_);
      throw TTransportException(TTransportException::UNKNOWN, "ff_kevent()", errno_copy);
    } else if (clientfd == serverSocket_) {
      // Ready to accept new connection
      int available = static_cast<int>(event_.data);
      //fprintf(stderr, "Debug: available value: %d\n", available);
      while (available > 0 ) {
        struct sockaddr_storage clientAddress;
        int size = sizeof(clientAddress);
        THRIFT_SOCKET clientSocket 
              = ff_accept(serverSocket_, reinterpret_cast<linux_sockaddr*>(&clientAddress), reinterpret_cast<socklen_t*>(&size));
        // fprintf(stderr,"clientSocket after ff_accept: %d, clientfd: %d\n", clientSocket, clientfd);
        fprintf(stderr, "New client connected!\n");
        if (clientSocket == THRIFT_INVALID_SOCKET) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No client ready, return nullptr or throw a specific exception
            //fprintf(stderr, "Debug: errno == EAGAIN || WOULDBLOCK\n");
            return nullptr;
          }
          int errno_copy = THRIFT_GET_SOCKET_ERROR;
          GlobalOutput.perror("TServerSocket::acceptImpl() ff_accept() ", errno_copy);
          throw TTransportException(TTransportException::UNKNOWN, "ff_accept()", errno_copy);
        }

        // Make sure client socket is non-blocking
        int flags = 1;
        if (ff_ioctl(clientSocket, FIONBIO, &flags) == -1) {
          int errno_copy = THRIFT_GET_SOCKET_ERROR;
          ff_close(clientSocket);
          GlobalOutput.perror("TServerSocket::acceptImpl() ff_ioctl() FIONBIO ", errno_copy);
          throw TTransportException(TTransportException::UNKNOWN, "ff_ioctl(FIONBIO)", errno_copy);
        }

        shared_ptr<TSocket> client = createSocket(clientSocket);
        client->setPath(path_);
        if (sendTimeout_ > 0) {
          client->setSendTimeout(sendTimeout_);
        }
        if (recvTimeout_ > 0) {
          client->setRecvTimeout(recvTimeout_);
        }
        if (keepAlive_) {
          client->setKeepAlive(keepAlive_);
        }
        client->setCachedAddress((sockaddr*)&clientAddress, size);

        // Add the newly connected client to event list
        EV_SET(&event_, clientSocket, EVFILT_READ, EV_ADD, 0, 0, NULL);
        if (ff_kevent(kq_, &event_, 1, NULL, 0, NULL) < 0) {
          int errno_copy = THRIFT_GET_SOCKET_ERROR;
          GlobalOutput.perror("TServerSocket::acceptImpl() ff_kevent() add client ", errno_copy);
          ff_close(clientSocket);
          throw TTransportException(TTransportException::UNKNOWN, "ff_kevent() add client", errno_copy);
        }
        
        //fprintf(stderr, "Debug: successfully added new client with clientSocket: %d\n", clientSocket);
        if (acceptCallback_)
          acceptCallback_(clientSocket);

        // Decrement the number of available connections
          available--;

        // Return the first accepted client
        return client;
      }
    }
  }
  return nullptr;
}

shared_ptr<TSocket> TServerSocket::createSocket(THRIFT_SOCKET clientSocket) {
  if (interruptableChildren_ && (!usingKqueue_ || !usingEpoll_)) {
    return std::make_shared<TSocket>(clientSocket, pChildInterruptSockReader_);
  } else if (interruptableChildren_ && (usingKqueue_ || usingEpoll_)) {
    return std::make_shared<TSocket>(clientSocket, this, pChildInterruptSockReader_);
  } else {
    return std::make_shared<TSocket>(clientSocket);
  }
}

void TServerSocket::notify(THRIFT_SOCKET notifySocket) {
  if (notifySocket != THRIFT_INVALID_SOCKET) {
    int8_t byte = 0;
    if (-1 == send(notifySocket, cast_sockopt(&byte), sizeof(int8_t), 0)) {
      GlobalOutput.perror("TServerSocket::notify() send() ", THRIFT_GET_SOCKET_ERROR);
    }
  }
}

void TServerSocket::interrupt() {
  concurrency::Guard g(rwMutex_);
  if (interruptSockWriter_ != THRIFT_INVALID_SOCKET) {
    notify(interruptSockWriter_);
  }
}

void TServerSocket::interruptChildren() {
  concurrency::Guard g(rwMutex_);
  if (childInterruptSockWriter_ != THRIFT_INVALID_SOCKET) {
    notify(childInterruptSockWriter_);
  }
}

void TServerSocket::close() {
  concurrency::Guard g(rwMutex_);
  if (serverSocket_ != THRIFT_INVALID_SOCKET) {
     // shutdown(serverSocket_, THRIFT_SHUT_RDWR);
    ff_shutdown(serverSocket_, SHUT_RDWR);
    // ::THRIFT_CLOSESOCKET(serverSocket_);
    ff_close(serverSocket_);
  }
  if (interruptSockWriter_ != THRIFT_INVALID_SOCKET) {
    // ::THRIFT_CLOSESOCKET(interruptSockWriter_);
    ff_close(interruptSockWriter_);
  }
  if (interruptSockReader_ != THRIFT_INVALID_SOCKET) {
    // ::THRIFT_CLOSESOCKET(interruptSockReader_);
    ff_close(interruptSockReader_);
  }
  if (childInterruptSockWriter_ != THRIFT_INVALID_SOCKET) {
    // ::THRIFT_CLOSESOCKET(childInterruptSockWriter_);
    ff_close(childInterruptSockWriter_);
  }
  serverSocket_ = THRIFT_INVALID_SOCKET;
  interruptSockWriter_ = THRIFT_INVALID_SOCKET;
  interruptSockReader_ = THRIFT_INVALID_SOCKET;
  childInterruptSockWriter_ = THRIFT_INVALID_SOCKET;
  pChildInterruptSockReader_.reset();
  listening_ = false;
}
} // namespace transport
} // namespace thrift
} // namespace apache
