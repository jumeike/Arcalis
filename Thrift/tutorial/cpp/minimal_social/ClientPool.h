#ifndef MINIMAL_SOCIAL_CLIENT_POOL_H
#define MINIMAL_SOCIAL_CLIENT_POOL_H

#include <memory>
#include <mutex>
#include <queue>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransport.h>
#include <thrift/protocol/TBinaryProtocol.h>

using namespace apache::thrift::transport;
using namespace apache::thrift::protocol;

template<typename ClientType>
class ClientPool {
public:
    ClientPool(const std::string& host, int port, size_t pool_size = 8) 
        : _host(host), _port(port), _pool_size(pool_size) {
        for (size_t i = 0; i < pool_size; ++i) {
            _clients.push(createClient());
        }
    }

    std::shared_ptr<ClientType> getClient() {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_clients.empty()) {
            return createClient();
        }
        auto client = _clients.front();
        _clients.pop();
        lock.unlock();
        return client;
    }

    void returnClient(std::shared_ptr<ClientType> client) {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_clients.size() < _pool_size) {
            _clients.push(client);
        }
    }

private:
    std::shared_ptr<ClientType> createClient() {
        auto socket = std::make_shared<TSocket>(_host, _port);
        auto transport = std::make_shared<TFramedTransport>(socket);
        auto protocol = std::make_shared<TBinaryProtocol>(transport);
        auto client = std::make_shared<ClientType>(protocol);
        transport->open();
        return client;
    }

    std::string _host;
    int _port;
    size_t _pool_size;
    std::queue<std::shared_ptr<ClientType>> _clients;
    std::mutex _mutex;
};

#endif
