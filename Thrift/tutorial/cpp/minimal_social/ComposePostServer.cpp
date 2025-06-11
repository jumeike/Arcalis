#include <iostream>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include "ComposePostHandler.h"

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::server;
using namespace minimal_social;

int main() {
    const int compose_post_port = 9090;
    const std::string user_service_host = "localhost";
    const int user_service_port = 9091;
    const std::string unique_id_service_host = "localhost";
    const int unique_id_service_port = 9092;

    // Create client pools
    auto user_client_pool = std::make_shared<ClientPool<minimal_social::UserServiceClient>>(
        user_service_host, user_service_port
    );
    auto unique_id_client_pool = std::make_shared<ClientPool<minimal_social::UniqueIdServiceClient>>(
        unique_id_service_host, unique_id_service_port
    );

    // Create handler
    auto handler = std::make_shared<minimal_social::ComposePostHandler>(
        user_client_pool, unique_id_client_pool
    );

    // Create processor
    auto processor = std::make_shared<minimal_social::ComposePostServiceProcessor>(handler);

    // Create server
    auto transport = std::make_shared<TServerSocket>(compose_post_port);
    auto transportFactory = std::make_shared<TFramedTransportFactory>();
    auto protocolFactory = std::make_shared<TBinaryProtocolFactory>();

    TThreadedServer server(
        processor, transport, transportFactory, protocolFactory
    );

    std::cout << "Starting ComposePost server on port " << compose_post_port << std::endl;
    server.serve();

    return 0;
}
