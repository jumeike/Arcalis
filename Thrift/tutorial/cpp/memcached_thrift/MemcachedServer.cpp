#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/protocol/TCompactProtocol.h>
#include <thrift/protocol/TJSONProtocol.h>
#include <thrift/server/TSimpleServer.h>
//#include <thrift/server/TFStackSimpleServer.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TServerUDPSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include "gen-cpp/MemcachedService.h"
// #include <libmemcached/memcached.h>
#include <iostream>
#include <memory>
#include <mutex>

// Save thrift package definitions
#define THRIFT_PACKAGE PACKAGE
#define THRIFT_PACKAGE_BUGREPORT PACKAGE_BUGREPORT
#define THRIFT_PACKAGE_NAME PACKAGE_NAME
#define THRIFT_PACKAGE_STRING PACKAGE_STRING
#define THRIFT_PACKAGE_TARNAME PACKAGE_TARNAME
#define THRIFT_PACKAGE_VERSION PACKAGE_VERSION

#undef PACKAGE
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME 
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

// External dependencies
extern "C" {
#include "memcached.h"
#include "slabs.h"
#include "storage.h"
#include "hash.h"
#include "assoc.h"
#include "stats_prefix.h"
#include <event2/event.h>
#include <event2/thread.h>
}

#include <thread>
#include <sys/time.h>
#include <time.h>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;
using namespace ::thrift_memcached;

class MemcachedServiceHandler : virtual public MemcachedServiceIf {
  private:
    // Reference to the global storage instance from memcached
    LIBEVENT_THREAD thread;
    std::mutex mutex_;
    struct event_base *main_base;
    struct event clockevent;
    std::thread event_thread;

    static void clock_handler(evutil_socket_t fd, short which, void *arg) {
      struct timeval t = {.tv_sec = 1, .tv_usec = 0};
      struct timespec ts;

      // Update current_time
      if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
          struct timeval tv;
          gettimeofday(&tv, NULL);
          current_time = (rel_time_t)(tv.tv_sec - process_started);
      } else {
          current_time = (rel_time_t)(ts.tv_sec - process_started);
      }

