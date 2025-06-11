// BufferedTUDPSocket.h
#ifndef _THRIFT_TRANSPORT_BUFFEREDTUDPSOCKET_H_
#define _THRIFT_TRANSPORT_BUFFEREDTUDPSOCKET_H_ 1

#pragma once

#include <queue>
#include <mutex>
#include <memory>
#include <iostream>
#include <thrift/transport/TUDPSocket.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_arp.h>
#include "DPDKResources.h"

#define BURST_SIZE 32
#define MAX_PACKET_SIZE 1500  // Standard MTU
#define MIN_PACKET_SIZE 1

using apache::thrift::transport::TTransportException;

namespace apache { namespace thrift { namespace transport {

class PacketBuffer {
public:
    PacketBuffer(const uint8_t* data, size_t size, const sockaddr_in& peer)
        : data_(data, data + size), peer_(peer) {}

    const uint8_t* data() const { return data_.data(); }
    size_t size() const { return data_.size(); }
    const sockaddr_in& peer() const { return peer_; }

private:
    std::vector<uint8_t> data_;
    sockaddr_in peer_;
};

class BufferedTUDPSocket : public TUDPSocket {
public:
    BufferedTUDPSocket(std::shared_ptr<apache::thrift::transport::DPDKResources> dpdkResources)
        : TUDPSocket(dpdkResources, std::shared_ptr<apache::thrift::TConfiguration>(new apache::thrift::TConfiguration())) {}

