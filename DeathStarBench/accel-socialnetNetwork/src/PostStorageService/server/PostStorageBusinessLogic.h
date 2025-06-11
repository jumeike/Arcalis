#ifndef SOCIAL_NETWORK_MICROSERVICES_POSTSTORAGEBUSINESSLOGIC_H
#define SOCIAL_NETWORK_MICROSERVICES_POSTSTORAGEBUSINESSLOGIC_H

#include <bson/bson.h>
#include <libmemcached/memcached.h>
#include <libmemcached/util.h>
#include <mongoc.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <map>
#include <vector>
#include <mutex>

#include "../../../gen-cpp/social_network_types.h"
#include "../../logger.h"

namespace social_network {

using json = nlohmann::json;

class PostStorageBusinessLogic {
 public:
  PostStorageBusinessLogic(memcached_pool_st* memcached_pool, 
                          mongoc_client_pool_t* mongodb_pool);
  ~PostStorageBusinessLogic() = default;

  // Core business logic functions
  void StorePost(int64_t req_id, const Post& post,
                 const std::map<std::string, std::string>& carrier);
  void ReadPost(Post& _return, int64_t req_id, int64_t post_id,
                const std::map<std::string, std::string>& carrier);
  void ReadPosts(std::vector<Post>& _return, int64_t req_id, 
                 const std::vector<int64_t>& post_ids,
                 const std::map<std::string, std::string>& carrier);

  // Metrics and monitoring
  void GetMetrics(std::map<std::string, int64_t>& metrics);
  void ResetMetrics();

 private:
  memcached_pool_st* _memcached_client_pool;
  mongoc_client_pool_t* _mongodb_client_pool;
  
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

  // Helper functions
  Post ParsePostFromBson(const bson_t* doc);
  Post ParsePostFromJson(const json& post_json);
  void SetPostToMemcached(int64_t post_id, const std::string& post_json);
  std::string PostToJsonString(const Post& post);
};

} // namespace social_network

#endif // SOCIAL_NETWORK_MICROSERVICES_POSTSTORAGEBUSINESSLOGIC_H
