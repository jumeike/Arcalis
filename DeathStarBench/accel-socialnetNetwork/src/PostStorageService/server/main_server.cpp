#include <signal.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TServerSocket.h>

#include "../../utils.h"
#include "../../utils_memcached.h"
#include "../../utils_mongodb.h"
#include "../../utils_thrift.h"
#include "PostStorageHandler.h"
#include "PostStorageBusinessLogic.h"

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

static memcached_pool_st* memcached_client_pool;
static mongoc_client_pool_t* mongodb_client_pool;

void sigintHandler(int sig) {
  if (memcached_client_pool != nullptr) {
    memcached_pool_destroy(memcached_client_pool);
  }
  if (mongodb_client_pool != nullptr) {
    mongoc_client_pool_destroy(mongodb_client_pool);
  }
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
  int num_requests = -1;  // -1 means read all
  
  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--trace-file" && i + 1 < argc) {
      trace_file = argv[++i];
    } else if (std::string(argv[i]) == "--num-requests" && i + 1 < argc) {
      num_requests = std::stoi(argv[++i]);
    } else if (std::string(argv[i]) == "--help") {
      std::cout << "Usage: " << argv[0] << " [options]\n";
      std::cout << "  --trace-file <file>     Trace file to replay (default: poststorage_traces/dpdk_to_rpc.bin)\n";
      std::cout << "  --num-requests <num>    Number of requests to process (default: all)\n";
      std::cout << "  --help                  Show this help\n";
      return 0;
    }
  }
  
  LOG(info) << "Trace file: " << trace_file;
  if (num_requests > 0) {
    LOG(info) << "Max requests: " << num_requests;
  }
  apache::thrift::transport::TSocket::setTraceConfig(trace_file, num_requests);
#endif //ENABLE_GEM5

#ifdef ENABLE_TRACING
  PacketLogger::getInstance().initializeLogFiles("poststorage_traces", false);
#endif

  json config_json;
  if (load_config_file("config/service-config.json", &config_json) != 0) {
    exit(EXIT_FAILURE);
  }

  int port = config_json["post-storage-service"]["port"];
  int mongodb_conns = config_json["post-storage-mongodb"]["connections"];
  int mongodb_timeout = config_json["post-storage-mongodb"]["timeout_ms"];
  int memcached_conns = config_json["post-storage-memcached"]["connections"];
  int memcached_timeout = config_json["post-storage-memcached"]["timeout_ms"];

  // Initialize connection pools
  memcached_client_pool = init_memcached_client_pool(
      config_json, "post-storage", 32, memcached_conns);
  mongodb_client_pool =
      init_mongodb_client_pool(config_json, "post-storage", mongodb_conns);

  if (memcached_client_pool == nullptr || mongodb_client_pool == nullptr) {
    return EXIT_FAILURE;
  }

  // Create MongoDB index
  mongoc_client_t* mongodb_client = mongoc_client_pool_pop(mongodb_client_pool);
  if (!mongodb_client) {
    LOG(fatal) << "Failed to pop mongoc client";
    return EXIT_FAILURE;
  }

  bool r = false;
  while (!r) {
    r = CreateIndex(mongodb_client, "post", "post_id", true);
    if (!r) {
      LOG(error) << "Failed to create mongodb index, try again";
      sleep(1);
    }
  }
  mongoc_client_pool_push(mongodb_client_pool, mongodb_client);

  LOG(info) << "MongoDB index created successfully";
  
  // After creating MongoDB index, add cleanup
  mongoc_client_t* cleanup_client = mongoc_client_pool_pop(mongodb_client_pool);
  if (cleanup_client) {
    auto collection = mongoc_client_get_collection(cleanup_client, "post", "post");
    if (collection) {
      bson_t* empty_filter = bson_new();
      bson_error_t error;
      bool deleted = mongoc_collection_delete_many(collection, empty_filter, nullptr, nullptr, &error);
      if (deleted) {
        LOG(info) << "Cleared existing posts from MongoDB";
      }
      bson_destroy(empty_filter);
      mongoc_collection_destroy(collection);
    }
    mongoc_client_pool_push(mongodb_client_pool, cleanup_client);
  }

  // Clear existing memcached content
  memcached_return_t memcached_rc;
  memcached_st* cleanup_memcached = memcached_pool_pop(memcached_client_pool, true, &memcached_rc);
  if (cleanup_memcached) {
  memcached_rc = memcached_flush(cleanup_memcached, 0);
  if (memcached_rc == MEMCACHED_SUCCESS) {
    LOG(info) << "Cleared existing posts from Memcached";
  } else {
    LOG(warning) << "Failed to clear Memcached: " << memcached_strerror(cleanup_memcached, memcached_rc);
  }
  memcached_pool_push(memcached_client_pool, cleanup_memcached);
  }

#ifdef ENABLE_GEM5_TEST
  map_m5_mem();
#endif

  // Create business logic instance
  auto business_logic = std::make_unique<PostStorageBusinessLogic>(
      memcached_client_pool, mongodb_client_pool);

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
  //TThreadedServer server(
#endif // ENABLE_GEM5
      std::make_shared<PostStorageServiceProcessor>(handler),
      server_socket,
      std::make_shared<TBufferedTransportFactory>(),
      std::make_shared<TBinaryProtocolFactory>());

  LOG(info) << "Starting the post-storage-service server on port " << port << "...";
  LOG(info) << "Business logic initialized and connected to RPC handler";
  LOG(info) << "Memcached pool: " << memcached_conns << " connections";
  LOG(info) << "MongoDB pool: " << mongodb_conns << " connections";

#ifdef ENABLE_GEM5_TEST
  m5_work_begin_addr(0,0); // switch cpu type
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
  int64_t cache_hits = business_metrics["cache_hits"];
  int64_t cache_misses = business_metrics["cache_misses"];
  int64_t store_requests = business_metrics["store_requests"];
  int64_t read_requests = business_metrics["read_requests"];
  int64_t read_multi_requests = business_metrics["read_multi_requests"];
  int64_t total_time = rpc_time + business_time;

  double rpc_fraction = (total_time > 0) ? (100.0 * rpc_time / total_time) : 0.0;
  double business_fraction = (total_time > 0) ? (100.0 * business_time / total_time) : 0.0;

  LOG(info) << "Performance metrics for "
            << rpc_metrics["requests_processed"] << " requests processed:";
  LOG(info) << "  RPC: " << rpc_time << " ns avg, "
            << rpc_fraction << "% of total";
  LOG(info) << "  Business: " << business_time << " ns avg, "
            << business_fraction << "% of total";
  LOG(info) << "  Cache hits: " << cache_hits
            << ", Cache misses: " << cache_misses
            << ", Cache hit rate: "
            << (cache_hits + cache_misses > 0 ? (100.0 * cache_hits / (cache_hits + cache_misses)) : 0.0)
            << "%";
  LOG(info) << "  Store requests: " << store_requests
            << ", Read requests: " << read_requests
            << ", Read multi requests: " << read_multi_requests;

#ifdef ENABLE_GEM5_TEST
  unmap_m5_mem();
#endif

  return 0;
}
