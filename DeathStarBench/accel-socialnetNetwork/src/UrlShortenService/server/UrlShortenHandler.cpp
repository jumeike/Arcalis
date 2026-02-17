#include "UrlShortenHandler.h"

namespace social_network {

UrlShortenHandler::UrlShortenHandler() {
  LOG(info) << "UrlShortenHandler initialized";
}

void UrlShortenHandler::ComposeUrls(
    std::vector<Url>& _return,
    int64_t req_id,
    const std::vector<std::string>& urls,
    const std::map<std::string, std::string>& carrier) {
  
  auto rpc_start_time = std::chrono::high_resolution_clock::now();
  
  // Process incoming RPC (header parsing, tracing setup)
  ProcessIncomingRpc(req_id, carrier);
  
  auto business_start_time = std::chrono::high_resolution_clock::now();
  
  // Delegate to business logic
  if (business_logic_ != nullptr) {
    business_logic_->ComposeUrls(_return, req_id, urls);
  } else {
    LOG(error) << "Business logic not set for request " << req_id;
    ServiceException se;
    se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
    se.message = "Business logic not initialized";
    throw se;
  }
  
  auto business_end_time = std::chrono::high_resolution_clock::now();
  
  // Process outgoing RPC (response preparation, tracing completion)
  ProcessOutgoingRpc();
  
  auto rpc_end_time = std::chrono::high_resolution_clock::now();
  
  // Update RPC layer metrics
  _rpc_requests_processed++;
  _total_rpc_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
      rpc_end_time - rpc_start_time).count();
  
  // Log performance metrics periodically
  if (req_id != 0 && req_id % 1000 == 0) {
    std::map<std::string, int64_t> rpc_metrics, business_metrics;
    GetRpcMetrics(rpc_metrics);
    GetBusinessMetrics(business_metrics);
    
    LOG_DEBUG(debug) << "Performance metrics for request " << req_id << ":";
    LOG_DEBUG(debug) << "  RPC: " << rpc_metrics["avg_rpc_time_ns"] << "ns avg, "
                     << rpc_metrics["requests_processed"] << " requests";
    LOG_DEBUG(debug) << "  Business: " << business_metrics["avg_processing_time_ns"] << "ns avg";
  }
}

void UrlShortenHandler::GetExtendedUrls(
    std::vector<std::string>& _return,
    int64_t req_id,
    const std::vector<std::string>& shortened_urls,
    const std::map<std::string, std::string>& carrier) {
  
  auto rpc_start_time = std::chrono::high_resolution_clock::now();
  
  // Process incoming RPC (header parsing, tracing setup)
  ProcessIncomingRpc(req_id, carrier);
  
  auto business_start_time = std::chrono::high_resolution_clock::now();
  
  // Delegate to business logic
  if (business_logic_ != nullptr) {
    business_logic_->GetExtendedUrls(_return, req_id, shortened_urls);
  } else {
    LOG(error) << "Business logic not set for request " << req_id;
    ServiceException se;
    se.errorCode = ErrorCode::SE_THRIFT_HANDLER_ERROR;
    se.message = "Business logic not initialized";
    throw se;
  }
  
  auto business_end_time = std::chrono::high_resolution_clock::now();
  
  // Process outgoing RPC (response preparation, tracing completion)
  ProcessOutgoingRpc();
  
  auto rpc_end_time = std::chrono::high_resolution_clock::now();
  
  // Update RPC layer metrics
  _rpc_requests_processed++;
  _total_rpc_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
      rpc_end_time - rpc_start_time).count();
  
  // Log performance metrics periodically
  if (req_id != 0 && req_id % 1000 == 0) {
    std::map<std::string, int64_t> rpc_metrics, business_metrics;
    GetRpcMetrics(rpc_metrics);
    GetBusinessMetrics(business_metrics);
    
    LOG_DEBUG(debug) << "Performance metrics for request " << req_id << ":";
    LOG_DEBUG(debug) << "  RPC: " << rpc_metrics["avg_rpc_time_ns"] << "ns avg, "
                     << rpc_metrics["requests_processed"] << " requests";
    LOG_DEBUG(debug) << "  Business: " << business_metrics["avg_processing_time_ns"] << "ns avg";
  }
}

void UrlShortenHandler::ProcessIncomingRpc(
    int64_t req_id,
    const std::map<std::string, std::string>& carrier) {
  
  auto tracing_start = std::chrono::high_resolution_clock::now();
  
  // Initialize tracing span (currently disabled)
  // TextMapReader reader(carrier);
  // std::map<std::string, std::string> writer_text_map;
  // TextMapWriter writer(writer_text_map);
  // auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  // auto span = opentracing::Tracer::Global()->StartSpan(
  //     "url_shorten_server", {opentracing::ChildOf(parent_span->get())});
  // opentracing::Tracer::Global()->Inject(span->context(), writer);
  
  auto tracing_end = std::chrono::high_resolution_clock::now();
  
  // Update tracing metrics
  _tracing_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
      tracing_end - tracing_start).count();
  
  LOG_DEBUG(debug) << "Processed incoming RPC for request " << req_id;
}

void UrlShortenHandler::ProcessOutgoingRpc() {
  auto tracing_start = std::chrono::high_resolution_clock::now();
  
  // Complete the tracing span (currently disabled)
  // span->Finish();
  
  auto tracing_end = std::chrono::high_resolution_clock::now();
  
  // Update tracing metrics
  _tracing_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
      tracing_end - tracing_start).count();
  
  LOG_DEBUG(debug) << "Processed outgoing RPC";
}

void UrlShortenHandler::setBusinessLogic(UrlShortenBusinessLogic* logic) {
  business_logic_ = logic;
  LOG(info) << "Business logic set successfully";
}

UrlShortenBusinessLogic* UrlShortenHandler::getBusinessLogic() const {
  return business_logic_;
}

void UrlShortenHandler::GetRpcMetrics(std::map<std::string, int64_t>& metrics) const {
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

void UrlShortenHandler::GetBusinessMetrics(std::map<std::string, int64_t>& metrics) const {
  if (business_logic_ != nullptr) {
    business_logic_->GetMetrics(metrics);
  } else {
    LOG(warning) << "Cannot get business metrics: business logic not set";
  }
}

} // namespace social_network
