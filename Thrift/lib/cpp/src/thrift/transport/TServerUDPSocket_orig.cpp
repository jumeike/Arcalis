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

#include <thrift/transport/PlatformSocket.h>
#include <thrift/transport/TServerUDPSocket.h>
#include <thrift/transport/TUDPSocket.h>
#include <thrift/transport/TSocketUtils.h>
#include <thrift/transport/SocketCommon.h>

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

TServerUDPSocket::TServerUDPSocket(int port)
  : port_(port),
    serverSocket_(THRIFT_INVALID_SOCKET),
    sendTimeout_(0),
    recvTimeout_(0),
    tcpSendBuffer_(0),
    tcpRecvBuffer_(0),
    listening_(false),
    interruptSockWriter_(THRIFT_INVALID_SOCKET),
    interruptSockReader_(THRIFT_INVALID_SOCKET) {
}

TServerUDPSocket::TServerUDPSocket(int port, int sendTimeout, int recvTimeout)
  : port_(port),
    serverSocket_(THRIFT_INVALID_SOCKET),
    sendTimeout_(sendTimeout),
    recvTimeout_(recvTimeout),
    tcpSendBuffer_(0),
    tcpRecvBuffer_(0),
    listening_(false),
    interruptSockWriter_(THRIFT_INVALID_SOCKET),
    interruptSockReader_(THRIFT_INVALID_SOCKET) {
}

TServerUDPSocket::TServerUDPSocket(const string& address, int port)
  : port_(port),
    address_(address),
    serverSocket_(THRIFT_INVALID_SOCKET),
    sendTimeout_(0),
    recvTimeout_(0),
    tcpSendBuffer_(0),
    tcpRecvBuffer_(0),
    listening_(false),
    interruptSockWriter_(THRIFT_INVALID_SOCKET),
    interruptSockReader_(THRIFT_INVALID_SOCKET) {
}

TServerUDPSocket::TServerUDPSocket(const string& path)
  : port_(0),
    path_(path),
    serverSocket_(THRIFT_INVALID_SOCKET),
    sendTimeout_(0),
    recvTimeout_(0),
    tcpSendBuffer_(0),
    tcpRecvBuffer_(0),
    listening_(false),
    interruptSockWriter_(THRIFT_INVALID_SOCKET),
    interruptSockReader_(THRIFT_INVALID_SOCKET) {
}

TServerUDPSocket::~TServerUDPSocket() {
  close();
}

void TServerUDPSocket::setSendTimeout(int sendTimeout) {
  sendTimeout_ = sendTimeout;
}

void TServerUDPSocket::setRecvTimeout(int recvTimeout) {
  recvTimeout_ = recvTimeout;
}

void TServerUDPSocket::setTcpSendBuffer(int tcpSendBuffer) {
  tcpSendBuffer_ = tcpSendBuffer;
}

void TServerUDPSocket::setTcpRecvBuffer(int tcpRecvBuffer) {
  tcpRecvBuffer_ = tcpRecvBuffer;
}

void TServerUDPSocket::_setup_sockopts() {
  if (tcpSendBuffer_ > 0) {
    if (setsockopt(serverSocket_,
                   SOL_SOCKET,
                   SO_SNDBUF,
                   cast_sockopt(&tcpSendBuffer_),
                   sizeof(tcpSendBuffer_)) == -1) {
      int errno_copy = THRIFT_GET_SOCKET_ERROR;
      GlobalOutput.perror("TServerUDPSocket::listen() setsockopt() SO_SNDBUF ", errno_copy);
      close();
      throw TTransportException(TTransportException::NOT_OPEN, 
                               "Could not set SO_SNDBUF", errno_copy);
    }
  }

  if (tcpRecvBuffer_ > 0) {
    if (setsockopt(serverSocket_,
                   SOL_SOCKET,
                   SO_RCVBUF,
                   cast_sockopt(&tcpRecvBuffer_),
                   sizeof(tcpRecvBuffer_)) == -1) {
      int errno_copy = THRIFT_GET_SOCKET_ERROR;
      GlobalOutput.perror("TServerUDPSocket::listen() setsockopt() SO_RCVBUF ", errno_copy);
      close();
      throw TTransportException(TTransportException::NOT_OPEN,
                               "Could not set SO_RCVBUF", errno_copy);
    }
  }

#ifdef SO_REUSEADDR
  {
    int one = 1;
    if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, 
                   cast_sockopt(&one), sizeof(one)) == -1) {
      int errno_copy = THRIFT_GET_SOCKET_ERROR;
      GlobalOutput.perror("TServerUDPSocket::listen() setsockopt() SO_REUSEADDR ", errno_copy);
      close();
      throw TTransportException(TTransportException::NOT_OPEN,
                               "Could not set SO_REUSEADDR", errno_copy);
    }
  }
