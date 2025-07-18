#ifndef PACKET_LOGGER_H
#define PACKET_LOGGER_H

#include <fstream>
#include <string>
#include <chrono>
#include <vector>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include "../../../gen-cpp/PostStorageService.h"

class PacketLogger {
public:
    static PacketLogger& getInstance() {
        static PacketLogger instance;
        return instance;
    }
    
    void initializeLogFiles(const std::string& dirName = "poststorage_traces", bool binary_mode = true) {
        binary_mode_ = binary_mode;
        [[maybe_unused]] int result = system(("mkdir -p " + dirName).c_str()); 
        
        std::string ext = binary_mode ? ".bin" : ".csv";
        auto flags = binary_mode ? (std::ios::out | std::ios::binary) : std::ios::out;
        
        dpdk_to_rpc_.open(dirName + "/dpdk_to_rpc_1k" + ext, flags);
        rpc_to_app_.open(dirName + "/rpc_to_app_1k" + ext, flags);
        app_to_rpc_.open(dirName + "/app_to_rpc_1k" + ext, flags);
        rpc_to_dpdk_.open(dirName + "/rpc_to_dpdk_1k" + ext, flags);

        dpdk_to_rpc.open(dirName + "/dpdk_to_rpc_1k" + ".csv", std::ios::out);
        rpc_to_app.open(dirName + "/rpc_to_app_1k" + ".csv", std::ios::out);
        app_to_rpc.open(dirName + "/app_to_rpc_1k" + ".csv", std::ios::out);
        rpc_to_dpdk.open(dirName + "/rpc_to_dpdk_1k" + ".csv", std::ios::out);
        
        // if (!binary_mode) {
            writeCSVHeaders();
        // }
    }
    
    void logDpdkToRpc(const void* data, uint16_t size) {
        writePacket(dpdk_to_rpc_, dpdk_to_rpc, 0, size, data);
    }
    
    // StorePost logging
    void logRpcToApp(int64_t req_id, const social_network::PostStorageService_StorePost_args& args) {
        std::vector<uint8_t> buffer;
        serializeStorePostArgs(args, buffer);
        
        StorePostHeader header = {getCurrentTimestamp(), req_id, args.post.post_id, static_cast<uint16_t>(buffer.size())};
        writeStorePostPacket(rpc_to_app_, header, buffer.data());
    }
    
    // ReadPost logging
    void logRpcToApp(int64_t req_id, const social_network::PostStorageService_ReadPost_args& args) {
        std::vector<uint8_t> buffer;
        serializeReadPostArgs(args, buffer);
        
        ReadPostHeader header = {getCurrentTimestamp(), req_id, args.post_id, static_cast<uint16_t>(buffer.size())};
        writeReadPostPacket(rpc_to_app_, header, buffer.data());
    }
    
    // ReadPosts logging
    void logRpcToApp(int64_t req_id, const social_network::PostStorageService_ReadPosts_args& args) {
        std::vector<uint8_t> buffer;
        serializeReadPostsArgs(args, buffer);
        
        ReadPostsHeader header = {getCurrentTimestamp(), req_id, static_cast<uint16_t>(args.post_ids.size()), static_cast<uint16_t>(buffer.size())};
        writeReadPostsPacket(rpc_to_app_, header, buffer.data());
    }
    
    void logAppToRpc(int64_t req_id, const social_network::PostStorageService_StorePost_result& res) {
        std::vector<uint8_t> buffer;
        serializeStorePostResult(res, buffer);
        
        BasicHeader header = {getCurrentTimestamp(), req_id, static_cast<uint16_t>(buffer.size())};
        writeBasicPacket(app_to_rpc_, header, buffer.data());
    }
    
    void logAppToRpc(int64_t req_id, const social_network::PostStorageService_ReadPost_result& res) {
        std::vector<uint8_t> buffer;
        serializeReadPostResult(res, buffer);
        
        ReadPostResponseHeader header = {getCurrentTimestamp(), req_id, res.success.post_id, static_cast<uint16_t>(buffer.size())};
        writeReadPostResponsePacket(app_to_rpc_, header, buffer.data());
    }
    
