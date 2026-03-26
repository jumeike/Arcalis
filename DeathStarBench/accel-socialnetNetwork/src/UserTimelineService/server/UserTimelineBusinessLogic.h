#ifndef SOCIAL_NETWORK_MICROSERVICES_USERTIMELINEBUSINESSLOGIC_H
#define SOCIAL_NETWORK_MICROSERVICES_USERTIMELINEBUSINESSLOGIC_H

#include <bson/bson.h>
#include <mongoc.h>
#include <sw/redis++/redis++.h>

#include <atomic>
#include <map>
#include <vector>
#include <mutex>
#include <future>
#include <unordered_map>
#include <cstring>

#ifdef ENABLE_NESTED_RPC_TIMING_MODEL
#include <cstdint>
#endif

#ifdef ENABLE_CEREBELLUM
#define cmd_send_dpdk_buf    0
#define cmd_send_dpdk_len    1
#define cmd_set_app_flag     2
#define cmd_send_app_resp    3
#define cmd_send_app_buf     4
#define cmd_set_dpdk_flag    5
#define cmd_nested_rpc_delay 6
#define nestedrpc_op_storepost 0
#define nestedrpc_op_readpost  1
#endif // ENABLE_CEREBELLUM

#ifdef ENABLE_TRACING
#include "PacketLogger.h"
#endif

#ifdef ENABLE_GEM5
#include "../../../gen-cpp/UserTimelineService.h"
#include <thrift/TDispatchProcessor.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TSocket.h>
#include "PacketReplaySocket.h"
#endif // ENABLE_GEM5

#ifdef ENABLE_GEM5_TEST
#include <gem5/m5ops.h>
#endif

#include "../../../gen-cpp/PostStorageService.h"
#include "../../../gen-cpp/social_network_types.h"
#include "../../ClientPool.h"
#include "../../ThriftClient.h"
#include "../../logger.h"

using namespace sw::redis;

namespace social_network {

class UserTimelineHandler;

class UserTimelineBusinessLogic {
 public:
  UserTimelineBusinessLogic(Redis* redis_pool, mongoc_client_pool_t* mongodb_pool,
                           ClientPool<ThriftClient<PostStorageServiceClient>>* post_client_pool);

  UserTimelineBusinessLogic(Redis* redis_replica_pool, Redis* redis_primary_pool, 
                           mongoc_client_pool_t* mongodb_pool,
                           ClientPool<ThriftClient<PostStorageServiceClient>>* post_client_pool);

  UserTimelineBusinessLogic(RedisCluster* redis_cluster_pool, mongoc_client_pool_t* mongodb_pool,
                           ClientPool<ThriftClient<PostStorageServiceClient>>* post_client_pool);

#ifdef ENABLE_GEM5
    ~UserTimelineBusinessLogic();
#else
    ~UserTimelineBusinessLogic() = default;
#endif // ENABLE_GEM5

  // Core business logic functions
  void WriteUserTimeline(int64_t req_id, int64_t post_id, int64_t user_id, int64_t timestamp,
                        const std::map<std::string, std::string>& carrier);

  void ReadUserTimeline(std::vector<Post>& _return, int64_t req_id, int64_t user_id, 
                       int start, int stop, const std::map<std::string, std::string>& carrier);

#ifdef ENABLE_GEM5
    void WriteUserTimeline();
    void ReadUserTimeline();
#endif // ENABLE_GEM5

  // Metrics and monitoring
  void GetMetrics(std::map<std::string, int64_t>& metrics);
  void ResetMetrics();

#ifdef ENABLE_NESTED_RPC_TIMING_MODEL
    void setNestedRpcTimingModel(bool enabled) { nested_rpc_timing_model_enabled_ = enabled; }
    void setNestedStorepostDelayUs(uint64_t us) { nested_storepost_delay_us_ = us; }
    void setNestedReadpostsDelayUs(uint64_t us) { nested_readposts_delay_us_ = us; }
#endif

#ifdef ENABLE_GEM5
    uint8_t* getRecvBuffer() const { return recv_buf_; }
    uint8_t* getRespBuffer() const { return resp_buf_; }
    size_t getBufferSize() const { return BUFFER_SIZE; }

    void setHandler(UserTimelineHandler* handler) { handler_ = handler; }
    void setTraceConfig(const std::string& file, int requests);
#endif // ENABLE_GEM5

