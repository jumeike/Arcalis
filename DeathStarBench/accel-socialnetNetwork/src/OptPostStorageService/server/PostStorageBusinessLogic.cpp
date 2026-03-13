#include "PostStorageBusinessLogic.h"
#include "PostStorageHandler.h"
#include <chrono>
#include <cstring>
#include <future>
// STEP 1: Add to top of PostStorageBusinessLogic.cpp (after existing includes)
#include <unordered_map>
#include <shared_mutex>

// Global local storage
static std::unordered_map<int64_t, social_network::Post> local_posts;
static std::shared_mutex posts_mutex;

namespace social_network {

social_network::PostStorageBusinessLogic::PostStorageBusinessLogic(
    void* unused1, void* unused2) {
  LOG_DEBUG(info) << "PostStorageBusinessLogic initialized with local storage";

#ifdef ENABLE_GEM5
  if (!initializeBuffers()) {
    LOG(error) << "Failed to initialize buffers";
  }
#endif // ENABLE_GEM5
}

#ifdef ENABLE_GEM5
PostStorageBusinessLogic::~PostStorageBusinessLogic() {
    cleanupBuffers();
}

bool PostStorageBusinessLogic::initializeBuffers() {
    try {
        raw_recv_buf_ = new uint8_t[BUFFER_SIZE + ALIGNMENT];
        raw_resp_buf_ = new uint8_t[BUFFER_SIZE + ALIGNMENT];
        recv_buf_ = allocateAlignedBuffer(raw_recv_buf_);
        resp_buf_ = allocateAlignedBuffer(raw_resp_buf_);

        std::memset(recv_buf_, 0, BUFFER_SIZE);
        std::memset(resp_buf_, 0, BUFFER_SIZE);

        LOG(info) << "PostStorage Buffers initialized - recv: " << std::hex << reinterpret_cast<uintptr_t>(recv_buf_);
        LOG(info) << "PostStorage Buffers initialized - resp: " << std::hex << reinterpret_cast<uintptr_t>(resp_buf_);
        return true;
    } catch (const std::exception& e) {
        LOG(error) << "PostStorage Buffer initialization failed: " << e.what();
        cleanupBuffers();
        return false;
    }
}

void PostStorageBusinessLogic::cleanupBuffers() {
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

uint8_t* PostStorageBusinessLogic::allocateAlignedBuffer(uint8_t* raw_buf) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(raw_buf);
    uintptr_t aligned_addr = (addr + 0x3F) & ~0x3F;
    return reinterpret_cast<uint8_t*>(aligned_addr);
}

void PostStorageBusinessLogic::setTraceConfig(const std::string& file, int requests) {
    trace_file_ = file;
    num_requests_ = requests;

    // Initialize socket's replay with config
    auto socket = getSocketFromTransport();
    if (socket) {
        socket->getReplaySocket().loadTrace(trace_file_, num_requests_);
    }
}

apache::thrift::transport::TSocket* PostStorageBusinessLogic::getSocketFromTransport() {
   auto buffered = dynamic_cast<apache::thrift::transport::TBufferedTransport*>(in_->getTransport().get());
   return buffered ? dynamic_cast<apache::thrift::transport::TSocket*>(buffered->getUnderlyingTransport().get()) : nullptr;
}

void PostStorageBusinessLogic::callSWread() {
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

    // Read based on operation type
    if (processor_->getEventHandler().get() != NULL) {
        std::string service_method = "PostStorageService." + fname;
        processor_->getEventHandler()->preRead(ctx_, service_method.c_str());
    }

    if (fname == "StorePost") {
        store_args_.read(in_.get());
    } else if (fname == "ReadPost") {
        read_args_.read(in_.get());
    } else if (fname == "ReadPosts") {
        read_posts_args_.read(in_.get());
    }

    in_->readMessageEnd();
    uint32_t bytes = in_->getTransport()->readEnd();
    if (processor_->getEventHandler().get() != NULL) {
        std::string service_method = "PostStorageService." + fname;
        processor_->getEventHandler()->postRead(ctx_, service_method.c_str(), bytes);
    }

#ifdef ENABLE_TRACING
    if (fname == "StorePost") {
        LOG_RPC_TO_APP(store_args_);
    } else if (fname == "ReadPost") {
        LOG_RPC_TO_APP(read_args_);
    } else if (fname == "ReadPosts") {
        LOG_RPC_TO_APP(read_posts_args_);
    }
#endif
}

bool PostStorageBusinessLogic::callSWdispatch() {
    return processor_->dispatchCall(in_.get(), out_.get(), fname_, seqid_, connectionContext_);
}

void PostStorageBusinessLogic::callSWwrite() {
#ifdef ENABLE_TRACING
    if (fname_ == "StorePost") {
        LOG_APP_TO_RPC(store_args_.req_id, store_result_);
    } else if (fname_ == "ReadPost") {
        LOG_APP_TO_RPC(read_args_.req_id, read_result_);
    } else if (fname_ == "ReadPosts") {
        LOG_APP_TO_RPC(read_posts_args_.req_id, read_posts_result_);
    }
#endif

    // Write response using stored result
    if (processor_->getEventHandler().get() != NULL) {
        std::string service_method = "PostStorageService." + fname_;
        processor_->getEventHandler()->preWrite(ctx_, service_method.c_str());
    }

    out_->writeMessageBegin(fname_, ::apache::thrift::protocol::T_REPLY, seqid_);

    if (fname_ == "StorePost") {
        store_result_.write(out_.get());
    } else if (fname_ == "ReadPost") {
        read_result_.write(out_.get());
    } else if (fname_ == "ReadPosts") {
        read_posts_result_.write(out_.get());
    }

    out_->writeMessageEnd();
    uint32_t bytes = out_->getTransport()->writeEnd();
    out_->getTransport()->flush();
    if (processor_->getEventHandler().get() != NULL) {
        std::string service_method = "PostStorageService." + fname_;
        processor_->getEventHandler()->postWrite(ctx_, service_method.c_str(), bytes);
    }
}

void PostStorageBusinessLogic::callSWsendresp(bool success) {
    handler_->success_ = success;
}

void PostStorageBusinessLogic::callSWSendBuf() {
    if (handler_->operation_type_ == 0) {
        // StorePost - read success flag
        bool success = *reinterpret_cast<bool*>(resp_buf_);
        LOG_DEBUG(debug) << "StorePost completed in SW Path: " << success;
    } else if (handler_->operation_type_ == 1) {
        // ReadPost - read postid
        handler_->current_post_ = *reinterpret_cast<Post*>(resp_buf_);
        LOG_DEBUG(debug) << "ReadPost completed in SW Path, post_id: " << handler_->current_post_.post_id;
    } else if (handler_->operation_type_ == 2) {
        // Read posts from resp_buf_
        int32_t count = *reinterpret_cast<int32_t*>(resp_buf_ + resp_buf_offset_);
        handler_->current_posts_.clear();
        handler_->current_posts_.reserve(count);
        for (int i = 0; i < count; i++) {
          Post post = *reinterpret_cast<Post*>(resp_buf_ + i * sizeof(Post));
          handler_->current_posts_.push_back(post);
          LOG_DEBUG(debug) << "ReadPosts completed in SW Path, post size: " << handler_->current_posts_.size();
        }
    }
}
#endif // ENABLE_GEM5

#ifdef ENABLE_CEREBELLUM
void PostStorageBusinessLogic::callEngineRead() {
    auto socket = getSocketFromTransport();

    // 1) Send recv buffer address
    uint8_t* recv_addr = socket->getReplaySocket().getRecvBufferAddr();
    volatile uint64_t cmd = reinterpret_cast<uint64_t>(recv_addr) | cmd_send_dpdk_buf;
    *sendAddress = cmd;
    volatile uint64_t ack = *readAddress;

    // 2) Send total data size
    size_t total_size = socket->getReplaySocket().getCurrentPacketSize();
    uint64_t dpdk_len = ((uint64_t)total_size & 0x7FF) << 4;
    cmd = dpdk_len | cmd_send_dpdk_len;
    *sendAddress = cmd;
    ack = *readAddress;

    // 3) Advance recv buffer pointer to next position
    socket->getReplaySocket().advanceReadPos();
}

bool PostStorageBusinessLogic::callEngineDispatch() {
    // 1) Send app recv buffer address
    volatile uint64_t cmd = reinterpret_cast<uint64_t>(recv_buf_) | cmd_set_app_flag;
    *sendAddress = cmd;

    // 2) Wait for Engine to set the request
    volatile uint64_t request = *readAddress;
    int operation_type = (request & 0xF);

    // 3) Call appropriate BusinessLogic Function
    if (operation_type == 0) {
        StorePost();
    } else if (operation_type == 1) {
        ReadPost();
    } else if (operation_type == 2) {
        ReadPosts();
    }

    LOG_DEBUG(debug) << "PostStorage operation " << operation_type << " completed in Engine Path";
    return true;
}

void PostStorageBusinessLogic::callEngineWrite() {
    auto socket = getSocketFromTransport();

    // 1) Send DPDK resp buffer address
    uint8_t* resp_addr = socket->getReplaySocket().getRespBufferAddr();
    volatile uint64_t cmd = reinterpret_cast<uint64_t>(resp_addr) | cmd_set_dpdk_flag;
    *sendAddress = cmd;
    volatile uint64_t ack = *readAddress;

    // 2) Advance resp buffer pointer to next position
    uint16_t data_size;
    if (handler_->operation_type_ == 0) {
        data_size = 22; // fixed response size for StorePost
    } else if (handler_->operation_type_ == 1) {
        data_size = 371; // fixed response size for ReadPost
    } else if (handler_->operation_type_ == 2) {
        data_size = 1071; // fixed response size for ReadPosts
    } else {
        LOG(error) << "Unknown operation type: " << handler_->operation_type_;
        return;
    }
    socket->getReplaySocket().advanceWritePos(data_size);
}

void PostStorageBusinessLogic::callEngineSendresp(bool success) {
    uint64_t response = 0;
    size_t response_len = 0;
    
    if (handler_->operation_type_ == 0) {
      // StorePost returns nothing
    } else if (handler_->operation_type_ == 1) {
        response_len = resp_buf_size_; // ReadPost returns serialized Post
    } else if (handler_->operation_type_ == 2) {
        response_len = resp_buf_size_; // ReadPosts returns serialized collection of Posts
    }
    
    response |= (handler_->operation_type_ & 0xF) << 4; // operation type
    response |= (success ? 1ULL : 0ULL) << 8; // success flag
    response |= (response_len & 0x7FF) << 9; // length of response data
    
    uint64_t cmd = response | cmd_send_app_resp;
    *sendAddress = cmd;
    volatile uint64_t ack = *readAddress;
}

void PostStorageBusinessLogic::callEngineSendBuf() {
    // 1) Send response buffer to Engine
    uint64_t cmd = reinterpret_cast<uint64_t>(resp_buf_) | cmd_send_app_buf;
    *sendAddress = cmd;
    volatile uint64_t ack = *readAddress;
}
#endif // ENABLE_CEREBELLUM

#ifdef ENABLE_GEM5
void PostStorageBusinessLogic::runLoop(apache::thrift::TDispatchProcessor* processor,
            std::shared_ptr<::apache::thrift::protocol::TProtocol> in,
            std::shared_ptr<::apache::thrift::protocol::TProtocol> out,
            void* connectionContext)
{
    LOG(info) << "JU:JU =========================================";
    LOG(info) << "JU:JU Start PostStorage business logic runLoop";

    // Store protocol objects
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
            #ifdef ENABLE_GEM5
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

        if (!res)
            break;

        #ifdef ENABLE_GEM5
        done = checkReplayEOF();
        if (done) {
            LOG(info) << "JU:JU EOF reached - trace replay complete";
        }
        #endif

        runs++;
    }

