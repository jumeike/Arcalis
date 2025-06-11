#include "UniqueIdBusinessLogic.h"
#include "UniqueIdHandler.h"
#include <fstream>
#include <unistd.h>

namespace social_network {

// Static member initialization
int64_t UniqueIdBusinessLogic::current_timestamp = -1;
int UniqueIdBusinessLogic::counter = 0;

UniqueIdBusinessLogic::UniqueIdBusinessLogic(const std::string& machine_id)
    : _machine_id(machine_id) {
  LOG(info) << "UniqueIdBusinessLogic initialized with machine_id: " << machine_id;

#ifdef ENABLE_GEM5
  if (!initializeBuffers()) {
    LOG(error) << "Failed to initialize buffers";
  }
#endif // ENABLE_GEM5
}

#ifdef ENABLE_GEM5
UniqueIdBusinessLogic::~UniqueIdBusinessLogic() {
    cleanupBuffers();
}

bool UniqueIdBusinessLogic::initializeBuffers() {
    try {
        raw_recv_buf_ = new uint8_t[BUFFER_SIZE + ALIGNMENT];
        raw_resp_buf_ = new uint8_t[BUFFER_SIZE + ALIGNMENT];
        recv_buf_ = allocateAlignedBuffer(raw_recv_buf_);
        resp_buf_ = allocateAlignedBuffer(raw_resp_buf_);

        std::memset(recv_buf_, 0, BUFFER_SIZE);
        std::memset(resp_buf_, 0, BUFFER_SIZE);

        LOG(info) << "Buffers initialized - recv: " << std::hex << reinterpret_cast<uintptr_t>(recv_buf_);
        LOG(info) << "Buffers initialized - resp: " << std::hex << reinterpret_cast<uintptr_t>(resp_buf_);
        return true;
    } catch (const std::exception& e) {
        LOG(error) << "Buffer initialization failed: " << e.what();
        cleanupBuffers();
        return false;
    }
}

void UniqueIdBusinessLogic::cleanupBuffers() {
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

uint8_t* UniqueIdBusinessLogic::allocateAlignedBuffer(uint8_t* raw_buf) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(raw_buf);
    uintptr_t aligned_addr = (addr + 0x3F) & ~0x3F;
    //printf("Original address: 0x%lx\n", addr);
    //printf("Aligned address: 0x%lx\n", aligned_addr);
    //printf("===============================\n");
    return reinterpret_cast<uint8_t*>(aligned_addr);
}

void UniqueIdBusinessLogic::setTraceConfig(const std::string& file, int requests) {
    trace_file_ = file;
    num_requests_ = requests;

    // Initialize socket's replay with config
    auto socket = getSocketFromTransport();
    if (socket) {
        socket->getReplaySocket().loadTrace(trace_file_, num_requests_);
    }
}

apache::thrift::transport::TSocket* UniqueIdBusinessLogic::getSocketFromTransport() {
   auto buffered = dynamic_cast<apache::thrift::transport::TBufferedTransport*>(in_->getTransport().get());
   return buffered ? dynamic_cast<apache::thrift::transport::TSocket*>(buffered->getUnderlyingTransport().get()) : nullptr;
}

void UniqueIdBusinessLogic::callSWread() {
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

    // Moved from process_ComposeUniqueId
    if (processor_->getEventHandler().get() != NULL) {
        processor_->getEventHandler()->preRead(ctx_, "UniqueIdService.ComposeUniqueId");
    }
    UniqueIdService_ComposeUniqueId_args args;
    args.read(in_.get());
    in_->readMessageEnd();
    uint32_t bytes = in_->getTransport()->readEnd();
    if (processor_->getEventHandler().get() != NULL) {
        processor_->getEventHandler()->postRead(ctx_, "UniqueIdService.ComposeUniqueId", bytes);
    }
    args_ = args;  // Store for dispatch
#ifdef ENABLE_TRACING    
    LOG_RPC_TO_APP(args);
#endif
}

bool UniqueIdBusinessLogic::callSWdispatch() {
    return processor_->dispatchCall(in_.get(), out_.get(), fname_, seqid_, connectionContext_);
}

void UniqueIdBusinessLogic::callSWwrite() {
#ifdef ENABLE_TRACING    
    LOG_APP_TO_RPC(args_.req_id, result_);
#endif
    // Write response using stored result
    if (processor_->getEventHandler().get() != NULL) {
        processor_->getEventHandler()->preWrite(ctx_, "UniqueIdService.ComposeUniqueId");
    }
    out_->writeMessageBegin("ComposeUniqueId", ::apache::thrift::protocol::T_REPLY, seqid_);
    result_.write(out_.get());
    out_->writeMessageEnd();
    uint32_t bytes = out_->getTransport()->writeEnd();
    out_->getTransport()->flush();
    if (processor_->getEventHandler().get() != NULL) {
        processor_->getEventHandler()->postWrite(ctx_, "UniqueIdService.ComposeUniqueId", bytes);
    }
}

void UniqueIdBusinessLogic::callSWsendresp(bool success) {
  handler_->success_ = success;
}

void UniqueIdBusinessLogic::callSWSendBuf() {
  handler_->current_post_id_ = *reinterpret_cast<int64_t*>(resp_buf_);
  LOG_DEBUG(debug) << "Generated Post ID in SW Path: " << handler_->current_post_id_;
}
#endif // ENABLE_GEM5

#ifdef ENABLE_CEREBELLUM
void UniqueIdBusinessLogic::callEngineRead() {
    auto socket = getSocketFromTransport(); //auto == apache::thrift::transport::TSocket* socket
    
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

bool UniqueIdBusinessLogic::callEngineDispatch() {
    // 1) Send app recv buffer address
    volatile uint64_t cmd = reinterpret_cast<uint64_t>(recv_buf_) | cmd_set_app_flag;
    *sendAddress = cmd;
    
    // 2) Wait for Engine to set the request
    volatile uint64_t request = *readAddress;
    PostType::type post_type = static_cast<PostType::type>((request) & 0xF);

    // 3) Call BusinessLogic Function
    int64_t post_id = ComposeUniqueId();
    LOG_DEBUG(debug) << "Post ID generated in Engine Path: " << post_id;
    return true;
}

void UniqueIdBusinessLogic::callEngineWrite() {
    auto socket = getSocketFromTransport();
    
    // 1) Send DPDK resp buffer address
    uint8_t* resp_addr = socket->getReplaySocket().getRespBufferAddr();
    volatile uint64_t cmd = reinterpret_cast<uint64_t>(resp_addr) | cmd_set_dpdk_flag;
    *sendAddress = cmd;
    volatile uint64_t ack = *readAddress;
    
    // 2) Advance resp buffer pointer to next position
    socket->getReplaySocket().advanceWritePos(39);
}


void UniqueIdBusinessLogic::callEngineSendresp(bool success) {
    //1) Send response success flag to Engine
    uint64_t response = 0;
    size_t uniqueid_len = sizeof(int64_t);
    response |= (success ? 1ULL : 0ULL) << 4;  // success flag
    response |= (uniqueid_len & 0x7FF) << 5; // length of unique ID (post_id)
    
    uint64_t cmd = response | cmd_send_app_resp;
    *sendAddress = cmd;
    volatile uint64_t ack = *readAddress;
}

void UniqueIdBusinessLogic::callEngineSendBuf() {
    //1) Send response buffer to Engine
    uint64_t cmd = reinterpret_cast<uint64_t>(resp_buf_) | cmd_send_app_buf;
    *sendAddress = cmd;
    volatile uint64_t ack = *readAddress;
}
#endif // ENABLE_CEREBELLUM

#ifdef ENABLE_GEM5
void UniqueIdBusinessLogic::runLoop(apache::thrift::TDispatchProcessor* processor,
            std::shared_ptr<::apache::thrift::protocol::TProtocol> in,
            std::shared_ptr<::apache::thrift::protocol::TProtocol> out,
            void* connectionContext)
{
    LOG(info) << "JU:JU =========================================";
    LOG(info) << "JU:JU Start UniqueId business logic runLoop";

    // Store protocol objects
    processor_ = processor;
    in_ = in;
    out_ = out;
    connectionContext_ = connectionContext;
    read_pos_ = 0;
    write_pos_ = 0;

    int runs = 0;

    //printf("JU:JU Initial Begin ROI\n");

    for (bool done = false; !done;) {
        if (runs == 10000) {
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

    #ifdef ENABLE_GEM5_TEST
    m5_work_end_addr(0, 0);
    LOG(info) << "JU:JU End ROI";
    #endif
    
    #ifdef ENABLE_GEM5
    if (validateReplay()) {
        LOG(info) << "JU:JU Replay validation PASSED";
    } else {
        LOG(info) << "JU:JU Replay validation FAILED";
    }
    #endif
    
    LOG(info) << "JU:JU Finished UniqueId business logic runLoop";
    LOG(info) << "JU:JU =========================================";

}

int64_t UniqueIdBusinessLogic::ComposeUniqueId() {
    auto start_time = std::chrono::high_resolution_clock::now();
    auto lock_start = std::chrono::high_resolution_clock::now();
    
    // Read req_id from recv_buf
    int64_t req_id = *reinterpret_cast<int64_t*>(recv_buf_);

    // Critical section for timestamp and counter management
    _thread_lock.lock();
    auto lock_end = std::chrono::high_resolution_clock::now();
    
    int64_t timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch())
                            .count() - CUSTOM_EPOCH;
    int idx = GetCounter(timestamp);
    
    _thread_lock.unlock();

    // Format timestamp to hex (10 characters)
    std::string timestamp_hex = FormatHexString(timestamp, 10);
    
    // Format counter to hex (3 characters)
    std::string counter_hex = FormatHexString(idx, 3);
    
    // Compose the final ID
    std::string post_id_str = _machine_id + timestamp_hex + counter_hex;
    int64_t post_id = stoul(post_id_str, nullptr, 16) & 0x7FFFFFFFFFFFFFFF;

    // Write post_id to resp_buf
    *reinterpret_cast<int64_t*>(resp_buf_) = post_id;
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    // Update metrics
    _requests_processed++;
    _total_processing_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
    _lock_contention_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(lock_end - lock_start).count();
    
    LOG_DEBUG(debug) << "Request " << req_id << " generated post_id: " << post_id;

    bool success = (post_id != -1);
#ifdef ENABLE_CEREBELLUM
    callEngineSendresp(success);
    callEngineSendBuf();
#else
    callSWsendresp(success);
    callSWSendBuf();
#endif // ENABLE_CEREBELLUM
  
  return post_id;
}
#else
int64_t UniqueIdBusinessLogic::ComposeUniqueId(int64_t req_id, PostType::type post_type) {
  auto start_time = std::chrono::high_resolution_clock::now();
  auto lock_start = std::chrono::high_resolution_clock::now();
  
  // Critical section for timestamp and counter management
  _thread_lock.lock();
  auto lock_end = std::chrono::high_resolution_clock::now();
  
  int64_t timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch())
                          .count() - CUSTOM_EPOCH;
  int idx = GetCounter(timestamp);
  
  _thread_lock.unlock();

  // Format timestamp to hex (10 characters)
  std::string timestamp_hex = FormatHexString(timestamp, 10);
  
  // Format counter to hex (3 characters)
  std::string counter_hex = FormatHexString(idx, 3);
  
  // Compose the final ID
  std::string post_id_str = _machine_id + timestamp_hex + counter_hex;
  int64_t post_id = stoul(post_id_str, nullptr, 16) & 0x7FFFFFFFFFFFFFFF;
  
  auto end_time = std::chrono::high_resolution_clock::now();
  
  // Update metrics
  _requests_processed++;
  _total_processing_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
  _lock_contention_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(lock_end - lock_start).count();
  
  LOG_DEBUG(debug) << "Request " << req_id << " generated post_id: " << post_id;
  
  return post_id;
}
#endif // ENABLE_GEM5

int UniqueIdBusinessLogic::GetCounter(int64_t timestamp) {
  int64_t current_ts = current_timestamp;
  
  if (current_ts > timestamp) {
    LOG(fatal) << "Timestamps are not incremental. Current: " << current_ts 
               << ", New: " << timestamp;
    exit(EXIT_FAILURE);
  }
  
  if (current_timestamp == timestamp) {
    return counter++;
  } else {
    // Update timestamp and reset counter
    current_timestamp = timestamp;
    counter = 0;
    return counter++;
  }  
}

std::string UniqueIdBusinessLogic::FormatHexString(int64_t value, size_t width) {
  std::stringstream sstream;
  sstream << std::hex << value;
  std::string hex_str(sstream.str());
  
  if (hex_str.size() > width) {
    hex_str.erase(0, hex_str.size() - width);
  } else if (hex_str.size() < width) {
    hex_str = std::string(width - hex_str.size(), '0') + hex_str;
  }
  
  return hex_str;
}

void UniqueIdBusinessLogic::GetMetrics(std::map<std::string, int64_t>& metrics) const {
  metrics["requests_processed"] = _requests_processed.load();
  metrics["total_processing_time_ns"] = _total_processing_time_ns.load();
  metrics["lock_contention_time_ns"] = _lock_contention_time_ns.load();
  
  uint64_t requests = _requests_processed.load();
  if (requests > 0) {
    metrics["avg_processing_time_ns"] = _total_processing_time_ns.load() / requests;
    metrics["avg_lock_contention_ns"] = _lock_contention_time_ns.load() / requests;
  } else {
    metrics["avg_processing_time_ns"] = 0;
    metrics["avg_lock_contention_ns"] = 0;
  }
}

void UniqueIdBusinessLogic::ResetMetrics() {
  _requests_processed.store(0);
  _total_processing_time_ns.store(0);
  _lock_contention_time_ns.store(0);
}

// Utility functions implementation
u_int16_t HashMacAddressPid(const std::string& mac) {
  u_int16_t hash = 0;
  std::string mac_pid = mac + std::to_string(getpid());
  for (unsigned int i = 0; i < mac_pid.size(); i++) {
    hash += (mac[i] << ((i & 1) * 8));
  }
  return hash;
}

std::string GetMachineId(std::string& netif) {
  std::string mac_hash;

  std::string mac_addr_filename = "/sys/class/net/" + netif + "/address";
  std::ifstream mac_addr_file;
  mac_addr_file.open(mac_addr_filename);
  if (!mac_addr_file) {
    LOG(fatal) << "Cannot read MAC address from net interface " << netif;
    return "";
  }
  std::string mac;
  mac_addr_file >> mac;
  if (mac == "") {
    LOG(fatal) << "Cannot read MAC address from net interface " << netif;
    return "";
  }
  mac_addr_file.close();

  LOG(info) << "MAC address = " << mac;

  std::stringstream stream;
  stream << std::hex << HashMacAddressPid(mac);
  mac_hash = stream.str();

  if (mac_hash.size() > 3) {
    mac_hash.erase(0, mac_hash.size() - 3);
  } else if (mac_hash.size() < 3) {
    mac_hash = std::string(3 - mac_hash.size(), '0') + mac_hash;
  }
  return mac_hash;
}

} // namespace social_network
