#ifndef PACKET_LOGGER_H
#define PACKET_LOGGER_H

#include <fstream>
#include <string>
#include <chrono>
#include <vector>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include "../../../gen-cpp/UniqueIdService.h"

class PacketLogger {
public:
    static PacketLogger& getInstance() {
        static PacketLogger instance;
        return instance;
    }
    
    void initializeLogFiles(const std::string& dirName = "uniqueid_traces", bool binary_mode = true) {
        binary_mode_ = binary_mode;
        //system(("mkdir -p " + dirName).c_str());
        [[maybe_unused]] int result = system(("mkdir -p " + dirName).c_str()); 
        
        std::string ext = binary_mode ? ".bin" : ".csv";
        auto flags = binary_mode ? (std::ios::out | std::ios::binary) : std::ios::out;
        
        dpdk_to_rpc_.open(dirName + "/dpdk_to_rpc" + ext, flags);
        rpc_to_app_.open(dirName + "/rpc_to_app" + ext, flags);
        app_to_rpc_.open(dirName + "/app_to_rpc" + ext, flags);
        rpc_to_dpdk_.open(dirName + "/rpc_to_dpdk" + ext, flags);
        
        if (!binary_mode) {
            writeCSVHeaders();
        }
    }
    
    void logDpdkToRpc(const void* data, uint16_t size) {
        writePacket(dpdk_to_rpc_, 0, size, data);
    }
    
    void logRpcToApp(int64_t req_id, int32_t post_type, const social_network::UniqueIdService_ComposeUniqueId_args& args) {
        std::vector<uint8_t> buffer;
        serializeArgs(args, buffer);
        
        AppHeader header = {getCurrentTimestamp(), req_id, post_type, static_cast<uint16_t>(buffer.size())};
        writeAppPacket(rpc_to_app_, header, buffer.data());
    }
    
    void logAppToRpc(int64_t req_id, int64_t result, const social_network::UniqueIdService_ComposeUniqueId_result& res) {
        std::vector<uint8_t> buffer;
        serializeResult(res, buffer);
        
        ResponseHeader header = {getCurrentTimestamp(), req_id, result, static_cast<uint16_t>(buffer.size())};
        writeResponsePacket(app_to_rpc_, header, buffer.data());
    }
    
    void logRpcToDpdk(const void* data, uint16_t size) {
        writePacket(rpc_to_dpdk_, 0, size, data);
    }
    
    ~PacketLogger() {
        dpdk_to_rpc_.close();
        rpc_to_app_.close();
        app_to_rpc_.close();
        rpc_to_dpdk_.close();
    }

private:
    PacketLogger() : binary_mode_(true) {}
    
    bool binary_mode_;
    
    // Packed structs for binary format
    struct __attribute__((packed)) BasicHeader {
        uint64_t timestamp;
        int64_t req_id;
        uint16_t size;
    };
    
    struct __attribute__((packed)) AppHeader {
        uint64_t timestamp;
        int64_t req_id;
        int32_t post_type;
        uint16_t size;
    };
    
    struct __attribute__((packed)) ResponseHeader {
        uint64_t timestamp;
        int64_t req_id;
        int64_t result;
        uint16_t size;
    };
    
