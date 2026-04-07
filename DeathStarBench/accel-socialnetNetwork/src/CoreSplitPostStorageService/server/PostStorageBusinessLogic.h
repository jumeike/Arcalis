#ifndef SOCIAL_NETWORK_MICROSERVICES_POSTSTORAGEBUSINESSLOGIC_H
#define SOCIAL_NETWORK_MICROSERVICES_POSTSTORAGEBUSINESSLOGIC_H

// REMOVED: Database includes
// #include <bson/bson.h>
// #include <libmemcached/memcached.h>
// #include <libmemcached/util.h>
// #include <mongoc.h>

#include <nlohmann/json.hpp>
#include <atomic>
#include <condition_variable>
#include <map>
#include <vector>
#include <mutex>

#include "../../../gen-cpp/social_network_types.h"
#include "../../logger.h"

#ifdef ENABLE_CEREBELLUM
#define cmd_send_dpdk_buf    0
#define cmd_send_dpdk_len    1
#define cmd_set_app_flag     2
#define cmd_send_app_resp    3
#define cmd_send_app_buf     4
#define cmd_set_dpdk_flag    5
#endif // ENABLE_CEREBELLUM

#ifdef ENABLE_TRACING
#include "PacketLogger.h"
#endif

#ifdef ENABLE_GEM5
#include "../../../gen-cpp/PostStorageService.h"
#include <thrift/TDispatchProcessor.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TSocket.h>
#include <PacketReplaySocket.h>
#include <gem5/m5ops.h>
#endif // ENABLE_GEM5

namespace social_network {

using json = nlohmann::json;

class PostStorageHandler; // Forward declaration

class PostStorageBusinessLogic {
 public:
  // UPDATED: Constructor no longer takes database pools
  PostStorageBusinessLogic(void* unused1 = nullptr, void* unused2 = nullptr);
#ifdef ENABLE_GEM5
  ~PostStorageBusinessLogic();
#else  
  ~PostStorageBusinessLogic() = default;
#endif

  // Core business logic functions
  void StorePost(int64_t req_id, const Post& post,
                 const std::map<std::string, std::string>& carrier);
  void ReadPost(Post& _return, int64_t req_id, int64_t post_id,
                const std::map<std::string, std::string>& carrier);
  void ReadPosts(std::vector<Post>& _return, int64_t req_id, 
                 const std::vector<int64_t>& post_ids,
                 const std::map<std::string, std::string>& carrier);

  // Buffer-based versions for accelerator offload
#ifdef ENABLE_GEM5
  void StorePost();
  void ReadPost();
  void ReadPosts();
#endif

  // Metrics and monitoring
  void GetMetrics(std::map<std::string, int64_t>& metrics);
  void ResetMetrics();

#ifdef ENABLE_GEM5
  // Buffer access methods
  uint8_t* getRecvBuffer() const { return recv_buf_; }
  uint8_t* getRespBuffer() const { return resp_buf_; }
  size_t getBufferSize() const { return BUFFER_SIZE; }
   
  void setHandler(PostStorageHandler* handler) { handler_ = handler; }
  void setTraceConfig(const std::string& file, int requests);
  void setCoreSplitConfig(int rpc_core, int business_core, bool enable_split = true) {
    rpc_core_ = rpc_core;
    business_core_ = business_core;
    enable_core_split_ = enable_split;
  }
#endif // ENABLE_GEM5

 private:
  // REMOVED: Database pools
  // memcached_pool_st* _memcached_client_pool;
  // mongoc_client_pool_t* _mongodb_client_pool;
  
  // Metrics (using atomics for thread safety)
  mutable std::mutex _metrics_mutex;
  std::atomic<uint64_t> _store_requests{0};
  std::atomic<uint64_t> _read_requests{0};
  std::atomic<uint64_t> _read_multi_requests{0};
  std::atomic<uint64_t> _cache_hits{0};
  std::atomic<uint64_t> _cache_misses{0};
  std::atomic<uint64_t> _mongodb_operations{0};
  std::atomic<uint64_t> _total_processing_time_ns{0};
  std::atomic<uint64_t> _mongodb_time_ns{0};
  std::atomic<uint64_t> _memcached_time_ns{0};

  // REMOVED: Database helper functions
  // Post ParsePostFromBson(const bson_t* doc);
  // Post ParsePostFromJson(const json& post_json);
  // void SetPostToMemcached(int64_t post_id, const std::string& post_json);
  // std::string PostToJsonString(const Post& post);

#ifdef ENABLE_GEM5
  // Pointer to PostStorageHandler
  PostStorageHandler* handler_;
  
  // Buffer management
  static constexpr size_t BUFFER_SIZE = 4096; // Larger buffer for post data
  static constexpr size_t ALIGNMENT = 0x40;