    void logAppToRpc(int64_t req_id, const social_network::PostStorageService_ReadPosts_result& res) {
        std::vector<uint8_t> buffer;
        serializeReadPostsResult(res, buffer);
        
        ReadPostsResponseHeader header = {getCurrentTimestamp(), req_id, static_cast<uint16_t>(res.success.size()), static_cast<uint16_t>(buffer.size())};
        writeReadPostsResponsePacket(app_to_rpc_, header, buffer.data());
    }
    
    void logRpcToDpdk(const void* data, uint16_t size) {
        writePacket(rpc_to_dpdk_, rpc_to_dpdk, 0, size, data);
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
    
    // Packed structs for binary format
    struct __attribute__((packed)) BasicHeader {
        uint64_t timestamp;
        int64_t req_id;
        uint16_t size;
    };
    
    struct __attribute__((packed)) StorePostHeader {
        uint64_t timestamp;
        int64_t req_id;
        int64_t post_id;
        uint16_t size;
    };
    
    struct __attribute__((packed)) ReadPostHeader {
        uint64_t timestamp;
        int64_t req_id;
        int64_t post_id;
        uint16_t size;
    };
    
    struct __attribute__((packed)) ReadPostResponseHeader {
        uint64_t timestamp;
        int64_t req_id;
        int64_t post_id;
        uint16_t size;
    };
    
    struct __attribute__((packed)) ReadPostsHeader {
        uint64_t timestamp;
        int64_t req_id;
        uint16_t post_count;
        uint16_t size;
    };
    
    struct __attribute__((packed)) ReadPostsResponseHeader {
        uint64_t timestamp;
        int64_t req_id;
        uint16_t post_count;
        uint16_t size;
    };
    
    uint64_t getCurrentTimestamp() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }
    
    void writePacket(std::ofstream& file, std::ofstream& file2, int64_t req_id, uint16_t size, const void* data) {
        if (!file.is_open() || !file2.is_open()) return;
        
        if (binary_mode_) {
            BasicHeader header = {getCurrentTimestamp(), req_id, size};
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            if (data && size > 0) {
                file.write(reinterpret_cast<const char*>(data), size);
            }
        }
        // } else {
            file2 << getCurrentTimestamp() << "," << req_id << "," << size << ",";
            writeHexData(file2, data, size);
            file2 << "\n";
            file2.flush();
        // }
        file.flush();
    }
    
    void writeBasicPacket(std::ofstream& file, const BasicHeader& header, const void* data) {
        if (!file.is_open()) return;
        
        if (binary_mode_) {
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            if (data && header.size > 0) {
                file.write(reinterpret_cast<const char*>(data), header.size);
            }
        } else {
            // CSV format: timestamp,req_id,operation_type,size,data_hex  
            file << header.timestamp << "," << header.req_id << ",0," << header.size << ",";
            writeHexData(file, data, header.size);
            file << "\n";
        }
        file.flush();
    }
    
    void writeStorePostPacket(std::ofstream& file, const StorePostHeader& header, const void* data) {
        if (!file.is_open()) return;
        
        if (binary_mode_) {
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            if (data && header.size > 0) {
                file.write(reinterpret_cast<const char*>(data), header.size);
            }
        } else {
            // CSV format: timestamp,req_id,operation_type,post_id,size,data_hex
            file << header.timestamp << "," << header.req_id << ",0," 
                 << header.post_id << "," << header.size << ",";
            writeHexData(file, data, header.size);
            file << "\n";
        }
        file.flush();
    }
    
    void writeReadPostPacket(std::ofstream& file, const ReadPostHeader& header, const void* data) {
        if (!file.is_open()) return;
        
        if (binary_mode_) {
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            if (data && header.size > 0) {
                file.write(reinterpret_cast<const char*>(data), header.size);
            }
        } else {
            // CSV format: timestamp,req_id,operation_type,post_id,size,data_hex
            file << header.timestamp << "," << header.req_id << ",1," 
                 << header.post_id << "," << header.size << ",";
            writeHexData(file, data, header.size);
            file << "\n";
        }
        file.flush();
    }
    
