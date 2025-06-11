#include "UserTimelineBusinessLogic.h"
//#include "../../tracing.h"

namespace social_network {

UserTimelineBusinessLogic::UserTimelineBusinessLogic(
    Redis* redis_pool, mongoc_client_pool_t* mongodb_pool,
    ClientPool<ThriftClient<PostStorageServiceClient>>* post_client_pool) {
  _redis_client_pool = redis_pool;
  _redis_replica_pool = nullptr;
  _redis_primary_pool = nullptr;
  _redis_cluster_client_pool = nullptr;
  _mongodb_client_pool = mongodb_pool;
  _post_client_pool = post_client_pool;
  LOG(info) << "UserTimelineBusinessLogic initialized with single Redis pool";
}

UserTimelineBusinessLogic::UserTimelineBusinessLogic(
    Redis* redis_replica_pool, Redis* redis_primary_pool, 
    mongoc_client_pool_t* mongodb_pool,
    ClientPool<ThriftClient<PostStorageServiceClient>>* post_client_pool) {
  _redis_client_pool = nullptr;
  _redis_replica_pool = redis_replica_pool;
  _redis_primary_pool = redis_primary_pool;
  _redis_cluster_client_pool = nullptr;
  _mongodb_client_pool = mongodb_pool;
  _post_client_pool = post_client_pool;
  LOG(info) << "UserTimelineBusinessLogic initialized with Redis replication";
}

UserTimelineBusinessLogic::UserTimelineBusinessLogic(
    RedisCluster* redis_cluster_pool, mongoc_client_pool_t* mongodb_pool,
    ClientPool<ThriftClient<PostStorageServiceClient>>* post_client_pool) {
  _redis_cluster_client_pool = redis_cluster_pool;
  _redis_replica_pool = nullptr;
  _redis_primary_pool = nullptr;
  _redis_client_pool = nullptr;
  _mongodb_client_pool = mongodb_pool;
  _post_client_pool = post_client_pool;
  LOG(info) << "UserTimelineBusinessLogic initialized with Redis cluster";
}

bool UserTimelineBusinessLogic::IsRedisReplicationEnabled() {
  return (_redis_primary_pool != nullptr || _redis_replica_pool != nullptr);
}

void UserTimelineBusinessLogic::WriteUserTimeline(
    int64_t req_id, int64_t post_id, int64_t user_id, int64_t timestamp,
    const std::map<std::string, std::string>& carrier) {
  auto processing_start = std::chrono::high_resolution_clock::now();
  
  // Initialize a span placeholder (tracing removed for simplicity)
  //TextMapReader reader(carrier);
  //std::map<std::string, std::string> writer_text_map;
  //TextMapWriter writer(writer_text_map);
  // Placeholder for tracing - can be restored if needed
  
  _write_requests++;

  try {
    // Write to MongoDB
    WriteTimelineToMongoDB(user_id, post_id, timestamp);
    
    // Update user's timeline in Redis
    UpdateRedisTimeline(std::to_string(user_id), std::to_string(post_id), 
                       static_cast<double>(timestamp), UpdateType::NOT_EXIST);
    
  } catch (const std::exception& e) {
    LOG(error) << "Error in WriteUserTimeline for user " << user_id 
               << ", post " << post_id << ": " << e.what();
    throw;
  }

  auto processing_end = std::chrono::high_resolution_clock::now();
  _total_processing_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
      processing_end - processing_start).count();
}

