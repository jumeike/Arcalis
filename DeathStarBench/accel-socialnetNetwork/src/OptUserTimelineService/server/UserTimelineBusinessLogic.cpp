#include "UserTimelineBusinessLogic.h"
#include "UserTimelineHandler.h"
#include <algorithm>
#include <cstring>
#include <shared_mutex>

namespace {
std::unordered_map<int64_t, std::vector<std::pair<int64_t, int64_t>>> local_timeline_store;
std::shared_mutex local_store_mutex;
} // namespace

namespace social_network {

UserTimelineBusinessLogic::UserTimelineBusinessLogic(
    ClientPool<ThriftClient<PostStorageServiceClient>>* post_client_pool,
    void* unused_pool_2, void* unused_pool_3) {
  _post_client_pool = post_client_pool;
  _unused_pool_2 = unused_pool_2;
  _unused_pool_3 = unused_pool_3;
  LOG(info) << "UserTimelineBusinessLogic initialized with local timeline storage and nested PostStorage RPC";

#ifdef ENABLE_GEM5
  if (!initializeBuffers()) {
    LOG(error) << "Failed to initialize UserTimeline GEM5 buffers";
  }
#endif // ENABLE_GEM5
}

#ifdef ENABLE_GEM5
UserTimelineBusinessLogic::~UserTimelineBusinessLogic() {
  cleanupBuffers();
}

bool UserTimelineBusinessLogic::initializeBuffers() {
  try {
    raw_recv_buf_ = new uint8_t[BUFFER_SIZE + ALIGNMENT];
    raw_resp_buf_ = new uint8_t[BUFFER_SIZE + ALIGNMENT];
    recv_buf_ = allocateAlignedBuffer(raw_recv_buf_);
    resp_buf_ = allocateAlignedBuffer(raw_resp_buf_);

    std::memset(recv_buf_, 0, BUFFER_SIZE);
    std::memset(resp_buf_, 0, BUFFER_SIZE);

    LOG(info) << "UserTimeline buffers initialized - recv: " << std::hex
              << reinterpret_cast<uintptr_t>(recv_buf_);
    LOG(info) << "UserTimeline buffers initialized - resp: " << std::hex
              << reinterpret_cast<uintptr_t>(resp_buf_);
    return true;
  } catch (const std::exception& e) {
    LOG(error) << "UserTimeline buffer initialization failed: " << e.what();
    cleanupBuffers();
    return false;
  }
}

void UserTimelineBusinessLogic::cleanupBuffers() {
  if (raw_recv_buf_) {
    delete[] raw_recv_buf_;
    raw_recv_buf_ = nullptr;
    recv_buf_ = nullptr;
  }
  if (raw_resp_buf_) {
    delete[] raw_resp_buf_;
    raw_resp_buf_ = nullptr;
    resp_buf_ = nullptr;
  }
}

uint8_t* UserTimelineBusinessLogic::allocateAlignedBuffer(uint8_t* raw_buf) {
  uintptr_t addr = reinterpret_cast<uintptr_t>(raw_buf);
  uintptr_t aligned_addr = (addr + 0x3F) & ~0x3F;
  return reinterpret_cast<uint8_t*>(aligned_addr);
}

void UserTimelineBusinessLogic::setTraceConfig(const std::string& file, int requests) {
  trace_file_ = file;
  num_requests_ = requests;

  auto socket = getSocketFromTransport();
  if (socket) {
    socket->getReplaySocket().loadTrace(trace_file_, num_requests_);
  }
}

apache::thrift::transport::TSocket* UserTimelineBusinessLogic::getSocketFromTransport() {
  auto buffered = dynamic_cast<apache::thrift::transport::TBufferedTransport*>(
      in_->getTransport().get());
  return buffered ? dynamic_cast<apache::thrift::transport::TSocket*>(
                        buffered->getUnderlyingTransport().get())
                  : nullptr;
}

void UserTimelineBusinessLogic::callSWread() {
  std::string fname;
  ::apache::thrift::protocol::TMessageType mtype;
  int32_t seqid;
  in_->readMessageBegin(fname, mtype, seqid);
  if (mtype != ::apache::thrift::protocol::T_CALL && mtype != ::apache::thrift::protocol::T_ONEWAY) {
    ::apache::thrift::GlobalOutput.printf("received invalid message type %d from client", mtype);
    return;
  }
  fname_ = fname;
  seqid_ = seqid;

  if (processor_->getEventHandler().get() != nullptr) {
    std::string service_method = "UserTimelineService." + fname;
    processor_->getEventHandler()->preRead(ctx_, service_method.c_str());
  }

  if (fname == "WriteUserTimeline") {
    write_args_.read(in_.get());
  } else if (fname == "ReadUserTimeline") {
    read_args_.read(in_.get());
  }

  in_->readMessageEnd();
  uint32_t bytes = in_->getTransport()->readEnd();
  if (processor_->getEventHandler().get() != nullptr) {
    std::string service_method = "UserTimelineService." + fname;
    processor_->getEventHandler()->postRead(ctx_, service_method.c_str(), bytes);
  }

#ifdef ENABLE_TRACING
  if (fname == "WriteUserTimeline") {
    LOG_RPC_TO_APP(write_args_);
  } else if (fname == "ReadUserTimeline") {
    LOG_RPC_TO_APP(read_args_);
  }
#endif
}

bool UserTimelineBusinessLogic::callSWdispatch() {
  return processor_->dispatchCall(in_.get(), out_.get(), fname_, seqid_, connectionContext_);
}

void UserTimelineBusinessLogic::callSWwrite() {
#ifdef ENABLE_TRACING
  if (fname_ == "WriteUserTimeline") {
    LOG_APP_TO_RPC(write_args_.req_id, write_result_);
  } else if (fname_ == "ReadUserTimeline") {
    LOG_APP_TO_RPC(read_args_.req_id, read_result_);
  }
#endif

  if (processor_->getEventHandler().get() != nullptr) {
    std::string service_method = "UserTimelineService." + fname_;
    processor_->getEventHandler()->preWrite(ctx_, service_method.c_str());
  }

  out_->writeMessageBegin(fname_, ::apache::thrift::protocol::T_REPLY, seqid_);

  if (fname_ == "WriteUserTimeline") {
    write_result_.write(out_.get());
  } else if (fname_ == "ReadUserTimeline") {
    read_result_.write(out_.get());
  }

  out_->writeMessageEnd();
  uint32_t bytes = out_->getTransport()->writeEnd();
  out_->getTransport()->flush();
  if (processor_->getEventHandler().get() != nullptr) {
    std::string service_method = "UserTimelineService." + fname_;
    processor_->getEventHandler()->postWrite(ctx_, service_method.c_str(), bytes);
  }
}

void UserTimelineBusinessLogic::callSWsendresp(bool success) {
  if (handler_) {
    handler_->success_ = success;
  } else {
    sw_path_success_ = success;
  }
}

void UserTimelineBusinessLogic::callSWSendBuf() {
  int op_type = handler_ ? handler_->operation_type_ : current_operation_type_;
  if (op_type != 1) {
    return;
  }

  int32_t count = *reinterpret_cast<int32_t*>(resp_buf_ + resp_buf_offset_);
  if (handler_) {
    handler_->current_posts_.clear();
    handler_->current_posts_.reserve(count);
  } else {
    sw_path_read_posts_.clear();
    sw_path_read_posts_.reserve(count);
  }

  size_t read_offset = 0;
  for (int i = 0; i < count; ++i) {
    Post post = readPostFromBuffer(resp_buf_, read_offset);
    if (handler_) {
      handler_->current_posts_.emplace_back(std::move(post));
    } else {
      sw_path_read_posts_.emplace_back(std::move(post));
    }
  }
}

void UserTimelineBusinessLogic::serializeReadUserTimelineResponse(const std::vector<Post>& posts) {
#ifdef ENABLE_CEREBELLUM
  // Cerebellum path: cache-line-aligned fixed slots for predictable accelerator reads.
  constexpr size_t CACHE_LINE_SIZE = 64;
  constexpr size_t POST_SIZE = 288;
  constexpr size_t CACHE_LINES_PER_POST =
      (POST_SIZE + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE;
  constexpr size_t SLOT_SIZE = CACHE_LINES_PER_POST * CACHE_LINE_SIZE;

  size_t slot_offset = 0;
  for (const auto& post : posts) {
    size_t write_offset = slot_offset;
    writePostToBuffer(resp_buf_, write_offset, post);

    if (write_offset < slot_offset + SLOT_SIZE) {
      std::memset(resp_buf_ + write_offset, 0, slot_offset + SLOT_SIZE - write_offset);
    }

    slot_offset += SLOT_SIZE;
  }

  resp_buf_offset_ = slot_offset;
  resp_buf_size_ = slot_offset;
#else
  size_t write_offset = 0;
  for (const auto& post : posts) {
    writePostToBuffer(resp_buf_, write_offset, post);
  }
  *reinterpret_cast<int32_t*>(resp_buf_ + write_offset) = static_cast<int32_t>(posts.size());
  resp_buf_offset_ = write_offset;
  resp_buf_size_ = write_offset + sizeof(int32_t);
#endif
}
#endif // ENABLE_GEM5

#ifdef ENABLE_CEREBELLUM
void UserTimelineBusinessLogic::callEngineRead() {
  auto socket = getSocketFromTransport();
  if (!socket || !sendAddress || !readAddress) {
    return;
  }

  uint8_t* recv_addr = socket->getReplaySocket().getRecvBufferAddr();
  volatile uint64_t cmd = reinterpret_cast<uint64_t>(recv_addr) | cmd_send_dpdk_buf;
  *sendAddress = cmd;
  volatile uint64_t ack = *readAddress;

  size_t total_size = socket->getReplaySocket().getCurrentPacketSize();
  uint64_t dpdk_len = (static_cast<uint64_t>(total_size) & 0x7FF) << 4;
  cmd = dpdk_len | cmd_send_dpdk_len;
  *sendAddress = cmd;
  ack = *readAddress;

  (void)ack;
  socket->getReplaySocket().advanceReadPos();
}

bool UserTimelineBusinessLogic::callEngineDispatch() {
  if (!sendAddress || !readAddress) {
    return false;
  }

  volatile uint64_t cmd = reinterpret_cast<uint64_t>(recv_buf_) | cmd_set_app_flag;
  *sendAddress = cmd;

  volatile uint64_t request = *readAddress;
  int operation_type = (request & 0xF);
  current_operation_type_ = operation_type;
  if (handler_) {
    handler_->operation_type_ = operation_type;
  }

  if (operation_type == 0) {
    WriteUserTimeline();
  } else if (operation_type == 1) {
    ReadUserTimeline();
  } else {
    LOG(error) << "Unknown UserTimeline operation type: " << operation_type;
    return false;
  }

  LOG_DEBUG(debug) << "UserTimeline operation " << operation_type
                   << " completed in Engine Path";
  return true;
}

void UserTimelineBusinessLogic::callEngineWrite() {
  auto socket = getSocketFromTransport();
  if (!socket || !sendAddress || !readAddress) {
    return;
  }

  // 1) Send DPDK resp buffer address
  uint8_t* resp_addr = socket->getReplaySocket().getRespBufferAddr();
  volatile uint64_t cmd = reinterpret_cast<uint64_t>(resp_addr) | cmd_set_dpdk_flag;
  *sendAddress = cmd;
  volatile uint64_t ack = *readAddress;
  (void)ack;
  // 2) Advance resp buffer pointer to next position
  uint16_t data_size;
  if (handler_->operation_type_ == 0) {
    data_size = 30; // fixed response size for WriteUserTimeline
  } else if (handler_->operation_type_ == 1) {
    data_size = 1078; // fixed response size for ReadUserTimeline
  } else {
    LOG(error) << "Unknown operation type: " << handler_->operation_type_;
    return;
  }
  socket->getReplaySocket().advanceWritePos(data_size);
}

void UserTimelineBusinessLogic::callEngineSendresp(bool success) {
  if (!sendAddress || !readAddress) {
    return;
  }

  uint64_t response = 0;
  int op_type = handler_ ? handler_->operation_type_ : current_operation_type_;
  response |= (op_type & 0xF) << 4;
  response |= (success ? 1ULL : 0ULL) << 8;
  response |= (resp_buf_size_ & 0x7FF) << 9; // 0 for WriteUserTimeline, variable for ReadUserTimeline

  uint64_t cmd = response | cmd_send_app_resp;
  *sendAddress = cmd;
  volatile uint64_t ack = *readAddress;
  (void)ack;
}

void UserTimelineBusinessLogic::callEngineSendBuf() {
  if (!sendAddress || !readAddress) {
    return;
  }

  uint64_t cmd = reinterpret_cast<uint64_t>(resp_buf_) | cmd_send_app_buf;
  *sendAddress = cmd;
  volatile uint64_t ack = *readAddress;
  (void)ack;
}
#endif // ENABLE_CEREBELLUM

#ifdef ENABLE_GEM5
void UserTimelineBusinessLogic::runLoop(
    apache::thrift::TDispatchProcessor* processor,
    std::shared_ptr<::apache::thrift::protocol::TProtocol> in,
    std::shared_ptr<::apache::thrift::protocol::TProtocol> out,
    void* connectionContext) {
  LOG(info) << "JU:JU =========================================";
  LOG(info) << "JU:JU Start UserTimeline business logic runLoop";

  processor_ = processor;
  in_ = in;
  out_ = out;
  connectionContext_ = connectionContext;
  read_pos_ = 0;
  write_pos_ = 0;

  int runs = 0;
  for (bool done = false; !done;) {
    if (runs == 1000) {
      LOG(info) << "JU:JU Begin ROI";
#ifdef ENABLE_GEM5_TEST
      m5_exit_addr(0);
#endif
    }

#ifdef ENABLE_CEREBELLUM
    callEngineRead();
    bool res = callEngineDispatch();
    callEngineWrite();
#else
    callSWread();
    bool res = callSWdispatch();
    callSWwrite();
#endif

    if (!res) {
      break;
    }

    done = checkReplayEOF();
    if (done) {
      LOG(info) << "JU:JU EOF reached - trace replay complete";
    }

    runs++;
  }

#ifdef ENABLE_GEM5_TEST
  m5_work_end_addr(0, 0);
  LOG(info) << "JU:JU End ROI";
#endif

  if (validateReplay()) {
    LOG(info) << "JU:JU UserTimeline replay validation PASSED";
  } else {
    LOG(info) << "JU:JU UserTimeline replay validation FAILED";
  }

  LOG(info) << "JU:JU Finished UserTimeline business logic runLoop";
  LOG(info) << "JU:JU =========================================";
}

void UserTimelineBusinessLogic::WriteUserTimeline() {
  size_t offset = 0;
  int64_t req_id = readInt64(recv_buf_, offset);
  current_operation_type_ = readInt32(recv_buf_, offset);
  if (handler_) {
    handler_->operation_type_ = current_operation_type_;
  }
  int64_t post_id = readInt64(recv_buf_, offset);
  int64_t user_id = readInt64(recv_buf_, offset);
  int64_t timestamp = readInt64(recv_buf_, offset);

  bool operation_success = false;
  try {
    const std::map<std::string, std::string> empty_carrier;
    WriteUserTimeline(req_id, post_id, user_id, timestamp, empty_carrier);
    resp_buf_offset_ = 0;
    resp_buf_size_ = 0;
    operation_success = true;
  } catch (const std::exception& e) {
    LOG(error) << "WriteUserTimeline (GEM5) failed: " << e.what();
    *reinterpret_cast<int32_t*>(resp_buf_) = -1;
    resp_buf_offset_ = 0;
    resp_buf_size_ = sizeof(int32_t);
  }

  LOG_DEBUG(debug) << "Request " << req_id << " wrote timeline entry post_id=" << post_id
                   << " user_id=" << user_id;

#ifdef ENABLE_CEREBELLUM
  callEngineSendresp(operation_success);
  callEngineSendBuf();
#else
  callSWsendresp(operation_success);
  callSWSendBuf();
#endif
}

void UserTimelineBusinessLogic::ReadUserTimeline() {
  size_t offset = 0;
  int64_t req_id = readInt64(recv_buf_, offset);
  current_operation_type_ = readInt32(recv_buf_, offset);
  if (handler_) {
    handler_->operation_type_ = current_operation_type_;
  }
  int64_t user_id = readInt64(recv_buf_, offset);
  int32_t start = readInt32(recv_buf_, offset);
  int32_t stop = readInt32(recv_buf_, offset);

  std::vector<Post> posts;
  bool operation_success = false;
  try {
    const std::map<std::string, std::string> empty_carrier;
    ReadUserTimeline(posts, req_id, user_id, start, stop, empty_carrier);
    serializeReadUserTimelineResponse(posts);
    operation_success = true;
  } catch (const std::exception& e) {
    LOG(error) << "ReadUserTimeline (GEM5) failed: " << e.what();
    *reinterpret_cast<int32_t*>(resp_buf_) = -1;
    resp_buf_offset_ = 0;
    resp_buf_size_ = sizeof(int32_t);
  }

  LOG_DEBUG(debug) << "Request " << req_id << " read timeline for user_id=" << user_id
                   << " posts=" << posts.size();

#ifdef ENABLE_CEREBELLUM
  callEngineSendresp(operation_success);
  callEngineSendBuf();
#else
  callSWsendresp(operation_success);
  callSWSendBuf();
#endif
}
#endif // ENABLE_GEM5

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
#ifdef ENABLE_GEM5
    // Keep post-storage state in sync for replayed writes.
    StorePostToPostService(req_id, post_id, user_id, timestamp, carrier);
#endif
    // Write to MongoDB
    WriteTimelineToMongoDB(user_id, post_id, timestamp);
    
    // Update user's timeline in Redis
    UpdateRedisTimeline(std::to_string(user_id), std::to_string(post_id),
               static_cast<double>(timestamp));
    
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
  try {
    _return = GetPostsFromPostService(req_id, post_ids, carrier);
  } catch (...) {
#ifdef ENABLE_GEM5
    // Keep replay loop alive when some post_ids are not present yet.
    _return.clear();
    LOG(warning) << "Returning empty posts for req_id=" << req_id
                 << " because post-storage lookup failed";
#else
    throw;
#endif
  }

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
  double timestamp) {
  auto redis_start = std::chrono::high_resolution_clock::now();

  try {
    const int64_t uid = std::stoll(user_id);
    const int64_t pid = std::stoll(post_id);
    const int64_t ts = static_cast<int64_t>(timestamp);

    std::unique_lock<std::shared_mutex> lock(local_store_mutex);
    auto& timeline = local_timeline_store[uid];
    const bool exists = std::any_of(
        timeline.begin(), timeline.end(),
        [pid](const auto& e) { return e.first == pid; });
    if (!exists) {
      timeline.insert(timeline.begin(), std::make_pair(pid, ts));
    }
    _redis_operations++;
  } catch (const std::exception& err) {
    LOG(error) << "Local timeline update error: " << err.what();
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
    const int64_t uid = std::stoll(user_id);
    std::unique_lock<std::shared_mutex> lock(local_store_mutex);
    auto& timeline = local_timeline_store[uid];

    for (const auto& kv : post_score_map) {
      const int64_t pid = std::stoll(kv.first);
      const int64_t ts = static_cast<int64_t>(kv.second);
      const bool exists = std::any_of(
          timeline.begin(), timeline.end(),
          [pid](const auto& e) { return e.first == pid; });
      if (!exists) {
        timeline.emplace_back(pid, ts);
      }
    }

    std::sort(
        timeline.begin(), timeline.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    _redis_operations++;
  } catch (const std::exception& err) {
    LOG(error) << "Local timeline batch update error: " << err.what();
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
    const int64_t uid = std::stoll(user_id);

    std::shared_lock<std::shared_mutex> lock(local_store_mutex);
    auto it = local_timeline_store.find(uid);
    if (it != local_timeline_store.end()) {
      const auto& timeline = it->second;
      const int s = std::max(0, start);
      const int e = std::min(stop, static_cast<int>(timeline.size()) - 1);
      for (int i = s; i <= e; ++i) {
        post_ids_str.emplace_back(std::to_string(timeline[i].first));
      }
    }

    _redis_operations++;

    if (!post_ids_str.empty()) {
      _cache_hits++;
    } else {
      _cache_misses++;
    }
  } catch (const std::exception& err) {
    LOG(error) << "Local timeline read error: " << err.what();
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

 std::unique_lock<std::shared_mutex> lock(local_store_mutex);
 auto& timeline = local_timeline_store[user_id];
 const bool exists = std::any_of(
     timeline.begin(), timeline.end(),
     [post_id](const auto& e) { return e.first == post_id; });
 if (!exists) {
   timeline.insert(timeline.begin(), std::make_pair(post_id, timestamp));
 }

 _mongodb_operations++;
 auto mongodb_end = std::chrono::high_resolution_clock::now();
 _mongodb_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
     mongodb_end - mongodb_start).count();
}


std::vector<std::pair<int64_t, int64_t>> UserTimelineBusinessLogic::ReadTimelineFromMongoDB(
   int64_t user_id, int start, int stop) {
 auto mongodb_start = std::chrono::high_resolution_clock::now();

 std::vector<std::pair<int64_t, int64_t>> timeline_posts;
 std::shared_lock<std::shared_mutex> lock(local_store_mutex);
 auto it = local_timeline_store.find(user_id);
 if (it != local_timeline_store.end()) {
   const auto& timeline = it->second;
   const int s = std::max(0, start);
   const int e = std::min(stop - 1, static_cast<int>(timeline.size()) - 1);
   for (int i = s; i <= e; ++i) {
     timeline_posts.emplace_back(timeline[i]);
   }
 }

 _mongodb_operations++;
 auto mongodb_end = std::chrono::high_resolution_clock::now();
 _mongodb_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
     mongodb_end - mongodb_start).count();

 return timeline_posts;
}

void UserTimelineBusinessLogic::StorePostToPostService(
    int64_t req_id, int64_t post_id, int64_t user_id, int64_t timestamp,
    const std::map<std::string, std::string>& carrier) {
  auto post_client_wrapper = _post_client_pool->Pop();
  if (!post_client_wrapper) {
    ServiceException se;
    se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
    se.message = "Failed to connect to post-storage-service";
    throw se;
  }

  Post post;
  const int32_t pseudo_thread_id = static_cast<int32_t>((user_id - 1 + 100) % 100);
  post.post_id = post_id;
  post.req_id = req_id;
  post.timestamp = timestamp;
  post.post_type = static_cast<PostType::type>(post_id % 4);

  char text_buf[65];
  snprintf(text_buf, sizeof(text_buf),
           "Sample post text from thread %03d with post_id %010ld      ",
           pseudo_thread_id % 1000, post_id);
  std::string text(text_buf);
  text.resize(64, ' ');
  post.text = std::move(text);

  post.creator.user_id = pseudo_thread_id + 1000;
  char username_buf[17];
  snprintf(username_buf, sizeof(username_buf), "user_%011d", pseudo_thread_id);
  std::string creator_name(username_buf);
  creator_name.resize(16, ' ');
  post.creator.username = std::move(creator_name);

  UserMention mention;
  mention.user_id = (pseudo_thread_id + 1) * 1000;
  char mention_buf[17];
  snprintf(mention_buf, sizeof(mention_buf), "mentioned_%05d", (pseudo_thread_id + 1) % 100000);
  std::string mention_name(mention_buf);
  mention_name.resize(16, ' ');
  mention.username = std::move(mention_name);
  post.user_mentions.push_back(std::move(mention));

  Media media;
  media.media_id = (post_id * 10) & 0x7FFFFFFFFFFFFFFF;
  static const char* kMediaTypes[] = {
      "image   ", "video   ", "audio   ", "document", "gif     "};
  media.media_type = kMediaTypes[post_id % 5];
  post.media.push_back(std::move(media));

  Url url;
  std::string short_url = "http://short.ly/" + std::to_string(post_id % 10000000000LL);
  std::string expanded_url = "http://example.com/full_url/" +
                             std::to_string(post_id % 100000000000000LL);
  if (short_url.size() < 32) {
    short_url.resize(32, ' ');
  } else if (short_url.size() > 32) {
    short_url.resize(32);
  }
  if (expanded_url.size() < 64) {
    expanded_url.resize(64, ' ');
  } else if (expanded_url.size() > 64) {
    expanded_url.resize(64);
  }
  url.shortened_url = std::move(short_url);
  url.expanded_url = std::move(expanded_url);
  post.urls.push_back(std::move(url));

  auto post_client = post_client_wrapper->GetClient();
  try {
    post_client->StorePost(req_id, post, carrier);
    _post_client_pool->Keepalive(post_client_wrapper);
    _post_service_calls++;
  } catch (...) {
    _post_client_pool->Remove(post_client_wrapper);
    LOG(error) << "Failed to store post in post-storage-service";
    throw;
  }
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
      LOG(debug) << "About to read posts from post-storage-service";
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
    LOG(debug) << "Successfully got posts from post-storage-service";
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
