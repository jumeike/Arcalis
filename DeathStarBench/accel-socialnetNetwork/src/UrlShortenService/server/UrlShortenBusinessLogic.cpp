#include "UrlShortenBusinessLogic.h"
#include "UrlShortenHandler.h"
#include <future>
#include <cstring>
#include <array>

namespace {
std::string makeDeterministicShortUrl(const std::string& expanded_url) {
  // FNV-1a 64-bit hash for stable cross-run short URL generation.
  uint64_t h = 1469598103934665603ULL;
  for (unsigned char c : expanded_url) {
    h ^= static_cast<uint64_t>(c);
    h *= 1099511628211ULL;
  }

  static constexpr char kBase62[] =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  std::array<char, 10> token{};
  for (int i = 9; i >= 0; --i) {
    token[i] = kBase62[h % 62];
    h /= 62;
  }

  return std::string(HOSTNAME) + std::string(token.data(), token.size());
}
} // namespace

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

#ifdef ENABLE_GEM5
  if (!initializeBuffers()) {
    LOG(error) << "Failed to initialize UrlShorten GEM5 buffers";
  }
#endif // ENABLE_GEM5
}

#ifdef ENABLE_GEM5
UrlShortenBusinessLogic::~UrlShortenBusinessLogic() {
  cleanupBuffers();
}

bool UrlShortenBusinessLogic::initializeBuffers() {
  try {
    raw_recv_buf_ = new uint8_t[BUFFER_SIZE + ALIGNMENT];
    raw_resp_buf_ = new uint8_t[BUFFER_SIZE + ALIGNMENT];
    recv_buf_ = allocateAlignedBuffer(raw_recv_buf_);
    resp_buf_ = allocateAlignedBuffer(raw_resp_buf_);

    std::memset(recv_buf_, 0, BUFFER_SIZE);
    std::memset(resp_buf_, 0, BUFFER_SIZE);

    LOG(info) << "UrlShorten buffers initialized - recv: " << std::hex
              << reinterpret_cast<uintptr_t>(recv_buf_);
    LOG(info) << "UrlShorten buffers initialized - resp: " << std::hex
              << reinterpret_cast<uintptr_t>(resp_buf_);
    return true;
  } catch (const std::exception& e) {
    LOG(error) << "UrlShorten buffer initialization failed: " << e.what();
    cleanupBuffers();
    return false;
  }
}