void UserTimelineBusinessLogic::ReadUserTimeline(
    std::vector<Post>& _return, int64_t req_id, int64_t user_id, 
    int start, int stop, const std::map<std::string, std::string>& carrier) {
  auto processing_start = std::chrono::high_resolution_clock::now();
  
  // Initialize a span placeholder (tracing removed for simplicity)
  //TextMapReader reader(carrier);
  //std::map<std::string, std::string> writer_text_map;
  //TextMapWriter writer(writer_text_map);
  // Placeholder for tracing - can be restored if needed
  
  _read_requests++;

  if (stop <= start || start < 0) {
    return;
  }

  // Get post IDs from Redis
  std::vector<std::string> post_ids_str = GetTimelineFromRedis(std::to_string(user_id), start, stop - 1);
  
  std::vector<int64_t> post_ids;
  for (const auto& post_id_str : post_ids_str) {
    post_ids.emplace_back(std::stoul(post_id_str));
  }

  // Find additional posts in MongoDB if needed
  int mongo_start = start + post_ids.size();
  std::unordered_map<std::string, double> redis_update_map;
  
  if (mongo_start < stop) {
    auto mongodb_posts = ReadTimelineFromMongoDB(user_id, 0, stop);
    
    for (size_t idx = 0; idx < mongodb_posts.size(); ++idx) {
      auto curr_post_id = mongodb_posts[idx].first;
      auto curr_timestamp = mongodb_posts[idx].second;
      
      if (static_cast<int>(idx) >= mongo_start) {
        // Avoid duplicates
        if (std::find(post_ids.begin(), post_ids.end(), curr_post_id) == post_ids.end()) {
          post_ids.emplace_back(curr_post_id);
        }
      }
      redis_update_map.insert(std::make_pair(std::to_string(curr_post_id),
                                            static_cast<double>(curr_timestamp)));
    }
  }

  // Fetch posts from PostStorage service
  _return = GetPostsFromPostService(req_id, post_ids, carrier);

  // Update Redis with MongoDB data if needed
  if (!redis_update_map.empty()) {
    UpdateRedisTimeline(std::to_string(user_id), redis_update_map);
  }

  auto processing_end = std::chrono::high_resolution_clock::now();
  _total_processing_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
      processing_end - processing_start).count();
}

void UserTimelineBusinessLogic::UpdateRedisTimeline(
    const std::string& user_id, const std::string& post_id, 
    double timestamp, UpdateType update_type) {
  auto redis_start = std::chrono::high_resolution_clock::now();
  
  try {
    if (_redis_client_pool) {
      _redis_client_pool->zadd(user_id, post_id, timestamp, update_type);
    } else if (IsRedisReplicationEnabled()) {
      _redis_primary_pool->zadd(user_id, post_id, timestamp, update_type);
    } else {
      _redis_cluster_client_pool->zadd(user_id, post_id, timestamp, update_type);
    }
    _redis_operations++;
  } catch (const Error& err) {
    LOG(error) << "Redis error in UpdateRedisTimeline: " << err.what();
    throw;
  }
  
  auto redis_end = std::chrono::high_resolution_clock::now();
  _redis_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
      redis_end - redis_start).count();
}

void UserTimelineBusinessLogic::UpdateRedisTimeline(
    const std::string& user_id, 
    const std::unordered_map<std::string, double>& post_score_map) {
  auto redis_start = std::chrono::high_resolution_clock::now();
  
  try {
    if (_redis_client_pool) {
      _redis_client_pool->zadd(user_id, post_score_map.begin(), post_score_map.end());
    } else if (IsRedisReplicationEnabled()) {
      _redis_primary_pool->zadd(user_id, post_score_map.begin(), post_score_map.end());
    } else {
      _redis_cluster_client_pool->zadd(user_id, post_score_map.begin(), post_score_map.end());
    }
    _redis_operations++;
  } catch (const Error& err) {
    LOG(error) << "Redis error in UpdateRedisTimeline (batch): " << err.what();
    throw;
  }
  
  auto redis_end = std::chrono::high_resolution_clock::now();
  _redis_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
      redis_end - redis_start).count();
}

std::vector<std::string> UserTimelineBusinessLogic::GetTimelineFromRedis(
    const std::string& user_id, int start, int stop) {
  auto redis_start = std::chrono::high_resolution_clock::now();
  
  std::vector<std::string> post_ids_str;
  try {
    if (_redis_client_pool) {
      _redis_client_pool->zrevrange(user_id, start, stop, std::back_inserter(post_ids_str));
    } else if (IsRedisReplicationEnabled()) {
      _redis_replica_pool->zrevrange(user_id, start, stop, std::back_inserter(post_ids_str));
    } else {
      _redis_cluster_client_pool->zrevrange(user_id, start, stop, std::back_inserter(post_ids_str));
    }
    _redis_operations++;
    
    if (!post_ids_str.empty()) {
      _cache_hits++;
    } else {
      _cache_misses++;
    }
  } catch (const Error& err) {
    LOG(error) << "Redis error in GetTimelineFromRedis: " << err.what();
    _cache_misses++;
    throw;
  }
  
  auto redis_end = std::chrono::high_resolution_clock::now();
  _redis_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
      redis_end - redis_start).count();
  
  return post_ids_str;
}

