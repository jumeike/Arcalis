#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/protocol/TCompactProtocol.h>
#include <thrift/protocol/TJSONProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/server/TFStackSimpleServer.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TServerUDPSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include "gen-cpp/MemcachedService.h"
#include <libmemcached/memcached.h>
#include <iostream>
#include <memory>

#include <ff_api.h>
#include <ff_config.h>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;
using namespace ::thrift_memcached;

class MemcachedServiceHandler : virtual public MemcachedServiceIf {
  private:
      using memcached_ptr = std::unique_ptr<memcached_st, decltype(&memcached_free)>;
      memcached_ptr memc;

  public:
      MemcachedServiceHandler() : memc(memcached_create(nullptr), memcached_free) {
          memcached_server_st* servers = memcached_server_list_append(nullptr, "localhost", 11211, nullptr);
          memcached_server_push(memc.get(), servers);
          memcached_server_list_free(servers);
          
          memcached_return_t rc = memcached_behavior_set(memc.get(), MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1);
          if (rc != MEMCACHED_SUCCESS) {
            printf("Failed to set binary protocol: %s\n", memcached_strerror(memc.get(), rc));
          }
      }

      void getRequest(std::vector<int8_t>& _return, const std::vector<int8_t>& key) override {
        size_t value_length;
        uint32_t flags;
        memcached_return_t rc;
        
        std::string key_str(reinterpret_cast<const char*>(key.data()), key.size());

        char* value = memcached_get(memc.get(), key_str.c_str(), key_str.length(), &value_length, &flags, &rc);
        
        if (rc == MEMCACHED_SUCCESS && value != nullptr) {
            _return.assign(reinterpret_cast<const int8_t*>(value), 
            reinterpret_cast<const int8_t*> (value + value_length));
            free(value);
        } else {
            _return.clear();
        }
      }

      bool setRequest(const std::vector<int8_t>& key, const std::vector<int8_t>& value) override {
        std::string key_str(reinterpret_cast<const char*>(key.data()), key.size());
        std::string value_str(reinterpret_cast<const char*>(value.data()), value.size());
        memcached_return_t rc = memcached_set(memc.get(), key_str.c_str(), key_str.length(), 
                                              value_str.c_str(), value_str.length(), 
                                              (time_t)0, (uint32_t)0);
        
        if (rc != MEMCACHED_SUCCESS) {
          printf("memcached_set failed with error: %s\n", memcached_strerror(memc.get(), rc));
        }
        return (rc == MEMCACHED_SUCCESS);
      }
  };

int main(int argc, char **argv) {
    //ff_init(argc, argv);
    int port = 9090;
    ::std::shared_ptr<MemcachedServiceHandler> handler(new MemcachedServiceHandler());
    ::std::shared_ptr<TProcessor> processor(new MemcachedServiceProcessor(handler));
    //::std::shared_ptr<TServerTransport> serverTransport(new TServerUDPSocket("192.168.1.1", port, true)); // usingKq = true
    ::std::shared_ptr<TServerTransport> serverTransport(new TServerUDPSocket("192.168.1.1", port));
    //::std::shared_ptr<TServerTransport> serverTransport(new TServerUDPSocket(port));
    //::std::shared_ptr<TServerTransport> serverTransport(new TServerSocket("192.168.1.1", port));
    ::std::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
    ::std::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());
    //::std::shared_ptr<TProtocolFactory> protocolFactory(new TJSONProtocolFactory());

    //TThreadedServer server(processor, serverTransport, transportFactory, protocolFactory);
    TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);
    //TFStackSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);
    std::cout << "Starting the server on port " << port << "..." << std::endl;
    server.serve();
    return 0;
}
