#include <iostream>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include "UniqueIdHandler.h"

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::server;
using namespace minimal_social;

int main() {
    const int port = 9092;

    // Create handler
    auto handler = std::make_shared<UniqueIdHandler>();

    // Create processor
    auto processor = std::make_shared<UniqueIdServiceProcessor>(handler);

    // Create server
    auto transport = std::make_shared<TServerSocket>(port);
    auto transportFactory = std::make_shared<TFramedTransportFactory>();
    auto protocolFactory = std::make_shared<TBinaryProtocolFactory>();

    TThreadedServer server(
        processor, transport, transportFactory, protocolFactory
    );

    std::cout << "Starting UniqueId server on port " << port << std::endl;
    server.serve();

    return 0;
}
