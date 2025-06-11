#include "PostStorageBusinessLogic.h"
#include <chrono>
#include <cstring>
#include <future>

namespace social_network {

PostStorageBusinessLogic::PostStorageBusinessLogic(
    memcached_pool_st* memcached_pool, mongoc_client_pool_t* mongodb_pool)
    : _memcached_client_pool(memcached_pool), _mongodb_client_pool(mongodb_pool) {
  LOG_DEBUG(info) << "PostStorageBusinessLogic initialized";
}

void PostStorageBusinessLogic::StorePost(int64_t req_id, const Post& post,
                                         const std::map<std::string, std::string>& carrier) {
  auto start_time = std::chrono::high_resolution_clock::now();
  
  _store_requests++;

  mongoc_client_t* mongodb_client = mongoc_client_pool_pop(_mongodb_client_pool);
  if (!mongodb_client) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_MONGODB_ERROR;
    se.message = "Failed to pop a client from MongoDB pool";
    throw se;
  }

  auto collection = mongoc_client_get_collection(mongodb_client, "post", "post");
  if (!collection) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_MONGODB_ERROR;
    se.message = "Failed to create collection post from DB post";
    mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
    throw se;
  }

  // Build BSON document
  bson_t* new_doc = bson_new();
  BSON_APPEND_INT64(new_doc, "post_id", post.post_id);
  BSON_APPEND_INT64(new_doc, "timestamp", post.timestamp);
  BSON_APPEND_UTF8(new_doc, "text", post.text.c_str());
  BSON_APPEND_INT64(new_doc, "req_id", post.req_id);
  BSON_APPEND_INT32(new_doc, "post_type", post.post_type);

  // Creator subdocument
  bson_t creator_doc;
  BSON_APPEND_DOCUMENT_BEGIN(new_doc, "creator", &creator_doc);
  BSON_APPEND_INT64(&creator_doc, "user_id", post.creator.user_id);
  BSON_APPEND_UTF8(&creator_doc, "username", post.creator.username.c_str());
  bson_append_document_end(new_doc, &creator_doc);

  // URLs array
  const char* key;
  int idx = 0;
  char buf[16];
  bson_t url_list;
  BSON_APPEND_ARRAY_BEGIN(new_doc, "urls", &url_list);
  for (const auto& url : post.urls) {
    bson_uint32_to_string(idx, &key, buf, sizeof buf);
    bson_t url_doc;
    BSON_APPEND_DOCUMENT_BEGIN(&url_list, key, &url_doc);
    BSON_APPEND_UTF8(&url_doc, "shortened_url", url.shortened_url.c_str());
    BSON_APPEND_UTF8(&url_doc, "expanded_url", url.expanded_url.c_str());
    bson_append_document_end(&url_list, &url_doc);
    idx++;
  }
  bson_append_array_end(new_doc, &url_list);

  // User mentions array
  bson_t user_mention_list;
  idx = 0;
  BSON_APPEND_ARRAY_BEGIN(new_doc, "user_mentions", &user_mention_list);
  for (const auto& user_mention : post.user_mentions) {
    bson_uint32_to_string(idx, &key, buf, sizeof buf);
    bson_t user_mention_doc;
    BSON_APPEND_DOCUMENT_BEGIN(&user_mention_list, key, &user_mention_doc);
    BSON_APPEND_INT64(&user_mention_doc, "user_id", user_mention.user_id);
    BSON_APPEND_UTF8(&user_mention_doc, "username", user_mention.username.c_str());
    bson_append_document_end(&user_mention_list, &user_mention_doc);
    idx++;
  }
  bson_append_array_end(new_doc, &user_mention_list);

  // Media array
  bson_t media_list;
  idx = 0;
  BSON_APPEND_ARRAY_BEGIN(new_doc, "media", &media_list);
  for (const auto& media : post.media) {
    bson_uint32_to_string(idx, &key, buf, sizeof buf);
    bson_t media_doc;
    BSON_APPEND_DOCUMENT_BEGIN(&media_list, key, &media_doc);
    BSON_APPEND_INT64(&media_doc, "media_id", media.media_id);
    BSON_APPEND_UTF8(&media_doc, "media_type", media.media_type.c_str());
    bson_append_document_end(&media_list, &media_doc);
    idx++;
  }
  bson_append_array_end(new_doc, &media_list);

  // Insert into MongoDB
  auto mongodb_start = std::chrono::high_resolution_clock::now();
  bson_error_t error;
  bool inserted = mongoc_collection_insert_one(collection, new_doc, nullptr, nullptr, &error);
  auto mongodb_end = std::chrono::high_resolution_clock::now();

  _mongodb_operations++;
  _mongodb_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(mongodb_end - mongodb_start).count();

  if (!inserted) {
    LOG(error) << "Error: Failed to insert post to MongoDB: " << error.message;
    ServiceException se;
    se.errorCode = ErrorCode::SE_MONGODB_ERROR;
    se.message = error.message;
    bson_destroy(new_doc);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
    throw se;
  }

  bson_destroy(new_doc);
  mongoc_collection_destroy(collection);
  mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);

  auto end_time = std::chrono::high_resolution_clock::now();
  _total_processing_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();

  LOG_DEBUG(debug) << "Stored post " << post.post_id << " for request " << req_id;
}