    #ifdef ENABLE_GEM5
    m5_work_end_addr(0, 0);
    LOG(info) << "JU:JU End ROI";

    if (validateReplay()) {
        LOG(info) << "JU:JU PostStorage Replay validation PASSED";
    } else {
        LOG(info) << "JU:JU PostStorage Replay validation FAILED";
    }
    //m5_work_end_addr(0, 0);
    //LOG(info) << "JU:JU End ROI";
   #endif

    LOG(info) << "JU:JU Finished PostStorage business logic runLoop";
    LOG(info) << "JU:JU =========================================";
}
#ifdef ENABLE_CEREBELLUM
void PostStorageBusinessLogic::serializePostToResponse(const Post& post) {
  uint8_t* buf = resp_buf_;
  size_t offset = 0;
  // Set the response size to full cache lines
  const size_t CACHE_LINE_SIZE = 64;
  const size_t POST_SIZE = 288;
  const size_t CACHE_LINES_PER_POST = (POST_SIZE + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE; // 5
  
  // Write Post fields in same order as StorePost
  writeInt64ToBuffer(buf, offset, post.post_id);
  writeInt64ToBuffer(buf, offset, post.creator.user_id);
  writeStringToBuffer(buf, offset, post.creator.username);
  writeInt64ToBuffer(buf, offset, post.req_id);
  writeStringToBuffer(buf, offset, post.text);
  
  // Write user_mentions vector
  writeInt32ToBuffer(buf, offset, static_cast<int32_t>(post.user_mentions.size()));
  for (const auto& mention : post.user_mentions) {
      writeInt64ToBuffer(buf, offset, mention.user_id);
      writeStringToBuffer(buf, offset, mention.username);
  }
  
  // Write media vector
  writeInt32ToBuffer(buf, offset, static_cast<int32_t>(post.media.size()));
  for (const auto& media : post.media) {
      writeInt64ToBuffer(buf, offset, media.media_id);
      writeStringToBuffer(buf, offset, media.media_type);
  }
  
  // Write urls vector
  writeInt32ToBuffer(buf, offset, static_cast<int32_t>(post.urls.size()));
  for (const auto& url : post.urls) {
      writeStringToBuffer(buf, offset, url.shortened_url);
      writeStringToBuffer(buf, offset, url.expanded_url);
  }
  
  writeInt64ToBuffer(buf, offset, post.timestamp);
  writeInt32ToBuffer(buf, offset, post.post_type);
  
  // Zero-fill remaining padding bytes (320 - 288 = 32 bytes)
  memset(resp_buf_ + offset, 0, 32);
  
  // Set the response size
  resp_buf_size_ = CACHE_LINES_PER_POST * CACHE_LINE_SIZE; // 320 bytes
  LOG(debug) << "Serialized Post to response buffer, size: " << resp_buf_size_;
}

void PostStorageBusinessLogic::serializePostsToResponse(const std::vector<Post>& posts) {
  uint8_t* buf = resp_buf_;
  size_t offset = 0;
  const size_t CACHE_LINE_SIZE = 64;
  const size_t POST_SIZE = 288; // Serialized post size
  const size_t CACHE_LINES_PER_POST = (POST_SIZE + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE; // 5 cache lines
  
  for (const auto& post : posts) {
      // Serialize post at current offset
      serializePostAtOffset(buf, offset, post);
      
      // Move to next cache line aligned address
      offset = CACHE_LINES_PER_POST * CACHE_LINE_SIZE;
      if (&post != &posts.back()) { // Not the last post
          // Align to next cache line boundary for next post
          buf += CACHE_LINES_PER_POST * CACHE_LINE_SIZE;
          offset = 0;
      }
  }
  // Set the response size
  resp_buf_size_ = posts.size() * CACHE_LINES_PER_POST * CACHE_LINE_SIZE; // 960 bytes 
  LOG(debug) << "Serialized Post to response buffer, size: " << resp_buf_size_;
}

void PostStorageBusinessLogic::serializePostAtOffset(uint8_t* buf, size_t base_offset, const Post& post) {
  size_t offset = base_offset;
  
  // Serialize Post fields in same order as ReadPost
  writeInt64ToBuffer(buf, offset, post.post_id);
  writeInt64ToBuffer(buf, offset, post.creator.user_id);
  writeStringToBuffer(buf, offset, post.creator.username);
  writeInt64ToBuffer(buf, offset, post.req_id);
  writeStringToBuffer(buf, offset, post.text);
  
  // Write user_mentions vector
  writeInt32ToBuffer(buf, offset, static_cast<int32_t>(post.user_mentions.size()));
  for (const auto& mention : post.user_mentions) {
      writeInt64ToBuffer(buf, offset, mention.user_id);
      writeStringToBuffer(buf, offset, mention.username);
  }

  // Write media vector
  writeInt32ToBuffer(buf, offset, static_cast<int32_t>(post.media.size()));
  for (const auto& media : post.media) {
      writeInt64ToBuffer(buf, offset, media.media_id);
      writeStringToBuffer(buf, offset, media.media_type);
  }
  
  // Write urls vector
  writeInt32ToBuffer(buf, offset, static_cast<int32_t>(post.urls.size()));
  for (const auto& url : post.urls) {
      writeStringToBuffer(buf, offset, url.shortened_url);
      writeStringToBuffer(buf, offset, url.expanded_url);
  }
  
  writeInt64ToBuffer(buf, offset, post.timestamp);
  writeInt32ToBuffer(buf, offset, post.post_type);
  
  // Zero-fill remaining padding bytes
  size_t used_bytes = offset - base_offset; // 288 bytes
  size_t padding_bytes = 320 - used_bytes;  // 32 bytes
  memset(buf + offset, 0, padding_bytes);
}
#endif // ENABLE_CEREBELLUM
void PostStorageBusinessLogic::StorePost() {
    auto start_time = std::chrono::high_resolution_clock::now();

#ifdef ENABLE_CEREBELLUM
    // For Cerebellum, we deserialize Post object from recv_buf_
    uint8_t* buf = recv_buf_;
    size_t offset = 0;
    
    int64_t req_id = readInt64(buf, offset);
    handler_->operation_type_ = readInt32(buf, offset);
    
    // Deserialize Post
    Post post;
    post.post_id = readInt64(buf, offset);
    post.creator.user_id = readInt64(buf, offset);
    post.creator.username = readString(buf, offset);
    post.req_id = readInt64(buf, offset);
    post.text = readString(buf, offset);
    
    // Read user_mentions
    int32_t mentions_size = readInt32(buf, offset);
    post.user_mentions.clear();
    for (int i = 0; i < mentions_size; i++) {
        UserMention mention;
        mention.user_id = readInt64(buf, offset);
        mention.username = readString(buf, offset);
        post.user_mentions.push_back(mention);
    }
    
    // Read media
    int32_t media_size = readInt32(buf, offset);
    post.media.clear();
    for (int i = 0; i < media_size; i++) {
        Media media;
        media.media_id = readInt64(buf, offset);
        media.media_type = readString(buf, offset);
        post.media.push_back(media);
    }
    
    // Read urls
    int32_t urls_size = readInt32(buf, offset);
    post.urls.clear();
    for (int i = 0; i < urls_size; i++) {
        Url url;
        url.shortened_url = readString(buf, offset);
        url.expanded_url = readString(buf, offset);
        post.urls.push_back(url);
    }
    
    post.timestamp = readInt64(buf, offset);
    post.post_type = static_cast<PostType::type>(readInt32(buf, offset));
#else
    // For non-Cerebellum, we read Post object directly from recv_buf_
    uint8_t* buf = recv_buf_;
    int64_t req_id = *reinterpret_cast<int64_t*>(buf);
    int32_t operation_type = *reinterpret_cast<int32_t*>(buf + 8);
    Post post = *reinterpret_cast<Post*>(buf + 12);
#endif // ENABLE_CEREBELLUM
    bool operation_success = false;
    try {
        // Call original StorePost logic with MongoDB
        std::map<std::string, std::string> empty_carrier;
        StorePost(req_id, post, empty_carrier);
        operation_success = true;

        // Write success to resp_buf
        *reinterpret_cast<bool*>(resp_buf_) = true;

    } catch (const std::exception& e) {
        LOG(error) << "StorePost failed: " << e.what();
        *reinterpret_cast<bool*>(resp_buf_) = false;
    }

    LOG_DEBUG(debug) << "Request " << req_id << " stored post_id: " << post.post_id;

#ifdef ENABLE_CEREBELLUM
    callEngineSendresp(operation_success);
    callEngineSendBuf();
#else
    callSWsendresp(operation_success);
    callSWSendBuf();
#endif // ENABLE_CEREBELLUM
}

void PostStorageBusinessLogic::ReadPost() {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Read request from recv_buf
    uint8_t* buf = recv_buf_;
    size_t offset = 0;

    int64_t req_id = readInt64(buf, offset);
    handler_->operation_type_ = readInt32(buf, offset);
    int64_t post_id = readInt64(buf, offset);

    bool operation_success = false;
    try {
        // Call original ReadPost logic with MongoDB/Memcached
        Post retrieved_post;
        std::map<std::string, std::string> empty_carrier;
        ReadPost(retrieved_post, req_id, post_id, empty_carrier);

#ifdef ENABLE_CEREBELLUM
        // For Cerebellum, Serialize result to resp_buf instead of direct copy
        serializePostToResponse(retrieved_post);
#else
        // For non-Cerebellum, we can directly copy the Post object
        *reinterpret_cast<Post*>(resp_buf_) = retrieved_post;
#endif // ENABLE_CEREBELLUM
        operation_success = true;

    } catch (const std::exception& e) {
        LOG(error) << "ReadPost failed: " << e.what();
        *reinterpret_cast<int64_t*>(resp_buf_) = -1; // Error indicator
    }

    LOG_DEBUG(debug) << "Request " << req_id << " read post_id: " << post_id
                    << " success: " << operation_success;

#ifdef ENABLE_CEREBELLUM
    callEngineSendresp(operation_success);
    callEngineSendBuf();
#else
    callSWsendresp(operation_success);
    callSWSendBuf();
#endif // ENABLE_CEREBELLUM
}

void PostStorageBusinessLogic::ReadPosts() {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Read request from recv_buf
    uint8_t* buf = recv_buf_;
    size_t offset = 0;
    
    int64_t req_id = readInt64(buf, offset);
    handler_->operation_type_ = readInt32(buf, offset);
    int32_t post_count = readInt32(buf, offset);
    
    // Read post_ids array
    std::vector<int64_t> post_ids;
    for (int i = 0; i < post_count; i++) {
        post_ids.push_back(readInt64(buf, offset));
    }

    bool operation_success = false;
    try {
        // Call original ReadPosts logic with MongoDB/Memcached
        std::vector<Post> retrieved_posts;
        std::map<std::string, std::string> empty_carrier;
        ReadPosts(retrieved_posts, req_id, post_ids, empty_carrier);

#ifdef ENABLE_CEREBELLUM
        // For Cerebellum, Serialize results to resp_buf instead of direct copy
        serializePostsToResponse(retrieved_posts);
#else
        // For non-Cerebellum, we can directly copy the vector of Posts
        // Write count at the start of resp_buf leads to segmentation fault
        // *reinterpret_cast<int32_t*>(resp_buf_) = retrieved_posts.size();

        // Write each Post to resp_buf
        resp_buf_offset_ = 0;
        for (int i = 0; i < retrieved_posts.size(); i++) {
          *reinterpret_cast<Post*>(resp_buf_ + resp_buf_offset_) = retrieved_posts[i]; // segfault here if offset is not 0
          resp_buf_offset_ += sizeof(Post);
        }
        // Write count at the end
        *reinterpret_cast<int32_t*>(resp_buf_ + resp_buf_offset_) = retrieved_posts.size();
#endif // ENABLE_CEREBELLUM
        operation_success = true;

    } catch (const std::exception& e) {
        LOG(error) << "ReadPosts failed: " << e.what();
        *reinterpret_cast<int32_t*>(resp_buf_) = -1; // Error indicator
    }

    LOG_DEBUG(debug) << "Request " << req_id << " read " << post_count << " posts";

#ifdef ENABLE_CEREBELLUM
    callEngineSendresp(operation_success);
    callEngineSendBuf();
#else
    callSWsendresp(operation_success);
    callSWSendBuf();
#endif // ENABLE_CEREBELLUM
}
#endif // ENABLE_GEM5

// STEP 2: Replace your StorePost function with this:
void PostStorageBusinessLogic::StorePost(int64_t req_id, const Post& post,
                                         const std::map<std::string, std::string>& carrier) {
  auto start_time = std::chrono::high_resolution_clock::now();
  
  _store_requests++;

  // Store in local memory instead of MongoDB
  {
    std::unique_lock<std::shared_mutex> lock(posts_mutex);
    local_posts[post.post_id] = post;
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  _total_processing_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();

  LOG_DEBUG(debug) << "Stored post " << post.post_id << " for request " << req_id << " in local storage";
}

// STEP 3: Replace your ReadPost function with this:
void PostStorageBusinessLogic::ReadPost(Post& _return, int64_t req_id, int64_t post_id,
                                        const std::map<std::string, std::string>& carrier) {
  auto start_time = std::chrono::high_resolution_clock::now();
  
  _read_requests++;

  // Read from local memory instead of Memcached/MongoDB
  {
    std::shared_lock<std::shared_mutex> lock(posts_mutex);
    auto it = local_posts.find(post_id);
    if (it != local_posts.end()) {
      _return = it->second;
      _cache_hits++; // Treat as cache hit since it's local
    } else {
      _cache_misses++;
      LOG(warning) << "Post_id: " << post_id << " doesn't exist in local storage";
      ServiceException se;
      se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
      se.message = "Post_id: " + std::to_string(post_id) + " doesn't exist in local storage";
      throw se;
    }
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  _total_processing_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();

  LOG_DEBUG(debug) << "Read post " << post_id << " for request " << req_id << " from local storage";
}

// STEP 4: Replace your ReadPosts function with this:
void PostStorageBusinessLogic::ReadPosts(std::vector<Post>& _return, int64_t req_id,
                                         const std::vector<int64_t>& post_ids,
                                         const std::map<std::string, std::string>& carrier) {
  auto start_time = std::chrono::high_resolution_clock::now();
  
  _read_multi_requests++;

  if (post_ids.empty()) {
    return;
  }

  // Read multiple posts from local memory
  _return.clear();
  _return.reserve(post_ids.size());
  
  {
    std::shared_lock<std::shared_mutex> lock(posts_mutex);
    for (int64_t post_id : post_ids) {
      auto it = local_posts.find(post_id);
      if (it != local_posts.end()) {
        _return.push_back(it->second);
        _cache_hits++;
      } else {
        _cache_misses++;
        LOG(error) << "Post_id: " << post_id << " not found in local storage";
        ServiceException se;
        se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
        se.message = "Post_id: " + std::to_string(post_id) + " not found in local storage";
        throw se;
      }
    }
  }

  if (_return.size() != post_ids.size()) {
    LOG(error) << "Could not find all requested posts";
    ServiceException se;
    se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
    se.message = "Could not find all requested posts";
    throw se;
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  _total_processing_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();

  LOG_DEBUG(debug) << "Read " << _return.size() << " posts for request " << req_id << " from local storage";
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