    void writeReadPostResponsePacket(std::ofstream& file, const ReadPostResponseHeader& header, const void* data) {
        if (!file.is_open()) return;
        
        if (binary_mode_) {
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            if (data && header.size > 0) {
                file.write(reinterpret_cast<const char*>(data), header.size);
            }
        } else {
            // CSV format: timestamp,req_id,operation_type,post_id,size,data_hex
            file << header.timestamp << "," << header.req_id << ",1," 
                 << header.post_id << "," << header.size << ",";
            writeHexData(file, data, header.size);
            file << "\n";
        }
        file.flush();
    }
    
    void writeReadPostsPacket(std::ofstream& file, const ReadPostsHeader& header, const void* data) {
        if (!file.is_open()) return;
        
        if (binary_mode_) {
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            if (data && header.size > 0) {
                file.write(reinterpret_cast<const char*>(data), header.size);
            }
        } else {
            // CSV format: timestamp,req_id,operation_type,post_count,size,data_hex
            file << header.timestamp << "," << header.req_id << ",2," 
                 << header.post_count << "," << header.size << ",";
            writeHexData(file, data, header.size);
            file << "\n";
        }
        file.flush();
    }
    
    void writeReadPostsResponsePacket(std::ofstream& file, const ReadPostsResponseHeader& header, const void* data) {
        if (!file.is_open()) return;
        
        if (binary_mode_) {
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            if (data && header.size > 0) {
                file.write(reinterpret_cast<const char*>(data), header.size);
            }
        } else {
            // CSV format: timestamp,req_id,operation_type,post_count,size,data_hex
            file << header.timestamp << "," << header.req_id << ",2," 
                 << header.post_count << "," << header.size << ",";
            writeHexData(file, data, header.size);
            file << "\n";
        }
        file.flush();
    }
    
