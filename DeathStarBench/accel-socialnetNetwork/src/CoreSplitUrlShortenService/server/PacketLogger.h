#ifndef PACKET_LOGGER_H
#define PACKET_LOGGER_H

#include <fstream>
#include <string>
#include <chrono>
#include <vector>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include "../../../gen-cpp/UrlShortenService.h"

class PacketLogger {
public:
    static PacketLogger& getInstance() {
        static PacketLogger instance;
        return instance;
    }
    
    void initializeLogFiles(const std::string& dirName = "urlshorten_traces", bool binary_mode = true) {
        binary_mode_ = binary_mode;
        [[maybe_unused]] int result = system(("mkdir -p " + dirName).c_str()); 
        
        std::string ext = binary_mode ? ".bin" : ".csv";
        auto flags = binary_mode ? (std::ios::out | std::ios::binary) : std::ios::out;
        
        // dpdk_to_rpc_.open(dirName + "/dpdk_to_rpc" + ext, flags);
        rpc_to_app_.open(dirName + "/rpc_to_app" + ext, flags);
        app_to_rpc_.open(dirName + "/app_to_rpc" + ext, flags);
        // rpc_to_dpdk_.open(dirName + "/rpc_to_dpdk" + ext, flags);

        // dpdk_to_rpc.open(dirName + "/dpdk_to_rpc" + ".csv", std::ios::out);
        rpc_to_app.open(dirName + "/rpc_to_app" + ".csv", std::ios::out);
        app_to_rpc.open(dirName + "/app_to_rpc" + ".csv", std::ios::out);
        // rpc_to_dpdk.open(dirName + "/rpc_to_dpdk" + ".csv", std::ios::out);
        
        writeCSVHeaders();
    }
    
    void logDpdkToRpc(const void* data, uint16_t size) {
        writePacket(dpdk_to_rpc_, dpdk_to_rpc, 0, size, data);
    }
    
    // ComposeUrls logging
    void logRpcToApp(int64_t req_id, const social_network::UrlShortenService_ComposeUrls_args& args) {
        std::vector<uint8_t> buffer;
        serializeComposeUrlsArgs(args, buffer);
        
        ComposeUrlsHeader header = {getCurrentTimestamp(), req_id, static_cast<uint16_t>(args.urls.size()), static_cast<uint16_t>(buffer.size())};
        writeComposeUrlsPacket(rpc_to_app_, rpc_to_app, 0, header, buffer.data());
    }
    
    // GetExtendedUrls logging
    void logRpcToApp(int64_t req_id, const social_network::UrlShortenService_GetExtendedUrls_args& args) {
        std::vector<uint8_t> buffer;
        serializeGetExtendedUrlsArgs(args, buffer);
        
        GetExtendedUrlsHeader header = {getCurrentTimestamp(), req_id, static_cast<uint16_t>(args.shortened_urls.size()), static_cast<uint16_t>(buffer.size())};
        writeGetExtendedUrlsPacket(rpc_to_app_, rpc_to_app, 1, header, buffer.data());
    }
    
    void logAppToRpc(int64_t req_id, const social_network::UrlShortenService_ComposeUrls_result& res) {
        std::vector<uint8_t> buffer;
        serializeComposeUrlsResult(res, buffer);
        
        ComposeUrlsResponseHeader header = {getCurrentTimestamp(), req_id, static_cast<uint16_t>(res.success.size()), static_cast<uint16_t>(buffer.size())};
        writeComposeUrlsResponsePacket(app_to_rpc_, app_to_rpc, 0, header, buffer.data());
    }
    
