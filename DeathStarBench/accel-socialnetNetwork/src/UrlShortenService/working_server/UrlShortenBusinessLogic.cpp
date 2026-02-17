#include "UrlShortenBusinessLogic.h"
#include <future>

namespace social_network {

// Static member initialization
std::mt19937 UrlShortenBusinessLogic::_generator = std::mt19937(
    std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() % 0xffffffff);

UrlShortenBusinessLogic::UrlShortenBusinessLogic(
    memcached_pool_st* memcached_pool,
    mongoc_client_pool_t* mongodb_pool)
    : _memcached_client_pool(memcached_pool),
      _mongodb_client_pool(mongodb_pool),
      _distribution(0, 61) {
  LOG(info) << "UrlShortenBusinessLogic initialized";
}

std::string UrlShortenBusinessLogic::_GenRandomStr(int length) {
  const char char_map[] = "abcdefghijklmnopqrstuvwxyzABCDEF"
                          "GHIJKLMNOPQRSTUVWXYZ0123456789";
  std::string return_str;
  _thread_lock.lock();
  for (int i = 0; i < length; ++i) {
    return_str.append(1, char_map[_distribution(_generator)]);
  }
  _thread_lock.unlock();
  return return_str;
}

void UrlShortenBusinessLogic::ComposeUrls(
    std::vector<Url>& _return,
    int64_t req_id,
    const std::vector<std::string>& urls) {
  
  auto start_time = std::chrono::high_resolution_clock::now();
  
  std::vector<Url> target_urls;
  std::future<void> mongo_future;

  if (!urls.empty()) {
    // Generate shortened URLs
    for (auto& url : urls) {
      Url new_target_url;
      new_target_url.expanded_url = url;
      new_target_url.shortened_url = HOSTNAME + _GenRandomStr(10);
      target_urls.emplace_back(new_target_url);
    }

    // Store in MongoDB asynchronously
    // mongo_future = std::async(std::launch::async, [&]() {
      _StoreUrlsInMongo(target_urls);
    // });
  }

  // Wait for MongoDB insertion to complete
  // if (!urls.empty()) {
  //   try {
  //     mongo_future.get();
  //   } catch (...) {
  //     LOG(error) << "Failed to upload shortened urls to MongoDB";
  //     throw;
  //   }
  // }

  _return = target_urls;
  
  auto end_time = std::chrono::high_resolution_clock::now();
  _requests_processed++;
  _total_processing_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
      end_time - start_time).count();
  
  LOG_DEBUG(debug) << "Request " << req_id << " composed " << urls.size() << " URLs";
}

void UrlShortenBusinessLogic::GetExtendedUrls(
    std::vector<std::string>& _return,
    int64_t req_id,
    const std::vector<std::string>& shortened_urls) {
  
  auto start_time = std::chrono::high_resolution_clock::now();
  
  std::vector<std::string> cache_misses;
  
  // Try memcached first
  auto memcached_start = std::chrono::high_resolution_clock::now();
  _FetchUrlsFromMemcached(_return, shortened_urls, cache_misses);
  auto memcached_end = std::chrono::high_resolution_clock::now();
  _memcached_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
      memcached_end - memcached_start).count();
  
  // Fetch cache misses from MongoDB
  if (!cache_misses.empty()) {
    auto mongo_start = std::chrono::high_resolution_clock::now();
    _FetchUrlsFromMongo(_return, cache_misses);
    auto mongo_end = std::chrono::high_resolution_clock::now();
    _mongo_insert_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
        mongo_end - mongo_start).count();
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  _requests_processed++;
  _total_processing_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
      end_time - start_time).count();
  
  LOG_DEBUG(debug) << "Request " << req_id << " fetched " << shortened_urls.size() 
                   << " extended URLs (" << cache_misses.size() << " cache misses)";
}

void UrlShortenBusinessLogic::_StoreUrlsInMongo(const std::vector<Url>& urls) {
  auto mongo_start = std::chrono::high_resolution_clock::now();
  
  mongoc_client_t* mongodb_client = mongoc_client_pool_pop(_mongodb_client_pool);
  if (!mongodb_client) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_MONGODB_ERROR;
    se.message = "Failed to pop a client from MongoDB pool";
    throw se;
  }
  
  auto collection = mongoc_client_get_collection(
      mongodb_client, "url-shorten", "url-shorten");
  if (!collection) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_MONGODB_ERROR;
    se.message = "Failed to create collection url-shorten from DB url-shorten";
    mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
    throw se;
  }

  mongoc_bulk_operation_t* bulk;
  bson_t* doc;
  bson_error_t error;
  bson_t reply;
  bool ret;
  
  bulk = mongoc_collection_create_bulk_operation_with_opts(collection, nullptr);
  for (auto& url : urls) {
    doc = bson_new();
    BSON_APPEND_UTF8(doc, "shortened_url", url.shortened_url.c_str());
    BSON_APPEND_UTF8(doc, "expanded_url", url.expanded_url.c_str());
    mongoc_bulk_operation_insert(bulk, doc);
    bson_destroy(doc);
  }
  
  ret = mongoc_bulk_operation_execute(bulk, &reply, &error);
  if (!ret) {
    LOG(error) << "MongoDB error: " << error.message;
    ServiceException se;
    se.errorCode = ErrorCode::SE_MONGODB_ERROR;
    se.message = "Failed to insert urls to MongoDB";
    bson_destroy(&reply);
    mongoc_bulk_operation_destroy(bulk);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
    throw se;
  }
  
  bson_destroy(&reply);
  mongoc_bulk_operation_destroy(bulk);
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
  
  auto mongo_end = std::chrono::high_resolution_clock::now();
  _mongo_insert_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
      mongo_end - mongo_start).count();
}

