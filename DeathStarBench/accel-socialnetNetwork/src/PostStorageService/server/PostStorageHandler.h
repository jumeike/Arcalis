#ifndef SOCIAL_NETWORK_MICROSERVICES_POSTSTORAGEHANDLER_H
#define SOCIAL_NETWORK_MICROSERVICES_POSTSTORAGEHANDLER_H

#include <atomic>
#include <memory>
#include <map>
#include <mutex>
#include <chrono>

#include "../../../gen-cpp/PostStorageService.h"
#include "../../../gen-cpp/social_network_types.h"
#include "../../logger.h"
#include "PostStorageBusinessLogic.h"

namespace social_network {

class PostStorageBusinessLogic; // Forward declaration

#ifdef ENABLE_GEM5
class PostStorageHandler : public social_network::PostStorageServiceIf {
#else
class PostStorageHandler : public PostStorageServiceIf {
#endif
 public:
  ~PostStorageHandler() override = default;
  PostStorageHandler();

  // Thrift service interface implementation
  void StorePost(int64_t req_id, const Post& post,
                 const std::map<std::string, std::string>& carrier) override;

  void ReadPost(Post& _return, int64_t req_id, int64_t post_id,
                const std::map<std::string, std::string>& carrier) override;

  void ReadPosts(std::vector<Post>& _return, int64_t req_id,
                 const std::vector<int64_t>& post_ids,
                 const std::map<std::string, std::string>& carrier) override;

  // Business logic management
  PostStorageBusinessLogic* business_logic_{nullptr};
  void setBusinessLogic(PostStorageBusinessLogic* logic);
  PostStorageBusinessLogic* getBusinessLogic() const;

  // Metrics and monitoring
  void GetRpcMetrics(std::map<std::string, int64_t>& metrics) const;
  void GetBusinessMetrics(std::map<std::string, int64_t>& metrics) const;
  
  int64_t req_id_;
  void setReqId(int64_t req_id) { req_id_ = req_id; }

#ifdef ENABLE_GEM5  
  void setRecvBuffer(uint8_t* buf);
  bool isReadyForRequest() const { return ready_for_request_; }
  
  // Results for different operations
  Post current_post_;
  std::vector<Post> current_posts_;
  bool success_;
  int operation_type_; // 0=StorePost, 1=ReadPost, 2=ReadPosts
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
  void ProcessIncomingRpc(int64_t req_id, 
                         const std::map<std::string, std::string>& carrier);
      
  void ProcessOutgoingRpc();
};

} // namespace social_network

#endif // SOCIAL_NETWORK_MICROSERVICES_POSTSTORAGEHANDLER_H
