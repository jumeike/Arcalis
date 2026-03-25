#include <signal.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TServerUDPSocket.h>
#include <thrift/transport/TSocket.h>

#include <boost/program_options.hpp>

#include "../../../gen-cpp/social_network_types.h"
#include "../../ClientPool.h"
#include "../../logger.h"
//#include "../tracing.h"
#include "../../utils.h"
#include "../../utils_mongodb.h"
#include "../../utils_redis.h"
#include "../../utils_thrift.h"
#include "UserTimelineHandler.h"
#include "UserTimelineBusinessLogic.h"

#ifdef ENABLE_TRACING
#include "PacketLogger.h"
#endif

#ifdef ENABLE_GEM5_TEST
#pragma message("Compiling with gem5 instructions")
#include <gem5/m5ops.h>
#include "m5_mmap.h"
#endif // ENABLE_GEM5_TEST

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
using apache::thrift::transport::TFramedTransportFactory;
using apache::thrift::transport::TBufferedTransportFactory;
using apache::thrift::transport::TServerSocket;
using apache::thrift::transport::TServerUDPSocket;
using namespace social_network;

void sigintHandler(int sig) { exit(EXIT_SUCCESS); }

int main(int argc, char *argv[]) {
#ifdef DEBUG_LOGGING
  std::cout << "DEBUG_LOGGING is defined!" << std::endl;
#else
  std::cout << "DEBUG_LOGGING is not defined!" << std::endl;
#endif

 signal(SIGINT, sigintHandler);
  init_logger();

  // Command line options
  namespace po = boost::program_options;
  po::options_description desc("Options");
  desc.add_options()
      ("help", "produce help message")
      ("redis-cluster",
       po::value<bool>()->default_value(false)->implicit_value(true),
       "Enable redis cluster mode")
#ifdef ENABLE_GEM5
      ("trace-file",
       po::value<std::string>(),
       "Trace file to replay")
      ("num-requests",
       po::value<int>(),
       "Number of requests to process")
#endif
    #ifdef ENABLE_NESTED_RPC_TIMING_MODEL
      ("enable-nested-rpc-timing-model", po::bool_switch()->default_value(false),
       "Enable nested PostStorage RPC timing model (replace StorePost/ReadPosts RPC with delay model)")
      ("nested-storepost-delay-us", po::value<uint64_t>(),
       "Delay in microseconds for nested StorePost timing model")
      ("nested-readposts-delay-us", po::value<uint64_t>(),
       "Delay in microseconds for nested ReadPosts timing model")
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

  bool redis_cluster_flag = false;
  if (vm.count("redis-cluster")) {
    if (vm["redis-cluster"].as<bool>()) {
      redis_cluster_flag = true;
    }
  }

#ifdef ENABLE_NESTED_RPC_TIMING_MODEL
  const bool enable_timing_model = vm["enable-nested-rpc-timing-model"].as<bool>();
  const bool has_storepost_delay_override = vm.count("nested-storepost-delay-us") > 0;
  const bool has_readposts_delay_override = vm.count("nested-readposts-delay-us") > 0;
#endif

  //SetUpTracer("config/jaeger-config.yml", "user-timeline-service");

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

  int mongodb_conns = config_json["user-timeline-mongodb"]["connections"];
  int mongodb_timeout = config_json["user-timeline-mongodb"]["timeout_ms"];

  int redis_cluster_config_flag = config_json["user-timeline-redis"]["use_cluster"];
  int redis_replica_config_flag = config_json["user-timeline-redis"]["use_replica"];

  auto mongodb_client_pool = init_mongodb_client_pool(config_json, "user-timeline", mongodb_conns);

  if (mongodb_client_pool == nullptr) {
    return EXIT_FAILURE;
  }

  if (redis_replica_config_flag && (redis_cluster_config_flag || redis_cluster_flag)) {
    LOG(error) << "Can't start service when Redis Cluster and Redis Replica are enabled at the same time";
    exit(EXIT_FAILURE);
  }

  ClientPool<ThriftClient<PostStorageServiceClient>> post_storage_client_pool(
      "post-storage-client", post_storage_addr, post_storage_port, 0,
      post_storage_conns, post_storage_timeout, post_storage_keepalive,
      config_json);

  mongoc_client_t *mongodb_client = mongoc_client_pool_pop(mongodb_client_pool);
  if (!mongodb_client) {
    LOG(fatal) << "Failed to pop mongoc client";
    return EXIT_FAILURE;
  }
  bool r = false;
  while (!r) {
    r = CreateIndex(mongodb_client, "user-timeline", "user_id", true);
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
    auto collection = mongoc_client_get_collection(cleanup_client, "user-timeline", "user-timeline");  // ← fixed
    if (collection) {
      bson_t* empty_filter = bson_new();
      bson_error_t error;
      bson_t reply;
      bson_init(&reply);

      bool deleted = mongoc_collection_delete_many(collection, empty_filter, nullptr, &reply, &error);

      if (deleted) {
        char* reply_str = bson_as_canonical_extended_json(&reply, nullptr);
        // LOG(info) << "[UserTimeline Cleanup] MongoDB deletion reply: " << reply_str;
        LOG(info) << "Cleared existing posts from MongoDB";
        bson_free(reply_str);
      } else {
        LOG(error) << "[UserTimeline Cleanup] MongoDB deletion failed: " << error.message;
      }

      bson_destroy(&reply);
      bson_destroy(empty_filter);
      mongoc_collection_destroy(collection);
    }
    mongoc_client_pool_push(mongodb_client_pool, cleanup_client);
  }


  if (!redis_cluster_flag && !redis_replica_config_flag) {
    Redis redis_cleanup_client = init_redis_client_pool(config_json, "user-timeline");
  
    try {
      redis_cleanup_client.flushdb();  // This is the correct API
      LOG(info) << "Cleared Redis database";
    } catch (const sw::redis::Error &err) {
      LOG(error) << "Failed to flush Redis DB: " << err.what();
    }
  }

  std::shared_ptr<TServerSocket> server_socket = get_server_socket(config_json, "0.0.0.0", port);
  //std::shared_ptr<TServerUDPSocket> server_socket = std::make_shared<TServerUDPSocket>(port); 

  // Create handler and business logic based on Redis configuration
  auto handler = std::make_shared<UserTimelineHandler>();
  std::unique_ptr<UserTimelineBusinessLogic> business_logic;

#ifdef ENABLE_GEM5_TEST
  map_m5_mem();
#endif

  if (redis_cluster_flag || redis_cluster_config_flag) {
    RedisCluster redis_client_pool = init_redis_cluster_client_pool(config_json, "user-timeline");
    business_logic = std::make_unique<UserTimelineBusinessLogic>(
        &redis_client_pool, mongodb_client_pool, &post_storage_client_pool);
  #ifdef ENABLE_NESTED_RPC_TIMING_MODEL
    business_logic->setNestedRpcTimingModel(enable_timing_model);
    if (has_storepost_delay_override) {
      business_logic->setNestedStorepostDelayUs(
        vm["nested-storepost-delay-us"].as<uint64_t>());
    }
    if (has_readposts_delay_override) {
      business_logic->setNestedReadpostsDelayUs(
        vm["nested-readposts-delay-us"].as<uint64_t>());
    }
  #endif
    handler->setBusinessLogic(business_logic.get());

#ifdef ENABLE_GEM5
    handler->setRecvBuffer(business_logic->getRecvBuffer());
    if (handler->isReadyForRequest()) {
      LOG(info) << "Handler ready for accelerator communication";
    }
#endif // ENABLE_GEM5

#ifdef ENABLE_GEM5_TEST
    m5_work_begin_addr(0, 0);
#ifdef ENABLE_CEREBELLUM
    CerebellumManagerFactory::waitingTillMSRReady();
    std::cout << "MSR ready \n";
    cerebellum_manager->sendJobMSR(CerebellumJob());

    uint64_t cpuid = 0;
    auto add = cerebellum_manager->getAddress(cpuid);
    sendAddress = add.first;
    readAddress = add.second;

    uint64_t num_init_commands = 0;
    *sendAddress = num_init_commands;
    volatile uint64_t temp0 = *readAddress;
    (void)temp0;

    business_logic->setAddresses(sendAddress, readAddress);
#endif // ENABLE_CEREBELLUM
#endif // ENABLE_GEM5_TEST

#ifdef ENABLE_GEM5
    TSimpleServer server(std::make_shared<UserTimelineServiceProcessor>(handler),
                         server_socket,
                         std::make_shared<TBufferedTransportFactory>(),
                         std::make_shared<TBinaryProtocolFactory>());
#else
    TThreadedServer server(std::make_shared<UserTimelineServiceProcessor>(handler),
                           server_socket,
                           std::make_shared<TBufferedTransportFactory>(),
                          std::make_shared<TBinaryProtocolFactory>());
#endif
    LOG(info) << "Starting the user-timeline-service server with Redis Cluster support...";
    server.serve();
  }
  else if (redis_replica_config_flag) {
    Redis redis_replica_client_pool = init_redis_replica_client_pool(config_json, "redis-replica");
    Redis redis_primary_client_pool = init_redis_replica_client_pool(config_json, "redis-primary");
    business_logic = std::make_unique<UserTimelineBusinessLogic>(
        &redis_replica_client_pool, &redis_primary_client_pool, mongodb_client_pool, &post_storage_client_pool);
  #ifdef ENABLE_NESTED_RPC_TIMING_MODEL
    business_logic->setNestedRpcTimingModel(enable_timing_model);
    if (has_storepost_delay_override) {
      business_logic->setNestedStorepostDelayUs(
        vm["nested-storepost-delay-us"].as<uint64_t>());
    }
    if (has_readposts_delay_override) {
      business_logic->setNestedReadpostsDelayUs(
        vm["nested-readposts-delay-us"].as<uint64_t>());
    }
  #endif
    handler->setBusinessLogic(business_logic.get());

  #ifdef ENABLE_GEM5
    handler->setRecvBuffer(business_logic->getRecvBuffer());
    if (handler->isReadyForRequest()) {
      LOG(info) << "Handler ready for accelerator communication";
    }
  #endif // ENABLE_GEM5

  #ifdef ENABLE_GEM5_TEST
    m5_work_begin_addr(0, 0);
  #ifdef ENABLE_CEREBELLUM
    CerebellumManagerFactory::waitingTillMSRReady();
    std::cout << "MSR ready \n";
    cerebellum_manager->sendJobMSR(CerebellumJob());

    uint64_t cpuid = 0;
    auto add = cerebellum_manager->getAddress(cpuid);
    sendAddress = add.first;
    readAddress = add.second;

    uint64_t num_init_commands = 0;
    *sendAddress = num_init_commands;
    volatile uint64_t temp0 = *readAddress;
    (void)temp0;

    business_logic->setAddresses(sendAddress, readAddress);
  #endif // ENABLE_CEREBELLUM
  #endif // ENABLE_GEM5_TEST

  #ifdef ENABLE_GEM5
    TSimpleServer server(std::make_shared<UserTimelineServiceProcessor>(handler),
               server_socket,
               std::make_shared<TBufferedTransportFactory>(),
               std::make_shared<TBinaryProtocolFactory>());
  #else
    TThreadedServer server(std::make_shared<UserTimelineServiceProcessor>(handler),
                           server_socket,
                          //  std::make_shared<TFramedTransportFactory>(),
                           std::make_shared<TBufferedTransportFactory>(),
                           std::make_shared<TBinaryProtocolFactory>());
  #endif
    LOG(info) << "Starting the user-timeline-service server with replicated Redis support...";
    server.serve();
  }
  else {
    Redis redis_client_pool = init_redis_client_pool(config_json, "user-timeline");
    business_logic = std::make_unique<UserTimelineBusinessLogic>(
        &redis_client_pool, mongodb_client_pool, &post_storage_client_pool);
  #ifdef ENABLE_NESTED_RPC_TIMING_MODEL
    business_logic->setNestedRpcTimingModel(enable_timing_model);
    if (has_storepost_delay_override) {
      business_logic->setNestedStorepostDelayUs(
        vm["nested-storepost-delay-us"].as<uint64_t>());
    }
    if (has_readposts_delay_override) {
      business_logic->setNestedReadpostsDelayUs(
        vm["nested-readposts-delay-us"].as<uint64_t>());
    }
  #endif
    handler->setBusinessLogic(business_logic.get());

#ifdef ENABLE_GEM5
    handler->setRecvBuffer(business_logic->getRecvBuffer());
    if (handler->isReadyForRequest()) {
      LOG(info) << "Handler ready for accelerator communication";
    }
#endif // ENABLE_GEM5

#ifdef ENABLE_GEM5_TEST
    m5_work_begin_addr(0, 0);
#ifdef ENABLE_CEREBELLUM
    CerebellumManagerFactory::waitingTillMSRReady();
    std::cout << "MSR ready \n";
    cerebellum_manager->sendJobMSR(CerebellumJob());

    uint64_t cpuid = 0;
    auto add = cerebellum_manager->getAddress(cpuid);
    sendAddress = add.first;
    readAddress = add.second;

    uint64_t num_init_commands = 0;
    *sendAddress = num_init_commands;
    volatile uint64_t temp0 = *readAddress;
    (void)temp0;

    business_logic->setAddresses(sendAddress, readAddress);
#endif // ENABLE_CEREBELLUM
#endif // ENABLE_GEM5_TEST

#ifdef ENABLE_GEM5
    TSimpleServer server(std::make_shared<UserTimelineServiceProcessor>(handler),
                         server_socket,
                         std::make_shared<TBufferedTransportFactory>(),
                         std::make_shared<TBinaryProtocolFactory>());
#else
    TThreadedServer server(std::make_shared<UserTimelineServiceProcessor>(handler),
                           server_socket,
                          //  std::make_shared<TFramedTransportFactory>(),
                          std::make_shared<TBufferedTransportFactory>(),
                           std::make_shared<TBinaryProtocolFactory>());
#endif
    LOG(info) << "Starting the user-timeline-service server on port " << port <<"...";
    server.serve();
  }

#ifdef ENABLE_GEM5_TEST
  unmap_m5_mem();
#endif

  return 0;
}