void PostStorageBusinessLogic::ReadPost(Post& _return, int64_t req_id, int64_t post_id,
                                        const std::map<std::string, std::string>& carrier) {
  auto start_time = std::chrono::high_resolution_clock::now();
  
  _read_requests++;

  std::string post_id_str = std::to_string(post_id);

  // Try memcached first
  memcached_return_t memcached_rc;
  auto memcached_start = std::chrono::high_resolution_clock::now();
  memcached_st* memcached_client = memcached_pool_pop(_memcached_client_pool, true, &memcached_rc);
  if (!memcached_client) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_MEMCACHED_ERROR;
    se.message = "Failed to pop a client from memcached pool";
    throw se;
  }

  size_t post_mmc_size;
  uint32_t memcached_flags;
  char* post_mmc = memcached_get(memcached_client, post_id_str.c_str(), post_id_str.length(),
                                &post_mmc_size, &memcached_flags, &memcached_rc);
  auto memcached_end = std::chrono::high_resolution_clock::now();
  _memcached_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(memcached_end - memcached_start).count();

  if (!post_mmc && memcached_rc != MEMCACHED_NOTFOUND) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_MEMCACHED_ERROR;
    se.message = memcached_strerror(memcached_client, memcached_rc);
    memcached_pool_push(_memcached_client_pool, memcached_client);
    throw se;
  }
  memcached_pool_push(_memcached_client_pool, memcached_client);

  if (post_mmc) {
    // Cache hit
    _cache_hits++;
    LOG_DEBUG(debug) << "Get post " << post_id << " cache hit from Memcached";
    json post_json = json::parse(std::string(post_mmc, post_mmc + post_mmc_size));
    _return = ParsePostFromJson(post_json);
    free(post_mmc);
  } else {
    // Cache miss - read from MongoDB
    _cache_misses++;
    mongoc_client_t* mongodb_client = mongoc_client_pool_pop(_mongodb_client_pool);
    if (!mongodb_client) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_MONGODB_ERROR;
      se.message = "Failed to pop a client from MongoDB pool";
      throw se;
    }

    auto collection = mongoc_client_get_collection(mongodb_client, "post", "post");
    if (!collection) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_MONGODB_ERROR;
      se.message = "Failed to create collection post from DB post";
      mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
      throw se;
    }

    bson_t* query = bson_new();
    BSON_APPEND_INT64(query, "post_id", post_id);
    
    auto mongodb_start = std::chrono::high_resolution_clock::now();
    mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(collection, query, nullptr, nullptr);
    const bson_t* doc;
    bool found = mongoc_cursor_next(cursor, &doc);
    auto mongodb_end = std::chrono::high_resolution_clock::now();
    
    _mongodb_operations++;
    _mongodb_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(mongodb_end - mongodb_start).count();

    if (!found) {
      bson_error_t error;
      if (mongoc_cursor_error(cursor, &error)) {
        LOG(warning) << error.message;
        bson_destroy(query);
        mongoc_cursor_destroy(cursor);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
        ServiceException se;
        se.errorCode = ErrorCode::SE_MONGODB_ERROR;
        se.message = error.message;
        throw se;
      } else {
        LOG(warning) << "Post_id: " << post_id << " doesn't exist in MongoDB";
        bson_destroy(query);
        mongoc_cursor_destroy(cursor);
        mongoc_collection_destroy(collection);
        mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
        ServiceException se;
        se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
        se.message = "Post_id: " + std::to_string(post_id) + " doesn't exist in MongoDB";
        throw se;
      }
    } else {
      LOG_DEBUG(debug) << "Post_id: " << post_id << " found in MongoDB";
      auto post_json_char = bson_as_json(doc, nullptr);
      json post_json = json::parse(post_json_char);
      _return = ParsePostFromJson(post_json);
      
      // Cache the result
      SetPostToMemcached(post_id, std::string(post_json_char));
      bson_free(post_json_char);
    }
    
    bson_destroy(query);
    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  _total_processing_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
}

