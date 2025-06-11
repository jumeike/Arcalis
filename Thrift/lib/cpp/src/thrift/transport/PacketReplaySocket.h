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
private:
    static constexpr size_t FIXED_BUFFER_SIZE = 64 * 1024 * 1024; // 64MB

    uint8_t* raw_recv_buf_;
    uint8_t* raw_resp_buf_;
    uint8_t* recv_buf_;    // Fixed aligned buffer
    uint8_t* resp_buf_;    // Fixed aligned buffer

    size_t recv_data_size_;
    size_t resp_data_size_;
    size_t read_pos_{0};
    size_t write_pos_{0};
    bool eof_reached_{false};

public:
    PacketReplaySocket() {
        raw_recv_buf_ = new uint8_t[FIXED_BUFFER_SIZE + 64];
        raw_resp_buf_ = new uint8_t[FIXED_BUFFER_SIZE + 64];
        recv_buf_ = allocateAligned(raw_recv_buf_);
        resp_buf_ = allocateAligned(raw_resp_buf_);
        recv_data_size_ = 0;
        resp_data_size_ = 0;
    }

    ~PacketReplaySocket() {
        delete[] raw_recv_buf_;
        delete[] raw_resp_buf_;
    }

    uint8_t* getRecvBufferAddr() const { return recv_buf_ + read_pos_; }
    uint8_t* getRespBufferAddr() const { return resp_buf_ + write_pos_; }
    size_t getRecvDataSize() const { return recv_data_size_; }
    size_t getRespDataSize() const { return resp_data_size_; }
    
    uint16_t getCurrentPacketSize() const {
        if (read_pos_ + sizeof(uint64_t) + sizeof(uint16_t) > recv_data_size_) return 0;
        uint16_t pkt_size = *reinterpret_cast<const uint16_t*>(recv_buf_ + read_pos_ + sizeof(uint64_t));
        return sizeof(uint64_t) + sizeof(uint16_t) + pkt_size; // timestamp + req_id (not included, see convertToAlignedFormat) + size + hex_data
    }

    void advanceReadPos() {
        if (read_pos_ + sizeof(uint64_t) + sizeof(uint16_t) <= recv_data_size_) {
            uint16_t pkt_size = *reinterpret_cast<const uint16_t*>(recv_buf_ + read_pos_ + sizeof(uint64_t));
            read_pos_ += sizeof(uint64_t) + sizeof(uint16_t) + pkt_size;
            read_pos_ = (read_pos_ + 63) & ~63;  // 64-byte align
        }

        // Check if EOF reached
        if (read_pos_ >= recv_data_size_) {
            eof_reached_ = true;
            //std::cout << "End of replay file reached. Set EOF flag.\n";
        } 
    }

    void advanceWritePos(uint16_t data_size) {
        size_t total_size = sizeof(uint64_t) + sizeof(uint16_t) + data_size;
        
        write_pos_ += total_size;
        write_pos_ = (write_pos_ + 63) & ~63;  // 64-byte align
        resp_data_size_ = write_pos_;

        if (write_pos_ + total_size > FIXED_BUFFER_SIZE) {
            std::cerr << "Response buffer overflow!" << std::endl;
        }
    }

    //void loadTrace(const std::string& filename) {
    void loadTrace(const std::string& filename, int max_requests = -1) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "Failed to open trace file: " << filename << std::endl;
            return;
        }

        size_t file_size = file.tellg();
        file.seekg(0);

        std::vector<uint8_t> raw_data(file_size);
        file.read(reinterpret_cast<char*>(raw_data.data()), file_size);

        convertToAlignedFormat(raw_data, max_requests);

        //std::cout << "Loaded trace from " << filename << ": " << recv_data_size_ << " bytes" << std::endl;
        //std::cout << "Aligned RPC data address: 0x" << std::hex
        //          << reinterpret_cast<uintptr_t>(recv_buf_) << std::dec << std::endl;
    }

    bool validateReplay(const std::string& expected_file) {
        std::ifstream expected(expected_file, std::ios::binary | std::ios::ate);
        if (!expected.is_open()) {
            printf("JU:JU Failed to open expected file\n");
            return false;
        }

        size_t file_size = expected.tellg();
        //printf("JU:JU Expected file size: %zu bytes\n", file_size);
        //printf("JU:JU Response data size: %zu bytes\n", resp_data_size_);

        expected.seekg(0);
        std::vector<uint8_t> expected_data(file_size);
        expected.read(reinterpret_cast<char*>(expected_data.data()), file_size);

        size_t resp_pos = 0, exp_pos = 0;
        int entry_count = 0;

        while (resp_pos < resp_data_size_ && exp_pos < expected_data.size()) {
            uint16_t resp_size = *reinterpret_cast<uint16_t*>(resp_buf_ + resp_pos + 8);
            uint16_t exp_size = *reinterpret_cast<uint16_t*>(expected_data.data() + exp_pos + 16); // Fix: BasicHeader size field offset

            //printf("JU:JU Entry %d - resp_size: %u, exp_size: %u\n", entry_count, resp_size, exp_size);

            //printf("JU:JU Entry %d First 20 raw bytes:\n", entry_count);
            //printf("  resp: ");
            //for(int i=0; i<20; i++) printf("%02x ", resp_buf_[resp_pos + i]);
            //printf("\n  exp:  ");
            //for(int i=0; i<20; i++) printf("%02x ", expected_data[exp_pos + i]);
            //printf("\n");

            if (resp_size != exp_size) {
                printf("JU:JU Size mismatch at entry %d\n", entry_count);
                return false;
            }

            // Compare data (can't do this because uniqueid generated during replay depends on timestamp. Thus, it is different.
            //if (memcmp(resp_buf_ + resp_pos + 10,
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
        if (read_pos_ + sizeof(uint64_t) + sizeof(uint16_t) > recv_data_size_) {
            std::cout << "No more data to read.\n";
            return 0;
        }

        uint16_t pkt_len = *reinterpret_cast<const uint16_t*>(recv_buf_ + read_pos_ + sizeof(uint64_t));

        uint32_t copy_len = std::min(max_len, static_cast<uint32_t>(pkt_len));
        std::memcpy(buf, recv_buf_ + read_pos_ + sizeof(uint64_t) + sizeof(uint16_t), copy_len);

        size_t total_size = sizeof(uint64_t) + sizeof(uint16_t) + pkt_len;
        read_pos_ += total_size;
        read_pos_ = (read_pos_ + 63) & ~63;

        // Check if EOF reached
        if (read_pos_ >= recv_data_size_) {
            eof_reached_ = true;
            //std::cout << "End of replay file reached. Set EOF flag.\n";
        } 
        
       return copy_len;
    }

    uint32_t write(const uint8_t* buf, uint32_t len) {
        size_t total_size = sizeof(uint64_t) + sizeof(uint16_t) + len;

        if (write_pos_ + total_size > FIXED_BUFFER_SIZE) {
            std::cerr << "Response buffer overflow!" << std::endl;
            return 0;
        }

        uint64_t timestamp = getCurrentTimestamp();
        *reinterpret_cast<uint64_t*>(resp_buf_ + write_pos_) = timestamp;
        *reinterpret_cast<uint16_t*>(resp_buf_ + write_pos_ + sizeof(uint64_t)) = len;
        std::memcpy(resp_buf_ + write_pos_ + sizeof(uint64_t) + sizeof(uint16_t), buf, len);

        write_pos_ += total_size;
        write_pos_ = (write_pos_ + 63) & ~63;
        resp_data_size_ = write_pos_;

        return len;
    }

    bool isEOF() const { return eof_reached_; }

private:
    uint8_t* allocateAligned(uint8_t* raw_buf) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(raw_buf);
        uintptr_t aligned_addr = (addr + 63) & ~63;
        return reinterpret_cast<uint8_t*>(aligned_addr);
    }

    void convertToAlignedFormat(const std::vector<uint8_t>& raw_data, int max_requests = -1) {
        // Parse binary log format: BasicHeader + data
        struct BasicHeader {
            uint64_t timestamp;
            int64_t req_id;
            uint16_t size;
        } __attribute__((packed));

        size_t pos = 0;
        size_t aligned_pos = 0;
        int request_count = 0;

        while (pos + sizeof(BasicHeader) <= raw_data.size()) {
            if (max_requests > 0 && request_count >= max_requests) break;
            const BasicHeader* header = reinterpret_cast<const BasicHeader*>(raw_data.data() + pos);

            if (pos + sizeof(BasicHeader) + header->size > raw_data.size()) break;
            if (aligned_pos + sizeof(uint64_t) + sizeof(uint16_t) + header->size > FIXED_BUFFER_SIZE) break;

            // Write timestamp, size, data
            *reinterpret_cast<uint64_t*>(recv_buf_ + aligned_pos) = header->timestamp;
            *reinterpret_cast<uint16_t*>(recv_buf_ + aligned_pos + sizeof(uint64_t)) = header->size;
            std::memcpy(recv_buf_ + aligned_pos + sizeof(uint64_t) + sizeof(uint16_t),
                       raw_data.data() + pos + sizeof(BasicHeader), header->size);

            pos += sizeof(BasicHeader) + header->size;
            aligned_pos += sizeof(uint64_t) + sizeof(uint16_t) + header->size;
            aligned_pos = (aligned_pos + 63) & ~63;  // 64-byte align
            request_count++;
        }

        recv_data_size_ = aligned_pos;
    }

    uint64_t getCurrentTimestamp() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }
};

#endif // PACKET_REPLAY_SOCKET_H
