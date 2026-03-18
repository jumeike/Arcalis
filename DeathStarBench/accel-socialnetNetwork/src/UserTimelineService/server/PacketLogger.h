#ifndef PACKET_LOGGER_H
#define PACKET_LOGGER_H

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

#include "../../../gen-cpp/UserTimelineService.h"

class PacketLogger {
 public:
  static PacketLogger& getInstance() {
    static PacketLogger instance;
    return instance;
  }

  void initializeLogFiles(const std::string& dirName = "usertimeline_traces",
                          bool binary_mode = true) {
    binary_mode_ = binary_mode;
    [[maybe_unused]] int result = system(("mkdir -p " + dirName).c_str());

    std::string ext = binary_mode ? ".bin" : ".csv";
    auto flags = binary_mode ? (std::ios::out | std::ios::binary) : std::ios::out;

    dpdk_to_rpc_.open(dirName + "/dpdk_to_rpc" + ext, flags);
    rpc_to_app_.open(dirName + "/rpc_to_app" + ext, flags);
    app_to_rpc_.open(dirName + "/app_to_rpc" + ext, flags);
    rpc_to_dpdk_.open(dirName + "/rpc_to_dpdk" + ext, flags);

    dpdk_to_rpc.open(dirName + "/dpdk_to_rpc.csv", std::ios::out);
    rpc_to_app.open(dirName + "/rpc_to_app.csv", std::ios::out);
    app_to_rpc.open(dirName + "/app_to_rpc.csv", std::ios::out);
    rpc_to_dpdk.open(dirName + "/rpc_to_dpdk.csv", std::ios::out);

    writeCSVHeaders();
  }

  void logDpdkToRpc(const void* data, uint16_t size) {
    writeBasicPacket(dpdk_to_rpc_, dpdk_to_rpc, 0, size, data);
  }

  void logRpcToApp(int64_t req_id,
                   const social_network::UserTimelineService_WriteUserTimeline_args& args) {
    std::vector<uint8_t> buffer;
    serializeWriteArgs(args, buffer);

    RequestHeader header = {getCurrentTimestamp(), req_id, 0,
                            static_cast<uint16_t>(buffer.size())};
    writeRequestPacket(rpc_to_app_, rpc_to_app, header,
                       buffer.empty() ? nullptr : buffer.data());
  }

  void logRpcToApp(int64_t req_id,
                   const social_network::UserTimelineService_ReadUserTimeline_args& args) {
    std::vector<uint8_t> buffer;
    serializeReadArgs(args, buffer);

    RequestHeader header = {getCurrentTimestamp(), req_id, 1,
                            static_cast<uint16_t>(buffer.size())};
    writeRequestPacket(rpc_to_app_, rpc_to_app, header,
                       buffer.empty() ? nullptr : buffer.data());
  }

  void logAppToRpc(int64_t req_id,
                   const social_network::UserTimelineService_WriteUserTimeline_result& res) {
    std::vector<uint8_t> buffer;
    serializeWriteResult(res, buffer);

    ResponseHeader header = {getCurrentTimestamp(), req_id, 0, 0,
                             static_cast<uint16_t>(buffer.size())};
    writeResponsePacket(app_to_rpc_, app_to_rpc, header,
                        buffer.empty() ? nullptr : buffer.data());
  }

  void logAppToRpc(int64_t req_id,
                   const social_network::UserTimelineService_ReadUserTimeline_result& res) {
    std::vector<uint8_t> buffer;
    serializeReadResult(res, buffer);

    uint16_t count = res.__isset.success
                         ? static_cast<uint16_t>(res.success.size())
                         : static_cast<uint16_t>(0);
    ResponseHeader header = {getCurrentTimestamp(), req_id, 1, count,
                             static_cast<uint16_t>(buffer.size())};
    writeResponsePacket(app_to_rpc_, app_to_rpc, header,
                        buffer.empty() ? nullptr : buffer.data());
  }

  void logRpcToDpdk(const void* data, uint16_t size) {
    writeBasicPacket(rpc_to_dpdk_, rpc_to_dpdk, 0, size, data);
  }

  ~PacketLogger() {
    dpdk_to_rpc_.close();
    rpc_to_app_.close();
    app_to_rpc_.close();
    rpc_to_dpdk_.close();
    dpdk_to_rpc.close();
    rpc_to_app.close();
    app_to_rpc.close();
    rpc_to_dpdk.close();
  }

 private:
  PacketLogger() : binary_mode_(true) {}

  bool binary_mode_;

  struct __attribute__((packed)) RequestHeader {
    uint64_t timestamp;
    int64_t req_id;
    uint16_t operation_type;
    uint16_t size;
  };

