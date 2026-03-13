#ifndef SOCIAL_NETWORK_MICROSERVICES_SRC_URLSHORTENSERVICE_URLSHORTENHANDLER_H_
#define SOCIAL_NETWORK_MICROSERVICES_SRC_URLSHORTENSERVICE_URLSHORTENHANDLER_H_

#include <chrono>
#include <mutex>
#include <memory>
#include <map>
#include <atomic>

#include "../../../gen-cpp/UrlShortenService.h"
#include "../../../gen-cpp/social_network_types.h"
#include "../../logger.h"
#include "UrlShortenBusinessLogic.h"

namespace social_network {

#ifdef ENABLE_GEM5
class UrlShortenHandler : public social_network::UrlShortenServiceIf {
#else
class UrlShortenHandler : public UrlShortenServiceIf {
#endif
public:
  UrlShortenHandler();
  ~UrlShortenHandler() override = default;

  // Thrift service interface implementation
  void ComposeUrls(std::vector<Url>& _return,
                   int64_t req_id,
                   const std::vector<std::string>& urls,
                   const std::map<std::string, std::string>& carrier) override;

  void GetExtendedUrls(std::vector<std::string>& _return,
                       int64_t req_id,
                       const std::vector<std::string>& shortened_urls,
                       const std::map<std::string, std::string>& carrier) override;

  // Business logic management
  void setBusinessLogic(UrlShortenBusinessLogic* logic);
  UrlShortenBusinessLogic* getBusinessLogic() const;

  // Metrics and monitoring
  void GetRpcMetrics(std::map<std::string, int64_t>& metrics) const;
  void GetBusinessMetrics(std::map<std::string, int64_t>& metrics) const;

  int64_t req_id_;
  void setReqId(int64_t req_id) { req_id_ = req_id; }

#ifdef ENABLE_GEM5
  void setRecvBuffer(uint8_t* buf);
  bool isReadyForRequest() const { return ready_for_request_; }

  // Results for different operations
  std::vector<Url> current_target_urls_;
  std::vector<std::string> current_extended_urls_;
  bool success_;
  int operation_type_; // 0=ComposeUrls, 1=GetExtendedUrls
#endif // ENABLE_GEM5

  UrlShortenBusinessLogic* business_logic_{nullptr};

private:

#ifdef ENABLE_GEM5
  bool ready_for_request_{false};
  uint8_t* recv_buffer_;  // Points to business logic's buffer
#endif // ENABLE_GEM5

  // RPC layer metrics
  mutable std::mutex _metrics_mutex;
  std::atomic<uint64_t> _rpc_requests_processed{0};
  std::atomic<uint64_t> _total_rpc_time_ns{0};
  std::atomic<uint64_t> _header_processing_time_ns{0};
  std::atomic<uint64_t> _tracing_time_ns{0};

  // Helper functions for RPC processing
  void ProcessIncomingRpc(
      int64_t req_id,
      const std::map<std::string, std::string>& carrier);
  
  void ProcessOutgoingRpc();
};

} // namespace social_network

#endif //SOCIAL_NETWORK_MICROSERVICES_SRC_URLSHORTENSERVICE_URLSHORTENHANDLER_H_
