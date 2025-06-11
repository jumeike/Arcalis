#include <iostream>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include "gen-cpp/ComposePostService.h"

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace minimal_social;

int main() {
    auto socket = std::make_shared<TSocket>("localhost", 9090);
    auto transport = std::make_shared<TFramedTransport>(socket);
    auto protocol = std::make_shared<TBinaryProtocol>(transport);
    ComposePostServiceClient client(protocol);

    try {
        transport->open();
        
        // Test post composition
        client.ComposePost(1, "user1", "Hello, this is a test post!");
        std::cout << "Post composed successfully!" << std::endl;
        
        transport->close();
    } catch (ServiceException& se) {
        std::cout << "Service Exception: " << se.message << std::endl;
    } catch (std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

    return 0;
}
