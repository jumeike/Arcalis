#ifndef SOCIAL_NETWORK_MICROSERVICES_UNIQUEIDHANDLER_H
#define SOCIAL_NETWORK_MICROSERVICES_UNIQUEIDHANDLER_H

#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <memory>
#include <map>

#include "../../../gen-cpp/UniqueIdService.h"
#include "../../../gen-cpp/social_network_types.h"
#include "../../logger.h"
//#include "../../tracing.h"
#include "UniqueIdBusinessLogic.h"

namespace social_network {
#ifdef ENABLE_GEM5
class UniqueIdHandler : public social_network::UniqueIdServiceIf {
#else
class UniqueIdHandler : public UniqueIdServiceIf {
#endif
 public:
  ~UniqueIdHandler() override = default;
  UniqueIdHandler();

  // Thrift service interface implementation
  int64_t ComposeUniqueId(int64_t req_id, PostType::type post_type,
                          const std::map<std::string, std::string>& carrier) override;

  // Business logic management
  UniqueIdBusinessLogic* business_logic_{nullptr};
  void setBusinessLogic(UniqueIdBusinessLogic* logic);
  UniqueIdBusinessLogic* getBusinessLogic() const;

  // Metrics and monitoring
  void GetRpcMetrics(std::map<std::string, int64_t>& metrics) const;
  void GetBusinessMetrics(std::map<std::string, int64_t>& metrics) const;
  int64_t req_id_;
  void setReqId(int64_t req_id) { req_id_ = req_id;  }
#ifdef ENABLE_GEM5  
  void setRecvBuffer(uint8_t* buf);
  bool isReadyForRequest() const { return ready_for_request_; }
  int64_t current_post_id_;
  bool success_;
#endif
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
//  std::shared_ptr<opentracing::Span> ProcessIncomingRpc(
//      int64_t req_id, 
//      PostType::type post_type,
//      const std::map<std::string, std::string>& carrier);
//      
//  void ProcessOutgoingRpc(std::shared_ptr<opentracing::Span> span);
  void ProcessIncomingRpc(
      int64_t req_id, 
      PostType::type post_type,
      const std::map<std::string, std::string>& carrier);
  void ProcessOutgoingRpc();

};

} // namespace social_network

#endif // SOCIAL_NETWORK_MICROSERVICES_UNIQUEIDHANDLER_H