    void logAppToRpc(int64_t req_id, const social_network::UrlShortenService_GetExtendedUrls_result& res) {
        std::vector<uint8_t> buffer;
        serializeGetExtendedUrlsResult(res, buffer);
        
        GetExtendedUrlsResponseHeader header = {getCurrentTimestamp(), req_id, static_cast<uint16_t>(res.success.size()), static_cast<uint16_t>(buffer.size())};
        writeGetExtendedUrlsResponsePacket(app_to_rpc_, app_to_rpc, 1, header, buffer.data());
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
    
    struct __attribute__((packed)) ComposeUrlsHeader {
        uint64_t timestamp;
        int64_t req_id;
        uint16_t url_count;
        uint16_t size;
    };
    
    struct __attribute__((packed)) ComposeUrlsResponseHeader {
        uint64_t timestamp;
        int64_t req_id;
        uint16_t url_count;
        uint16_t size;
    };
    
    struct __attribute__((packed)) GetExtendedUrlsHeader {
        uint64_t timestamp;
        int64_t req_id;
        uint16_t url_count;
        uint16_t size;
    };
    
    struct __attribute__((packed)) GetExtendedUrlsResponseHeader {
        uint64_t timestamp;
        int64_t req_id;
        uint16_t url_count;
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
        file2 << getCurrentTimestamp() << "," << req_id << "," << size << ",";
        writeHexData(file2, data, size);
        file2 << "\n";
        file2.flush();
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
            file << header.timestamp << "," << header.req_id << ",0," << header.size << ",";
            writeHexData(file, data, header.size);
            file << "\n";
        }
        file.flush();
    }
    
