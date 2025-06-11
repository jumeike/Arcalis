// PacketLogger.h
#pragma once
#include <fstream>
#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>

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

    void logDPDKToRPC(const uint8_t* data, size_t len) {
        if (dpdk_to_rpc_.is_open()) {
            writePacket(dpdk_to_rpc_, data, len);
        }
    }

    void logRPCToLogic(const uint8_t* data, size_t len) {
        if (rpc_to_logic_.is_open()) {
            writePacket(rpc_to_logic_, data, len);
        }
    }

    void logLogicToRPC(const uint8_t* data, size_t len) {
        if (logic_to_rpc_.is_open()) {
            writePacket(logic_to_rpc_, data, len);
        }
    }

    void logRPCToDPDK(const uint8_t* data, size_t len) {
        if (rpc_to_dpdk_.is_open()) {
            writePacket(rpc_to_dpdk_, data, len);
        }
    }

    ~PacketLogger() {
        dpdk_to_rpc_.close();
        rpc_to_logic_.close();
        logic_to_rpc_.close();
        rpc_to_dpdk_.close();
    }

private:
    PacketLogger() {} // Private constructor for singleton
    
    void writePacket(std::ofstream& file, const uint8_t* data, size_t len) {
        // Write timestamp
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        auto now_ms = std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count() % 1000000;
        
        // Write packet header: [timestamp(8 bytes)][length(4 bytes)]
        uint64_t timestamp = static_cast<uint64_t>(now_c) * 1000000 + now_ms;
        uint32_t length = static_cast<uint32_t>(len);
        
        file.write(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
        file.write(reinterpret_cast<char*>(&length), sizeof(length));
        file.write(reinterpret_cast<const char*>(data), len);
        file.flush();
    }

    std::ofstream dpdk_to_rpc_;
    std::ofstream rpc_to_logic_;
    std::ofstream logic_to_rpc_;
    std::ofstream rpc_to_dpdk_;
};