#endif
}

void TServerUDPSocket::_setup_unixdomain_sockopts() {
  // No specific Unix domain socket options needed for UDP
}

void TServerUDPSocket::listen() {
  THRIFT_SOCKET sv[2];
  // Create the socket pair used to interrupt
  if (THRIFT_SOCKETPAIR(AF_LOCAL, SOCK_STREAM, 0, sv) == -1) {
    GlobalOutput.perror("TServerUDPSocket::listen() socketpair() ", THRIFT_GET_SOCKET_ERROR);
    interruptSockWriter_ = THRIFT_INVALID_SOCKET;
    interruptSockReader_ = THRIFT_INVALID_SOCKET;
  } else {
    interruptSockWriter_ = sv[1];
    interruptSockReader_ = sv[0];
  }

  // Validate port number
  if (port_ < 0 || port_ > 0xFFFF) {
    throw TTransportException(TTransportException::BAD_ARGS, "Specified port is invalid");
  }

  // Create UDP socket
  struct addrinfo hints, *res, *res0;
  int error;
  char port[sizeof("65535")];
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
  sprintf(port, "%d", port_);

  // Resolve address for binding
  error = getaddrinfo(address_.empty() ? nullptr : address_.c_str(), port, &hints, &res0);
  if (error) {
    string errStr = "TServerUDPSocket::listen() getaddrinfo() " + string(THRIFT_GAI_STRERROR(error));
    GlobalOutput(errStr.c_str());
    close();
    throw TTransportException(TTransportException::NOT_OPEN,
                             "Could not resolve address for server socket.");
  }

  // Pick first address that works
  for (res = res0; res; res = res->ai_next) {
    serverSocket_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (serverSocket_ == -1) {
      continue;
    }

    _setup_sockopts();
    
    if (isUnixDomainSocket()) {
      _setup_unixdomain_sockopts();
    }

    if (bind(serverSocket_, res->ai_addr, static_cast<int>(res->ai_addrlen)) == 0) {
      break; // Success
    }

    ::THRIFT_CLOSESOCKET(serverSocket_);
    serverSocket_ = THRIFT_INVALID_SOCKET;
  }

  // Free address structure memory
  freeaddrinfo(res0);

  // Throw if we failed to bind
  if (serverSocket_ == THRIFT_INVALID_SOCKET) {
    char errbuf[1024];
    if (isUnixDomainSocket()) {
      sprintf(errbuf, "TServerUDPSocket::listen() Could not bind to Unix domain socket at %s", path_.c_str());
    } else {
      sprintf(errbuf, "TServerUDPSocket::listen() Could not bind to port %d", port_);
    }
    throw TTransportException(TTransportException::NOT_OPEN, string(errbuf));
  }

  if (listenCallback_) {
    listenCallback_(serverSocket_);
  }

  // The socket is now listening and ready for UDP datagrams
  listening_ = true;
}