  uint8_t* raw_recv_buf_;
  uint8_t* raw_resp_buf_;
  uint8_t* recv_buf_;    // Receive Buffer
  uint8_t* resp_buf_;    // Response Buffer
  size_t resp_buf_offset_;
  size_t resp_buf_size_;

  uint8_t* allocateAlignedBuffer(uint8_t* raw_buf);
  bool initializeBuffers();
  void cleanupBuffers();
  
  // Trace File management
  std::string trace_file_;
  int num_requests_;
  apache::thrift::transport::TSocket* getSocketFromTransport();
  bool checkReplayEOF() {
     auto socket = getSocketFromTransport();
     return socket ? socket->isReplayEOF() : false;
  }
  bool validateReplay() {
     auto socket = getSocketFromTransport();
     return socket ? socket->getReplaySocket().validateReplay("poststorage_traces/rpc_to_dpdk_1k.bin") : false;
  }

  int rpc_core_{0};
  int business_core_{1};
  bool enable_core_split_{true};
  std::mutex split_mutex_;
  std::condition_variable split_cv_;
  bool split_request_ready_{false};
  bool split_dispatch_done_{false};
  bool split_stop_worker_{false};
  bool split_dispatch_result_{false};
#endif // ENABLE_GEM5
 public:
#ifdef ENABLE_GEM5
// SW path member variables
  std::string fname_;
  int32_t seqid_;
  void* ctx_;
  apache::thrift::TDispatchProcessor* processor_;
  std::shared_ptr<::apache::thrift::protocol::TProtocol> in_;
  std::shared_ptr<::apache::thrift::protocol::TProtocol> out_;
  
  // Results for different operations
  PostStorageService_StorePost_result store_result_;
  PostStorageService_StorePost_args store_args_;
  PostStorageService_ReadPost_result read_result_;
  PostStorageService_ReadPost_args read_args_;
  PostStorageService_ReadPosts_result read_posts_result_;
  PostStorageService_ReadPosts_args read_posts_args_;
  
  void* connectionContext_;
  size_t read_pos_, write_pos_;
  
// SW path member functions
  void callSWread();
  bool callSWdispatch();
  void callSWwrite();
  void callSWsendresp(bool success);
  void callSWSendBuf();
  void runLoop(apache::thrift::TDispatchProcessor* processor,
            std::shared_ptr<::apache::thrift::protocol::TProtocol> in,
            std::shared_ptr<::apache::thrift::protocol::TProtocol> out,
            void* connectionContext); 
  // Helper functions for software deserialization
  int32_t readInt32(uint8_t* buf, size_t& offset) {
    int32_t value = *reinterpret_cast<int32_t*>(buf + offset);
    offset += 4;
    return value;
  }

  int64_t readInt64(uint8_t* buf, size_t& offset) {
    int64_t value = *reinterpret_cast<int64_t*>(buf + offset);
    offset += 8;
    return value;
  }

  std::string readString(uint8_t* buf, size_t& offset) {
    int32_t length = readInt32(buf, offset);
    std::string str(reinterpret_cast<char*>(buf + offset), length);
    offset += length;
    return str;
  }
#endif // ENABLE_GEM5  

#ifdef ENABLE_CEREBELLUM
// HW Accelerator path member variables
  volatile uint64_t* readAddress;
  volatile uint64_t* sendAddress;
// HW Accelerator path member functions
  void callEngineRead();
  bool callEngineDispatch();
  void callEngineWrite();
  void callEngineSendresp(bool success);
  void callEngineSendBuf();
  void setAddresses(volatile uint64_t* sAddress, volatile uint64_t* rAddress) {
         sendAddress = sAddress;
         readAddress = rAddress;
     }

  // Helper functions for writing to response buffer
  void serializePostToResponse(const Post& post);
  void serializePostsToResponse(const std::vector<Post>& posts);
  void serializePostAtOffset(uint8_t* buf, size_t base_offset, const Post& post);

  void writeInt32ToBuffer(uint8_t* buf, size_t& offset, int32_t value) {
      *reinterpret_cast<int32_t*>(buf + offset) = value;
      offset += 4;
  }

  void writeInt64ToBuffer(uint8_t* buf, size_t& offset, int64_t value) {
      *reinterpret_cast<int64_t*>(buf + offset) = value;
      offset += 8;
  }

  void writeStringToBuffer(uint8_t* buf, size_t& offset, const std::string& str) {
      // Write length then data
      writeInt32ToBuffer(buf, offset, static_cast<int32_t>(str.size()));
      memcpy(buf + offset, str.data(), str.size());
      offset += str.size();
  }
#endif // ENABLE_CEREBELLUM
};
} // namespace social_network

#endif // SOCIAL_NETWORK_MICROSERVICES_POSTSTORAGEBUSINESSLOGIC_H
