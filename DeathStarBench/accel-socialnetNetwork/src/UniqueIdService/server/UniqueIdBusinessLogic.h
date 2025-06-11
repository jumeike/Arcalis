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

#ifdef ENABLE_CEREBELLUM
#define cmd_send_dpdk_buf    0
#define cmd_send_dpdk_len    1
#define cmd_set_app_flag     2
#define cmd_send_app_resp    3
#define cmd_send_app_buf     4
#define cmd_set_dpdk_flag    5
#endif // ENABLE_CEREBELLUM

#ifdef ENABLE_TRACING // If ENABLE_GEM5 is undef, log_rpc_to_app & log_app_to_rpc are unavailable
#include "PacketLogger.h"
#endif

#ifdef ENABLE_GEM5
#include "../../../gen-cpp/UniqueIdService.h"
#include <thrift/TDispatchProcessor.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TSocket.h> 
#include "PacketReplaySocket.h"
#endif // ENABLE_GEM5

namespace social_network {

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::system_clock;

class UniqueIdHandler; //Forward declaration

class UniqueIdBusinessLogic {
 public:
  UniqueIdBusinessLogic(const std::string& machine_id);
#ifdef ENABLE_GEM5
  // Define a cleanup buffer destructor
  ~UniqueIdBusinessLogic();
#else
  ~UniqueIdBusinessLogic() = default;
#endif // ENABLE_GEM5

  // Core business logic function
  int64_t ComposeUniqueId(int64_t req_id, PostType::type post_type);
  int64_t ComposeUniqueId(); // Buffer-based version for accelerator offload

  // Metrics and monitoring
  void GetMetrics(std::map<std::string, int64_t>& metrics) const;
  void ResetMetrics();

#ifdef ENABLE_GEM5
  // Buffer access methods (add to existing public methods)
  uint8_t* getRecvBuffer() const { return recv_buf_; }
  uint8_t* getRespBuffer() const { return resp_buf_; }
  size_t getBufferSize() const { return BUFFER_SIZE; }
   
  void setHandler(UniqueIdHandler* handler) { handler_ = handler; }
  void setTraceConfig(const std::string& file, int requests);
#endif // ENABLE_GEM5
 
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

#ifdef ENABLE_GEM5
  // Pointer to UniqueIdHandler
  UniqueIdHandler* handler_;
  // Buffer management (add to existing private members)
  static constexpr size_t BUFFER_SIZE = 1024;
  static constexpr size_t ALIGNMENT = 0x40;

  uint8_t* raw_recv_buf_;
  uint8_t* raw_resp_buf_;
  uint8_t* recv_buf_;    // Receive Buffer
  uint8_t* resp_buf_;    // Response Buffer

  uint8_t* allocateAlignedBuffer(uint8_t* raw_buf);
  bool initializeBuffers();
  void cleanupBuffers();
  // Trace File management
  std::string trace_file_;
  int num_requests_;
  apache::thrift::transport::TSocket* getSocketFromTransport();
  bool checkReplayEOF() {
     //auto buffered = dynamic_cast<apache::thrift::transport::TBufferedTransport*>(in_->getTransport().get());
     //auto socket = buffered ? dynamic_cast<apache::thrift::transport::TSocket*>(buffered->getUnderlyingTransport().get()) : nullptr;
     auto socket = getSocketFromTransport();
     return socket ? socket->isReplayEOF() : false;
  }
  bool validateReplay() {
     //auto buffered = dynamic_cast<apache::thrift::transport::TBufferedTransport*>(in_->getTransport().get());
     //auto socket = buffered ? dynamic_cast<apache::thrift::transport::TSocket*>(buffered->getUnderlyingTransport().get()) : nullptr;
     auto socket = getSocketFromTransport();
     return socket ? socket->getReplaySocket().validateReplay("traces/rpc_to_dpdk.bin") : false;
  }
#endif // ENABLE_GEM5
 public:
#ifdef ENABLE_GEM5
// SW path member variables
  std::string fname_;
  int32_t seqid_;
  void* ctx_;
  apache::thrift::TDispatchProcessor* processor_;
  std::shared_ptr<::apache::thrift::protocol::TProtocol> in_;
  std::shared_ptr<::apache::thrift::protocol::TProtocol> out_;
  UniqueIdService_ComposeUniqueId_result result_;
  UniqueIdService_ComposeUniqueId_args args_;
  void* connectionContext_;
  size_t read_pos_, write_pos_;
// SW path member functions
  void callSWread();
  bool callSWdispatch();
  void callSWwrite();
  void callSWsendresp(bool success);
  void callSWSendBuf();
  void runLoop(apache::thrift::TDispatchProcessor* processor,
            std::shared_ptr<::apache::thrift::protocol::TProtocol> in,
            std::shared_ptr<::apache::thrift::protocol::TProtocol> out,
            void* connectionContext); 
#endif // ENABLE_GEM5

#ifdef ENABLE_CEREBELLUM
// HW Accelerator path member variables
  volatile uint64_t* readAddress;
  volatile uint64_t* sendAddress;
// HW Accelerator path member functions
  void callEngineRead();
  bool callEngineDispatch();
  void callEnginewrite();
  void callEngineSendresp(bool success);
  void callEngineSendBuf();
  void setAddresses(volatile uint64_t* sAddress, volatile uint64_t* rAddress) {
         sendAddress = sAddress;
         readAddress = rAddress;
     }
#endif // ENABLE_CEREBELLUM

};

// Utility functions for machine ID generation
u_int16_t HashMacAddressPid(const std::string& mac);
std::string GetMachineId(std::string& netif);

} // namespace social_network

#endif // SOCIAL_NETWORK_MICROSERVICES_UNIQUEIDBUSINESSLOGIC_H