void PostStorageBusinessLogic::ReadPosts(std::vector<Post>& _return, int64_t req_id,
                                         const std::vector<int64_t>& post_ids,
                                         const std::map<std::string, std::string>& carrier) {
  auto start_time = std::chrono::high_resolution_clock::now();
  
  _read_multi_requests++;

  if (post_ids.empty()) {
    return;
  }

  std::set<int64_t> post_ids_not_cached(post_ids.begin(), post_ids.end());
  if (post_ids_not_cached.size() != post_ids.size()) {
    LOG(error) << "Post_ids are duplicated";
    ServiceException se;
    se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
    se.message = "Post_ids are duplicated";
    throw se;
  }

  std::map<int64_t, Post> return_map;
  
  // Try to get from memcached first
  auto memcached_start = std::chrono::high_resolution_clock::now();
  memcached_return_t memcached_rc;
  auto memcached_client = memcached_pool_pop(_memcached_client_pool, true, &memcached_rc);
  if (!memcached_client) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_MEMCACHED_ERROR;
    se.message = "Failed to pop a client from memcached pool";
    throw se;
  }

  // Prepare keys for multi-get
  char** keys = new char*[post_ids.size()];
  size_t* key_sizes = new size_t[post_ids.size()];
  int idx = 0;
  for (auto& post_id : post_ids) {
    std::string key_str = std::to_string(post_id);
    keys[idx] = new char[key_str.length() + 1];
    strcpy(keys[idx], key_str.c_str());
    key_sizes[idx] = key_str.length();
    idx++;
  }

  memcached_rc = memcached_mget(memcached_client, keys, key_sizes, post_ids.size());
  if (memcached_rc != MEMCACHED_SUCCESS) {
    LOG(error) << "Cannot get post_ids of request " << req_id << ": "
               << memcached_strerror(memcached_client, memcached_rc);
    ServiceException se;
    se.errorCode = ErrorCode::SE_MEMCACHED_ERROR;
    se.message = memcached_strerror(memcached_client, memcached_rc);
    memcached_pool_push(_memcached_client_pool, memcached_client);
    throw se;
  }

  // Fetch results
  char return_key[MEMCACHED_MAX_KEY];
  size_t return_key_length;
  char* return_value;
  size_t return_value_length;
  uint32_t flags;

  while (true) {
    return_value = memcached_fetch(memcached_client, return_key, &return_key_length,
                                  &return_value_length, &flags, &memcached_rc);
    if (return_value == nullptr) {
      LOG_DEBUG(debug) << "Memcached mget finished";
      break;
    }
    if (memcached_rc != MEMCACHED_SUCCESS) {
      free(return_value);
      memcached_quit(memcached_client);
      memcached_pool_push(_memcached_client_pool, memcached_client);
      LOG(error) << "Cannot get posts of request " << req_id;
      ServiceException se;
      se.errorCode = ErrorCode::SE_MEMCACHED_ERROR;
      se.message = "Cannot get posts of request " + std::to_string(req_id);
      throw se;
    }

    Post new_post;
    json post_json = json::parse(std::string(return_value, return_value + return_value_length));
    new_post = ParsePostFromJson(post_json);
    return_map.insert(std::make_pair(new_post.post_id, new_post));
    post_ids_not_cached.erase(new_post.post_id);
    _cache_hits++;
    free(return_value);
  }

  auto memcached_end = std::chrono::high_resolution_clock::now();
  _memcached_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(memcached_end - memcached_start).count();

  memcached_quit(memcached_client);
  memcached_pool_push(_memcached_client_pool, memcached_client);
  
  // Clean up keys
  for (int i = 0; i < post_ids.size(); ++i) {
    delete[] keys[i];
  }
  delete[] keys;
  delete[] key_sizes;

  // Handle cache misses - get from MongoDB
  if (!post_ids_not_cached.empty()) {
    _cache_misses += post_ids_not_cached.size();
    
    auto mongodb_start = std::chrono::high_resolution_clock::now();
    mongoc_client_t* mongodb_client = mongoc_client_pool_pop(_mongodb_client_pool);
    if (!mongodb_client) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_MONGODB_ERROR;
      se.message = "Failed to pop a client from MongoDB pool";
      throw se;
    }

    auto collection = mongoc_client_get_collection(mongodb_client, "post", "post");
    if (!collection) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_MONGODB_ERROR;
      se.message = "Failed to create collection post from DB post";
      mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
      throw se;
    }

    // Build query for multiple post_ids
    bson_t* query = bson_new();
    bson_t query_child;
    bson_t query_post_id_list;
    const char* key;
    idx = 0;
    char buf[16];

    BSON_APPEND_DOCUMENT_BEGIN(query, "post_id", &query_child);
    BSON_APPEND_ARRAY_BEGIN(&query_child, "$in", &query_post_id_list);
    for (auto& item : post_ids_not_cached) {
      bson_uint32_to_string(idx, &key, buf, sizeof buf);
      BSON_APPEND_INT64(&query_post_id_list, key, item);
      idx++;
    }
    bson_append_array_end(&query_child, &query_post_id_list);
    bson_append_document_end(query, &query_child);

    mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(collection, query, nullptr, nullptr);
    const bson_t* doc;
    std::map<int64_t, std::string> post_json_map;

    while (true) {
      bool found = mongoc_cursor_next(cursor, &doc);
      if (!found) {
        break;
      }
      Post new_post;
      char* post_json_char = bson_as_json(doc, nullptr);
      json post_json = json::parse(post_json_char);
      new_post = ParsePostFromJson(post_json);
      post_json_map.insert({new_post.post_id, std::string(post_json_char)});
      return_map.insert({new_post.post_id, new_post});
      bson_free(post_json_char);
    }

    auto mongodb_end = std::chrono::high_resolution_clock::now();
    _mongodb_operations++;
    _mongodb_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(mongodb_end - mongodb_start).count();

    bson_error_t error;
    if (mongoc_cursor_error(cursor, &error)) {
      LOG(warning) << error.message;
      bson_destroy(query);
      mongoc_cursor_destroy(cursor);
      mongoc_collection_destroy(collection);
      mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
      ServiceException se;
      se.errorCode = ErrorCode::SE_MONGODB_ERROR;
      se.message = error.message;
      throw se;
    }

    bson_destroy(query);
    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(collection);
    mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);

    // Cache the results from MongoDB (async)
    std::vector<std::future<void>> set_futures;
    set_futures.emplace_back(std::async(std::launch::async, [&]() {
      memcached_return_t _rc;
      auto _memcached_client = memcached_pool_pop(_memcached_client_pool, true, &_rc);
      if (!_memcached_client) {
        LOG(error) << "Failed to pop a client from memcached pool";
        return;
      }
      for (auto& it : post_json_map) {
        std::string id_str = std::to_string(it.first);
        _rc = memcached_set(_memcached_client, id_str.c_str(), id_str.length(),
                           it.second.c_str(), it.second.length(),
                           static_cast<time_t>(0), static_cast<uint32_t>(0));
      }
      memcached_pool_push(_memcached_client_pool, _memcached_client);
    }));

    // Wait for caching to complete
    try {
      for (auto& it : set_futures) {
        it.get();
      }
    } catch (...) {
      LOG(warning) << "Failed to set posts to memcached";
    }
  }

  if (return_map.size() != post_ids.size()) {
    LOG(error) << "Return set incomplete";
    ServiceException se;
    se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
    se.message = "Return set incomplete";
    throw se;
  }

  // Return posts in the same order as requested
  for (auto& post_id : post_ids) {
    _return.emplace_back(return_map[post_id]);
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  _total_processing_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
}