  struct __attribute__((packed)) BasicHeader {
    uint64_t timestamp;
    int64_t req_id;
    uint16_t size;
  };

  struct __attribute__((packed)) ResponseHeader {
    uint64_t timestamp;
    int64_t req_id;
    uint16_t operation_type;
    uint16_t item_count;
    uint16_t size;
  };

  struct __attribute__((packed)) SerializedWriteArgs {
    int64_t req_id;
    int64_t post_id;
    int64_t user_id;
    int64_t timestamp;
    uint16_t carrier_size;
  };

  struct __attribute__((packed)) SerializedReadArgs {
    int64_t req_id;
    int64_t user_id;
    int32_t start;
    int32_t stop;
    uint16_t carrier_size;
  };

  struct __attribute__((packed)) SerializedWriteResultPrefix {
    uint8_t se_isset;
    int32_t error_code;
    uint16_t message_size;
  };

  struct __attribute__((packed)) SerializedReadResultPrefix {
    uint8_t success_isset;
    uint8_t se_isset;
    int32_t error_code;
    uint16_t message_size;
    uint16_t post_count;
  };

  uint64_t getCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
  }

  size_t serializedCarrierSize(const std::map<std::string, std::string>& carrier) {
    size_t total = 0;
    for (const auto& kv : carrier) {
      total += kv.first.size() + 1 + kv.second.size() + 1;
    }
    return total;
  }

  void writeCarrier(uint8_t* dst, const std::map<std::string, std::string>& carrier) {
    uint8_t* ptr = dst;
    for (const auto& kv : carrier) {
      std::memcpy(ptr, kv.first.data(), kv.first.size());
      ptr += kv.first.size();
      *ptr++ = '=';
      std::memcpy(ptr, kv.second.data(), kv.second.size());
      ptr += kv.second.size();
      *ptr++ = ';';
    }
  }

  void writeRequestPacket(std::ofstream& bin_file, std::ofstream& csv_file,
                          const RequestHeader& header, const void* data) {
    if (bin_file.is_open()) {
      bin_file.write(reinterpret_cast<const char*>(&header), sizeof(header));
      if (data && header.size > 0) {
        bin_file.write(reinterpret_cast<const char*>(data), header.size);
      }
      bin_file.flush();
    }

    if (csv_file.is_open()) {
      csv_file << header.timestamp << "," << header.req_id << ","
               << header.operation_type << "," << header.size << ",";
      writeHexData(csv_file, data, header.size);
      csv_file << "\n";
      csv_file.flush();
    }
  }

  void writeBasicPacket(std::ofstream& bin_file, std::ofstream& csv_file,
                        int64_t req_id, uint16_t size, const void* data) {
    if (bin_file.is_open()) {
      BasicHeader header = {getCurrentTimestamp(), req_id, size};
      bin_file.write(reinterpret_cast<const char*>(&header), sizeof(header));
      if (data && size > 0) {
        bin_file.write(reinterpret_cast<const char*>(data), size);
      }
      bin_file.flush();
    }

    if (csv_file.is_open()) {
      csv_file << getCurrentTimestamp() << "," << req_id << "," << size << ",";
      writeHexData(csv_file, data, size);
      csv_file << "\n";
      csv_file.flush();
    }
  }

  void writeResponsePacket(std::ofstream& bin_file, std::ofstream& csv_file,
                           const ResponseHeader& header, const void* data) {
    if (bin_file.is_open()) {
      bin_file.write(reinterpret_cast<const char*>(&header), sizeof(header));
      if (data && header.size > 0) {
        bin_file.write(reinterpret_cast<const char*>(data), header.size);
      }
      bin_file.flush();
    }

    if (csv_file.is_open()) {
      csv_file << header.timestamp << "," << header.req_id << ","
               << header.operation_type << "," << header.item_count << ","
               << header.size << ",";
      writeHexData(csv_file, data, header.size);
      csv_file << "\n";
      csv_file.flush();
    }
  }

  void writeCSVHeaders() {
    if (dpdk_to_rpc.is_open()) {
      dpdk_to_rpc << "timestamp,req_id,size,data_hex\n";
    }
    if (rpc_to_app.is_open()) {
      rpc_to_app << "timestamp,req_id,operation_type,size,data_hex\n";
    }
    if (app_to_rpc.is_open()) {
      app_to_rpc << "timestamp,req_id,operation_type,item_count,size,data_hex\n";
    }
    if (rpc_to_dpdk.is_open()) {
      rpc_to_dpdk << "timestamp,req_id,size,data_hex\n";
    }
  }

  void writeHexData(std::ofstream& file, const void* data, uint16_t size) {
    if (!data || size == 0) {
      return;
    }
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
    for (uint16_t i = 0; i < size; ++i) {
      file << std::hex << std::setfill('0') << std::setw(2)
           << static_cast<int>(bytes[i]);
    }
    file << std::dec;
  }

  void serializeWriteArgs(
      const social_network::UserTimelineService_WriteUserTimeline_args& args,
      std::vector<uint8_t>& buffer) {
    size_t carrier_size = serializedCarrierSize(args.carrier);
    buffer.resize(sizeof(SerializedWriteArgs) + carrier_size);

    auto* header = reinterpret_cast<SerializedWriteArgs*>(buffer.data());
    header->req_id = args.req_id;
    header->post_id = args.post_id;
    header->user_id = args.user_id;
    header->timestamp = args.timestamp;
    header->carrier_size = static_cast<uint16_t>(carrier_size);

    if (carrier_size > 0) {
      writeCarrier(buffer.data() + sizeof(SerializedWriteArgs), args.carrier);
    }
  }

  void serializeReadArgs(
      const social_network::UserTimelineService_ReadUserTimeline_args& args,
      std::vector<uint8_t>& buffer) {
    size_t carrier_size = serializedCarrierSize(args.carrier);
    buffer.resize(sizeof(SerializedReadArgs) + carrier_size);

    auto* header = reinterpret_cast<SerializedReadArgs*>(buffer.data());
    header->req_id = args.req_id;
    header->user_id = args.user_id;
    header->start = args.start;
    header->stop = args.stop;
    header->carrier_size = static_cast<uint16_t>(carrier_size);

    if (carrier_size > 0) {
      writeCarrier(buffer.data() + sizeof(SerializedReadArgs), args.carrier);
    }
  }

  void serializeWriteResult(
      const social_network::UserTimelineService_WriteUserTimeline_result& res,
      std::vector<uint8_t>& buffer) {
    const std::string message = res.__isset.se ? res.se.message : "";
    buffer.resize(sizeof(SerializedWriteResultPrefix) + message.size());

    auto* prefix = reinterpret_cast<SerializedWriteResultPrefix*>(buffer.data());
    prefix->se_isset = res.__isset.se ? 1 : 0;
    prefix->error_code = res.__isset.se ? static_cast<int32_t>(res.se.errorCode) : 0;
    prefix->message_size = static_cast<uint16_t>(message.size());

    if (!message.empty()) {
      std::memcpy(buffer.data() + sizeof(SerializedWriteResultPrefix),
                  message.data(), message.size());
    }
  }

  void serializeReadResult(
      const social_network::UserTimelineService_ReadUserTimeline_result& res,
      std::vector<uint8_t>& buffer) {
    const std::string message = res.__isset.se ? res.se.message : "";
    uint16_t post_count = res.__isset.success
                              ? static_cast<uint16_t>(res.success.size())
                              : static_cast<uint16_t>(0);
    size_t payload_size = sizeof(SerializedReadResultPrefix) + message.size() +
                          static_cast<size_t>(post_count) * sizeof(int64_t);
    buffer.resize(payload_size);

    auto* prefix = reinterpret_cast<SerializedReadResultPrefix*>(buffer.data());
    prefix->success_isset = res.__isset.success ? 1 : 0;
    prefix->se_isset = res.__isset.se ? 1 : 0;
    prefix->error_code = res.__isset.se ? static_cast<int32_t>(res.se.errorCode) : 0;
    prefix->message_size = static_cast<uint16_t>(message.size());
    prefix->post_count = post_count;

    size_t offset = sizeof(SerializedReadResultPrefix);
    if (!message.empty()) {
      std::memcpy(buffer.data() + offset, message.data(), message.size());
      offset += message.size();
    }

    if (post_count > 0) {
      for (const auto& post : res.success) {
        *reinterpret_cast<int64_t*>(buffer.data() + offset) = post.post_id;
        offset += sizeof(int64_t);
      }
    }
  }

  std::ofstream dpdk_to_rpc_;
  std::ofstream rpc_to_app_;
  std::ofstream app_to_rpc_;
  std::ofstream rpc_to_dpdk_;
  std::ofstream dpdk_to_rpc;
  std::ofstream rpc_to_app;
  std::ofstream app_to_rpc;
  std::ofstream rpc_to_dpdk;
};

#define LOG_DPDK_TO_RPC(data, size) \
  PacketLogger::getInstance().logDpdkToRpc(data, size)

#define LOG_RPC_TO_APP(args) \
  PacketLogger::getInstance().logRpcToApp(args.req_id, args)

#define LOG_APP_TO_RPC(req_id, result_) \
  PacketLogger::getInstance().logAppToRpc(req_id, result_)

#define LOG_RPC_TO_DPDK(data, size) \
  PacketLogger::getInstance().logRpcToDpdk(data, size)

#endif // PACKET_LOGGER_H
