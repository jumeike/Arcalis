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
#ifdef debug_logging
  std::cout << "DEBUG_LOGGING is defined!" << std::endl;
#else
  std::cout << "DEBUG_LOGGING is not defined!" << std::endl;
#endif

  signal(SIGINT, sigintHandler);
  init_logger();
  // SetUpTracer("config/jaeger-config.yml", "post-storage-service"); // Commented out for simplicity

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

  // Create business logic instance
  auto business_logic = std::make_unique<PostStorageBusinessLogic>(
      memcached_client_pool, mongodb_client_pool);

  // Create service handler and set business logic
  auto handler = std::make_shared<PostStorageHandler>();
  handler->setBusinessLogic(business_logic.get());

  // Create server
  std::shared_ptr<TServerSocket> server_socket = get_server_socket(config_json, "0.0.0.0", port);
  TThreadedServer server(
  //TSimpleServer server(
      std::make_shared<PostStorageServiceProcessor>(handler),
      server_socket,
      std::make_shared<TBufferedTransportFactory>(),
      std::make_shared<TBinaryProtocolFactory>());

  LOG(info) << "Starting the post-storage-service server on port " << port << "...";
  LOG(info) << "Business logic initialized and connected to RPC handler";
  LOG(info) << "Memcached pool: " << memcached_conns << " connections";
  LOG(info) << "MongoDB pool: " << mongodb_conns << " connections";
  
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


  return 0;
}