// Helper function implementations
Post PostStorageBusinessLogic::ParsePostFromJson(const json& post_json) {
  Post post;
  post.req_id = post_json["req_id"];
  post.timestamp = post_json["timestamp"];
  post.post_id = post_json["post_id"];
  post.creator.user_id = post_json["creator"]["user_id"];
  post.creator.username = post_json["creator"]["username"];
  post.post_type = post_json["post_type"];
  post.text = post_json["text"];
  
  for (auto& item : post_json["media"]) {
    Media media;
    media.media_id = item["media_id"];
    media.media_type = item["media_type"];
    post.media.emplace_back(media);
  }
  
  for (auto& item : post_json["user_mentions"]) {
    UserMention user_mention;
    user_mention.username = item["username"];
    user_mention.user_id = item["user_id"];
    post.user_mentions.emplace_back(user_mention);
  }
  
  for (auto& item : post_json["urls"]) {
    Url url;
    url.shortened_url = item["shortened_url"];
    url.expanded_url = item["expanded_url"];
    post.urls.emplace_back(url);
  }
  
  return post;
}

void PostStorageBusinessLogic::SetPostToMemcached(int64_t post_id, const std::string& post_json) {
  memcached_return_t memcached_rc;
  auto memcached_client = memcached_pool_pop(_memcached_client_pool, true, &memcached_rc);
  if (!memcached_client) {
    LOG(error) << "Failed to pop a client from memcached pool";
    return;
  }
  
  std::string id_str = std::to_string(post_id);
  memcached_rc = memcached_set(memcached_client, id_str.c_str(), id_str.length(),
                              post_json.c_str(), post_json.length(),
                              static_cast<time_t>(0), static_cast<uint32_t>(0));
  if (memcached_rc != MEMCACHED_SUCCESS) {
    LOG(warning) << "Failed to set post to Memcached: "
                 << memcached_strerror(memcached_client, memcached_rc);
  }
  
  memcached_pool_push(_memcached_client_pool, memcached_client);
}

