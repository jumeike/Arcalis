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

#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/ThreadFactory.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/protocol/TJSONProtocol.h>
#include <thrift/protocol/TCompactProtocol.h>
#include <thrift/server/TSimpleServer.h>
//#include <thrift/server/TFStackSimpleServer.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TServerUDPSocket.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/TToString.h>

#include <iostream>
#include <stdexcept>
#include <sstream>

//#include <ff_api.h>
//#include <ff_config.h>

#include "../gen-cpp/Calculator.h"

using namespace std;
using namespace apache::thrift;
using namespace apache::thrift::concurrency;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::server;

using namespace tutorial;
using namespace shared;

class CalculatorHandler : public CalculatorIf {
public:
  CalculatorHandler() = default;

  void ping() override { cout << "ping()" << '\n'; }

  int32_t add(const int32_t n1, const int32_t n2) override {
    cout << "add(" << n1 << ", " << n2 << ")" << '\n';
    return n1 + n2;
  }

  int32_t calculate(const int32_t logid, const Work& work) override {
    cout << "calculate(" << logid << ", " << work << ")" << '\n';
    int32_t val;

    switch (work.op) {
    case Operation::ADD:
      val = work.num1 + work.num2;
      break;
    case Operation::SUBTRACT:
      val = work.num1 - work.num2;
      break;
    case Operation::MULTIPLY:
      val = work.num1 * work.num2;
      break;
    case Operation::DIVIDE:
      if (work.num2 == 0) {
        InvalidOperation io;
        io.whatOp = work.op;
        io.why = "Cannot divide by 0";
        throw io;
      }
      val = work.num1 / work.num2;
      break;
    default:
      InvalidOperation io;
      io.whatOp = work.op;
      io.why = "Invalid Operation";
      throw io;
    }

    SharedStruct ss;
    ss.key = logid;
    ss.value = to_string(val);

    log[logid] = ss;

    return val;
  }

  void getStruct(SharedStruct& ret, const int32_t logid) override {
    cout << "getStruct(" << logid << ")" << '\n';
    ret = log[logid];
  }

  void zip() override { cout << "zip()" << '\n'; }

protected:
  map<int32_t, SharedStruct> log;
};

/*
  CalculatorIfFactory is code generated.
  CalculatorCloneFactory is useful for getting access to the server side of the
  transport.  It is also useful for making per-connection state.  Without this
  CloneFactory, all connections will end up sharing the same handler instance.
*/
class CalculatorCloneFactory : virtual public CalculatorIfFactory {
 public:
  ~CalculatorCloneFactory() override = default;
  CalculatorIf* getHandler(const ::apache::thrift::TConnectionInfo& connInfo) override
  {
    std::shared_ptr<TSocket> sock = std::dynamic_pointer_cast<TSocket>(connInfo.transport);
    cout << "Incoming connection\n";
    cout << "\tSocketInfo: "  << sock->getSocketInfo() << "\n";
    cout << "\tPeerHost: "    << sock->getPeerHost() << "\n";
    cout << "\tPeerAddress: " << sock->getPeerAddress() << "\n";
    cout << "\tPeerPort: "    << sock->getPeerPort() << "\n";
    return new CalculatorHandler;
  }
  void releaseHandler( ::shared::SharedServiceIf* handler) override {
    delete handler;
  }
};

int main(int argc, char* argv[]) {
  //ff_init(argc, argv);
  TSimpleServer server(
  //TFStackSimpleServer server(
    std::make_shared<CalculatorProcessor>(std::make_shared<CalculatorHandler>()),
    std::make_shared<TServerSocket>("localhost", 9090),  // fstack/default tcp implementation
    //std::make_shared<TServerUDPSocket>("192.168.1.1", 9090), // default udp implementation
    //std::make_shared<TServerUDPSocket>(9090), // dpdk implementation
    //std::make_shared<TServerUDPSocket>("192.168.1.1", 9090, false), //fstack udp implementation (usingKq=true/false)
    std::make_shared<TBufferedTransportFactory>(),
    std::make_shared<TBinaryProtocolFactory>());
    //std::make_shared<TJSONProtocolFactory>());

  cout << "Starting the server..." << '\n';
  server.serve();  // This will call ff_run() internally
  cout << "Done." << '\n';
  return 0;
}
