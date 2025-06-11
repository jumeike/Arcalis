#include "UniqueIdHandler.h"

namespace social_network {

UniqueIdHandler::UniqueIdHandler() {
  LOG(info) << "UniqueIdHandler initialized";
}

int64_t UniqueIdHandler::ComposeUniqueId(
    int64_t req_id, PostType::type post_type,
    const std::map<std::string, std::string>& carrier) {
  
  auto rpc_start_time = std::chrono::high_resolution_clock::now();
  
  // Process incoming RPC (header parsing, tracing setup)
  //auto span = ProcessIncomingRpc(req_id, post_type, carrier);
  
  auto business_start_time = std::chrono::high_resolution_clock::now();
  
  // Delegate to business logic
  int64_t post_id = 0;
  if (business_logic_ != nullptr) {
    post_id = business_logic_->ComposeUniqueId(req_id, post_type);
  } else {
    LOG(error) << "Business logic not set for request " << req_id;
    // Return error or throw exception
    post_id = -1;
  }
  
  auto business_end_time = std::chrono::high_resolution_clock::now();
  
  // Process outgoing RPC (response preparation, tracing completion)
  //ProcessOutgoingRpc(span);
  
  auto rpc_end_time = std::chrono::high_resolution_clock::now();
  
  // Update RPC layer metrics
  _rpc_requests_processed++;
  _total_rpc_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(rpc_end_time - rpc_start_time).count();
  
  // Log performance metrics periodically
  if (req_id != 0 && req_id % 1000 == 0) {
    std::map<std::string, int64_t> rpc_metrics, business_metrics;
    GetRpcMetrics(rpc_metrics);
    GetBusinessMetrics(business_metrics);
    
    LOG_DEBUG(debug) << "Performance metrics for request " << req_id << ":";
    LOG_DEBUG(debug) << "  RPC: " << rpc_metrics["avg_rpc_time_ns"] << "ns avg, "
              << rpc_metrics["requests_processed"] << " requests";
    LOG_DEBUG(debug) << "  Business: " << business_metrics["avg_processing_time_ns"] << "ns avg, "
              << business_metrics["avg_lock_contention_ns"] << "ns lock contention";
  }
  
  return post_id;
}

//std::shared_ptr<opentracing::Span> UniqueIdHandler::ProcessIncomingRpc(
void UniqueIdHandler::ProcessIncomingRpc(
    int64_t req_id, 
    PostType::type post_type,
    const std::map<std::string, std::string>& carrier) {
  
  auto tracing_start = std::chrono::high_resolution_clock::now();
  
  // Initialize tracing span
//  TextMapReader reader(carrier);
//  std::map<std::string, std::string> writer_text_map;
//  TextMapWriter writer(writer_text_map);
//  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
//  auto span = opentracing::Tracer::Global()->StartSpan(
//      "compose_unique_id_server", {opentracing::ChildOf(parent_span->get())});
//  opentracing::Tracer::Global()->Inject(span->context(), writer);
  
  auto tracing_end = std::chrono::high_resolution_clock::now();
  
  // Update tracing metrics
  _tracing_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(tracing_end - tracing_start).count();
  
  LOG_DEBUG(debug) << "Processed incoming RPC for request " << req_id;
  
  //return span;
}

//void UniqueIdHandler::ProcessOutgoingRpc(std::shared_ptr<opentracing::Span> span) {
void UniqueIdHandler::ProcessOutgoingRpc() {
  auto tracing_start = std::chrono::high_resolution_clock::now();
  
  // Complete the tracing span
//  span->Finish();
  
  auto tracing_end = std::chrono::high_resolution_clock::now();
  
  // Update tracing metrics
  _tracing_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(tracing_end - tracing_start).count();
  
  LOG_DEBUG(debug) << "Processed outgoing RPC"; }

void UniqueIdHandler::setBusinessLogic(UniqueIdBusinessLogic* logic) {
  business_logic_ = logic;
  LOG(info) << "Business logic set successfully";
}

UniqueIdBusinessLogic* UniqueIdHandler::getBusinessLogic() const {
  return business_logic_;
}

void UniqueIdHandler::GetRpcMetrics(std::map<std::string, int64_t>& metrics) const {
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

void UniqueIdHandler::GetBusinessMetrics(std::map<std::string, int64_t>& metrics) const {
  if (business_logic_ != nullptr) {
    business_logic_->GetMetrics(metrics);
  } else {
    LOG(warning) << "Cannot get business metrics: business logic not set";
  }
}

} // namespace social_network
