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
#include "UrlShortenHandler.h"
#include "UrlShortenBusinessLogic.h"
#include "nlohmann/json.hpp"

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
  
  // SetUpTracer("config/jaeger-config.yml", "url-shorten-service");
  
  json config_json;
  if (load_config_file("config/service-config.json", &config_json) != 0) {
    exit(EXIT_FAILURE);
  }

  int port = config_json["url-shorten-service"]["port"];
  int mongodb_conns = config_json["url-shorten-mongodb"]["connections"];
  int mongodb_timeout = config_json["url-shorten-mongodb"]["timeout_ms"];
  int memcached_conns = config_json["url-shorten-memcached"]["connections"];
  int memcached_timeout = config_json["url-shorten-memcached"]["timeout_ms"];

  memcached_client_pool = init_memcached_client_pool(
      config_json, "url-shorten", 32, memcached_conns);
  mongodb_client_pool = init_mongodb_client_pool(
      config_json, "url-shorten", mongodb_conns);

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
    r = CreateIndex(mongodb_client, "url-shorten", "shortened_url", true);
    if (!r) {
      LOG(error) << "Failed to create mongodb index, try again";
      sleep(1);
    }
  }
  mongoc_client_pool_push(mongodb_client_pool, mongodb_client);

  LOG(info) << "MongoDB index created successfully";

  // Create business logic instance
  auto business_logic = std::make_unique<UrlShortenBusinessLogic>(
      memcached_client_pool, mongodb_client_pool);

  // Create service handler and set business logic
  auto handler = std::make_shared<UrlShortenHandler>();
  handler->setBusinessLogic(business_logic.get());

  // Create server
  std::shared_ptr<TServerSocket> server_socket = get_server_socket(
      config_json, "0.0.0.0", port);

  TThreadedServer server(
  //TSimpleServer server(
      std::make_shared<UrlShortenServiceProcessor>(handler),
      server_socket,
      std::make_shared<TBufferedTransportFactory>(),
      std::make_shared<TBinaryProtocolFactory>());

  LOG(info) << "Starting the url-shorten-service server on port " << port << "...";
  LOG(info) << "Business logic initialized and connected to RPC handler";
  LOG(info) << "Memcached pool: " << memcached_conns << " connections";
  LOG(info) << "MongoDB pool: " << mongodb_conns << " connections";
  
  server.serve();

  return 0;
}