void UrlShortenBusinessLogic::cleanupBuffers() {
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

uint8_t* UrlShortenBusinessLogic::allocateAlignedBuffer(uint8_t* raw_buf) {
  uintptr_t addr = reinterpret_cast<uintptr_t>(raw_buf);
  uintptr_t aligned_addr = (addr + 0x3F) & ~0x3F;
  return reinterpret_cast<uint8_t*>(aligned_addr);
}

void UrlShortenBusinessLogic::setTraceConfig(const std::string& file, int requests) {
  trace_file_ = file;
  num_requests_ = requests;

  auto socket = getSocketFromTransport();
  if (socket) {
    socket->getReplaySocket().loadTrace(trace_file_, num_requests_);
  }
}

apache::thrift::transport::TSocket* UrlShortenBusinessLogic::getSocketFromTransport() {
  auto buffered = dynamic_cast<apache::thrift::transport::TBufferedTransport*>(
      in_->getTransport().get());
  return buffered ? dynamic_cast<apache::thrift::transport::TSocket*>(
                        buffered->getUnderlyingTransport().get())
                  : nullptr;
}

void UrlShortenBusinessLogic::callSWread() {
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

  if (processor_->getEventHandler().get() != NULL) {
    std::string service_method = "UrlShortenService." + fname;
    processor_->getEventHandler()->preRead(ctx_, service_method.c_str());
  }

  if (fname == "ComposeUrls") {
    compose_args_.read(in_.get());
  } else if (fname == "GetExtendedUrls") {
    get_args_.read(in_.get());
  }

  in_->readMessageEnd();
  uint32_t bytes = in_->getTransport()->readEnd();
  if (processor_->getEventHandler().get() != NULL) {
    std::string service_method = "UrlShortenService." + fname;
    processor_->getEventHandler()->postRead(ctx_, service_method.c_str(), bytes);
  }

#ifdef ENABLE_TRACING
  if (fname == "ComposeUrls") {
    LOG_RPC_TO_APP(compose_args_);
  } else if (fname == "GetExtendedUrls") {
    LOG_RPC_TO_APP(get_args_);
  }
#endif
}

bool UrlShortenBusinessLogic::callSWdispatch() {
  return processor_->dispatchCall(in_.get(), out_.get(), fname_, seqid_, connectionContext_);
}

void UrlShortenBusinessLogic::callSWwrite() {
#ifdef ENABLE_TRACING
  if (fname_ == "ComposeUrls") {
    LOG_APP_TO_RPC(compose_args_.req_id, compose_result_);
  } else if (fname_ == "GetExtendedUrls") {
    LOG_APP_TO_RPC(get_args_.req_id, get_result_);
  }
#endif

  if (processor_->getEventHandler().get() != NULL) {
    std::string service_method = "UrlShortenService." + fname_;
    processor_->getEventHandler()->preWrite(ctx_, service_method.c_str());
  }

  out_->writeMessageBegin(fname_, ::apache::thrift::protocol::T_REPLY, seqid_);

  if (fname_ == "ComposeUrls") {
    compose_result_.write(out_.get());
  } else if (fname_ == "GetExtendedUrls") {
    get_result_.write(out_.get());
  }

  out_->writeMessageEnd();
  uint32_t bytes = out_->getTransport()->writeEnd();
  out_->getTransport()->flush();
  if (processor_->getEventHandler().get() != NULL) {
    std::string service_method = "UrlShortenService." + fname_;
    processor_->getEventHandler()->postWrite(ctx_, service_method.c_str(), bytes);
  }
}

void UrlShortenBusinessLogic::callSWsendresp(bool success) {
  handler_->success_ = success;
}

void UrlShortenBusinessLogic::callSWSendBuf() {
  if (handler_->operation_type_ == 0) {
    // ComposeUrls - read serialized Url objects from resp_buf_
    int32_t count = *reinterpret_cast<int32_t*>(resp_buf_ + resp_buf_offset_);
    handler_->current_target_urls_.clear();
    handler_->current_target_urls_.reserve(count);
    size_t read_offset = 0;
    for (int i = 0; i < count; i++) {
      Url url = readUrlFromBuffer(resp_buf_, read_offset);
      handler_->current_target_urls_.push_back(url);
    }
    LOG_DEBUG(debug) << "ComposeUrls completed in SW Path, count: " << count;
  } else if (handler_->operation_type_ == 1) {
    // GetExtendedUrls - read strings from resp_buf_
    int32_t count = *reinterpret_cast<int32_t*>(resp_buf_ + resp_buf_offset_);
    handler_->current_extended_urls_.clear();
    handler_->current_extended_urls_.reserve(count);
    
    size_t read_offset = 0;
    for (int i = 0; i < count; i++) {
      std::string url = readString(resp_buf_, read_offset);
      handler_->current_extended_urls_.push_back(url);
    }
    LOG_DEBUG(debug) << "GetExtendedUrls completed in SW Path, count: " << count;
  }
}
#endif // ENABLE_GEM5

#ifdef ENABLE_GEM5
void UrlShortenBusinessLogic::serializeComposeUrlsResponse(const std::vector<Url>& urls) {
#ifdef ENABLE_CEREBELLUM
  // Cerebellum path: cache-line-aligned slots so the accelerator reads each entry
  // at a predictable 64B-aligned offset.
  constexpr size_t CACHE_LINE_SIZE = 64;
  constexpr size_t COMPOSE_URL_SIZE = 31 + 68;
  constexpr size_t CACHE_LINES_PER_URL =
      (COMPOSE_URL_SIZE + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE;
  constexpr size_t SLOT_SIZE = CACHE_LINES_PER_URL * CACHE_LINE_SIZE;

  size_t slot_offset = 0;
  for (const auto& url : urls) {
    size_t write_offset = slot_offset;
    writeUrlToBuffer(resp_buf_, write_offset, url);

    if (write_offset < slot_offset + SLOT_SIZE) {
      std::memset(resp_buf_ + write_offset, 0, slot_offset + SLOT_SIZE - write_offset);
    }

    slot_offset += SLOT_SIZE;
  }

  resp_buf_offset_ = slot_offset;
  resp_buf_size_ = slot_offset;
#else
  // SW path: packed sequential layout so callSWSendBuf / readUrlFromBuffer
  // can iterate without skipping slot gaps.
  size_t write_offset = 0;
  for (const auto& url : urls) {
    writeUrlToBuffer(resp_buf_, write_offset, url);
  }
  *reinterpret_cast<int32_t*>(resp_buf_ + write_offset) = static_cast<int32_t>(urls.size());
  resp_buf_offset_ = write_offset;
  resp_buf_size_ = write_offset + sizeof(int32_t);
#endif
}

void UrlShortenBusinessLogic::serializeExtendedUrlsResponse(
    const std::vector<std::string>& extended_urls) {
#ifdef ENABLE_CEREBELLUM
  // Cerebellum path: cache-line-aligned slots.
  constexpr size_t CACHE_LINE_SIZE = 64;
  constexpr size_t EXTENDED_URL_SIZE = 68;
  constexpr size_t CACHE_LINES_PER_URL =
      (EXTENDED_URL_SIZE + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE;
  constexpr size_t SLOT_SIZE = CACHE_LINES_PER_URL * CACHE_LINE_SIZE;

  size_t slot_offset = 0;
  for (const auto& url : extended_urls) {
    size_t write_offset = slot_offset;
    writeStringToBuffer(resp_buf_, write_offset, url);

    if (write_offset < slot_offset + SLOT_SIZE) {
      std::memset(resp_buf_ + write_offset, 0, slot_offset + SLOT_SIZE - write_offset);
    }

    slot_offset += SLOT_SIZE;
  }

  resp_buf_offset_ = slot_offset;
  resp_buf_size_ = slot_offset;
#else
  // SW path: packed sequential layout so callSWSendBuf / readString
  // can iterate without skipping slot gaps.
  size_t write_offset = 0;
  for (const auto& url : extended_urls) {
    writeStringToBuffer(resp_buf_, write_offset, url);
  }
  *reinterpret_cast<int32_t*>(resp_buf_ + write_offset) =
      static_cast<int32_t>(extended_urls.size());
  resp_buf_offset_ = write_offset;
  resp_buf_size_ = write_offset + sizeof(int32_t);
#endif
}
#endif // ENABLE_GEM5

#ifdef ENABLE_CEREBELLUM

void UrlShortenBusinessLogic::callEngineRead() {
  auto socket = getSocketFromTransport();
  if (!socket || !sendAddress || !readAddress) {
    return;
  }
  // 1) Send recv buffer address
  uint8_t* recv_addr = socket->getReplaySocket().getRecvBufferAddr();
  volatile uint64_t cmd = reinterpret_cast<uint64_t>(recv_addr) | cmd_send_dpdk_buf;
  *sendAddress = cmd;
  volatile uint64_t ack = *readAddress;

  // 2) Send total data size
  size_t total_size = socket->getReplaySocket().getCurrentPacketSize();
  uint64_t dpdk_len = (static_cast<uint64_t>(total_size) & 0x7FF) << 4;
  cmd = dpdk_len | cmd_send_dpdk_len;
  *sendAddress = cmd;
  ack = *readAddress;

  (void)ack;
  // 3) Advance recv buffer pointer to next position
  socket->getReplaySocket().advanceReadPos();
}

bool UrlShortenBusinessLogic::callEngineDispatch() {
  if (!sendAddress || !readAddress) {
    return false;
  }
  // 1) Send app recv buffer address
  volatile uint64_t cmd = reinterpret_cast<uint64_t>(recv_buf_) | cmd_set_app_flag;
  *sendAddress = cmd;

  // 2) Wait for Engine to set the request
  volatile uint64_t request = *readAddress;
  int operation_type = (request & 0xF);

  // 3) Call appropriate BusinessLogic Function
  if (operation_type == 0) {
    ComposeUrls();
  } else if (operation_type == 1) {
    GetExtendedUrls();
  }

  LOG_DEBUG(debug) << "UrlShorten operation " << operation_type << " completed in Engine Path";
  return true;
}

void UrlShortenBusinessLogic::callEngineWrite() {
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
    data_size = 350; // fixed response size for ComposeUrls
  } else if (handler_->operation_type_ == 1) {
    data_size = 240; // fixed response size for GetExtendedUrls
  } else {
    LOG(error) << "Unknown operation type: " << handler_->operation_type_;
    return;
  }

  socket->getReplaySocket().advanceWritePos(data_size);
}

void UrlShortenBusinessLogic::callEngineSendresp(bool success) {
  if (!sendAddress || !readAddress) {
    return;
  }

  uint64_t response = 0;
  size_t response_len = 0;

  if (handler_->operation_type_ == 0) {
    response_len = resp_buf_size_; // For ComposeUrls
  } else if (handler_->operation_type_ == 1) {
    response_len = resp_buf_size_; // For GetExtendedUrls
  }

  response |= (handler_->operation_type_ & 0xF) << 4; // operation type
  response |= (success ? 1ULL : 0ULL) << 8; // success flag
  response |= (response_len & 0x7FF) << 9; // length of response data

  uint64_t cmd = response | cmd_send_app_resp;
  *sendAddress = cmd;
  volatile uint64_t ack = *readAddress;
  (void)ack;
}

void UrlShortenBusinessLogic::callEngineSendBuf() {
  if (!sendAddress || !readAddress) {
    return;
  }
  // 1) Send response buffer to Engine
  uint64_t cmd = reinterpret_cast<uint64_t>(resp_buf_) | cmd_send_app_buf;
  *sendAddress = cmd;
  volatile uint64_t ack = *readAddress;
  (void)ack;
}
#endif // ENABLE_CEREBELLUM

#ifdef ENABLE_GEM5
void UrlShortenBusinessLogic::runLoop(
    apache::thrift::TDispatchProcessor* processor,
    std::shared_ptr<::apache::thrift::protocol::TProtocol> in,
    std::shared_ptr<::apache::thrift::protocol::TProtocol> out,
    void* connectionContext) {
  LOG(info) << "JU:JU =========================================";
  LOG(info) << "JU:JU Start UrlShorten business logic runLoop";

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
    LOG(info) << "JU:JU Replay validation PASSED";
  } else {
    LOG(info) << "JU:JU Replay validation FAILED";
  }

  LOG(info) << "JU:JU Finished UrlShorten business logic runLoop";
  LOG(info) << "JU:JU =========================================";
}

void UrlShortenBusinessLogic::ComposeUrls() {
  auto start_time = std::chrono::high_resolution_clock::now();

  // Read from recv_buf_: [int64_t req_id][int32_t op_type][int32_t url_count][...url strings...]
  size_t offset = 0;
  int64_t req_id    = readInt64(recv_buf_, offset);
  handler_->operation_type_ = readInt32(recv_buf_, offset); // op_type (0 = ComposeUrls)
  int32_t url_count = readInt32(recv_buf_, offset);

  std::vector<std::string> urls;
  for (int i = 0; i < url_count; i++) {
    urls.push_back(readString(recv_buf_, offset));
  }

  std::vector<Url> target_urls;
  bool operation_success = false;
  try {
    if (!urls.empty()) {
      for (auto& url : urls) {
        Url new_target_url;
        new_target_url.expanded_url = url;
        new_target_url.shortened_url = makeDeterministicShortUrl(url);
        target_urls.emplace_back(new_target_url);
      }
      _StoreUrlsInMongo(target_urls);
    }

    // Serialize response for both SW and Cerebellum paths.
    serializeComposeUrlsResponse(target_urls);
    operation_success = true;
  } catch (const std::exception& e) {
    LOG(error) << "ComposeUrls failed: " << e.what();
    *reinterpret_cast<int32_t*>(resp_buf_) = -1;
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  _requests_processed++;
  _compose_requests++;
  _total_processing_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
      end_time - start_time).count();

  LOG_DEBUG(debug) << "Request " << req_id << " composed " << url_count
                   << " URLs in GEM5 path";

#ifdef ENABLE_CEREBELLUM
  callEngineSendresp(operation_success);
  callEngineSendBuf();
#else
  callSWsendresp(operation_success);
  callSWSendBuf();
#endif // ENABLE_CEREBELLUM
}

void UrlShortenBusinessLogic::GetExtendedUrls() {
  auto start_time = std::chrono::high_resolution_clock::now();

  // Read from recv_buf_: [int64_t req_id][int32_t op_type][int32_t url_count][...shortened_url strings...]
  size_t offset = 0;
  int64_t req_id    = readInt64(recv_buf_, offset);
  handler_->operation_type_ = readInt32(recv_buf_, offset);  // op_type (1 = GetExtendedUrls)
  int32_t url_count = readInt32(recv_buf_, offset);

  std::vector<std::string> shortened_urls;
  for (int i = 0; i < url_count; i++) {
    shortened_urls.push_back(readString(recv_buf_, offset));
  }

  std::vector<std::string> extended_urls;
  bool operation_success = false;
  try {
    GetExtendedUrls(extended_urls, req_id, shortened_urls);

    // Serialize response for both SW and Cerebellum paths.
    serializeExtendedUrlsResponse(extended_urls);
    operation_success = true;
  } catch (const std::exception& e) {
    LOG(error) << "GetExtendedUrls failed: " << e.what();
    *reinterpret_cast<int32_t*>(resp_buf_) = -1;
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  _requests_processed++;
  _get_extended_requests++;
  _total_processing_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
      end_time - start_time).count();

  LOG_DEBUG(debug) << "Request " << req_id << " fetched " << url_count
                   << " extended URLs in GEM5 path";

#ifdef ENABLE_CEREBELLUM
  callEngineSendresp(operation_success);
  callEngineSendBuf();
#else
  callSWsendresp(operation_success);
  callSWSendBuf();
#endif // ENABLE_CEREBELLUM
}
#endif // ENABLE_GEM5

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
      new_target_url.shortened_url = makeDeterministicShortUrl(url);
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
  _compose_requests++;
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
  _get_extended_requests++;
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
  metrics["compose_requests"] = _compose_requests.load();
  metrics["get_extended_requests"] = _get_extended_requests.load();
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
  _compose_requests.store(0);
  _get_extended_requests.store(0);
  _total_processing_time_ns.store(0);
  _mongo_insert_time_ns.store(0);
  _memcached_time_ns.store(0);
}

} // namespace social_network
