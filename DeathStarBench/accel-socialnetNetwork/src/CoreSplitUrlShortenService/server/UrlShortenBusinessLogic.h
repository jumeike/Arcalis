#ifndef SOCIAL_NETWORK_MICROSERVICES_URLSHORTENBUSINESSLOGIC_H
#define SOCIAL_NETWORK_MICROSERVICES_URLSHORTENBUSINESSLOGIC_H

#include <random>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>
#include <atomic>
#include <map>
#include <memory>
#include <condition_variable>

// Local-storage optimized variant does not use MongoDB/Memcached pools.

#include "../../../gen-cpp/social_network_types.h"
#include "../../logger.h"

#define HOSTNAME "http://short-url/"

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
#include "../../../gen-cpp/UrlShortenService.h"
#include <thrift/TDispatchProcessor.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TSocket.h>
#include "PacketReplaySocket.h"
#ifdef ENABLE_GEM5_TEST
#include <gem5/m5ops.h>
#endif // ENABLE_GEM5_TEST
#endif // ENABLE_GEM5

namespace social_network {

class UrlShortenHandler;

class UrlShortenBusinessLogic {
public:
  UrlShortenBusinessLogic(void* unused1 = nullptr,
                          void* unused2 = nullptr);
#ifdef ENABLE_GEM5
  ~UrlShortenBusinessLogic();
#else
  ~UrlShortenBusinessLogic() = default;
#endif // ENABLE_GEM5

  // Core business logic functions
  void ComposeUrls(std::vector<Url>& _return,
                   int64_t req_id,
                   const std::vector<std::string>& urls);
#ifdef ENABLE_GEM5
  void ComposeUrls();      // Buffer-based version for GEM5/accelerator path
  void GetExtendedUrls();  // Buffer-based version for GEM5/accelerator path
#endif // ENABLE_GEM5

  void GetExtendedUrls(std::vector<std::string>& _return,
                       int64_t req_id,
                       const std::vector<std::string>& shortened_urls);

  // Metrics and monitoring
  void GetMetrics(std::map<std::string, int64_t>& metrics) const;
  void ResetMetrics();

#ifdef ENABLE_GEM5
  uint8_t* getRecvBuffer() const { return recv_buf_; }
  uint8_t* getRespBuffer() const { return resp_buf_; }
  size_t getBufferSize() const { return BUFFER_SIZE; }

  void setHandler(UrlShortenHandler* handler) { handler_ = handler; }
  void setTraceConfig(const std::string& file, int requests);
  void setCoreSplitConfig(int rpc_core, int business_core, bool enable_split = true) {
    rpc_core_ = rpc_core;
    business_core_ = business_core;
    enable_core_split_ = enable_split;
  }
#endif // ENABLE_GEM5

private:
  void* _unused_pool_1{nullptr};
  void* _unused_pool_2{nullptr};
  
  // Random string generation
  static std::mt19937 _generator;
  std::uniform_int_distribution<int> _distribution;
  std::mutex _thread_lock;
  
  // Metrics
  std::atomic<uint64_t> _requests_processed{0};
  std::atomic<uint64_t> _compose_requests{0};
  std::atomic<uint64_t> _get_extended_requests{0};
  std::atomic<uint64_t> _total_processing_time_ns{0};
  std::atomic<uint64_t> _mongo_insert_time_ns{0};
  std::atomic<uint64_t> _memcached_time_ns{0};

  // Helper functions
  std::string _GenRandomStr(int length);
  void _StoreUrlsInMongo(const std::vector<Url>& urls);
  void _StoreUrlsInMemcached(const std::vector<Url>& urls);
  void _FetchUrlsFromMemcached(std::vector<std::string>& _return,
                               const std::vector<std::string>& shortened_urls,
                               std::vector<std::string>& cache_misses);
  void _FetchUrlsFromMongo(std::vector<std::string>& _return,
                           const std::vector<std::string>& shortened_urls);

#ifdef ENABLE_GEM5
  UrlShortenHandler* handler_{nullptr};
  static constexpr size_t BUFFER_SIZE = 64 * 1024;
  static constexpr size_t ALIGNMENT = 0x40;

