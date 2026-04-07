#include <signal.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TServerSocket.h>

#include "../../utils.h"
#include "../../utils_thrift.h"
#include "UrlShortenHandler.h"
#include "UrlShortenBusinessLogic.h"
#include "nlohmann/json.hpp"

#ifdef ENABLE_GEM5_TEST
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
  std::string trace_file = "urlshorten_traces/dpdk_to_rpc.bin";
  int num_requests = -1;  // -1 means read all
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
      std::cout << "  --trace-file <file>     Trace file to replay (default: urlshorten_traces/dpdk_to_rpc.bin)\n";
      std::cout << "  --num-requests <num>    Number of requests to process (default: all)\n";
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
#endif // ENABLE_GEM5

#ifdef ENABLE_TRACING
  PacketLogger::getInstance().initializeLogFiles("urlshorten_traces", true);
#endif

  // SetUpTracer("config/jaeger-config.yml", "url-shorten-service");

  json config_json;
  if (load_config_file("config/service-config.json", &config_json) != 0) {
    exit(EXIT_FAILURE);
  }

  int port = config_json["url-shorten-service"]["port"];
  LOG(info) << "Using local in-memory URL storage (MongoDB/Memcached disabled)";

#ifdef ENABLE_GEM5_TEST
  map_m5_mem();
#endif

  // Create business logic instance
  auto business_logic = std::make_unique<UrlShortenBusinessLogic>(nullptr, nullptr);

#ifdef ENABLE_GEM5
  business_logic->setCoreSplitConfig(rpc_core, business_core, enable_core_split);
#endif

  // Create service handler and set business logic
  auto handler = std::make_shared<UrlShortenHandler>();
  handler->setBusinessLogic(business_logic.get());

#ifdef ENABLE_GEM5
  handler->setRecvBuffer(business_logic->getRecvBuffer());

  if (handler->isReadyForRequest()) {
    LOG(info) << "Handler ready for accelerator communication";
  }
#endif // ENABLE_GEM5

  // Create server
  std::shared_ptr<TServerSocket> server_socket = get_server_socket(
      config_json, "0.0.0.0", port);

#ifdef ENABLE_GEM5
  TSimpleServer server(
#else
  TSimpleServer server(
#endif // ENABLE_GEM5
      std::make_shared<UrlShortenServiceProcessor>(handler),
      server_socket,
      std::make_shared<TBufferedTransportFactory>(),
      std::make_shared<TBinaryProtocolFactory>());

  LOG(info) << "Starting the url-shorten-service server on port " << port << "...";
  LOG(info) << "Business logic initialized and connected to RPC handler";
  LOG(info) << "Local storage mode enabled";
  LOG(info) << "Core-split baseline running over ENABLE_GEM5 buffer path";

#ifdef ENABLE_GEM5_TEST
  m5_work_begin_addr(0, 0); // switch cpu type
#ifdef ENABLE_CEREBELLUM
  CerebellumManagerFactory::waitingTillMSRReady();
  std::cout << "MSR ready \n";
  cerebellum_manager->sendJobMSR(CerebellumJob());

  uint64_t cpuid = 0;

  printf("Allocating uncacheable page to communicate with the engine.\n");
  auto add = cerebellum_manager->getAddress(cpuid);

  sendAddress = add.first;
  readAddress = add.second;

  // Initialize engine commands
  uint64_t num_init_commands = 0;
  *sendAddress = num_init_commands;
  volatile uint64_t temp0 = *readAddress;

  business_logic->setAddresses(sendAddress, readAddress);
#endif // ENABLE_CEREBELLUM
#endif // ENABLE_GEM5_TEST

  server.serve();

  std::map<std::string, int64_t> rpc_metrics, business_metrics;
  handler->GetRpcMetrics(rpc_metrics);
  handler->GetBusinessMetrics(business_metrics);

  int64_t rpc_time = rpc_metrics["avg_rpc_time_ns"];
  int64_t business_time = business_metrics["avg_processing_time_ns"];
  int64_t compose_requests = business_metrics["compose_requests"];
  int64_t get_extended_requests = business_metrics["get_extended_requests"];
  int64_t total_time = rpc_time + business_time;

  double rpc_fraction = (total_time > 0) ? (100.0 * rpc_time / total_time) : 0.0;
  double business_fraction = (total_time > 0) ? (100.0 * business_time / total_time) : 0.0;

  LOG(info) << "Performance metrics for "
            << rpc_metrics["requests_processed"] << " requests processed:";
  LOG(info) << "  RPC: " << rpc_time << " ns avg, "
            << rpc_fraction << "% of total";
  LOG(info) << "  Business: " << business_time << " ns avg, "
            << business_fraction << "% of total";
  LOG(info) << "  Store requests: " << compose_requests
            << ", Read requests: " << 0
            << ", Read multi requests: " << get_extended_requests;

#ifdef ENABLE_GEM5_TEST
  unmap_m5_mem();
#endif

  return 0;
}

