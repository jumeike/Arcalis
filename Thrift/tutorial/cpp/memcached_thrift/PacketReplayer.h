#include <fstream>
#include <vector>
#include <cstdint>
#include <chrono>
#include <thread>

class PacketReplayer {
public:
    struct PacketHeader {
        uint64_t timestamp;
        uint32_t length;
    };

    static void replayToMemcached(const std::string& logfile, MemcachedBusinessLogic& logic) {
        std::ifstream file(logfile, std::ios::binary);
        PacketHeader header;
        uint64_t last_timestamp = 0;

        while (file.read(reinterpret_cast<char*>(&header), sizeof(header))) {
            std::vector<int8_t> data(header.length);
            file.read(reinterpret_cast<char*>(data.data()), header.length);

            // Simulate original timing
            if (last_timestamp != 0) {
                uint64_t delay = header.timestamp - last_timestamp;
                std::this_thread::sleep_for(std::chrono::microseconds(delay));
            }
            last_timestamp = header.timestamp;

            // Extract key length from data (first sizeof(size_t) bytes)
            size_t keylen = *reinterpret_cast<size_t*>(data.data());
            size_t valuelen = header.length - keylen;

            // Copy to business logic's receive buffer
            memcpy(logic.getRecvBuffer(), data.data(), header.length);
            
            // Process packet
            logic.handleReq(GET, keylen, valuelen);
        }
    }
};