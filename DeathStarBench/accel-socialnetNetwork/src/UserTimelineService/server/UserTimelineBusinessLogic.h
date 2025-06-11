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

#include "../../../gen-cpp/PostStorageService.h"
#include "../../../gen-cpp/social_network_types.h"
#include "../../ClientPool.h"
#include "../../ThriftClient.h"
#include "../../logger.h"

using namespace sw::redis;

namespace social_network {

class UserTimelineBusinessLogic {
 public:
  UserTimelineBusinessLogic(Redis* redis_pool, mongoc_client_pool_t* mongodb_pool,
                           ClientPool<ThriftClient<PostStorageServiceClient>>* post_client_pool);

  UserTimelineBusinessLogic(Redis* redis_replica_pool, Redis* redis_primary_pool, 
                           mongoc_client_pool_t* mongodb_pool,
                           ClientPool<ThriftClient<PostStorageServiceClient>>* post_client_pool);

  UserTimelineBusinessLogic(RedisCluster* redis_cluster_pool, mongoc_client_pool_t* mongodb_pool,
                           ClientPool<ThriftClient<PostStorageServiceClient>>* post_client_pool);

  ~UserTimelineBusinessLogic() = default;

  // Core business logic functions
  void WriteUserTimeline(int64_t req_id, int64_t post_id, int64_t user_id, int64_t timestamp,
                        const std::map<std::string, std::string>& carrier);

  void ReadUserTimeline(std::vector<Post>& _return, int64_t req_id, int64_t user_id, 
                       int start, int stop, const std::map<std::string, std::string>& carrier);

  // Metrics and monitoring
  void GetMetrics(std::map<std::string, int64_t>& metrics);
  void ResetMetrics();

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
  std::vector<Post> GetPostsFromPostService(int64_t req_id, const std::vector<int64_t>& post_ids,
                                           const std::map<std::string, std::string>& carrier);
};

} // namespace social_network

#endif // SOCIAL_NETWORK_MICROSERVICES_USERTIMELINEBUSINESSLOGIC_H
