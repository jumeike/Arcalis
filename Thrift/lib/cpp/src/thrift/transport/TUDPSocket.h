#ifndef _THRIFT_TRANSPORT_TUDPSOCKET_H_
#define _THRIFT_TRANSPORT_TUDPSOCKET_H_ 1

#include <string>

#include <thrift/transport/TTransport.h>
#include <thrift/transport/TVirtualTransport.h>
#include <thrift/transport/TServerUDPSocket.h>
#include <thrift/transport/PlatformSocket.h>

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

namespace apache {
namespace thrift {
namespace transport {

/**
 * UDP Socket implementation of the TTransport interface.
 */
class TUDPSocket : public TVirtualTransport<TUDPSocket> {
public:
  /**
   * Constructs a new socket. Note that this does NOT actually connect the
   * socket.
   */
  TUDPSocket(std::shared_ptr<TConfiguration> config = nullptr);

  /**
   * Constructs a new socket. Note that this does NOT actually connect the
   * socket.
   *
   * @param host An IP address or hostname to connect to
   * @param port The port to connect on
   */
  TUDPSocket(const std::string& host, int port, std::shared_ptr<TConfiguration> config = nullptr);

  /**
   * Constructs a new Unix domain socket.
   * Note that this does NOT actually connect the socket.
   *
   * @param path The Unix domain socket e.g. "/tmp/ThriftTest.binary.thrift"
   */
  TUDPSocket(const std::string& path, std::shared_ptr<TConfiguration> config = nullptr);

  /**
   * Destroys the socket object, closing it if necessary.
   */
  ~TUDPSocket() override;

  /**
   * Whether the socket is alive.
   *
   * @return Is the socket alive?
   */
  bool isOpen() const override;

  /**
   * Checks whether there is more data available in the socket.
   */
  bool peek() override;

  /**
   * Creates and opens the UDP socket.
   *
   * @throws TTransportException If the socket could not connect
   */
  void open() override;

  /**
   * Shuts down communications on the socket.
   */
  void close() override;

  /**
   * Reads from the underlying socket.
   * \returns the number of bytes read or 0 indicates EOF
   * \throws TTransportException of types:
   *           NOT_OPEN means the socket has been closed
   *           UNKNOWN means something unexpected happened
   */
  virtual uint32_t read(uint8_t* buf, uint32_t len);

  /**
   * Writes to the underlying socket.
   */
  virtual void write(const uint8_t* buf, uint32_t len);

  /**
   * Writes to the underlying socket.  Does single send() and returns result.
   */
  virtual uint32_t write_partial(const uint8_t* buf, uint32_t len); 

  /**
   * Get the host that the socket is connected to
   *
   * @return string host identifier
   */
  std::string getHost() const;

  /**
   * Get the port that the socket is connected to
   *
   * @return int port number 
   */
  int getPort() const;

  /**
   * Get the Unix domain socket path
   * 
   * @return std::string path
   */
  std::string getPath() const;

  /**
   * Whether this is a Unix domain socket
   *
   * @return bool true if Unix domain socket
   */
  bool isUnixDomainSocket() const;

  /**
   * Set the host that socket will connect to
   *
   * @param host host identifier
   */
  void setHost(std::string host);

  /**
   * Set the port that socket will connect to
   *
   * @param port port number
   */
  void setPort(int port);

  /**
   * Set the Unix domain socket path
   *
   * @param path std::string path
   */
  void setPath(std::string path);

  /**
   * Set the receive timeout
   */
  void setRecvTimeout(int ms);

  /**
   * Set the send timeout
   */
  void setSendTimeout(int ms);

  /**
   * Set the max number of recv retries in case of an THRIFT_EAGAIN
   * error
   */
  void setMaxRecvRetries(int maxRecvRetries);

  /**
   * Get socket information formatted as a string <Host: x Port: x>
   */
  std::string getSocketInfo() const;

  /**
   * Returns the DNS name of the host to which the socket is connected
   */
  std::string getPeerHost() const;

  /**
   * Returns the address of the host to which the socket is connected
   */
  std::string getPeerAddress() const;

  /**
   * Returns the port of the host to which the socket is connected
   */
  int getPeerPort() const;

  /**
   * Returns the underlying socket file descriptor.
   */
  THRIFT_SOCKET getSocketFD() { return socket_; }

  /**
   * Set a cache of the peer address (used when trivially available: e.g.
   * accept() or connect()). Only caches IPV4 and IPV6; unset for others.
   */
  void setCachedAddress(const sockaddr* addr, socklen_t len);

  /*
   * Returns a cached copy of the peer address.
   */
  sockaddr* getCachedAddress(socklen_t* len) const;

  /**
   * Get the origin the socket is connected to
   *
   * @return string peer host identifier and port
   */
  const std::string getOrigin() const override;

  /**
   * Constructor to create socket from file descriptor.
   */
  TUDPSocket(THRIFT_SOCKET socket, std::shared_ptr<TConfiguration> config = nullptr);

  struct sockaddr_storage peerAddr_;  // Store peer address for UDP
  socklen_t peerAddrLen_;            // Peer address length

protected:
  /** Connect socket to destination address */
  void openConnection(struct addrinfo* res);

  /** Host to connect to */
  std::string host_;

  /** Port number to connect on */
  int port_;

  /** UNIX domain socket path */
  std::string path_;

  /** Underlying socket handle */ 
  THRIFT_SOCKET socket_;

  /** Peer hostname */
  mutable std::string peerHost_;

  /** Peer address */
  mutable std::string peerAddress_;

  /** Peer port */
  mutable int peerPort_;

  /** Send timeout in ms */
  int sendTimeout_;

  /** Recv timeout in ms */
  int recvTimeout_;

  /** Recv EAGAIN retries */
  int maxRecvRetries_;
  
  /**
   * A shared socket pointer that will interrupt a blocking read if data
   * becomes available on it
   */
  std::shared_ptr<THRIFT_SOCKET> interruptListener_;
  
  /** Cached peer address */
  union {
    sockaddr_in ipv4;
    sockaddr_in6 ipv6;
  } cachedPeerAddr_;

private:
  void local_open();
  void unix_open();
};

}
}
} // apache::thrift::transport

#endif // #ifndef _THRIFT_TRANSPORT_TUDPSOCKET_H_
