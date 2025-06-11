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

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <stdint.h>
#include <thrift/server/TFStackServerFramework.h>

namespace apache {
namespace thrift {
namespace server {

using apache::thrift::concurrency::Synchronized;
using apache::thrift::protocol::TProtocol;
using apache::thrift::protocol::TProtocolFactory;
using std::bind;
using std::shared_ptr;
using apache::thrift::transport::TServerTransport;
using apache::thrift::transport::TTransport;
using apache::thrift::transport::TTransportException;
using apache::thrift::transport::TTransportFactory;
using std::string;

TFStackServerFramework::TFStackServerFramework(const shared_ptr<TProcessorFactory>& processorFactory,
                                   const shared_ptr<TServerTransport>& serverTransport,
                                   const shared_ptr<TTransportFactory>& transportFactory,
                                   const shared_ptr<TProtocolFactory>& protocolFactory)
  : TServer(processorFactory, serverTransport, transportFactory, protocolFactory),
    clients_(0),
    hwm_(0),
    limit_(INT64_MAX),
    connectedClient_(false) {
}

TFStackServerFramework::TFStackServerFramework(const shared_ptr<TProcessor>& processor,
                                   const shared_ptr<TServerTransport>& serverTransport,
                                   const shared_ptr<TTransportFactory>& transportFactory,
                                   const shared_ptr<TProtocolFactory>& protocolFactory)
  : TServer(processor, serverTransport, transportFactory, protocolFactory),
    clients_(0),
    hwm_(0),
    limit_(INT64_MAX),
    connectedClient_(false) {
}

TFStackServerFramework::TFStackServerFramework(const shared_ptr<TProcessorFactory>& processorFactory,
                                   const shared_ptr<TServerTransport>& serverTransport,
                                   const shared_ptr<TTransportFactory>& inputTransportFactory,
                                   const shared_ptr<TTransportFactory>& outputTransportFactory,
                                   const shared_ptr<TProtocolFactory>& inputProtocolFactory,
                                   const shared_ptr<TProtocolFactory>& outputProtocolFactory)
  : TServer(processorFactory,
            serverTransport,
            inputTransportFactory,
            outputTransportFactory,
            inputProtocolFactory,
            outputProtocolFactory),
    clients_(0),
    hwm_(0),
    limit_(INT64_MAX),
    connectedClient_(false) {
}

TFStackServerFramework::TFStackServerFramework(const shared_ptr<TProcessor>& processor,
                                   const shared_ptr<TServerTransport>& serverTransport,
                                   const shared_ptr<TTransportFactory>& inputTransportFactory,
                                   const shared_ptr<TTransportFactory>& outputTransportFactory,
                                   const shared_ptr<TProtocolFactory>& inputProtocolFactory,
                                   const shared_ptr<TProtocolFactory>& outputProtocolFactory)
  : TServer(processor,
            serverTransport,
            inputTransportFactory,
            outputTransportFactory,
            inputProtocolFactory,
            outputProtocolFactory),
    clients_(0),
    hwm_(0),
    limit_(INT64_MAX),
    connectedClient_(false) {
}

TFStackServerFramework::~TFStackServerFramework() = default;

template <typename T>
static void releaseOneDescriptor(const string& name, T& pTransport) {
  if (pTransport) {
    try {
      pTransport->close();
    } catch (const TTransportException& ttx) {
      string errStr = string("TFStackServerFramework " + name + " close failed: ") + ttx.what();
      GlobalOutput(errStr.c_str());
    }
  }
}

bool TFStackServerFramework::process_one() {
  static shared_ptr<TTransport> client;
  static shared_ptr<TTransport> inputTransport;
  static shared_ptr<TTransport> outputTransport;
  static shared_ptr<TProtocol> inputProtocol;
  static shared_ptr<TProtocol> outputProtocol;
  try {
    if (!connectedClient_) {      
      // Reset resources from previous processing
      outputProtocol.reset();
      inputProtocol.reset();
      outputTransport.reset();
      inputTransport.reset();
      client.reset();
      pClient_.reset();

      // Use non-blocking accept
      client = serverTransport_->accept();

      if (!client) {
        // No client ready, return false to indicate no processing done
        return false;
      }

      inputTransport = inputTransportFactory_->getTransport(client);
      outputTransport = outputTransportFactory_->getTransport(client);
      if (!outputProtocolFactory_) {
        inputProtocol = inputProtocolFactory_->getProtocol(inputTransport, outputTransport);
        outputProtocol = inputProtocol;
      } else {
        inputProtocol = inputProtocolFactory_->getProtocol(inputTransport);
        outputProtocol = outputProtocolFactory_->getProtocol(outputTransport);
      }

      pClient_ = std::make_shared<TConnectedClient>(
        getProcessor(inputProtocol, outputProtocol, client),
        inputProtocol,
        outputProtocol,
        eventHandler_,
        client
      );
      
      //connectedClient_ = true;
    }

    // Process RPC for connected client
    if (pClient_) {
      // First, check if clientSocket has been closed by remote node (i.e client disconnected)  
      // fprintf(stderr, "Debug: checking if client is open or closed.\n");
      if (!client->isOpen()) {
          //fprintf(stderr, "Debug: Client is closed.\n");
          clientSocketClosed_ = true;
          connectedClient_ = false;
      }
      else {
          // fprintf(stderr, "Debug: Client is still open.\n");
          clientSocketClosed_ = false;
          connectedClient_ = true;
      }
      onClientConnected(pClient_);
      // After processing, we keep connectedClient_ true to continue processing in the next iteration
      // If you want to disconnect after each RPC, set connectedClient_ = false; here
      if (!connectedClient_) {
          // fprintf(stderr, "returning false in process_one() after calling client.cleanup()\n");
          return false;
      }
    }

    return true; // Indicate that we processed a client

  } catch (TTransportException& ttx) {
    // Handle exceptions
    releaseOneDescriptor("inputTransport", inputTransport);
    releaseOneDescriptor("outputTransport", outputTransport);
    releaseOneDescriptor("client", client);

    if (ttx.getType() == TTransportException::TIMED_OUT
        || ttx.getType() == TTransportException::CLIENT_DISCONNECT) {
      // Accept timeout and client disconnect - return false to continue processing in main loop
      // connectedClient_ = false;
      // pClient_.reset();
      // return false;
    } else if (ttx.getType() == TTransportException::END_OF_FILE
               || ttx.getType() == TTransportException::INTERRUPTED) {
      // Server was interrupted. Log and return false.
      GlobalOutput.printf("Server interrupted: %s", ttx.what());
      // connectedClient_ = false;
      // pClient_.reset();
      // return false;
    } else {
      // All other transport exceptions are logged.
      string errStr = string("TServerTransport exception: ") + ttx.what();
      GlobalOutput(errStr.c_str());
      // connectedClient_ = false;
      // pClient_.reset();
      // return false;
    }
  } catch (std::exception& x) {
    GlobalOutput.printf("std::exception in process_one: %s", x.what());
    // connectedClient_ = false;
    // pClient_.reset();
    // return false;
  }
  //releaseOneDescriptor("serverTransport", serverTransport_);
  return false;
}

int64_t TFStackServerFramework::getConcurrentClientLimit() const {
  Synchronized sync(mon_);
  return limit_;
}

int64_t TFStackServerFramework::getConcurrentClientCount() const {
  Synchronized sync(mon_);
  return clients_;
}

int64_t TFStackServerFramework::getConcurrentClientCountHWM() const {
  Synchronized sync(mon_);
  return hwm_;
}

void TFStackServerFramework::setConcurrentClientLimit(int64_t newLimit) {
  if (newLimit < 1) {
    throw std::invalid_argument("newLimit must be greater than zero");
  }
  Synchronized sync(mon_);
  limit_ = newLimit;
  if (limit_ - clients_ > 0) {
    mon_.notify();
  }
}

void TFStackServerFramework::stop() {
  // Order is important because serve() releases serverTransport_ when it is
  // interrupted, which closes the socket that interruptChildren uses.
  serverTransport_->interruptChildren();
  serverTransport_->interrupt();
}

void TFStackServerFramework::newlyConnectedClient(const shared_ptr<TConnectedClient>& pClient) {
  {
    Synchronized sync(mon_);
    ++clients_;
    hwm_ = (std::max)(hwm_, clients_);
  }

  onClientConnected(pClient); //in TSimpleServer.cpp:88
}

void TFStackServerFramework::disposeConnectedClient(TConnectedClient* pClient) {
  onClientDisconnected(pClient);
  delete pClient;

  Synchronized sync(mon_);
  if (limit_ - --clients_ > 0) {
    mon_.notify();
  }
}

}
}
} // apache::thrift::server

