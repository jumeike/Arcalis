#include "PostStorageBusinessLogic.h"
#include "PostStorageHandler.h"
#include <chrono>
#include <cstring>
#include <future>

namespace social_network {

PostStorageBusinessLogic::PostStorageBusinessLogic(
    memcached_pool_st* memcached_pool, mongoc_client_pool_t* mongodb_pool)
    : _memcached_client_pool(memcached_pool), _mongodb_client_pool(mongodb_pool) {
  LOG_DEBUG(info) << "PostStorageBusinessLogic initialized";

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
    LOG(debug) << "Memcached error, falling back to MongoDB";
    // ServiceException se;
    // se.errorCode = ErrorCode::SE_MEMCACHED_ERROR;
    // se.message = "Error in ReadPost !post_mmc && memcached_rc != MEMCACHED_NOTFOUND";
    // se.message = memcached_strerror(memcached_client, memcached_rc);
    // memcached_pool_push(_memcached_client_pool, memcached_client);
    // throw se;
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
    se.message = "Error in ReadPosts -> memcached_rc != MEMCACHED_SUCCESS";
    //se.message = memcached_strerror(memcached_client, memcached_rc);
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
    std::vector<int64_t> missing_ids;
    for (auto& post_id : post_ids) {
        if (return_map.find(post_id) == return_map.end()) {
            missing_ids.push_back(post_id);
        }
    }
    
    LOG(error) << "Missing post IDs: ";
    for (auto& id : missing_ids) {
        LOG(error) << "  " << id;
    }

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
    LOG(debug) << "Failed to set post to Memcached: "
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
