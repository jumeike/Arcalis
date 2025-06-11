// PacketLogger.h
#pragma once
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <cstdint>

class PacketLogger {
public:
    static PacketLogger& getInstance() {
        static PacketLogger instance;
        return instance;
    }

    void initializeLogFiles(const std::string& baseDir) {
        dpdk_to_rpc_.open(baseDir + "/dpdk_to_rpc.log", std::ios::out | std::ios::binary);
        rpc_to_logic_.open(baseDir + "/rpc_to_logic.log", std::ios::out | std::ios::binary);
        logic_to_rpc_.open(baseDir + "/logic_to_rpc.log", std::ios::out | std::ios::binary);
        rpc_to_dpdk_.open(baseDir + "/rpc_to_dpdk.log", std::ios::out | std::ios::binary);
    }

    void logDPDKToRPC(const uint8_t* data, uint16_t len) {
        writePacket(dpdk_to_rpc_, reinterpret_cast<const int8_t*>(data), len);
    }

    void logRPCToLogic(const uint8_t* data, uint16_t len) {
        writePacket(rpc_to_logic_, reinterpret_cast<const int8_t*>(data), len);
    }

    void logRPCToLogic(const std::vector<int8_t>& data, uint16_t keyLen, 
                       uint16_t valueLen, bool isGet) {
        if (!rpc_to_logic_.is_open()) return;
        
        EnhancedHeader header;
        header.timestamp = getCurrentTimestamp();
        header.keyLength = keyLen;
        header.valueLength = valueLen;
        header.requestType = isGet ? 1 : 0;
        
        rpc_to_logic_.write(reinterpret_cast<const char*>(&header), sizeof(header));
        rpc_to_logic_.write(reinterpret_cast<const char*>(data.data()), data.size());
        rpc_to_logic_.flush();
    }

    void logLogicToRPC(const uint8_t* data, uint16_t len) {
        writePacket(logic_to_rpc_, reinterpret_cast<const int8_t*>(data), len);
    }

    void logLogicToRPC(const std::vector<int8_t>& data, uint16_t keyLen, 
                       uint16_t valueLen, bool isGet) {
        if (!logic_to_rpc_.is_open()) return;
        
        EnhancedHeader header;
        header.timestamp = getCurrentTimestamp();
        header.keyLength = keyLen;
        header.valueLength = valueLen;
        header.requestType = isGet ? 1 : 0;
        
        logic_to_rpc_.write(reinterpret_cast<const char*>(&header), sizeof(header));
        logic_to_rpc_.write(reinterpret_cast<const char*>(data.data()), data.size());
        logic_to_rpc_.flush();
    }

    void logRPCToDPDK(const uint8_t* data, uint16_t len) {
        writePacket(rpc_to_dpdk_, reinterpret_cast<const int8_t*>(data), len);
    }

    ~PacketLogger() {
        dpdk_to_rpc_.close();
        rpc_to_logic_.close();
        logic_to_rpc_.close();
        rpc_to_dpdk_.close();
    }

private:
    PacketLogger() {} // Private constructor for singleton

    struct EnhancedHeader {
        uint64_t timestamp;
        uint16_t keyLength;
        uint16_t valueLength;
        uint8_t requestType;  // SET = 0, GET = 1
    };

    uint64_t getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        auto now_ms = std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count() % 1000000;
        return static_cast<uint64_t>(now_c) * 1000000 + now_ms;
    }
    
    void writePacket(std::ofstream& file, const int8_t* data, uint16_t len) {
        if (!file.is_open()) return;
        
        uint64_t timestamp = getCurrentTimestamp();
        uint16_t length = len;
        
        file.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
        file.write(reinterpret_cast<const char*>(&length), sizeof(length));
        file.write(reinterpret_cast<const char*>(data), len);
        file.flush();
    }

    std::ofstream dpdk_to_rpc_;
    std::ofstream rpc_to_logic_;
    std::ofstream logic_to_rpc_;
    std::ofstream rpc_to_dpdk_;
};