void UserTimelineBusinessLogic::WriteTimelineToMongoDB(
   int64_t user_id, int64_t post_id, int64_t timestamp) {
 auto mongodb_start = std::chrono::high_resolution_clock::now();

 mongoc_client_t* mongodb_client = mongoc_client_pool_pop(_mongodb_client_pool);
 if (!mongodb_client) {
   ServiceException se;
   se.errorCode = ErrorCode::SE_MONGODB_ERROR;
   se.message = "Failed to pop a client from MongoDB pool";
   throw se;
 }

 auto collection = mongoc_client_get_collection(mongodb_client, "user-timeline", "user-timeline");
 if (!collection) {
   ServiceException se;
   se.errorCode = ErrorCode::SE_MONGODB_ERROR;
   se.message = "Failed to create collection user-timeline from MongoDB";
   mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
   throw se;
 }

 bson_t* query = bson_new();
 BSON_APPEND_INT64(query, "user_id", user_id);

 bson_t* update = BCON_NEW("$push", "{", "posts", "{", "$each", "[", "{",
                          "post_id", BCON_INT64(post_id),
                          "timestamp", BCON_INT64(timestamp), "}", "]",
                          "$position", BCON_INT32(0), "}", "}");

 bson_error_t error;
 bson_t reply;

 bool updated = mongoc_collection_find_and_modify(collection, query, nullptr, update,
                                                 nullptr, false, true, true, &reply, &error);

 if (!updated) {
   updated = mongoc_collection_find_and_modify(collection, query, nullptr, update,
                                              nullptr, false, false, true, &reply, &error);
   if (!updated) {
     LOG(error) << "Failed to update user-timeline for user " << user_id
                << " to MongoDB: " << error.message;
     ServiceException se;
     se.errorCode = ErrorCode::SE_MONGODB_ERROR;
     se.message = error.message;
     bson_destroy(update);
     bson_destroy(query);
     bson_destroy(&reply);
     mongoc_collection_destroy(collection);
     mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
     throw se;
   }
 }

 bson_destroy(update);
 bson_destroy(&reply);
 bson_destroy(query);
 mongoc_collection_destroy(collection);
 mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);

 _mongodb_operations++;
 auto mongodb_end = std::chrono::high_resolution_clock::now();
 _mongodb_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
     mongodb_end - mongodb_start).count();
}


std::vector<std::pair<int64_t, int64_t>> UserTimelineBusinessLogic::ReadTimelineFromMongoDB(
   int64_t user_id, int start, int stop) {
 auto mongodb_start = std::chrono::high_resolution_clock::now();

 std::vector<std::pair<int64_t, int64_t>> timeline_posts;

 mongoc_client_t* mongodb_client = mongoc_client_pool_pop(_mongodb_client_pool);
 if (!mongodb_client) {
   ServiceException se;
   se.errorCode = ErrorCode::SE_MONGODB_ERROR;
   se.message = "Failed to pop a client from MongoDB pool";
   throw se;
 }

 auto collection = mongoc_client_get_collection(mongodb_client, "user-timeline", "user-timeline");
 if (!collection) {
   ServiceException se;
   se.errorCode = ErrorCode::SE_MONGODB_ERROR;
   se.message = "Failed to create collection user-timeline from MongoDB";
   mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
   throw se;
 }

 bson_t* query = BCON_NEW("user_id", BCON_INT64(user_id));
 bson_t* opts = BCON_NEW("projection", "{", "posts", "{", "$slice", "[",
                        BCON_INT32(0), BCON_INT32(stop), "]", "}", "}");

 mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(collection, query, opts, nullptr);
 const bson_t* doc;
 bool found = mongoc_cursor_next(cursor, &doc);

 if (found) {
   bson_iter_t iter_0;
   bson_iter_t iter_1;
   bson_iter_t post_id_child;
   bson_iter_t timestamp_child;
   int idx = 0;

   bson_iter_init(&iter_0, doc);
   bson_iter_init(&iter_1, doc);

   while (bson_iter_find_descendant(&iter_0,
                                   ("posts." + std::to_string(idx) + ".post_id").c_str(),
                                   &post_id_child) &&
          BSON_ITER_HOLDS_INT64(&post_id_child) &&
          bson_iter_find_descendant(&iter_1,
                                   ("posts." + std::to_string(idx) + ".timestamp").c_str(),
                                   &timestamp_child) &&
          BSON_ITER_HOLDS_INT64(&timestamp_child)) {

     auto curr_post_id = bson_iter_int64(&post_id_child);
     auto curr_timestamp = bson_iter_int64(&timestamp_child);

     timeline_posts.emplace_back(curr_post_id, curr_timestamp);

     bson_iter_init(&iter_0, doc);
     bson_iter_init(&iter_1, doc);
     idx++;
   }
 }

 bson_destroy(opts);
 bson_destroy(query);
 mongoc_cursor_destroy(cursor);
 mongoc_collection_destroy(collection);
 mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);

 _mongodb_operations++;
 auto mongodb_end = std::chrono::high_resolution_clock::now();
 _mongodb_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
     mongodb_end - mongodb_start).count();

 return timeline_posts;
}
 
