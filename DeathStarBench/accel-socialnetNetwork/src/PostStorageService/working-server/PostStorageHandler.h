#ifndef SOCIAL_NETWORK_MICROSERVICES_POSTSTORAGEHANDLER_H
#define SOCIAL_NETWORK_MICROSERVICES_POSTSTORAGEHANDLER_H

#include <atomic>
#include <memory>
#include <map>
#include <mutex>

#include "../../../gen-cpp/PostStorageService.h"
#include "../../logger.h"
#include "PostStorageBusinessLogic.h"

namespace social_network {

class PostStorageHandler : public PostStorageServiceIf {
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
  void setBusinessLogic(PostStorageBusinessLogic* logic);
  PostStorageBusinessLogic* getBusinessLogic() const;

  // Metrics and monitoring
  void GetRpcMetrics(std::map<std::string, int64_t>& metrics) const;
  void GetBusinessMetrics(std::map<std::string, int64_t>& metrics) const;

 private:
  PostStorageBusinessLogic* business_logic_{nullptr};
  
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