  uint8_t* raw_recv_buf_{nullptr};
  uint8_t* raw_resp_buf_{nullptr};
  uint8_t* recv_buf_{nullptr};
  uint8_t* resp_buf_{nullptr};
  size_t resp_buf_offset_{0};
  size_t resp_buf_size_{0};

  uint8_t* allocateAlignedBuffer(uint8_t* raw_buf);
  bool initializeBuffers();
  void cleanupBuffers();

  std::string trace_file_;
  int num_requests_{0};

  int rpc_core_{0};
  int business_core_{1};
  bool enable_core_split_{true};
  std::mutex split_mutex_;
  std::condition_variable split_cv_;
  bool split_request_ready_{false};
  bool split_dispatch_done_{false};
  bool split_stop_worker_{false};
  bool split_dispatch_result_{false};

  apache::thrift::transport::TSocket* getSocketFromTransport();
  bool checkReplayEOF() {
    auto socket = getSocketFromTransport();
    return socket ? socket->isReplayEOF() : false;
  }
  bool validateReplay() {
    auto socket = getSocketFromTransport();
    return socket ? socket->getReplaySocket().validateReplay("urlshorten_traces/rpc_to_dpdk.bin") : false;
  }
#endif // ENABLE_GEM5

public:
#ifdef ENABLE_GEM5
  // SW path members
  std::string fname_;
  int32_t seqid_{0};
  void* ctx_{nullptr};
  apache::thrift::TDispatchProcessor* processor_{nullptr};
  std::shared_ptr<::apache::thrift::protocol::TProtocol> in_;
  std::shared_ptr<::apache::thrift::protocol::TProtocol> out_;
  void* connectionContext_{nullptr};
  size_t read_pos_{0};
  size_t write_pos_{0};

  // Args/result structs for each operation
  UrlShortenService_ComposeUrls_args compose_args_;
  UrlShortenService_ComposeUrls_result compose_result_;
  UrlShortenService_GetExtendedUrls_args get_args_;
  UrlShortenService_GetExtendedUrls_result get_result_;

  // SW path methods
  void callSWread();
  bool callSWdispatch();
  void callSWwrite();
  void callSWsendresp(bool success);
  void callSWSendBuf();
  void runLoop(apache::thrift::TDispatchProcessor* processor,
               std::shared_ptr<::apache::thrift::protocol::TProtocol> in,
               std::shared_ptr<::apache::thrift::protocol::TProtocol> out,
               void* connectionContext);

  // Response serialization helpers shared by SW and Cerebellum paths.
  void serializeComposeUrlsResponse(const std::vector<Url>& urls);
  void serializeExtendedUrlsResponse(const std::vector<std::string>& extended_urls);

  // Helper functions for buffer deserialization
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

    void writeUrlToBuffer(uint8_t* buf, size_t& offset, const Url& url) {
      writeStringToBuffer(buf, offset, url.shortened_url);
      writeStringToBuffer(buf, offset, url.expanded_url);
    }

    Url readUrlFromBuffer(uint8_t* buf, size_t& offset) {
      Url url;
      url.shortened_url = readString(buf, offset);
      url.expanded_url = readString(buf, offset);
      return url;
    }
#endif // ENABLE_GEM5

#ifdef ENABLE_CEREBELLUM
  // HW accelerator path members
  volatile uint64_t* readAddress{nullptr};
  volatile uint64_t* sendAddress{nullptr};

  // HW accelerator path methods
  void callEngineRead();
  bool callEngineDispatch();
  void callEngineWrite();
  void callEngineSendresp(bool success);
  void callEngineSendBuf();
  void setAddresses(volatile uint64_t* sAddress, volatile uint64_t* rAddress) {
    sendAddress = sAddress;
    readAddress = rAddress;
  }
#endif // ENABLE_CEREBELLUM
};

} // namespace social_network

#endif // SOCIAL_NETWORK_MICROSERVICES_URLSHORTENBUSINESSLOGIC_H
