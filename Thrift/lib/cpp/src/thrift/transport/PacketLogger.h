#ifndef PACKET_LOGGER_H
#define PACKET_LOGGER_H

#include <fstream>
#include <string>
#include <chrono>
#include <vector>
#include <cstdint>
#include <cstring>
#include <iomanip>

class PacketLogger {
public:
    static PacketLogger& getInstance() {
        static PacketLogger instance;
        return instance;
    }
    
    void initializeLogFiles(const std::string& dirName = "uniqueid_traces", bool binary_mode = true) {
        binary_mode_ = binary_mode;
        system(("mkdir -p " + dirName).c_str());
        
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
    
    void logRpcToApp(int64_t req_id, int32_t post_type, const void* data, uint16_t size) {
        AppHeader header = {getCurrentTimestamp(), req_id, post_type, size};
        writeAppPacket(rpc_to_app_, header, data);
    }
    
    void logAppToRpc(int64_t req_id, int64_t result, const void* data, uint16_t size) {
        ResponseHeader header = {getCurrentTimestamp(), req_id, result, size};
        writeResponsePacket(app_to_rpc_, header, data);
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
    
    std::ofstream dpdk_to_rpc_;
    std::ofstream rpc_to_app_;
    std::ofstream app_to_rpc_;
    std::ofstream rpc_to_dpdk_;
};

// Convenience macros
#define LOG_DPDK_TO_RPC(data, size) \
    PacketLogger::getInstance().logDpdkToRpc(data, size)

#define LOG_RPC_TO_APP(req_id, post_type, data, size) \
    PacketLogger::getInstance().logRpcToApp(req_id, post_type, data, size)

#define LOG_APP_TO_RPC(req_id, result, data, size) \
    PacketLogger::getInstance().logAppToRpc(req_id, result, data, size)

#define LOG_RPC_TO_DPDK(data, size) \
    PacketLogger::getInstance().logRpcToDpdk(data, size)

#endif // PACKET_LOGGER_H
