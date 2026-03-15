#include "UrlShortenBusinessLogic.h"
#include "UrlShortenHandler.h"
#include <future>
#include <cstring>
#include <unordered_map>
#include <shared_mutex>
#include <array>

static std::unordered_map<std::string, std::string> local_url_store;
static std::shared_mutex local_url_store_mutex;

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
    void* unused1,
    void* unused2)
    : _unused_pool_1(unused1),
      _unused_pool_2(unused2),
      _distribution(0, 61) {
  LOG(info) << "UrlShortenBusinessLogic initialized with local storage";

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
  constexpr size_t CACHE_LINE_SIZE = 64;
  constexpr size_t COMPOSE_URL_SIZE = 31 + 68; // 99B per URL (31B for short URL, 68B for expanded URL, 4 for count)
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
  #ifndef ENABLE_CEREBELLUM // Write at the end to help non-Cerebullum (SW) path read the count
  *reinterpret_cast<int32_t*>(resp_buf_ + slot_offset) = static_cast<int32_t>(urls.size());
  resp_buf_size_ = slot_offset + sizeof(int32_t);
  #endif
}

void UrlShortenBusinessLogic::serializeExtendedUrlsResponse(
    const std::vector<std::string>& extended_urls) {
  constexpr size_t CACHE_LINE_SIZE = 64;
  constexpr size_t EXTENDED_URL_SIZE = 68; // 68B per URL (expanded URL string, 4 for count)
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
  #ifndef ENABLE_CEREBELLUM // Write at the end to help non-Cerebellum (SW) path read the count
    *reinterpret_cast<int32_t*>(resp_buf_ + slot_offset) = static_cast<int32_t>(extended_urls.size());
    resp_buf_size_ = slot_offset + sizeof(int32_t);
  #endif
}
#endif // ENABLE_GEM5

#ifdef ENABLE_CEREBELLUM
void UrlShortenBusinessLogic::callEngineRead() {
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

bool UrlShortenBusinessLogic::callEngineDispatch() {
  if (!sendAddress || !readAddress) {
    return false;
  }

  volatile uint64_t cmd = reinterpret_cast<uint64_t>(recv_buf_) | cmd_set_app_flag;
  *sendAddress = cmd;

  volatile uint64_t request = *readAddress;
  int operation_type = (request & 0xF);

  if (operation_type == 0) {
    ComposeUrls();
  } else if (operation_type == 1) {
    GetExtendedUrls();
  }

  return true;
}

void UrlShortenBusinessLogic::callEngineWrite() {
  auto socket = getSocketFromTransport();
  if (!socket || !sendAddress || !readAddress) {
    return;
  }

  uint8_t* resp_addr = socket->getReplaySocket().getRespBufferAddr();
  volatile uint64_t cmd = reinterpret_cast<uint64_t>(resp_addr) | cmd_set_dpdk_flag;
  *sendAddress = cmd;
  volatile uint64_t ack = *readAddress;
  (void)ack;

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

  response |= (handler_->operation_type_ & 0xF) << 4;
  response |= (success ? 1ULL : 0ULL) << 8;
  response |= (response_len & 0x7FF) << 9;
  uint64_t cmd = response | cmd_send_app_resp;
  *sendAddress = cmd;
  volatile uint64_t ack = *readAddress;
  (void)ack;
}

void UrlShortenBusinessLogic::callEngineSendBuf() {
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
  readInt32(recv_buf_, offset);                      // op_type (0 = ComposeUrls)
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
  readInt32(recv_buf_, offset);                      // op_type (1 = GetExtendedUrls)
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
  auto start = std::chrono::high_resolution_clock::now();
  {
    std::unique_lock<std::shared_mutex> lock(local_url_store_mutex);
    for (const auto& url : urls) {
      local_url_store[url.shortened_url] = url.expanded_url;
    }
  }
  auto end = std::chrono::high_resolution_clock::now();
  _mongo_insert_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
      end - start).count();
}

void UrlShortenBusinessLogic::_FetchUrlsFromMemcached(
    std::vector<std::string>& _return,
    const std::vector<std::string>& shortened_urls,
    std::vector<std::string>& cache_misses) {
  std::shared_lock<std::shared_mutex> lock(local_url_store_mutex);
  for (const auto& shortened_url : shortened_urls) {
    auto it = local_url_store.find(shortened_url);
    if (it != local_url_store.end()) {
      _return.push_back(it->second);
    } else {
      cache_misses.push_back(shortened_url);
    }
  }
}

void UrlShortenBusinessLogic::_FetchUrlsFromMongo(
    std::vector<std::string>& _return,
    const std::vector<std::string>& shortened_urls) {
  std::shared_lock<std::shared_mutex> lock(local_url_store_mutex);
  for (const auto& shortened_url : shortened_urls) {
    auto it = local_url_store.find(shortened_url);
    if (it != local_url_store.end()) {
      _return.push_back(it->second);
    } else {
      LOG(warning) << "Shortened URL not found in local storage: " << shortened_url;
      _return.push_back("");
    }
  }
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
