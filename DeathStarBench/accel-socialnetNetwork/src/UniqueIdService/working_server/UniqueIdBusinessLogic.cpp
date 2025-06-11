#include "UniqueIdBusinessLogic.h"
#include <fstream>
#include <unistd.h>

namespace social_network {

// Static member initialization
int64_t UniqueIdBusinessLogic::current_timestamp = -1;
int UniqueIdBusinessLogic::counter = 0;

UniqueIdBusinessLogic::UniqueIdBusinessLogic(const std::string& machine_id)
    : _machine_id(machine_id) {
  LOG(info) << "UniqueIdBusinessLogic initialized with machine_id: " << machine_id;
}

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
