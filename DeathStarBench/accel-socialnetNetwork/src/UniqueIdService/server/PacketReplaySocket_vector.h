#ifndef PACKET_REPLAY_SOCKET_H
#define PACKET_REPLAY_SOCKET_H

#include <vector>
#include <cstdint>
#include <fstream>
#include <chrono>
#include <cstring>
#include <iostream>

template <typename T, std::size_t Alignment>
struct aligned_allocator {
    using value_type = T;

    T* allocate(std::size_t n) {
        void* ptr;
        if (posix_memalign(&ptr, Alignment, n * sizeof(T)) != 0) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(ptr);
    }

//    T* allocate(std::size_t n) {
//        void* ptr = std::aligned_alloc(Alignment, n * sizeof(T));
//        return static_cast<T*>(ptr);
//    }
 
    void deallocate(T* p, std::size_t n) {
        std::free(p);
    }
    
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    
    template <typename U>
    struct rebind {
        using other = aligned_allocator<U, Alignment>;
    };
};

class PacketReplaySocket {
public:
    std::vector<uint8_t, aligned_allocator<uint8_t, 64>> alignedRpcData;
    std::vector<uint8_t, aligned_allocator<uint8_t, 64>> alignedRespData;
    
private:
    size_t read_pos_{0};
    size_t write_pos_{0};
    bool eof_reached_{false};

public:
    void loadTrace(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "Failed to open trace file: " << filename << std::endl;
            return;
        }
        
        size_t file_size = file.tellg();
        file.seekg(0);
        
        std::vector<uint8_t> raw_data(file_size);
        file.read(reinterpret_cast<char*>(raw_data.data()), file_size);
        
        // Convert binary log to aligned format
        convertToAlignedFormat(raw_data);
        
