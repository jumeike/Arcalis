// STEP 6: Replace your main() function with this simplified version:
#include <signal.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TServerSocket.h>

#include "../../utils.h"
#include "../../utils_thrift.h"
#include "PostStorageHandler.h"
#include "PostStorageBusinessLogic.h"

#ifdef ENABLE_GEM5
#pragma message("Compiling with gem5 instructions")
#include <gem5/m5ops.h>
#include "m5_mmap.h"
#endif // ENABLE_GEM5

#ifdef ENABLE_CEREBELLUM
#pragma message("Compiling with cerebellum")
#include "cerebellum_job.h"
#include "cerebellum_manager.h"
CerebellumManagerFactory factory = CerebellumManagerFactory();
auto cerebellum_manager = factory.getManager();
uint64_t* sendAddress = nullptr;
uint64_t* readAddress = nullptr;
#define cmd_tb_addr_beg   0x001
#define cmd_tb_addr_end   0x002
#endif // ENABLE_CEREBELLUM

using apache::thrift::protocol::TBinaryProtocolFactory;
using apache::thrift::server::TThreadedServer;
using apache::thrift::server::TSimpleServer;
using apache::thrift::transport::TFramedTransportFactory;
using apache::thrift::transport::TBufferedTransportFactory;
using apache::thrift::transport::TServerSocket;
using namespace social_network;

void sigintHandler(int sig) {
  LOG(info) << "Shutting down post-storage service";
  exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[]) {
#ifdef DEBUG_LOGGING
  std::cout << "DEBUG_LOGGING is defined!" << std::endl;
#else
  std::cout << "DEBUG_LOGGING is NOT defined!" << std::endl;
#endif

  signal(SIGINT, sigintHandler);
  init_logger();

#ifdef ENABLE_GEM5  
  std::string trace_file = "poststorage_traces/dpdk_to_rpc.bin";
  int num_requests = -1;
  int rpc_core = 0;
  int business_core = 1;
  bool enable_core_split = true;
  
  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--trace-file" && i + 1 < argc) {
      trace_file = argv[++i];
    } else if (std::string(argv[i]) == "--num-requests" && i + 1 < argc) {
      num_requests = std::stoi(argv[++i]);
    } else if (std::string(argv[i]) == "--rpc-core" && i + 1 < argc) {
      rpc_core = std::stoi(argv[++i]);
    } else if (std::string(argv[i]) == "--business-core" && i + 1 < argc) {
      business_core = std::stoi(argv[++i]);
    } else if (std::string(argv[i]) == "--disable-core-split") {
      enable_core_split = false;
    } else if (std::string(argv[i]) == "--help") {
      std::cout << "Usage: " << argv[0] << " [options]\n";
      std::cout << "  --trace-file <file>     Trace file to replay\n";
      std::cout << "  --num-requests <num>    Number of requests to process\n";
      std::cout << "  --rpc-core <id>         RPC thread core (default: 0)\n";
      std::cout << "  --business-core <id>    Business dispatch core (default: 1)\n";
      std::cout << "  --disable-core-split    Run classic single-thread SW dispatch path\n";
      std::cout << "  --help                  Show this help\n";
      return 0;
    }
  }
  
  LOG(info) << "Trace file: " << trace_file;
  if (num_requests > 0) {
    LOG(info) << "Max requests: " << num_requests;
  }
  LOG(info) << "Core split enabled: " << (enable_core_split ? "true" : "false")
            << ", rpc_core=" << rpc_core << ", business_core=" << business_core;
  apache::thrift::transport::TSocket::setTraceConfig(trace_file, num_requests);
#endif //ENABLE_GEM5

#ifdef ENABLE_TRACING
  PacketLogger::getInstance().initializeLogFiles("poststorage_traces", false);
#endif

  json config_json;
  if (load_config_file("config/service-config.json", &config_json) != 0) {
    LOG(warning) << "Could not load config file, using defaults";
    config_json["post-storage-service"]["port"] = 9090;
  }

  int port = config_json["post-storage-service"]["port"];

  // NO MORE DATABASE INITIALIZATION - everything is local now!

