/*
 * 64-bit Unique Id Generator
 *
 * ------------------------------------------------------------------------
 * |0| 11 bit machine ID |      40-bit timestamp         | 12-bit counter |
 * ------------------------------------------------------------------------
 *
 * 11-bit machine Id code by hashing the MAC address
 * 40-bit UNIX timestamp in millisecond precision with custom epoch
 * 12 bit counter which increases monotonically on single process
 *
 */
#include <signal.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TServerSocket.h>

#include "../../utils.h"
#include "../../utils_thrift.h"
#include "UniqueIdHandler.h"
#include "UniqueIdBusinessLogic.h"

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

int main(int argc, char *argv[]) {
#ifdef DEBUG_LOGGING
  std::cout << "DEBUG_LOGGING is defined!" << std::endl;
#else
  std::cout << "DEBUG_LOGGING is NOT defined!" << std::endl;
#endif

  signal(SIGINT, sigintHandler);
  init_logger();
  //SetUpTracer("config/jaeger-config.yml", "unique-id-service");
#ifdef ENABLE_GEM5  
  PacketLogger::getInstance().initializeLogFiles("traces", false);
#else
  PacketLogger::getInstance().initializeLogFiles("traces", true);
#endif  
  json config_json;
  if (load_config_file("config/service-config.json", &config_json) != 0) {
    exit(EXIT_FAILURE);
  }

  int port = config_json["unique-id-service"]["port"];
  std::string netif = config_json["unique-id-service"]["netif"];
  std::string machine_id = GetMachineId(netif);
  if (machine_id == "") {
    exit(EXIT_FAILURE);
  }

  LOG(info) << "machine_id = " << machine_id;

  // Create business logic instance
  auto business_logic = std::make_unique<UniqueIdBusinessLogic>(machine_id);

  // Create service handler and set business logic
  auto handler = std::make_shared<UniqueIdHandler>();
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
  TThreadedServer server(
#endif // ENABLE_GEM5
      std::make_shared<UniqueIdServiceProcessor>(handler),
      server_socket,
      std::make_shared<TBufferedTransportFactory>(),
      std::make_shared<TBinaryProtocolFactory>());

  LOG(info) << "Starting the unique-id-service server ...";
  LOG(info) << "Business logic initialized and connected to RPC handler";

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