    // In BufferedTUDPSocket::read()
    uint32_t read(uint8_t* buf, uint32_t len) override {
        std::cout << "Starting read operation\n";
        try {
            if (!isOpen()) {
                std::cerr << "Socket not open\n";
                return 0;  // Return 0 instead of throwing
            }

            uint64_t start_time = rte_get_timer_cycles();
            uint64_t timeout_cycles = (uint64_t)getRecvTimeout() * rte_get_timer_hz() / 1000;

            while (true) {
                try {
                    // Check queue first
                    {
                        std::lock_guard<std::mutex> lock(queue_mutex_);
                        if (!packet_queue_.empty()) {
                            auto& packet = packet_queue_.front();
                            uint32_t copy_len = std::min(len, static_cast<uint32_t>(packet.size()));
                            memcpy(buf, packet.data(), copy_len);
                            setPeerAddress(packet.peer()); 
                            packet_queue_.pop();
                            return copy_len;
                        }
                    }

                    // Check timeout
                    if (getRecvTimeout() > 0) {
                        uint64_t elapsed = rte_get_timer_cycles() - start_time;
                        if (elapsed > timeout_cycles) {
                            std::cerr << "Read timeout\n";
                            return 0;
                        }
                    }

                    // Receive burst
                    struct rte_mbuf* pkts_burst[BURST_SIZE];
                    const uint16_t nb_rx = rte_eth_rx_burst(getPortId(), 0, pkts_burst, BURST_SIZE);

                    if (nb_rx > 0) {
                        std::cout << "nb_rx: " << nb_rx << std::endl;

                        for (uint16_t i = 0; i < nb_rx; i++) {
                            try {
                                std::cout << "Processing packet " << i << " of " << nb_rx << std::endl;
                                struct rte_mbuf* m = pkts_burst[i];
                                
                                if (!m) {
                                    std::cerr << "Null mbuf at index " << i << std::endl;
                                    continue;
                                }

                                // Get packet headers
                                struct rte_ether_hdr* eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr*);
                                uint16_t ether_type = rte_be_to_cpu_16(eth_hdr->ether_type);

                                if (ether_type == RTE_ETHER_TYPE_ARP) {
                                    handleArpPacket(m, eth_hdr);
                                    rte_pktmbuf_free(m);
                                    continue;
                                }

                                if (ether_type != RTE_ETHER_TYPE_IPV4) {
                                    std::cout << "Non-IPv4 packet\n";
                                    rte_pktmbuf_free(m);
                                    continue;
                                }

                                struct rte_ipv4_hdr* ip_hdr = (struct rte_ipv4_hdr*)(eth_hdr + 1);
                                if (ip_hdr->next_proto_id != IPPROTO_UDP) {
                                    std::cout << "Non-UDP packet\n";
                                    rte_pktmbuf_free(m);
                                    continue;
                                }

                                struct rte_udp_hdr* udp_hdr = (struct rte_udp_hdr*)(ip_hdr + 1);
                                
                                if (getPort() != 0 && rte_be_to_cpu_16(udp_hdr->dst_port) != getPort()) {
                                    std::cout << "Port mismatch\n";
                                    rte_pktmbuf_free(m);
                                    continue;
                                }

                                uint16_t udp_payload_len = rte_be_to_cpu_16(udp_hdr->dgram_len) - sizeof(struct rte_udp_hdr);
                                if (udp_payload_len > MAX_PACKET_SIZE || udp_payload_len < MIN_PACKET_SIZE) {
                                    std::cerr << "Invalid payload length: " << udp_payload_len << std::endl;
                                    rte_pktmbuf_free(m);
                                    continue;
                                }
                                
                                // First packet processing
                                if (i == 0) {
                                    uint8_t* payload = rte_pktmbuf_mtod_offset(m, uint8_t*, 
                                        sizeof(struct rte_ether_hdr) + 
                                        sizeof(struct rte_ipv4_hdr) + 
                                        sizeof(struct rte_udp_hdr));

                                    sockaddr_in peer;
                                    peer.sin_family = AF_INET;
                                    peer.sin_port = udp_hdr->src_port;
                                    peer.sin_addr.s_addr = ip_hdr->src_addr;
                                    
                                    uint32_t copy_len = std::min(len, static_cast<uint32_t>(udp_payload_len));
                                    memcpy(buf, payload, copy_len);
                                    setPeerAddress(peer);
                                    rte_pktmbuf_free(m);

                                    // Queue remaining packets
                                    for (uint16_t j = 1; j < nb_rx; j++) {
                                        try {
                                            if (processAndQueuePacket(pkts_burst[j])) {
                                                rte_pktmbuf_free(pkts_burst[j]);
                                            }
                                        } catch (const std::exception& e) {
                                            std::cerr << "Error queueing packet " << j << ": " << e.what() << std::endl;
                                            rte_pktmbuf_free(pkts_burst[j]);
                                        }
                                    }
                                    
                                    if (copy_len > 0) {
                                        return copy_len;
                                    }
                                } else {
                                    if (processAndQueuePacket(m)) {
                                        rte_pktmbuf_free(m);
                                    }
                                }
                            } catch (const std::exception& e) {
                                std::cerr << "Error processing packet " << i << ": " << e.what() << std::endl;
                                if (pkts_burst[i]) {
                                    rte_pktmbuf_free(pkts_burst[i]);
                                }
                            }
                        }

                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error in read loop: " << e.what() << std::endl;
                    // Continue the loop instead of dying
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Fatal error in read: " << e.what() << std::endl;
            return 0;
        }
    } 

private:
    std::queue<PacketBuffer> packet_queue_;
    std::mutex queue_mutex_;

    uint16_t getPortId() const { return TUDPSocket::getDPDKResources()->portId; }
    rte_mempool* getMbufPool() const { return TUDPSocket::getDPDKResources()->mbufPool; }
    int getPort() const { return TUDPSocket::getPort(); }
    uint32_t getLocalIpAddress() const { return TUDPSocket::getLocalIpAddress(); }
    int getRecvTimeout() const { return TUDPSocket::getTimeout(); }
    void setPeerAddress(const sockaddr_in& addr) { TUDPSocket::setPeerAddr(addr); }
    bool isOpen() const { return TUDPSocket::isOpen(); }

    void handleArpPacket(struct rte_mbuf* m, struct rte_ether_hdr* eth_hdr) {
        std::cout << "Handling ARP packet\n";
        struct rte_arp_hdr* arp_hdr = (struct rte_arp_hdr*)(eth_hdr + 1);
        
        if (rte_be_to_cpu_16(arp_hdr->arp_opcode) == RTE_ARP_OP_REQUEST) {
            if (arp_hdr->arp_data.arp_tip == getLocalIpAddress()) {
                // Create and send ARP reply
                struct rte_mbuf* reply = rte_pktmbuf_alloc(getMbufPool());
                if (reply) {
                    char* pkt = rte_pktmbuf_append(reply, 
                        sizeof(struct rte_ether_hdr) + sizeof(struct rte_arp_hdr));
                    
                    if (pkt) {
                        struct rte_ether_hdr* reply_eth = 
                            rte_pktmbuf_mtod(reply, struct rte_ether_hdr*);
                        struct rte_arp_hdr* reply_arp = 
                            (struct rte_arp_hdr*)(reply_eth + 1);

                        // Set ethernet header
                        reply_eth->dst_addr = eth_hdr->src_addr;
                        rte_eth_macaddr_get(getPortId(), &reply_eth->src_addr);
                        reply_eth->ether_type = eth_hdr->ether_type;

                        // Set ARP header
                        reply_arp->arp_hardware = arp_hdr->arp_hardware;
                        reply_arp->arp_protocol = arp_hdr->arp_protocol;
                        reply_arp->arp_hlen = arp_hdr->arp_hlen;
                        reply_arp->arp_plen = arp_hdr->arp_plen;
                        reply_arp->arp_opcode = rte_cpu_to_be_16(RTE_ARP_OP_REPLY);

                        // Set ARP data
                        rte_eth_macaddr_get(getPortId(), 
                            &reply_arp->arp_data.arp_sha);
                        reply_arp->arp_data.arp_sip = getLocalIpAddress();
                        reply_arp->arp_data.arp_tha = arp_hdr->arp_data.arp_sha;
                        reply_arp->arp_data.arp_tip = arp_hdr->arp_data.arp_sip;

                        // Send ARP reply
                        uint16_t nb_tx = rte_eth_tx_burst(getPortId(), 
                            0, &reply, 1);
                        if (nb_tx == 0) {
                            rte_pktmbuf_free(reply);
                        }
                    } else {
                        rte_pktmbuf_free(reply);
                    }
                }
            }
        }
    }

    bool processAndQueuePacket(struct rte_mbuf* m) {
        try {
            struct rte_ether_hdr* eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr*);
            if (rte_be_to_cpu_16(eth_hdr->ether_type) != RTE_ETHER_TYPE_IPV4) {
                return true;
            }

            struct rte_ipv4_hdr* ip_hdr = (struct rte_ipv4_hdr*)(eth_hdr + 1);
            if (ip_hdr->next_proto_id != IPPROTO_UDP) {
                return true;
            }

            struct rte_udp_hdr* udp_hdr = (struct rte_udp_hdr*)(ip_hdr + 1);
            if (getPort() != 0 && rte_be_to_cpu_16(udp_hdr->dst_port) != getPort()) {
                return true;
            }

            uint16_t udp_payload_len = rte_be_to_cpu_16(udp_hdr->dgram_len) - sizeof(struct rte_udp_hdr);
            std::cout << "Queueing packet with payload length: " << udp_payload_len << std::endl;
            uint8_t* payload = rte_pktmbuf_mtod_offset(m, uint8_t*, 
                sizeof(struct rte_ether_hdr) + 
                sizeof(struct rte_ipv4_hdr) + 
                sizeof(struct rte_udp_hdr));

            sockaddr_in peer;
            peer.sin_family = AF_INET;
            peer.sin_port = udp_hdr->src_port;
            peer.sin_addr.s_addr = ip_hdr->src_addr;

            std::lock_guard<std::mutex> lock(queue_mutex_);
            packet_queue_.emplace(payload, udp_payload_len, peer);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error in processAndQueuePacket: " << e.what() << std::endl;
            return true;
        }
    }
};
}}} // apache::thrift::transport

#endif
