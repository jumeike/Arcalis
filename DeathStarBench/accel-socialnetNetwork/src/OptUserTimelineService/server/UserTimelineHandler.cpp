#include "UserTimelineHandler.h"

namespace social_network {

UserTimelineHandler::UserTimelineHandler() {
  LOG(info) << "UserTimelineHandler initialized";
}

#ifdef ENABLE_GEM5
void UserTimelineHandler::setRecvBuffer(uint8_t* buf) {
  recv_buffer_ = buf;
  ready_for_request_ = true;
  if (business_logic_ != nullptr) {
    business_logic_->setHandler(this);
  }
  LOG(debug) << "UserTimelineHandler receive buffer set to: "
             << std::hex << reinterpret_cast<uintptr_t>(buf);
}
#endif // ENABLE_GEM5

void UserTimelineHandler::WriteUserTimeline(
    int64_t req_id, int64_t post_id, int64_t user_id, int64_t timestamp,
    const std::map<std::string, std::string>& carrier) {
  auto rpc_start_time = std::chrono::high_resolution_clock::now();
  
  // Process incoming RPC (header parsing, tracing setup)
  ProcessIncomingRpc(req_id, carrier);
  
  // Delegate to business logic
  if (business_logic_ != nullptr) {
#ifdef ENABLE_GEM5
    operation_type_ = 0;
    success_ = false;

    uint8_t* buf = business_logic_->getRecvBuffer();
    *reinterpret_cast<int64_t*>(buf + 0) = req_id;
    *reinterpret_cast<int32_t*>(buf + 8) = 0;
    *reinterpret_cast<int64_t*>(buf + 12) = post_id;
    *reinterpret_cast<int64_t*>(buf + 20) = user_id;
    *reinterpret_cast<int64_t*>(buf + 28) = timestamp;

    business_logic_->WriteUserTimeline();
#else
    business_logic_->WriteUserTimeline(req_id, post_id, user_id, timestamp, carrier);
#endif
  } else {
    LOG(error) << "Business logic not set for WriteUserTimeline request " << req_id;
    ServiceException se;
    se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
    se.message = "Business logic not initialized";
    throw se;
  }
  
  // Process outgoing RPC (response preparation, tracing completion)
  ProcessOutgoingRpc();
  
  auto rpc_end_time = std::chrono::high_resolution_clock::now();
  
  // Update RPC layer metrics
  _rpc_requests_processed++;
  _total_rpc_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(rpc_end_time - rpc_start_time).count();
  
  // Log performance metrics periodically
  if (req_id % 1000 == 0) {
    std::map<std::string, int64_t> rpc_metrics, business_metrics;
    GetRpcMetrics(rpc_metrics);
    GetBusinessMetrics(business_metrics);
    
    LOG(debug) << "WriteUserTimeline metrics for request " << req_id << ":";
    LOG(debug) << "  RPC: " << rpc_metrics["avg_rpc_time_ns"] << "ns avg, "
              << rpc_metrics["requests_processed"] << " requests";
    LOG(debug) << "  Business: " << business_metrics["avg_processing_time_ns"] << "ns avg, "
              << business_metrics["write_requests"] << " write requests";
  }
}

void UserTimelineHandler::ReadUserTimeline(
    std::vector<Post>& _return, int64_t req_id, int64_t user_id, 
    int start, int stop, const std::map<std::string, std::string>& carrier) {
  auto rpc_start_time = std::chrono::high_resolution_clock::now();

  // Process incoming RPC (header parsing, tracing setup)
  ProcessIncomingRpc(req_id, carrier);

  // Delegate to business logic
  if (business_logic_ != nullptr) {
#ifdef ENABLE_GEM5
    operation_type_ = 1;
    success_ = false;
    current_posts_.clear();

    uint8_t* buf = business_logic_->getRecvBuffer();
    *reinterpret_cast<int64_t*>(buf + 0) = req_id;
    *reinterpret_cast<int32_t*>(buf + 8) = 1;
    *reinterpret_cast<int64_t*>(buf + 12) = user_id;
    *reinterpret_cast<int32_t*>(buf + 20) = start;
    *reinterpret_cast<int32_t*>(buf + 24) = stop;

    business_logic_->ReadUserTimeline();
#else
    business_logic_->ReadUserTimeline(_return, req_id, user_id, start, stop, carrier);
#endif
  } else {
    LOG(error) << "Business logic not set for ReadUserTimeline request " << req_id;
    ServiceException se;
    se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
    se.message = "Business logic not initialized";
    throw se;
  }

#ifdef ENABLE_GEM5
  if (success_) {
    _return = current_posts_;
  }
#endif

  // Process outgoing RPC (response preparation, tracing completion)
  ProcessOutgoingRpc();

  auto rpc_end_time = std::chrono::high_resolution_clock::now();

  // Update RPC layer metrics
  _rpc_requests_processed++;
  _total_rpc_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(rpc_end_time - rpc_start_time).count();

  // Log performance metrics periodically
  if (req_id != 0 && req_id % 1000 == 0) {
    std::map<std::string, int64_t> rpc_metrics, business_metrics;
    GetRpcMetrics(rpc_metrics);
    GetBusinessMetrics(business_metrics);

    LOG(debug) << "ReadUserTimeline metrics for request " << req_id << ":";
    LOG(debug) << "  RPC: " << rpc_metrics["avg_rpc_time_ns"] << "ns avg, "
              << rpc_metrics["requests_processed"] << " requests";
    LOG(debug) << "  Business: " << business_metrics["avg_processing_time_ns"] << "ns avg, "
              << business_metrics["cache_hit_rate_percent"] << "% cache hit rate, "
              << (stop - start) << " posts requested";
  }
}

void UserTimelineHandler::ProcessIncomingRpc(int64_t req_id, 
                                            const std::map<std::string, std::string>& carrier) {
  auto tracing_start = std::chrono::high_resolution_clock::now();
  
  // Note: Tracing code removed for simplicity - add back if needed
  // For now, just simulate header processing
  
  auto tracing_end = std::chrono::high_resolution_clock::now();
  
  // Update tracing metrics
  _tracing_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(tracing_end - tracing_start).count();
  
  LOG_DEBUG(debug) << "Processed incoming RPC for request " << req_id;
}

void UserTimelineHandler::ProcessOutgoingRpc() {
  auto tracing_start = std::chrono::high_resolution_clock::now();
  
  // Note: Response processing would go here
  
  auto tracing_end = std::chrono::high_resolution_clock::now();
  
  // Update tracing metrics
  _tracing_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(tracing_end - tracing_start).count();
  
  LOG_DEBUG(debug) << "Processed outgoing RPC";
}

void UserTimelineHandler::setBusinessLogic(UserTimelineBusinessLogic* logic) {
  business_logic_ = logic;
  LOG(info) << "UserTimeline business logic set successfully";
}

UserTimelineBusinessLogic* UserTimelineHandler::getBusinessLogic() const {
  return business_logic_;
}

void UserTimelineHandler::GetRpcMetrics(std::map<std::string, int64_t>& metrics) const {
  std::lock_guard<std::mutex> lock(_metrics_mutex);
  
  metrics["requests_processed"] = _rpc_requests_processed.load();
  metrics["total_rpc_time_ns"] = _total_rpc_time_ns.load();
  metrics["header_processing_time_ns"] = _header_processing_time_ns.load();
  metrics["tracing_time_ns"] = _tracing_time_ns.load();
  
  uint64_t requests = _rpc_requests_processed.load();
  if (requests > 0) {
    metrics["avg_rpc_time_ns"] = _total_rpc_time_ns.load() / requests;
    metrics["avg_header_time_ns"] = _header_processing_time_ns.load() / requests;
    metrics["avg_tracing_time_ns"] = _tracing_time_ns.load() / requests;
  } else {
    metrics["avg_rpc_time_ns"] = 0;
    metrics["avg_header_time_ns"] = 0;
    metrics["avg_tracing_time_ns"] = 0;
  }
}

void UserTimelineHandler::GetBusinessMetrics(std::map<std::string, int64_t>& metrics) const {
  if (business_logic_ != nullptr) {
    business_logic_->GetMetrics(metrics);
  } else {
    LOG(warning) << "Cannot get business metrics: business logic not set";
  }
}

} // namespace social_network
