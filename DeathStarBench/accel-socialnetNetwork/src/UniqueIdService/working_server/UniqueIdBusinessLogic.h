#ifndef SOCIAL_NETWORK_MICROSERVICES_UNIQUEIDBUSINESSLOGIC_H
#define SOCIAL_NETWORK_MICROSERVICES_UNIQUEIDBUSINESSLOGIC_H

#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <atomic>
#include <map>

#include "../../../gen-cpp/social_network_types.h"
#include "../../logger.h"

// Custom Epoch (January 1, 2018 Midnight GMT = 2018-01-01T00:00:00Z)
#define CUSTOM_EPOCH 1514764800000

namespace social_network {

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::system_clock;

class UniqueIdBusinessLogic {
 public:
  UniqueIdBusinessLogic(const std::string& machine_id);
  ~UniqueIdBusinessLogic() = default;

  // Core business logic function
  int64_t ComposeUniqueId(int64_t req_id, PostType::type post_type);

  // Metrics and monitoring
  void GetMetrics(std::map<std::string, int64_t>& metrics) const;
  void ResetMetrics();

 private:
  std::string _machine_id;
  std::mutex _thread_lock;
  
  // Static counters for ID generation (protected by mutex)
  static int64_t current_timestamp;
  static int counter;
  
  // Metrics (simple counters, protected by mutex when accessed)
  std::atomic<uint64_t> _requests_processed{0};
  std::atomic<uint64_t> _total_processing_time_ns{0};
  std::atomic<uint64_t> _lock_contention_time_ns{0};

  // Helper functions
  int GetCounter(int64_t timestamp);
  std::string FormatHexString(int64_t value, size_t width);
};

// Utility functions for machine ID generation
u_int16_t HashMacAddressPid(const std::string& mac);
std::string GetMachineId(std::string& netif);

} // namespace social_network

#endif // SOCIAL_NETWORK_MICROSERVICES_UNIQUEIDBUSINESSLOGIC_H
