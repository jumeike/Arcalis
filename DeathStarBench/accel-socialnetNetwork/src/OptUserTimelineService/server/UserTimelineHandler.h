#ifndef SOCIAL_NETWORK_MICROSERVICES_SRC_USERTIMELINESERVICE_USERTIMELINEHANDLER_H_
#define SOCIAL_NETWORK_MICROSERVICES_SRC_USERTIMELINESERVICE_USERTIMELINEHANDLER_H_

#include <chrono>
#include <atomic>
#include <memory>
#include <map>
#include <mutex>

#include "../../../gen-cpp/UserTimelineService.h"
#include "../../../gen-cpp/social_network_types.h"
#include "../../logger.h"
#include "UserTimelineBusinessLogic.h"

namespace social_network {

#ifdef ENABLE_GEM5
class UserTimelineHandler : public social_network::UserTimelineServiceIf {
#else
class UserTimelineHandler : public UserTimelineServiceIf {
#endif
 public:
  UserTimelineHandler();
  ~UserTimelineHandler() override = default;

  // Thrift service interface implementation
  void WriteUserTimeline(int64_t req_id, int64_t post_id, int64_t user_id, int64_t timestamp,
                        const std::map<std::string, std::string>& carrier) override;

  void ReadUserTimeline(std::vector<Post>& _return, int64_t req_id, int64_t user_id, 
                       int start, int stop, const std::map<std::string, std::string>& carrier) override;

  // Business logic management
  void setBusinessLogic(UserTimelineBusinessLogic* logic);
  UserTimelineBusinessLogic* getBusinessLogic() const;

  // Metrics and monitoring
  void GetRpcMetrics(std::map<std::string, int64_t>& metrics) const;
  void GetBusinessMetrics(std::map<std::string, int64_t>& metrics) const;

#ifdef ENABLE_GEM5
  void setRecvBuffer(uint8_t* buf);
  bool isReadyForRequest() const { return ready_for_request_; }

  std::vector<Post> current_posts_;
  bool success_{false};
  int operation_type_{-1};  // 0=WriteUserTimeline, 1=ReadUserTimeline
#endif // ENABLE_GEM5

 private:
  UserTimelineBusinessLogic* business_logic_{nullptr};

#ifdef ENABLE_GEM5
  bool ready_for_request_{false};
  uint8_t* recv_buffer_{nullptr};
#endif // ENABLE_GEM5
  
  // RPC layer metrics
  mutable std::mutex _metrics_mutex;
  std::atomic<uint64_t> _rpc_requests_processed{0};
  std::atomic<uint64_t> _total_rpc_time_ns{0};
  std::atomic<uint64_t> _header_processing_time_ns{0};
  std::atomic<uint64_t> _tracing_time_ns{0};

  // Helper functions for RPC processing
  void ProcessIncomingRpc(int64_t req_id, 
                         const std::map<std::string, std::string>& carrier);
  void ProcessOutgoingRpc();
};

} // namespace social_network

#endif // SOCIAL_NETWORK_MICROSERVICES_SRC_USERTIMELINESERVICE_USERTIMELINEHANDLER_H_