    void writeComposeUrlsPacket(std::ofstream& file, std::ofstream& csv, int op_type, const ComposeUrlsHeader& header, const void* data) {
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            if (data && header.size > 0)
                file.write(reinterpret_cast<const char*>(data), header.size);
            file.flush();
        }
        if (csv.is_open()) {
            csv << header.timestamp << "," << header.req_id << "," << op_type << ","
                << header.url_count << "," << header.size << ",";
            writeHexData(csv, data, header.size);
            csv << "\n";
            csv.flush();
        }
    }
    
    void writeComposeUrlsResponsePacket(std::ofstream& file, std::ofstream& csv, int op_type, const ComposeUrlsResponseHeader& header, const void* data) {
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            if (data && header.size > 0)
                file.write(reinterpret_cast<const char*>(data), header.size);
            file.flush();
        }
        if (csv.is_open()) {
            csv << header.timestamp << "," << header.req_id << "," << op_type << ","
                << header.url_count << "," << header.size << ",";
            writeHexData(csv, data, header.size);
            csv << "\n";
            csv.flush();
        }
    }
    
    void writeGetExtendedUrlsPacket(std::ofstream& file, std::ofstream& csv, int op_type, const GetExtendedUrlsHeader& header, const void* data) {
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            if (data && header.size > 0)
                file.write(reinterpret_cast<const char*>(data), header.size);
            file.flush();
        }
        if (csv.is_open()) {
            csv << header.timestamp << "," << header.req_id << "," << op_type << ","
                << header.url_count << "," << header.size << ",";
            writeHexData(csv, data, header.size);
            csv << "\n";
            csv.flush();
        }
    }
    
    void writeGetExtendedUrlsResponsePacket(std::ofstream& file, std::ofstream& csv, int op_type, const GetExtendedUrlsResponseHeader& header, const void* data) {
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            if (data && header.size > 0)
                file.write(reinterpret_cast<const char*>(data), header.size);
            file.flush();
        }
        if (csv.is_open()) {
            csv << header.timestamp << "," << header.req_id << "," << op_type << ","
                << header.url_count << "," << header.size << ",";
            writeHexData(csv, data, header.size);
            csv << "\n";
            csv.flush();
        }
    }
    
    void writeCSVHeaders() {
        if (dpdk_to_rpc.is_open())
            dpdk_to_rpc << "timestamp,req_id,size,data_hex\n";
        if (rpc_to_app.is_open())
            rpc_to_app << "timestamp,req_id,operation_type,url_count,size,data_hex\n";
        if (app_to_rpc.is_open())
            app_to_rpc << "timestamp,req_id,operation_type,url_count,size,data_hex\n";
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
    
    void serializeComposeUrlsArgs(const social_network::UrlShortenService_ComposeUrls_args& args, std::vector<uint8_t>& buffer) {
        struct __attribute__((packed)) SerializedComposeUrlsArgs {
            int64_t req_id;
            uint16_t url_count;
            uint16_t carrier_size;
        };
        
        size_t carrier_data_size = 0;
        for (const auto& pair : args.carrier) {
            carrier_data_size += pair.first.size() + 1 + pair.second.size() + 1;
        }
        
        buffer.resize(sizeof(SerializedComposeUrlsArgs) + carrier_data_size);
        
        SerializedComposeUrlsArgs* header = reinterpret_cast<SerializedComposeUrlsArgs*>(buffer.data());
        header->req_id = args.req_id;
        header->url_count = args.urls.size();
        header->carrier_size = carrier_data_size;
        
        uint8_t* carrier_ptr = buffer.data() + sizeof(SerializedComposeUrlsArgs);
        for (const auto& pair : args.carrier) {
            std::memcpy(carrier_ptr, pair.first.data(), pair.first.size());
            carrier_ptr += pair.first.size();
            *carrier_ptr++ = '=';
            std::memcpy(carrier_ptr, pair.second.data(), pair.second.size());
            carrier_ptr += pair.second.size();
            *carrier_ptr++ = ';';
        }
    }
    
    void serializeComposeUrlsResult(const social_network::UrlShortenService_ComposeUrls_result& result, std::vector<uint8_t>& buffer) {
        struct __attribute__((packed)) SerializedComposeUrlsResult {
            uint16_t url_count;
            uint8_t success_isset;
        };
        
        buffer.resize(sizeof(SerializedComposeUrlsResult));
        SerializedComposeUrlsResult* header = reinterpret_cast<SerializedComposeUrlsResult*>(buffer.data());
        header->url_count = result.success.size();
        header->success_isset = result.__isset.success ? 1 : 0;
    }
    
    void serializeGetExtendedUrlsArgs(const social_network::UrlShortenService_GetExtendedUrls_args& args, std::vector<uint8_t>& buffer) {
        struct __attribute__((packed)) SerializedGetExtendedUrlsArgs {
            int64_t req_id;
            uint16_t url_count;
            uint16_t carrier_size;
        };
        
        size_t carrier_data_size = 0;
        for (const auto& pair : args.carrier) {
            carrier_data_size += pair.first.size() + 1 + pair.second.size() + 1;
        }
        
        buffer.resize(sizeof(SerializedGetExtendedUrlsArgs) + carrier_data_size);
        
        SerializedGetExtendedUrlsArgs* header = reinterpret_cast<SerializedGetExtendedUrlsArgs*>(buffer.data());
        header->req_id = args.req_id;
        header->url_count = args.shortened_urls.size();
        header->carrier_size = carrier_data_size;
        
        uint8_t* carrier_ptr = buffer.data() + sizeof(SerializedGetExtendedUrlsArgs);
        for (const auto& pair : args.carrier) {
            std::memcpy(carrier_ptr, pair.first.data(), pair.first.size());
            carrier_ptr += pair.first.size();
            *carrier_ptr++ = '=';
            std::memcpy(carrier_ptr, pair.second.data(), pair.second.size());
            carrier_ptr += pair.second.size();
            *carrier_ptr++ = ';';
        }
    }
    
    void serializeGetExtendedUrlsResult(const social_network::UrlShortenService_GetExtendedUrls_result& result, std::vector<uint8_t>& buffer) {
        struct __attribute__((packed)) SerializedGetExtendedUrlsResult {
            uint16_t url_count;
            uint8_t success_isset;
        };
        
        buffer.resize(sizeof(SerializedGetExtendedUrlsResult));
        SerializedGetExtendedUrlsResult* header = reinterpret_cast<SerializedGetExtendedUrlsResult*>(buffer.data());
        header->url_count = result.success.size();
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