      // Reschedule timer
      MemcachedServiceHandler *handler = (MemcachedServiceHandler *)arg;
      evtimer_del(&handler->clockevent);
      evtimer_set(&handler->clockevent, clock_handler, handler);
      event_base_set(handler->main_base, &handler->clockevent);
      evtimer_add(&handler->clockevent, &t);
    }

  public:
    MemcachedServiceHandler() {
      // Initialize settings
      settings.use_cas = true;
      settings.maxbytes = static_cast<size_t>(4ULL * 1024 * 1024 * 1024); // 4GB
      settings.maxconns = 1024;
      settings.factor = 1.25;
      settings.chunk_size = 256;
      settings.num_threads = 1;
      settings.item_size_max = 1024 * 1024;
      settings.slab_page_size = 1024 * 1024;
      settings.slab_chunk_size_max = settings.slab_page_size / 2;
      settings.hashpower_init = 0;
      settings.oldest_live = 0;

      enum hashfunc_type hash_type = MURMUR3_HASH;

      if (hash_init(hash_type) != 0) {
        throw std::runtime_error("Failed to initialize hash_algorithm!\n");
    }
      
      // Initialize stats
      memset(&stats, 0, sizeof(struct stats));
      memset(&stats_state, 0, sizeof(struct stats_state));
      stats_state.accepting_conns = true;
      process_started = time(0) - ITEM_UPDATE_INTERVAL - 2;
      stats_prefix_init(settings.prefix_delimiter);

      // Initialize subsystems
      slabs_init(settings.maxbytes, settings.factor, true, nullptr, nullptr, false);
      assoc_init(settings.hashpower_init);
      memcached_thread_init(settings.num_threads, nullptr);
      
      // Initialize thread stats
      threadlocal_stats_reset();
      void *result = slabs_alloc(48, 1, 0);
      std::cerr << "Initial slab allocation: " << result << std::endl;
      // if (!slabs_alloc(settings.chunk_size, 1, 0)) {
      //     throw std::runtime_error("Failed to initialize storage");
      // }

      // Initialize thread
      memset(&thread, 0, sizeof(LIBEVENT_THREAD));

      // Initialize event base
       main_base = event_base_new();
       if (!main_base) {
           throw std::runtime_error("Failed to create event base");
       }

       // Setup timer
       struct timeval t = {.tv_sec = 1, .tv_usec = 0};
       evtimer_set(&clockevent, clock_handler, this);
       event_base_set(main_base, &clockevent);
       evtimer_add(&clockevent, &t);

       // Start event loop
       event_thread = std::thread([this]() {
           event_base_dispatch(main_base);
       });
       event_thread.detach();
    }

    void getRequest(std::vector<int8_t>& _return, const std::vector<int8_t>& key) override {
      // std::lock_guard<std::mutex> lock(mutex_);
      std::string key_str(reinterpret_cast<const char*>(key.data()), key.size());
      // printf("GET attempt with key size: %zu, key content: ", key.size());
      // for(size_t i = 0; i < key.size(); i++) {
      //     printf("%02x ", key[i]);
      // }
      // printf("\n");
      // Calculate hash value for key
      // uint32_t hv = hash(key_str.c_str(), key_str.length());
      
      // Get the item directly from cache
      item* it = item_get(key_str.c_str(), key_str.length(), &thread, true);
      
      if (it != nullptr) {
          // Extract value from item
          const char* value_ptr = ITEM_data(it);
          size_t value_len = it->nbytes;
          
          // Copy value to return buffer
          _return.assign(reinterpret_cast<const int8_t*>(value_ptr),
                      reinterpret_cast<const int8_t*>(value_ptr + value_len));
          
          // Release our reference
          item_remove(it);
      } else {
        std::cout << "Item not found for key: " << key_str << std::endl;
        _return.clear();
      }
    }

    bool setRequest(const std::vector<int8_t>& key, const std::vector<int8_t>& value) override {
      // std::lock_guard<std::mutex> lock(mutex_);
      std::string key_str(reinterpret_cast<const char*>(key.data()), key.size());
      // printf("SET attempt with key size: %zu, key content: ", key.size());
      // for(size_t i = 0; i < key.size(); i++) {
      //     printf("%02x ", key[i]);
      // }
      // printf("\n");

        // printf("SET attempt with value size: %zu, value content: ", value.size());
        // for(size_t i = 0; i < value.size(); i++) {
        //     printf("%02x ", value[i]);
        // }
        // printf("\n");

      // std::cerr << "Setting key:" << key_str << " value len:" << value.size() << std::endl;
      // Calculate hash
      // uint32_t hv = hash(key_str.c_str(), key_str.length());
      // size_t used_slabs = slabs_clsid(settings.item_size_max);
      // std::cerr << "Used slabs: " << used_slabs << std::endl;
      // Allocate new item
      item* it = item_alloc(key_str.c_str(), key_str.length(), 0, 0, value.size());
      if (it == nullptr) {
        std::cerr << "item_alloc failed" << std::endl;
          return false;
      }
      
      // Copy value into item
      memcpy(ITEM_data(it), value.data(), value.size());
      
      // Store the item
      enum store_item_type status = store_item(it, NREAD_SET, &thread, nullptr, nullptr, 0, false);
      // std::cerr << "store_item returned:" << status << std::endl;
      return (status == STORED);
    }

    ~MemcachedServiceHandler() {
      if (main_base) {
          event_base_loopbreak(main_base);
          event_base_free(main_base);
      }
    }
  };

int main(int argc, char **argv) {
    //ff_init(argc, argv);
    int port = 9090;
    ::std::shared_ptr<MemcachedServiceHandler> handler(new MemcachedServiceHandler());
    ::std::shared_ptr<TProcessor> processor(new MemcachedServiceProcessor(handler));
    //::std::shared_ptr<TServerTransport> serverTransport(new TServerUDPSocket("192.168.1.1", port, true)); // usingKq = true
    //::std::shared_ptr<TServerTransport> serverTransport(new TServerUDPSocket("localhost", port));
    //::std::shared_ptr<TServerTransport> serverTransport(new TServerUDPSocket(port));
    ::std::shared_ptr<TServerTransport> serverTransport(new TServerSocket("localhost", port));
    ::std::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
    ::std::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());
    // ::std::shared_ptr<TProtocolFactory> protocolFactory(new TJSONProtocolFactory());

    //TThreadedServer server(processor, serverTransport, transportFactory, protocolFactory);
    TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);
    //TFStackSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);
    std::cout << "Starting the server on port " << port << "..." << std::endl;
    server.serve();
    return 0;
}