    uint64_t getCurrentTimestamp() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }
    
    void writePacket(std::ofstream& file, int64_t req_id, uint16_t size, const void* data) {
        if (!file.is_open()) return;
        
        if (binary_mode_) {
            BasicHeader header = {getCurrentTimestamp(), req_id, size};
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            if (data && size > 0) {
                file.write(reinterpret_cast<const char*>(data), size);
            }
        } else {
            // CSV format: timestamp,req_id,size,data_hex
            file << getCurrentTimestamp() << "," << req_id << "," << size << ",";
            writeHexData(file, data, size);
            file << "\n";
        }
        file.flush();
    }
    
    void writeAppPacket(std::ofstream& file, const AppHeader& header, const void* data) {
        if (!file.is_open()) return;
        
        if (binary_mode_) {
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            if (data && header.size > 0) {
                file.write(reinterpret_cast<const char*>(data), header.size);
            }
        } else {
            // CSV format: timestamp,req_id,post_type,size,data_hex
            file << header.timestamp << "," << header.req_id << "," 
                 << header.post_type << "," << header.size << ",";
            writeHexData(file, data, header.size);
            file << "\n";
        }
        file.flush();
    }
    
    void writeResponsePacket(std::ofstream& file, const ResponseHeader& header, const void* data) {
        if (!file.is_open()) return;
        
        if (binary_mode_) {
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            if (data && header.size > 0) {
                file.write(reinterpret_cast<const char*>(data), header.size);
            }
        } else {
            // CSV format: timestamp,req_id,result,size,data_hex
            file << header.timestamp << "," << header.req_id << "," 
                 << header.result << "," << header.size << ",";
            writeHexData(file, data, header.size);
            file << "\n";
        }
        file.flush();
    }
    
    void writeCSVHeaders() {
        if (dpdk_to_rpc_.is_open()) 
            dpdk_to_rpc_ << "timestamp,req_id,size,data_hex\n";
        if (rpc_to_app_.is_open()) 
            rpc_to_app_ << "timestamp,req_id,post_type,size,data_hex\n";
        if (app_to_rpc_.is_open()) 
            app_to_rpc_ << "timestamp,req_id,result,size,data_hex\n";
        if (rpc_to_dpdk_.is_open()) 
            rpc_to_dpdk_ << "timestamp,req_id,size,data_hex\n";
    }
    
    void writeHexData(std::ofstream& file, const void* data, uint16_t size) {
        if (!data || size == 0) return;
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
        for (uint16_t i = 0; i < size; i++) {
            file << std::hex << std::setfill('0') << std::setw(2) 
                 << static_cast<int>(bytes[i]);
        }
        file << std::dec;
    }
    
    void serializeArgs(const social_network::UniqueIdService_ComposeUniqueId_args& args, std::vector<uint8_t>& buffer) {
        // Calculate total size needed
        size_t carrier_data_size = 0;
        for (const auto& pair : args.carrier) {
            carrier_data_size += pair.first.size() + 1 + pair.second.size() + 1; // key=value;
        }
        
        struct __attribute__((packed)) SerializedArgs {
            int64_t req_id;
            int32_t post_type;
            uint16_t carrier_size;
        };
        
        buffer.resize(sizeof(SerializedArgs) + carrier_data_size);
        
        // Write header
        SerializedArgs* header = reinterpret_cast<SerializedArgs*>(buffer.data());
        header->req_id = args.req_id;
        header->post_type = args.post_type;
        header->carrier_size = carrier_data_size;
        
        // Write carrier data
        uint8_t* carrier_ptr = buffer.data() + sizeof(SerializedArgs);
        for (const auto& pair : args.carrier) {
            std::memcpy(carrier_ptr, pair.first.data(), pair.first.size());
            carrier_ptr += pair.first.size();
            *carrier_ptr++ = '=';
            std::memcpy(carrier_ptr, pair.second.data(), pair.second.size());
            carrier_ptr += pair.second.size();
            *carrier_ptr++ = ';';
        }
    }
    
    void serializeResult(const social_network::UniqueIdService_ComposeUniqueId_result& result, std::vector<uint8_t>& buffer) {
        struct __attribute__((packed)) SerializedResult {
            int64_t success;
            uint8_t success_isset;
        };
        
        buffer.resize(sizeof(SerializedResult));
        SerializedResult* header = reinterpret_cast<SerializedResult*>(buffer.data());
        header->success = result.success;
        header->success_isset = result.__isset.success ? 1 : 0;
    }

    std::ofstream dpdk_to_rpc_;
    std::ofstream rpc_to_app_;
    std::ofstream app_to_rpc_;
    std::ofstream rpc_to_dpdk_;
};


// Convenience macros
#define LOG_DPDK_TO_RPC(data, size) \
    PacketLogger::getInstance().logDpdkToRpc(data, size)

#define LOG_RPC_TO_APP(args) \
    PacketLogger::getInstance().logRpcToApp(args.req_id, args.post_type, args)

#define LOG_APP_TO_RPC(req_id, result_) \
    PacketLogger::getInstance().logAppToRpc(req_id, result_.success, result_)

#define LOG_RPC_TO_DPDK(data, size) \
    PacketLogger::getInstance().logRpcToDpdk(data, size)
#endif // PACKET_LOGGER_H