#ifdef ENABLE_GEM5
  map_m5_mem();
#endif

  // Create business logic instance (no database pools needed)
  auto business_logic = std::make_unique<PostStorageBusinessLogic>(nullptr, nullptr);

#ifdef ENABLE_GEM5
  business_logic->setCoreSplitConfig(rpc_core, business_core, enable_core_split);
#endif

  // Create service handler and set business logic
  auto handler = std::make_shared<PostStorageHandler>();
  handler->setBusinessLogic(business_logic.get());
  
#ifdef ENABLE_GEM5
  handler->setRecvBuffer(business_logic->getRecvBuffer());

  if (handler->isReadyForRequest()) {
    LOG(info) << "Handler ready for accelerator communication";
  }
#endif // ENABLE_GEM5

  // Create server
  std::shared_ptr<TServerSocket> server_socket = get_server_socket(config_json, "0.0.0.0", port);
#ifdef ENABLE_GEM5
  TSimpleServer server(
#else
  TSimpleServer server(
#endif // ENABLE_GEM5
      std::make_shared<PostStorageServiceProcessor>(handler),
      server_socket,
      std::make_shared<TBufferedTransportFactory>(),
      std::make_shared<TBinaryProtocolFactory>());

  LOG(info) << "Starting the post-storage-service server on port " << port << "...";
  LOG(info) << "Using LOCAL IN-MEMORY storage (no external databases)";
  LOG(info) << "Core-split baseline running over ENABLE_GEM5 buffer path";

#ifdef ENABLE_GEM5
  m5_work_begin_addr(0,0);
#ifdef ENABLE_CEREBELLUM
  CerebellumManagerFactory::waitingTillMSRReady();
  std::cout << "MSR ready \n";
  cerebellum_manager->sendJobMSR(CerebellumJob());

  uint64_t cpuid = 0;
  printf("Allocating uncacheable page to communicate with the engine.\n");
  auto add = cerebellum_manager->getAddress(cpuid);

  sendAddress = add.first;
  readAddress = add.second;

  uint64_t num_init_commands = 0;
  *sendAddress = num_init_commands;
  volatile uint64_t temp0 = *readAddress;

  business_logic->setAddresses(sendAddress, readAddress);
#endif // ENABLE_CEREBELLUM
#endif // ENABLE_GEM5
  
  server.serve();
  
  // Print metrics
  std::map<std::string, int64_t> rpc_metrics, business_metrics;
  handler->GetRpcMetrics(rpc_metrics);
  handler->GetBusinessMetrics(business_metrics);

  int64_t rpc_time = rpc_metrics["avg_rpc_time_ns"];
  int64_t business_time = business_metrics["avg_processing_time_ns"];
  int64_t cache_hits = business_metrics["cache_hits"];
  int64_t cache_misses = business_metrics["cache_misses"];
  int64_t store_requests = business_metrics["store_requests"];
  int64_t read_requests = business_metrics["read_requests"];
  int64_t read_multi_requests = business_metrics["read_multi_requests"];
  int64_t total_time = rpc_time + business_time;

  double rpc_fraction = (total_time > 0) ? (100.0 * rpc_time / total_time) : 0.0;
  double business_fraction = (total_time > 0) ? (100.0 * business_time / total_time) : 0.0;

  LOG(info) << "LOCAL STORAGE Performance metrics for "
            << rpc_metrics["requests_processed"] << " requests processed:";
  LOG(info) << "  RPC: " << rpc_time << " ns avg, " << rpc_fraction << "% of total";
  LOG(info) << "  Business: " << business_time << " ns avg, " << business_fraction << "% of total";
  LOG(info) << "  Cache hits: " << cache_hits << ", Cache misses: " << cache_misses;
  LOG(info) << "  Store requests: " << store_requests << ", Read requests: " << read_requests 
            << ", Read multi requests: " << read_multi_requests;

#ifdef ENABLE_GEM5
  unmap_m5_mem();
#endif

  return 0;
}