shared_ptr<TTransport> TServerUDPSocket::acceptImpl() {
  if (serverSocket_ == THRIFT_INVALID_SOCKET) {
    throw TTransportException(TTransportException::NOT_OPEN, "TServerUDPSocket not listening");
  }

  struct THRIFT_POLLFD fds[2];
  std::memset(fds, 0, sizeof(fds));
  fds[0].fd = serverSocket_;
  fds[0].events = THRIFT_POLLIN;
  if (interruptSockReader_ != THRIFT_INVALID_SOCKET) {
    fds[1].fd = interruptSockReader_;
    fds[1].events = THRIFT_POLLIN;
  }

  if (THRIFT_POLL(fds, 2, -1) < 0) {
    // Error cases
    if (THRIFT_GET_SOCKET_ERROR == THRIFT_EINTR) {
      return nullptr;
    }
    int errno_copy = THRIFT_GET_SOCKET_ERROR;
    GlobalOutput.perror("TServerUDPSocket::acceptImpl() THRIFT_POLL() ", errno_copy);
    throw TTransportException(TTransportException::UNKNOWN, "Unknown", errno_copy);
  }

  // Check for interrupt signal
  if (interruptSockReader_ != THRIFT_INVALID_SOCKET && (fds[1].revents & THRIFT_POLLIN)) {
    int8_t buf;
    if (recv(interruptSockReader_, cast_sockopt(&buf), sizeof(int8_t), 0) == -1) {
      GlobalOutput.perror("TServerUDPSocket::acceptImpl() recv() interrupt ",
                         THRIFT_GET_SOCKET_ERROR);
    }
    throw TTransportException(TTransportException::INTERRUPTED);
  }

  if (fds[0].revents & THRIFT_POLLIN) {
    struct sockaddr_storage clientAddress;
    socklen_t size = sizeof(clientAddress);
    
    // For UDP we don't create new sockets - we share the server socket
    shared_ptr<TUDPSocket> client = createSocket(serverSocket_);
    // Cache the client address for sending responses
    client->setCachedAddress((struct sockaddr*)&clientAddress, size);
    
    if (sendTimeout_ > 0) {
      client->setSendTimeout(sendTimeout_);
    }
    if (recvTimeout_ > 0) {
      client->setRecvTimeout(recvTimeout_);
    }

    return client;
  }

  return nullptr;
}

shared_ptr<TUDPSocket> TServerUDPSocket::createSocket(THRIFT_SOCKET clientSocket) {
  return std::make_shared<TUDPSocket>(clientSocket);
}

void TServerUDPSocket::interrupt() {
  concurrency::Guard g(rwMutex_);
  if (interruptSockWriter_ != THRIFT_INVALID_SOCKET) {
    notify(interruptSockWriter_);
  }
}

void TServerUDPSocket::notify(THRIFT_SOCKET notifySocket) {
  if (notifySocket != THRIFT_INVALID_SOCKET) {
    int8_t byte = 0;
    if (send(notifySocket, cast_sockopt(&byte), sizeof(int8_t), 0) == -1) {
      GlobalOutput.perror("TServerUDPSocket::notify() send() ", THRIFT_GET_SOCKET_ERROR);
    }
  }
}

void TServerUDPSocket::close() {
  concurrency::Guard g(rwMutex_);
  if (serverSocket_ != THRIFT_INVALID_SOCKET) {
    ::THRIFT_CLOSESOCKET(serverSocket_);
  }
  if (interruptSockWriter_ != THRIFT_INVALID_SOCKET) {
    ::THRIFT_CLOSESOCKET(interruptSockWriter_);
  }
  if (interruptSockReader_ != THRIFT_INVALID_SOCKET) {
    ::THRIFT_CLOSESOCKET(interruptSockReader_);
  }
  serverSocket_ = THRIFT_INVALID_SOCKET;
  interruptSockWriter_ = THRIFT_INVALID_SOCKET;
  interruptSockReader_ = THRIFT_INVALID_SOCKET;
  listening_ = false;
}

bool TServerUDPSocket::isOpen() const {
  return serverSocket_ != THRIFT_INVALID_SOCKET;
}

int TServerUDPSocket::getPort() const {
  return port_;
}

std::string TServerUDPSocket::getPath() const {
  return path_;
}

bool TServerUDPSocket::isUnixDomainSocket() const {
  return !path_.empty();
}

}
}
} // apache::thrift::transport