    void writeCSVHeaders() {
        if (dpdk_to_rpc.is_open()) 
            dpdk_to_rpc << "timestamp,req_id,size,data_hex\n";
        if (rpc_to_app.is_open()) 
            rpc_to_app << "timestamp,req_id,operation_type,post_id_or_count,size,data_hex\n";
        if (app_to_rpc.is_open()) 
            app_to_rpc << "timestamp,req_id,operation_type,post_id_or_count,size,data_hex\n";
        if (rpc_to_dpdk.is_open()) 
            rpc_to_dpdk << "timestamp,req_id,size,data_hex\n";
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
    
    void serializeStorePostArgs(const social_network::PostStorageService_StorePost_args& args, std::vector<uint8_t>& buffer) {
        struct __attribute__((packed)) SerializedStorePostArgs {
            int64_t req_id;
            int64_t post_id;
            uint16_t carrier_size;
        };
        
        size_t carrier_data_size = 0;
        for (const auto& pair : args.carrier) {
            carrier_data_size += pair.first.size() + 1 + pair.second.size() + 1;
        }
        
        buffer.resize(sizeof(SerializedStorePostArgs) + carrier_data_size);
        
        SerializedStorePostArgs* header = reinterpret_cast<SerializedStorePostArgs*>(buffer.data());
        header->req_id = args.req_id;
        header->post_id = args.post.post_id;
        header->carrier_size = carrier_data_size;
        
        uint8_t* carrier_ptr = buffer.data() + sizeof(SerializedStorePostArgs);
        for (const auto& pair : args.carrier) {
            std::memcpy(carrier_ptr, pair.first.data(), pair.first.size());
            carrier_ptr += pair.first.size();
            *carrier_ptr++ = '=';
            std::memcpy(carrier_ptr, pair.second.data(), pair.second.size());
            carrier_ptr += pair.second.size();
            *carrier_ptr++ = ';';
        }
    }
    
    void serializeStorePostResult(const social_network::PostStorageService_StorePost_result& result, std::vector<uint8_t>& buffer) {
        buffer.resize(1);
        buffer[0] = 1; // Success indicator
    }
    
    void serializeReadPostArgs(const social_network::PostStorageService_ReadPost_args& args, std::vector<uint8_t>& buffer) {
        struct __attribute__((packed)) SerializedReadPostArgs {
            int64_t req_id;
            int64_t post_id;
            uint16_t carrier_size;
        };
        
        size_t carrier_data_size = 0;
        for (const auto& pair : args.carrier) {
            carrier_data_size += pair.first.size() + 1 + pair.second.size() + 1;
        }
        
        buffer.resize(sizeof(SerializedReadPostArgs) + carrier_data_size);
        
        SerializedReadPostArgs* header = reinterpret_cast<SerializedReadPostArgs*>(buffer.data());
        header->req_id = args.req_id;
        header->post_id = args.post_id;
        header->carrier_size = carrier_data_size;
        
        uint8_t* carrier_ptr = buffer.data() + sizeof(SerializedReadPostArgs);
        for (const auto& pair : args.carrier) {
            std::memcpy(carrier_ptr, pair.first.data(), pair.first.size());
            carrier_ptr += pair.first.size();
            *carrier_ptr++ = '=';
            std::memcpy(carrier_ptr, pair.second.data(), pair.second.size());
            carrier_ptr += pair.second.size();
            *carrier_ptr++ = ';';
        }
    }
    
    void serializeReadPostResult(const social_network::PostStorageService_ReadPost_result& result, std::vector<uint8_t>& buffer) {
        struct __attribute__((packed)) SerializedReadPostResult {
            int64_t post_id;
            uint8_t success_isset;
        };
        
        buffer.resize(sizeof(SerializedReadPostResult));
        SerializedReadPostResult* header = reinterpret_cast<SerializedReadPostResult*>(buffer.data());
        header->post_id = result.success.post_id;
        header->success_isset = result.__isset.success ? 1 : 0;
    }
    
    void serializeReadPostsArgs(const social_network::PostStorageService_ReadPosts_args& args, std::vector<uint8_t>& buffer) {
        struct __attribute__((packed)) SerializedReadPostsArgs {
            int64_t req_id;
            uint16_t post_count;
            uint16_t carrier_size;
        };
        
        size_t carrier_data_size = 0;
        for (const auto& pair : args.carrier) {
            carrier_data_size += pair.first.size() + 1 + pair.second.size() + 1;
        }
        
        size_t post_ids_size = args.post_ids.size() * sizeof(int64_t);
        
        buffer.resize(sizeof(SerializedReadPostsArgs) + post_ids_size + carrier_data_size);
        
        SerializedReadPostsArgs* header = reinterpret_cast<SerializedReadPostsArgs*>(buffer.data());
        header->req_id = args.req_id;
        header->post_count = args.post_ids.size();
        header->carrier_size = carrier_data_size;
        
        uint8_t* data_ptr = buffer.data() + sizeof(SerializedReadPostsArgs);
        
        // Write post_ids
        for (auto post_id : args.post_ids) {
            *reinterpret_cast<int64_t*>(data_ptr) = post_id;
            data_ptr += sizeof(int64_t);
        }
        
        // Write carrier
        for (const auto& pair : args.carrier) {
            std::memcpy(data_ptr, pair.first.data(), pair.first.size());
            data_ptr += pair.first.size();
            *data_ptr++ = '=';
            std::memcpy(data_ptr, pair.second.data(), pair.second.size());
            data_ptr += pair.second.size();
            *data_ptr++ = ';';
        }
    }
    
    void serializeReadPostsResult(const social_network::PostStorageService_ReadPosts_result& result, std::vector<uint8_t>& buffer) {
        struct __attribute__((packed)) SerializedReadPostsResult {
            uint16_t post_count;
            uint8_t success_isset;
        };
        
        buffer.resize(sizeof(SerializedReadPostsResult));
        SerializedReadPostsResult* header = reinterpret_cast<SerializedReadPostsResult*>(buffer.data());
        header->post_count = result.success.size();
        header->success_isset = result.__isset.success ? 1 : 0;
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

// Convenience macros
#define LOG_DPDK_TO_RPC(data, size) \
    PacketLogger::getInstance().logDpdkToRpc(data, size)

#define LOG_RPC_TO_APP(args) \
    PacketLogger::getInstance().logRpcToApp(args.req_id, args)

#define LOG_APP_TO_RPC(req_id, result_) \
    PacketLogger::getInstance().logAppToRpc(req_id, result_)

#define LOG_RPC_TO_DPDK(data, size) \
    PacketLogger::getInstance().logRpcToDpdk(data, size)

#endif // PACKET_LOGGER_H
