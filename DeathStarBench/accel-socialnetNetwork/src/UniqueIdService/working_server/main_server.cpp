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

  // Create server
  std::shared_ptr<TServerSocket> server_socket = get_server_socket(config_json, "0.0.0.0", port);
  TThreadedServer server(
  //TSimpleServer server(
      std::make_shared<UniqueIdServiceProcessor>(handler),
      server_socket,
      std::make_shared<TFramedTransportFactory>(),
      std::make_shared<TBinaryProtocolFactory>());

  LOG(info) << "Starting the unique-id-service server ...";
  LOG(info) << "Business logic initialized and connected to RPC handler";
  
  server.serve();

  return 0;
}