 private:
  Redis* _redis_client_pool;
  Redis* _redis_replica_pool;
  Redis* _redis_primary_pool;
  RedisCluster* _redis_cluster_client_pool;
  mongoc_client_pool_t* _mongodb_client_pool;
  ClientPool<ThriftClient<PostStorageServiceClient>>* _post_client_pool;

  // Metrics (using atomics for thread safety)
  mutable std::mutex _metrics_mutex;
  std::atomic<uint64_t> _write_requests{0};
  std::atomic<uint64_t> _read_requests{0};
  std::atomic<uint64_t> _redis_operations{0};
  std::atomic<uint64_t> _mongodb_operations{0};
  std::atomic<uint64_t> _post_service_calls{0};
  std::atomic<uint64_t> _cache_hits{0};
  std::atomic<uint64_t> _cache_misses{0};
  std::atomic<uint64_t> _total_processing_time_ns{0};
  std::atomic<uint64_t> _redis_time_ns{0};
  std::atomic<uint64_t> _mongodb_time_ns{0};
  std::atomic<uint64_t> _post_service_time_ns{0};

  // Helper functions
  bool IsRedisReplicationEnabled();
  void UpdateRedisTimeline(const std::string& user_id, const std::string& post_id, 
                          double timestamp, UpdateType update_type = UpdateType::NOT_EXIST);
  void UpdateRedisTimeline(const std::string& user_id, 
                          const std::unordered_map<std::string, double>& post_score_map);
  std::vector<std::string> GetTimelineFromRedis(const std::string& user_id, int start, int stop);
  void WriteTimelineToMongoDB(int64_t user_id, int64_t post_id, int64_t timestamp);
  std::vector<std::pair<int64_t, int64_t>> ReadTimelineFromMongoDB(int64_t user_id, int start, int stop);
    void StorePostToPostService(int64_t req_id, int64_t post_id, int64_t user_id, int64_t timestamp,
                                                            const std::map<std::string, std::string>& carrier);
  std::vector<Post> GetPostsFromPostService(int64_t req_id, const std::vector<int64_t>& post_ids,
                                           const std::map<std::string, std::string>& carrier);
    Post buildGeneratedPost(int64_t req_id, int64_t post_id, int64_t user_id,
                                                    int64_t timestamp) const;

#ifdef ENABLE_GEM5
    UserTimelineHandler* handler_{nullptr};
    static constexpr size_t BUFFER_SIZE = 128 * 1024;
    static constexpr size_t ALIGNMENT = 0x40;

    uint8_t* raw_recv_buf_{nullptr};
    uint8_t* raw_resp_buf_{nullptr};
    uint8_t* recv_buf_{nullptr};
    uint8_t* resp_buf_{nullptr};
    size_t resp_buf_offset_{0};
    size_t resp_buf_size_{0};
    int32_t current_operation_type_{-1};
    bool sw_path_success_{false};
    std::vector<Post> sw_path_read_posts_;

    uint8_t* allocateAlignedBuffer(uint8_t* raw_buf);
    bool initializeBuffers();
    void cleanupBuffers();

    std::string trace_file_;
    int num_requests_{0};

    apache::thrift::transport::TSocket* getSocketFromTransport();
    bool checkReplayEOF() {
        auto socket = getSocketFromTransport();
        return socket ? socket->isReplayEOF() : false;
    }
    bool validateReplay() {
        auto socket = getSocketFromTransport();
        return socket ? socket->getReplaySocket().validateReplay("usertimeline_traces/rpc_to_dpdk.bin") : false;
    }
#endif // ENABLE_GEM5

 public:
#ifdef ENABLE_GEM5
    std::string fname_;
    int32_t seqid_{0};
    void* ctx_{nullptr};
    apache::thrift::TDispatchProcessor* processor_{nullptr};
    std::shared_ptr<::apache::thrift::protocol::TProtocol> in_;
    std::shared_ptr<::apache::thrift::protocol::TProtocol> out_;
    void* connectionContext_{nullptr};
    size_t read_pos_{0};
    size_t write_pos_{0};

    UserTimelineService_WriteUserTimeline_args write_args_;
    UserTimelineService_WriteUserTimeline_result write_result_;
    UserTimelineService_ReadUserTimeline_args read_args_;
    UserTimelineService_ReadUserTimeline_result read_result_;

    void callSWread();
    bool callSWdispatch();
    void callSWwrite();
    void callSWsendresp(bool success);
    void callSWSendBuf();
    void runLoop(apache::thrift::TDispatchProcessor* processor,
                             std::shared_ptr<::apache::thrift::protocol::TProtocol> in,
                             std::shared_ptr<::apache::thrift::protocol::TProtocol> out,
                             void* connectionContext);