void PostStorageBusinessLogic::GetMetrics(std::map<std::string, int64_t>& metrics) {
  std::lock_guard<std::mutex> lock(_metrics_mutex);
  
  metrics["store_requests"] = _store_requests.load();
  metrics["read_requests"] = _read_requests.load();
  metrics["read_multi_requests"] = _read_multi_requests.load();
  metrics["cache_hits"] = _cache_hits.load();
  metrics["cache_misses"] = _cache_misses.load();
  metrics["mongodb_operations"] = _mongodb_operations.load();
  metrics["total_processing_time_ns"] = _total_processing_time_ns.load();
  metrics["mongodb_time_ns"] = _mongodb_time_ns.load();
  metrics["memcached_time_ns"] = _memcached_time_ns.load();
  
  uint64_t total_requests = _store_requests.load() + _read_requests.load() + _read_multi_requests.load();
  if (total_requests > 0) {
    metrics["avg_processing_time_ns"] = _total_processing_time_ns.load() / total_requests;
  } else {
    metrics["avg_processing_time_ns"] = 0;
  }
  
  uint64_t total_cache_ops = _cache_hits.load() + _cache_misses.load();
  if (total_cache_ops > 0) {
    metrics["cache_hit_rate_percent"] = (_cache_hits.load() * 100) / total_cache_ops;
  } else {
    metrics["cache_hit_rate_percent"] = 0;
  }
}

void PostStorageBusinessLogic::ResetMetrics() {
  std::lock_guard<std::mutex> lock(_metrics_mutex);
  
  _store_requests.store(0);
  _read_requests.store(0);
  _read_multi_requests.store(0);
  _cache_hits.store(0);
  _cache_misses.store(0);
  _mongodb_operations.store(0);
  _total_processing_time_ns.store(0);
  _mongodb_time_ns.store(0);
  _memcached_time_ns.store(0);
}

} // namespace social_network
