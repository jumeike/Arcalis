#include <signal.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TSocket.h>

#include <boost/program_options.hpp>

#include "../../logger.h"
#include "../../utils.h"
#include "../../utils_thrift.h"
#include "UserTimelineBusinessLogic.h"
#include "UserTimelineHandler.h"

#ifdef ENABLE_TRACING
#include "PacketLogger.h"
#endif

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
#endif // ENABLE_CEREBELLUM

using apache::thrift::protocol::TBinaryProtocolFactory;
using apache::thrift::server::TThreadedServer;
using apache::thrift::server::TSimpleServer;
using apache::thrift::transport::TBufferedTransportFactory;
using apache::thrift::transport::TServerSocket;
using namespace social_network;

void sigintHandler(int sig) {
  (void)sig;
  exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[]) {
#ifdef DEBUG_LOGGING
  std::cout << "DEBUG_LOGGING is defined!" << std::endl;
#else
  std::cout << "DEBUG_LOGGING is not defined!" << std::endl;
#endif

  signal(SIGINT, sigintHandler);
  init_logger();

  namespace po = boost::program_options;
  po::options_description desc("Options");
  desc.add_options()
      ("help", "produce help message")
#ifdef ENABLE_GEM5
      ("trace-file", po::value<std::string>(), "Trace file to replay")
      ("num-requests", po::value<int>(), "Number of requests to process")
#endif
      ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 0;
  }

#ifdef ENABLE_GEM5
  std::string trace_file = "usertimeline_traces/dpdk_to_rpc.bin";
  int num_requests = -1;

  if (vm.count("trace-file")) {
    trace_file = vm["trace-file"].as<std::string>();
  }
  if (vm.count("num-requests")) {
    num_requests = vm["num-requests"].as<int>();
  }

  LOG(info) << "Trace file: " << trace_file;
  if (num_requests > 0) {
    LOG(info) << "Max requests: " << num_requests;
  }
  apache::thrift::transport::TSocket::setTraceConfig(trace_file, num_requests);
#endif // ENABLE_GEM5

#ifdef ENABLE_TRACING
  PacketLogger::getInstance().initializeLogFiles("usertimeline_traces", true);
#endif

  json config_json;
  if (load_config_file("config/service-config.json", &config_json) != 0) {
    exit(EXIT_FAILURE);
  }

  int port = config_json["user-timeline-service"]["port"];

  int post_storage_port = config_json["post-storage-service"]["port"];
  std::string post_storage_addr = config_json["post-storage-service"]["addr"];
  int post_storage_conns = config_json["post-storage-service"]["connections"];
  int post_storage_timeout = config_json["post-storage-service"]["timeout_ms"];
  int post_storage_keepalive = config_json["post-storage-service"]["keepalive_ms"];

  ClientPool<ThriftClient<PostStorageServiceClient>> post_storage_client_pool(
      "post-storage-client", post_storage_addr, post_storage_port, 0,
      post_storage_conns, post_storage_timeout, post_storage_keepalive,
      config_json);

  LOG(info) << "Using local in-memory timeline storage with nested PostStorage RPC";

#ifdef ENABLE_GEM5
  map_m5_mem();
#endif

  auto business_logic = std::make_unique<UserTimelineBusinessLogic>(
      &post_storage_client_pool, nullptr, nullptr);
  auto handler = std::make_shared<UserTimelineHandler>();
  handler->setBusinessLogic(business_logic.get());

#ifdef ENABLE_GEM5
  handler->setRecvBuffer(business_logic->getRecvBuffer());
  if (handler->isReadyForRequest()) {
    LOG(info) << "Handler ready for accelerator communication";
  }
#endif // ENABLE_GEM5

  std::shared_ptr<TServerSocket> server_socket =
      get_server_socket(config_json, "0.0.0.0", port);

#ifdef ENABLE_GEM5
  TSimpleServer server(
#else
  TThreadedServer server(
#endif
      std::make_shared<UserTimelineServiceProcessor>(handler),
      server_socket,
      std::make_shared<TBufferedTransportFactory>(),
      std::make_shared<TBinaryProtocolFactory>());

  LOG(info) << "Starting the opt-user-timeline-service server on port " << port << "...";
  LOG(info) << "Local storage mode enabled";

#ifdef ENABLE_GEM5
  m5_work_begin_addr(0, 0);
#ifdef ENABLE_CEREBELLUM
  CerebellumManagerFactory::waitingTillMSRReady();
  std::cout << "MSR ready\n";
  cerebellum_manager->sendJobMSR(CerebellumJob());

  uint64_t cpuid = 0;
  
  printf("Allocating uncacheable page to comminucate with the engine.\n");
  auto add = cerebellum_manager->getAddress(cpuid);
  
  sendAddress = add.first;
  readAddress = add.second;

  uint64_t num_init_commands = 0;
  *sendAddress = num_init_commands;
  volatile uint64_t temp0 = *readAddress;
  (void)temp0;

  business_logic->setAddresses(sendAddress, readAddress);
#endif // ENABLE_CEREBELLUM
#endif // ENABLE_GEM5

  server.serve();

  std::map<std::string, int64_t> rpc_metrics, business_metrics;
  handler->GetRpcMetrics(rpc_metrics);
  handler->GetBusinessMetrics(business_metrics);

  int64_t rpc_time = rpc_metrics["avg_rpc_time_ns"];
  int64_t business_time = business_metrics["avg_processing_time_ns"];
  int64_t total_time = rpc_time + business_time;

  double rpc_fraction = (total_time > 0) ? (100.0 * rpc_time / total_time) : 0.0;
  double business_fraction = (total_time > 0) ? (100.0 * business_time / total_time) : 0.0;

  LOG(info) << "Performance metrics for "
            << rpc_metrics["requests_processed"] << " requests processed:";
  LOG(info) << "  RPC: " << rpc_time << " ns avg, "
            << rpc_fraction << "% of total";
  LOG(info) << "  Business: " << business_time << " ns avg, "
            << business_fraction << "% of total";

#ifdef ENABLE_GEM5
  unmap_m5_mem();
#endif

  return 0;
}