std::vector<Post> UserTimelineBusinessLogic::GetPostsFromPostService(
    int64_t req_id, const std::vector<int64_t>& post_ids,
    const std::map<std::string, std::string>& carrier) {
  auto post_service_start = std::chrono::high_resolution_clock::now();
  
  std::future<std::vector<Post>> post_future = std::async(std::launch::async, [&]() {
    auto post_client_wrapper = _post_client_pool->Pop();
    if (!post_client_wrapper) {
      ServiceException se;
      se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
      se.message = "Failed to connect to post-storage-service";
      throw se;
    }
    
    std::vector<Post> _return_posts;
    auto post_client = post_client_wrapper->GetClient();
    try {
      LOG(info) << "About to read posts from post-storage-service";
      post_client->ReadPosts(_return_posts, req_id, post_ids, carrier);
    } catch (...) {
      _post_client_pool->Remove(post_client_wrapper);
      LOG(error) << "Failed to read posts from post-storage-service";
      throw;
    }
    _post_client_pool->Keepalive(post_client_wrapper);
    return _return_posts;
  });
  
  std::vector<Post> posts;
  try {
    posts = post_future.get();
    _post_service_calls++;
  } catch (...) {
    LOG(error) << "Failed to get posts from post-storage-service";
    throw;
  }
  
  auto post_service_end = std::chrono::high_resolution_clock::now();
  _post_service_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
      post_service_end - post_service_start).count();
  
  return posts;
}

void UserTimelineBusinessLogic::GetMetrics(std::map<std::string, int64_t>& metrics) {
  std::lock_guard<std::mutex> lock(_metrics_mutex);
  
  metrics["write_requests"] = _write_requests.load();
  metrics["read_requests"] = _read_requests.load();
  metrics["redis_operations"] = _redis_operations.load();
  metrics["mongodb_operations"] = _mongodb_operations.load();
  metrics["post_service_calls"] = _post_service_calls.load();
  metrics["cache_hits"] = _cache_hits.load();
  metrics["cache_misses"] = _cache_misses.load();
  metrics["total_processing_time_ns"] = _total_processing_time_ns.load();
  metrics["redis_time_ns"] = _redis_time_ns.load();
  metrics["mongodb_time_ns"] = _mongodb_time_ns.load();
  metrics["post_service_time_ns"] = _post_service_time_ns.load();
  
  uint64_t total_requests = _write_requests.load() + _read_requests.load();
  uint64_t total_cache_requests = _cache_hits.load() + _cache_misses.load();
  
  if (total_requests > 0) {
    metrics["avg_processing_time_ns"] = _total_processing_time_ns.load() / total_requests;
  } else {
    metrics["avg_processing_time_ns"] = 0;
  }
  
  if (total_cache_requests > 0) {
    metrics["cache_hit_rate_percent"] = (_cache_hits.load() * 100) / total_cache_requests;
  } else {
    metrics["cache_hit_rate_percent"] = 0;
  }
  
  if (_redis_operations.load() > 0) {
    metrics["avg_redis_time_ns"] = _redis_time_ns.load() / _redis_operations.load();
  } else {
    metrics["avg_redis_time_ns"] = 0;
  }
  
  if (_mongodb_operations.load() > 0) {
    metrics["avg_mongodb_time_ns"] = _mongodb_time_ns.load() / _mongodb_operations.load();
  } else {
    metrics["avg_mongodb_time_ns"] = 0;
  }
  
  if (_post_service_calls.load() > 0) {
    metrics["avg_post_service_time_ns"] = _post_service_time_ns.load() / _post_service_calls.load();
  } else {
    metrics["avg_post_service_time_ns"] = 0;
  }
}

void UserTimelineBusinessLogic::ResetMetrics() {
  std::lock_guard<std::mutex> lock(_metrics_mutex);
  
  _write_requests = 0;
  _read_requests = 0;
  _redis_operations = 0;
  _mongodb_operations = 0;
  _post_service_calls = 0;
  _cache_hits = 0;
  _cache_misses = 0;
  _total_processing_time_ns = 0;
  _redis_time_ns = 0;
  _mongodb_time_ns = 0;
  _post_service_time_ns = 0;
  
  LOG(info) << "UserTimelineBusinessLogic metrics reset";
}

} // namespace social_network