    void serializeReadUserTimelineResponse(const std::vector<Post>& posts);

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
        writeInt32ToBuffer(buf, offset, static_cast<int32_t>(str.size()));
        std::memcpy(buf + offset, str.data(), str.size());
        offset += str.size();
    }

    void writePostToBuffer(uint8_t* buf, size_t& offset, const Post& post) {
        writeInt64ToBuffer(buf, offset, post.post_id);
        writeInt64ToBuffer(buf, offset, post.creator.user_id);
        writeStringToBuffer(buf, offset, post.creator.username);
        writeInt64ToBuffer(buf, offset, post.req_id);
        writeStringToBuffer(buf, offset, post.text);

        writeInt32ToBuffer(buf, offset, static_cast<int32_t>(post.user_mentions.size()));
        for (const auto& mention : post.user_mentions) {
            writeInt64ToBuffer(buf, offset, mention.user_id);
            writeStringToBuffer(buf, offset, mention.username);
        }

        writeInt32ToBuffer(buf, offset, static_cast<int32_t>(post.media.size()));
        for (const auto& media : post.media) {
            writeInt64ToBuffer(buf, offset, media.media_id);
            writeStringToBuffer(buf, offset, media.media_type);
        }

        writeInt32ToBuffer(buf, offset, static_cast<int32_t>(post.urls.size()));
        for (const auto& url : post.urls) {
            writeStringToBuffer(buf, offset, url.shortened_url);
            writeStringToBuffer(buf, offset, url.expanded_url);
        }

        writeInt64ToBuffer(buf, offset, post.timestamp);
        writeInt32ToBuffer(buf, offset, post.post_type);
    }

    Post readPostFromBuffer(uint8_t* buf, size_t& offset) {
        Post post;
        post.post_id = readInt64(buf, offset);
        post.creator.user_id = readInt64(buf, offset);
        post.creator.username = readString(buf, offset);
        post.req_id = readInt64(buf, offset);
        post.text = readString(buf, offset);

        int32_t mention_count = readInt32(buf, offset);
        post.user_mentions.reserve(mention_count);
        for (int32_t i = 0; i < mention_count; ++i) {
            UserMention mention;
            mention.user_id = readInt64(buf, offset);
            mention.username = readString(buf, offset);
            post.user_mentions.emplace_back(std::move(mention));
        }

        int32_t media_count = readInt32(buf, offset);
        post.media.reserve(media_count);
        for (int32_t i = 0; i < media_count; ++i) {
            Media media;
            media.media_id = readInt64(buf, offset);
            media.media_type = readString(buf, offset);
            post.media.emplace_back(std::move(media));
        }

        int32_t url_count = readInt32(buf, offset);
        post.urls.reserve(url_count);
        for (int32_t i = 0; i < url_count; ++i) {
            Url url;
            url.shortened_url = readString(buf, offset);
            url.expanded_url = readString(buf, offset);
            post.urls.emplace_back(std::move(url));
        }

        post.timestamp = readInt64(buf, offset);
        post.post_type = static_cast<PostType::type>(readInt32(buf, offset));
        return post;
    }
#endif // ENABLE_GEM5

#ifdef ENABLE_CEREBELLUM
    volatile uint64_t* readAddress{nullptr};
    volatile uint64_t* sendAddress{nullptr};
    uint64_t storepost_delay_ticks_{6354445};
    uint64_t readpost_delay_ticks_{10580009};

    void callEngineRead();
    bool callEngineDispatch();
    void callEngineWrite();
    void callEngineSendresp(bool success);
    void callEngineSendBuf();
    bool callEngineDelay(uint8_t nested_rpc_op_kind, uint64_t delay_ticks);
    void setAddresses(volatile uint64_t* sAddress, volatile uint64_t* rAddress) {
        sendAddress = sAddress;
        readAddress = rAddress;
    }
#endif // ENABLE_CEREBELLUM

#ifdef ENABLE_NESTED_RPC_TIMING_MODEL
    bool nested_rpc_timing_model_enabled_{false};
    uint64_t nested_storepost_delay_us_{16};
    uint64_t nested_readposts_delay_us_{41};
#endif // ENABLE_NESTED_RPC_TIMING_MODEL
};

} // namespace social_network

#endif // SOCIAL_NETWORK_MICROSERVICES_USERTIMELINEBUSINESSLOGIC_H