void UrlShortenBusinessLogic::_FetchUrlsFromMemcached(
    std::vector<std::string>& _return,
    const std::vector<std::string>& shortened_urls,
    std::vector<std::string>& cache_misses) {
  
  memcached_return_t rc;
  memcached_st* memcached_client = memcached_pool_pop(
      _memcached_client_pool, true, &rc);
  
  if (!memcached_client) {
    LOG(warning) << "Failed to pop memcached client";
    cache_misses = shortened_urls;
    return;
  }

  for (auto& shortened_url : shortened_urls) {
    size_t value_length;
    uint32_t flags;
    char* value = memcached_get(
        memcached_client,
        shortened_url.c_str(),
        shortened_url.length(),
        &value_length,
        &flags,
        &rc);
    
    if (rc == MEMCACHED_SUCCESS) {
      _return.emplace_back(std::string(value, value_length));
      free(value);
    } else {
      cache_misses.push_back(shortened_url);
    }
  }
  
  memcached_pool_push(_memcached_client_pool, memcached_client);
}

void UrlShortenBusinessLogic::_FetchUrlsFromMongo(
    std::vector<std::string>& _return,
    const std::vector<std::string>& shortened_urls) {
  
  mongoc_client_t* mongodb_client = mongoc_client_pool_pop(_mongodb_client_pool);
  if (!mongodb_client) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_MONGODB_ERROR;
    se.message = "Failed to pop a client from MongoDB pool";
    throw se;
  }
  
  auto collection = mongoc_client_get_collection(
      mongodb_client, "url-shorten", "url-shorten");
  if (!collection) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_MONGODB_ERROR;
    se.message = "Failed to get collection url-shorten";
    mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
    throw se;
  }

  for (auto& shortened_url : shortened_urls) {
    bson_t* query = bson_new();
    BSON_APPEND_UTF8(query, "shortened_url", shortened_url.c_str());
    
    mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(
        collection, query, nullptr, nullptr);
    
    const bson_t* doc;
    if (mongoc_cursor_next(cursor, &doc)) {
      bson_iter_t iter;
      if (bson_iter_init_find(&iter, doc, "expanded_url")) {
        const char* expanded_url = bson_iter_utf8(&iter, nullptr);
        _return.emplace_back(expanded_url);
        
        // Store in memcached for future requests
        memcached_return_t rc;
        memcached_st* memcached_client = memcached_pool_pop(
            _memcached_client_pool, true, &rc);
        if (memcached_client) {
          memcached_set(memcached_client,
                       shortened_url.c_str(),
                       shortened_url.length(),
                       expanded_url,
                       strlen(expanded_url),
                       0, 0);
          memcached_pool_push(_memcached_client_pool, memcached_client);
        }
      }
    } else {
      LOG(warning) << "Shortened URL not found: " << shortened_url;
      _return.emplace_back("");
    }
    
    bson_destroy(query);
    mongoc_cursor_destroy(cursor);
  }
  
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
}

void UrlShortenBusinessLogic::GetMetrics(std::map<std::string, int64_t>& metrics) const {
  metrics["requests_processed"] = _requests_processed.load();
  metrics["total_processing_time_ns"] = _total_processing_time_ns.load();
  metrics["mongo_insert_time_ns"] = _mongo_insert_time_ns.load();
  metrics["memcached_time_ns"] = _memcached_time_ns.load();
  
  uint64_t requests = _requests_processed.load();
  if (requests > 0) {
    metrics["avg_processing_time_ns"] = _total_processing_time_ns.load() / requests;
    metrics["avg_mongo_time_ns"] = _mongo_insert_time_ns.load() / requests;
    metrics["avg_memcached_time_ns"] = _memcached_time_ns.load() / requests;
  } else {
    metrics["avg_processing_time_ns"] = 0;
    metrics["avg_mongo_time_ns"] = 0;
    metrics["avg_memcached_time_ns"] = 0;
  }
}

void UrlShortenBusinessLogic::ResetMetrics() {
  _requests_processed.store(0);
  _total_processing_time_ns.store(0);
  _mongo_insert_time_ns.store(0);
  _memcached_time_ns.store(0);
}

} // namespace social_network
