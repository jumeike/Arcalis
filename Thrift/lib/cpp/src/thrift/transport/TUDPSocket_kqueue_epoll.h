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

#ifndef _THRIFT_TRANSPORT_TUDPSOCKET_H_
#define _THRIFT_TRANSPORT_TUDPSOCKET_H_ 1

#include <string>

#include <thrift/transport/TTransport.h>
#include <thrift/transport/TVirtualTransport.h>
#include <thrift/transport/TServerSocket.h>
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
 * TCP Socket implementation of the TTransport interface.
 *
 */
class TUDPSocket : public TVirtualTransport<TUDPSocket> {
public:
  /**
   * Constructs a new socket. Note that this does NOT actually connect the
   * socket.
   *
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
   * or a zero-prefixed string to create an abstract domain socket on Linux.
   */
  TUDPSocket(const std::string& path, std::shared_ptr<TConfiguration> config = nullptr);

  /**
   * Destroyes the socket object, closing it if necessary.
   */
  ~TUDPSocket() override;

  /**
   * Whether the socket is alive.
   *
   * @return Is the socket alive?
   */
  bool isOpen() const override;

  /**
   * Checks whether there is more data available in the socket to read.
   *
   * This call blocks until at least one byte is available or the socket is closed.
   */
  bool peek() override;

  /**
   * Creates and opens the UNIX socket.
   *
   * @throws TTransportException If the socket could not connect
   */
  void open() override;

  /**
   * Shuts down communications on the socket.
   */
  void close() override;

  /**
   * Determines whether there is pending data to read or not.
   *
   * This call does not block.
   * \throws TTransportException of types:
   *           NOT_OPEN means the socket has been closed
   *           UNKNOWN means something unexpected happened
   * \returns true if there is pending data to read, false otherwise
   */
  virtual bool hasPendingDataToRead();

  /**
   * Reads from the underlying socket.
   * \returns the number of bytes read or 0 indicates EOF
   * \throws TTransportException of types:
   *           INTERRUPTED means the socket was interrupted
   *                       out of a blocking call
   *           NOT_OPEN means the socket has been closed
   *           TIMED_OUT means the receive timeout expired
   *           UNKNOWN means something unexpected happened
   */
  virtual uint32_t read(uint8_t* buf, uint32_t len);

  /**
   * Writes to the underlying socket.  Loops until done or fail.
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
   * Get the Unix domain socket path that the socket is connected to
   *
   * @return std::string path
   */
  std::string getPath() const;

  /**
   * Whether the socket is a Unix domain socket. This is the same as checking
   * if getPath() is not empty.
   *
   * @return Is the socket a Unix domain socket?
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
   * Set the Unix domain socket path for the socket
   *
   * @param path std::string path
   */
  void setPath(std::string path);

  /**
   * Controls whether the linger option is set on the socket.
   *
   * @param on      Whether SO_LINGER is on
   * @param linger  If linger is active, the number of seconds to linger for
   */
  void setLinger(bool on, int linger);

  /**
   * Whether to enable/disable Nagle's algorithm.
   *
   * @param noDelay Whether or not to disable the algorithm.
   * @return
   */
  void setNoDelay(bool noDelay);

  /**
   * Set the connect timeout
   */
  void setConnTimeout(int ms);

  void setGenericTimeout(THRIFT_SOCKET s, int timeout_ms, int optname);
  
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
   * Set SO_KEEPALIVE
   */
  void setKeepAlive(bool keepAlive);

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
   **/
  int getPeerPort() const;

  /**
   * Returns the underlying socket file descriptor.
   */
  THRIFT_SOCKET getSocketFD() { return socket_; }

  /**
   * (Re-)initialize a TUDPSocket for the supplied descriptor.  This is only
   * intended for use by TNonblockingServer -- other use may result in
   * unfortunate surprises.
   *
   * @param fd the descriptor for an already-connected socket
   */
  void setSocketFD(THRIFT_SOCKET fd);

  /*
   * Returns a cached copy of the peer address.
   */
  sockaddr* getCachedAddress(socklen_t* len) const;

  /**
   * Sets whether to use a low minimum TCP retransmission timeout.
   */
  static void setUseLowMinRto(bool useLowMinRto);

  /**
   * Gets whether to use a low minimum TCP retransmission timeout.
   */
  static bool getUseLowMinRto();

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

  /**
   * New Constructor to create socket from file descriptor for UDP connections.
   */
  TUDPSocket(THRIFT_SOCKET socket, TServerUDPSocket* serverSocketInstance, 
          bool isUDP, bool usingKqueue, std::shared_ptr<TConfiguration> config = nullptr);

  /**
   * Constructor to create socket from file descriptor that
   * can be interrupted safely.
   */
  TUDPSocket(THRIFT_SOCKET socket, std::shared_ptr<THRIFT_SOCKET> interruptListener,
         std::shared_ptr<TConfiguration> config = nullptr);
  
  /**
   * New Constructor to create socket from file descriptor and 
   * a pointer to TServerSocket instance.
   */
  // TSocket(THRIFT_SOCKET socket, TServerSocket* serverSocketInstance, 
  //        std::shared_ptr<THRIFT_SOCKET> interruptListener, 
  //        std::shared_ptr<TConfiguration> config = nullptr);

  /**
   *  Returns the kqueue descriptor for the TServerSocket instance 
   *  that created it
   */
  int getServerKqueueDescriptor() const;
  int getServerEpfdDescriptor() const;

  /**
   *  Returns the Event Struct for the TServerSocket instance 
   *  that created it
   */
  const struct kevent& getServerEvent() const;

  /**
   *  Returns the TriggerEvent Struct for the TServerSocket instance
   *  that created it
   */
  const struct kevent& getServerTriggerEvent() const;

  /**
   *  Returns the Events Array for the TServerSocket instance
   *  that created it
   */
  const struct kevent* getServerEvents() const;
  struct epoll_event* getServerEpollEvents() const;

  /**
   * Set a cache of the peer address (used when trivially available: e.g.
   * accept() or connect()). Only caches IPV4 and IPV6; unset for others.
   */
  void setCachedAddress(const sockaddr* addr, socklen_t len);

  // Add UDP flag
  bool isUDP_{false};
  bool usingKqueue_{true};
  struct sockaddr_storage peerAddr_;  // Store peer address for UDP
  socklen_t peerAddrLen_;            // Peer address length

protected:
  /** connect, called by open */
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

  /**
   * A shared socket pointer that will interrupt a blocking read if data
   * becomes available on it
   */
  std::shared_ptr<THRIFT_SOCKET> interruptListener_;

  /** Connect timeout in ms */
  int connTimeout_;

  /** Send timeout in ms */
  int sendTimeout_;

  /** Recv timeout in ms */
  int recvTimeout_;

  /** Keep alive on */
  bool keepAlive_;

  /** Linger on */
  bool lingerOn_;

  /** Linger val */
  int lingerVal_;

  /** Nodelay */
  bool noDelay_;

  /** Recv EGAIN retries */
  int maxRecvRetries_;

  /** Cached peer address */
  union {
    sockaddr_in ipv4;
    sockaddr_in6 ipv6;
  } cachedPeerAddr_;

  /** Whether to use low minimum TCP retransmission timeout */
  static bool useLowMinRto_;

private:
  transport::TServerUDPSocket* serverSocketInstance_;
  // transport::TServerSocket* serverTCPSocketInstance_;
  void unix_open();
  void local_open();
};
}
}
} // apache::thrift::transport

#endif // #ifndef _THRIFT_TRANSPORT_TUDPSOCKET_H_