        std::cout << "Loaded trace: " << alignedRpcData.size() << " bytes" << std::endl;
        std::cout << "Aligned RPC data address: 0x" << std::hex 
                  << reinterpret_cast<uintptr_t>(alignedRpcData.data()) << std::dec << std::endl;
    }    

    bool validateReplay(const std::string& expected_file) {
        std::ifstream expected(expected_file, std::ios::binary | std::ios::ate);
        if (!expected.is_open()) {
            printf("JU:JU Failed to open expected file\n");
            return false;
        }
        
        size_t file_size = expected.tellg();
        //printf("JU:JU Expected file size: %zu bytes\n", file_size);
        //printf("JU:JU Response data size: %zu bytes\n", alignedRespData.size());
        
        expected.seekg(0);
        std::vector<uint8_t> expected_data(file_size);
        expected.read(reinterpret_cast<char*>(expected_data.data()), file_size);
        
        size_t resp_pos = 0, exp_pos = 0;
        int entry_count = 0;
        
        while (resp_pos < alignedRespData.size() && exp_pos < expected_data.size()) {
            uint16_t resp_size = *reinterpret_cast<uint16_t*>(alignedRespData.data() + resp_pos + 8);
            uint16_t exp_size = *reinterpret_cast<uint16_t*>(expected_data.data() + exp_pos + 16); // Fix: BasicHeader size field offset

            //printf("JU:JU Entry %d - resp_size: %u, exp_size: %u\n", entry_count, resp_size, exp_size);
 
            //printf("JU:JU Entry %d First 20 raw bytes:\n", entry_count);
            //printf("  resp: ");
            //for(int i=0; i<20; i++) printf("%02x ", alignedRespData[resp_pos + i]);
            //printf("\n  exp:  ");
            //for(int i=0; i<20; i++) printf("%02x ", expected_data[exp_pos + i]);
            //printf("\n");
           
            if (resp_size != exp_size) {
                printf("JU:JU Size mismatch at entry %d\n", entry_count);
                return false;
            }

            // Compare data (can't do this because uniqueid generated during replay depends on timestamp. Thus, it is different.)
            //if (memcmp(alignedRespData.data() + resp_pos + 10,
            //      expected_data.data() + exp_pos + 18, resp_size) != 0) return false;
            
            entry_count++;
            resp_pos += 10 + resp_size;
            resp_pos = (resp_pos + 63) & ~63;
            exp_pos += 18 + exp_size;
        }
        
        //printf("JU:JU Validated %d entries successfully\n", entry_count);
        return true;
    }

    uint32_t read(uint8_t* buf, uint32_t max_len) {
        if (read_pos_ + sizeof(uint64_t) + sizeof(uint16_t) > alignedRpcData.size()) {
            std::cout << "No more data to read.\n";
            return 0;
        }

        // Read timestamp and length
        uint64_t timestamp = *reinterpret_cast<const uint64_t*>(alignedRpcData.data() + read_pos_);
        uint16_t pkt_len = *reinterpret_cast<const uint16_t*>(alignedRpcData.data() + read_pos_ + sizeof(uint64_t));

        uint32_t copy_len = std::min(max_len, static_cast<uint32_t>(pkt_len));

        // Copy packet data
        std::memcpy(buf, alignedRpcData.data() + read_pos_ + sizeof(uint64_t) + sizeof(uint16_t), copy_len);

        // Move to next aligned position
        size_t total_size = sizeof(uint64_t) + sizeof(uint16_t) + pkt_len;
        read_pos_ += total_size;
        read_pos_ = (read_pos_ + 63) & ~63;  // 64-byte align

        // Check if EOF reached
        if (read_pos_ >= alignedRpcData.size()) {
            eof_reached_ = true;
            //std::cout << "End of replay file reached. Set EOF flag.\n";
        } 
        
        return copy_len;
    }

    uint32_t write(const uint8_t* buf, uint32_t len) {
        size_t total_size = sizeof(uint64_t) + sizeof(uint16_t) + len;

        if (write_pos_ + total_size > alignedRespData.size()) {
            alignedRespData.resize(write_pos_ + total_size + 64);
        }

        // Write timestamp, length, and data
        uint64_t timestamp = getCurrentTimestamp();
        *reinterpret_cast<uint64_t*>(alignedRespData.data() + write_pos_) = timestamp;
        *reinterpret_cast<uint16_t*>(alignedRespData.data() + write_pos_ + sizeof(uint64_t)) = len;
        std::memcpy(alignedRespData.data() + write_pos_ + sizeof(uint64_t) + sizeof(uint16_t), buf, len);

        write_pos_ += total_size;
        write_pos_ = (write_pos_ + 63) & ~63;  // 64-byte align

        return len;
    }
    
    bool isEOF() const { return eof_reached_; }
    
private:
    void convertToAlignedFormat(const std::vector<uint8_t>& raw_data) {
        // Parse binary log format: BasicHeader + data
        struct BasicHeader {
            uint64_t timestamp;
            int64_t req_id;
            uint16_t size;
        } __attribute__((packed));
        
        size_t pos = 0;
        size_t aligned_pos = 0;
        
        while (pos + sizeof(BasicHeader) <= raw_data.size()) {
            const BasicHeader* header = reinterpret_cast<const BasicHeader*>(raw_data.data() + pos);
            
            if (pos + sizeof(BasicHeader) + header->size > raw_data.size()) break;
            
            size_t total_size = sizeof(uint64_t) + sizeof(uint16_t) + header->size;
            alignedRpcData.resize(aligned_pos + total_size);
            
            // Write timestamp, size, data
            *reinterpret_cast<uint64_t*>(alignedRpcData.data() + aligned_pos) = header->timestamp;
            *reinterpret_cast<uint16_t*>(alignedRpcData.data() + aligned_pos + sizeof(uint64_t)) = header->size;
            std::memcpy(alignedRpcData.data() + aligned_pos + sizeof(uint64_t) + sizeof(uint16_t),
                       raw_data.data() + pos + sizeof(BasicHeader), header->size);
            
            pos += sizeof(BasicHeader) + header->size;
            aligned_pos += total_size;
            aligned_pos = (aligned_pos + 63) & ~63;  // 64-byte align
        }
        
        alignedRpcData.resize(aligned_pos);
    }
    
    uint64_t getCurrentTimestamp() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }
};

#endif // PACKET_REPLAY_SOCKET_H
