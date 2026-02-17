#ifndef SOCIAL_NETWORK_MICROSERVICES_URLSHORTENBUSINESSLOGIC_H
#define SOCIAL_NETWORK_MICROSERVICES_URLSHORTENBUSINESSLOGIC_H

#include <random>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>
#include <atomic>
#include <map>

#include <mongoc.h>
#include <libmemcached/memcached.h>
#include <libmemcached/util.h>
#include <bson/bson.h>

#include "../../../gen-cpp/social_network_types.h"
#include "../../logger.h"

#define HOSTNAME "http://short-url/"

namespace social_network {

class UrlShortenBusinessLogic {
public:
  UrlShortenBusinessLogic(memcached_pool_st* memcached_pool, 
                          mongoc_client_pool_t* mongodb_pool);
  ~UrlShortenBusinessLogic() = default;

  // Core business logic functions
  void ComposeUrls(std::vector<Url>& _return,
                   int64_t req_id,
                   const std::vector<std::string>& urls);

  void GetExtendedUrls(std::vector<std::string>& _return,
                       int64_t req_id,
                       const std::vector<std::string>& shortened_urls);

  // Metrics and monitoring
  void GetMetrics(std::map<std::string, int64_t>& metrics) const;
  void ResetMetrics();

private:
  memcached_pool_st* _memcached_client_pool;
  mongoc_client_pool_t* _mongodb_client_pool;
  
  // Random string generation
  static std::mt19937 _generator;
  std::uniform_int_distribution<int> _distribution;
  std::mutex _thread_lock;
  
  // Metrics
  std::atomic<uint64_t> _requests_processed{0};
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
};

} // namespace social_network

#endif // SOCIAL_NETWORK_MICROSERVICES_URLSHORTENBUSINESSLOGIC_H
