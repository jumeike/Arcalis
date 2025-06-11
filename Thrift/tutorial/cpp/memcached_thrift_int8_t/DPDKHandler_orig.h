// DPDKHandler.h
#pragma once

#include <atomic>
#include <thread>
#include <functional>
#include <memory>
#include <mutex>
#include <condition_variable>

// DPDK headers
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_udp.h>
#include <rte_ip.h>
#include <rte_ether.h>

// Network headers
#include <netinet/in.h>
#include <arpa/inet.h>

class DPDKHandler {
public:
    static const uint16_t RX_RING_SIZE = 4096;
    static const uint16_t TX_RING_SIZE = 4096;
    static const uint16_t NUM_MBUFS = 16382;
    static const uint16_t MBUF_CACHE_SIZE = 512;
    
    static DPDKHandler& getInstance() {
        static DPDKHandler instance;
        return instance;
    }

    bool init(uint16_t port_id = 0);
    void cleanup();

    // Callback type for received data
    using PacketCallback = std::function<void(uint8_t*, uint32_t)>;

    // Register callback for received packets
    void setPacketCallback(PacketCallback cb) {
        packetCallback_ = cb;
    }

    void setResponseCallback(PacketCallback cb) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        responseCallback_ = cb;
    }

    // Send data
    bool sendData(const uint8_t* data, uint32_t len);

    // Start polling in separate thread
    void startPolling();

    // Stop polling
    void stopPolling();
    
    // Set IP Address
    void setLocalIpAddress(const std::string& ipStr);

private:
    DPDKHandler() = default;
    ~DPDKHandler() { cleanup(); }

    void pollLoop();
    bool setupDPDKPort();
    struct rte_mempool* createMempool();
    void handleArpPacket(struct rte_mbuf* m);

    std::atomic<bool> running_{false};
    std::thread pollThread_;
    PacketCallback packetCallback_;
    PacketCallback responseCallback_;     // Additional callback for response waiting
    std::mutex callback_mutex_;
    
    struct sockaddr_in peerAddr_;
    uint32_t localIpAddress_;

    uint16_t portId_{0};
    int port_{9090};
    struct rte_mempool* mbufPool_{nullptr};
    struct rte_eth_dev_info devInfo_{};
    struct rte_eth_conf portConf_{};
};
