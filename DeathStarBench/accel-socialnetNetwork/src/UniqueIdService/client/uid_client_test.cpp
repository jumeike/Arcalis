#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <map>
#include <fstream>
#include <iomanip>
#include <random>

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/transport/TBufferTransports.h>

#include "../../../gen-cpp/UniqueIdService.h"
#include "../../../gen-cpp/social_network_types.h"

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace social_network;

// Global metrics for test harness
struct TestMetrics {
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> successful_requests{0};
    std::atomic<uint64_t> failed_requests{0};
    std::atomic<uint64_t> total_latency_ns{0};
    std::atomic<uint64_t> min_latency_ns{UINT64_MAX};
    std::atomic<uint64_t> max_latency_ns{0};
    std::vector<uint64_t> latency_samples;
    std::mutex latency_mutex;
    
    void record_latency(uint64_t latency_ns) {
        total_latency_ns += latency_ns;
        
        // Update min/max atomically
        uint64_t current_min = min_latency_ns.load();
        while (latency_ns < current_min && 
               !min_latency_ns.compare_exchange_weak(current_min, latency_ns));
               
        uint64_t current_max = max_latency_ns.load();
        while (latency_ns > current_max && 
               !max_latency_ns.compare_exchange_weak(current_max, latency_ns));
        
        // Store sample for percentile calculation (with sampling to avoid memory issues)
        if (total_requests % 100 == 0) {  // Sample every 100th request
            std::lock_guard<std::mutex> lock(latency_mutex);
            latency_samples.push_back(latency_ns);
        }
    }
};

TestMetrics global_metrics;

void client_thread(int thread_id, const std::string& server_host, int server_port, 
                   int requests_per_thread, int warmup_requests, bool verbose) {
    try {
        // Create Thrift client connection
        std::shared_ptr<TTransport> socket(new TSocket(server_host, server_port));
        std::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
        std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
        UniqueIdServiceClient client(protocol);
        
        transport->open();
        
        if (verbose) {
            std::cout << "Thread " << thread_id << " connected to server" << std::endl;
        }
        
        // Warmup phase
        for (int i = 0; i < warmup_requests; i++) {
            std::map<std::string, std::string> carrier;
            //carrier["trace-id"] = "warmup-" + std::to_string(thread_id) + "-" + std::to_string(i);
            carrier["trace-id"] = "warmup-0000-0000";  // Fixed 10-char string
            try {
                client.ComposeUniqueId(i, PostType::POST, carrier);
            } catch (const TException& e) {
                if (verbose) {
                    std::cerr << "Warmup error in thread " << thread_id << ": " << e.what() << std::endl;
                }
            }
        }
        
        if (verbose) {
            std::cout << "Thread " << thread_id << " completed warmup" << std::endl;
        }
        
        // Random number generator for request variation
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> post_type_dist(0, 3);
        
        // Measurement phase
        for (int i = 0; i < requests_per_thread; i++) {
            global_metrics.total_requests++;
            
            // Create carrier with some tracing information
            std::map<std::string, std::string> carrier;
            /*
            carrier["trace-id"] = "test-" + std::to_string(thread_id) + "-" + std::to_string(i);
            carrier["span-id"] = std::to_string(thread_id * 100000 + i);
            carrier["user-id"] = std::to_string(thread_id % 100);  // Simulate different users
            */
            carrier["trace-id"] = "test-0000-0000";  // Fixed 10-char string
            carrier["span-id"] = "00";               // Fixed 2-char string
            char user_id_str[11];
            snprintf(user_id_str, sizeof(user_id_str), "%010ld", i % 10000000000); // 10-digit with leading zeros
            carrier["user-id"] = user_id_str;        // Fixed 10-char incrementing
            
            // Vary post types
            PostType::type post_type = static_cast<PostType::type>(post_type_dist(gen));
            
            auto start_time = std::chrono::high_resolution_clock::now();
            
            try {
                int64_t unique_id = client.ComposeUniqueId(thread_id * 100000 + i, post_type, carrier);
                
                auto end_time = std::chrono::high_resolution_clock::now();
                uint64_t latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
                
                global_metrics.successful_requests++;
                global_metrics.record_latency(latency_ns);
                
                // Validate the unique ID (basic sanity check)
                if (unique_id <= 0) {
                    std::cerr << "Thread " << thread_id << " received invalid ID: " << unique_id << std::endl;
                    global_metrics.failed_requests++;
                } else if (verbose && i % 1000 == 0) {
                    std::cout << "Thread " << thread_id << " - Request " << i 
                              << " - ID: 0x" << std::hex << unique_id << std::dec
                              << " - Latency: " << (latency_ns / 1000) << "μs" << std::endl;
                }
                
            } catch (const TException& e) {
                auto end_time = std::chrono::high_resolution_clock::now();
                uint64_t latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
                
                global_metrics.failed_requests++;
                global_metrics.record_latency(latency_ns);  // Record even failed requests for complete picture
                
                if (verbose) {
                    std::cerr << "Thread " << thread_id << " - Request " << i 
                              << " failed: " << e.what() << std::endl;
                }
            }
            
            // Optional: Add some think time to simulate realistic load
            // std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        
        transport->close();
        
        if (verbose) {
            std::cout << "Thread " << thread_id << " completed all requests" << std::endl;
        }
        
    } catch (const TException& e) {
        std::cerr << "Thread " << thread_id << " connection error: " << e.what() << std::endl;
    }
}

void print_results(int total_threads, int requests_per_thread, 
                   std::chrono::milliseconds total_duration) {
    uint64_t total_reqs = global_metrics.total_requests.load();
    uint64_t successful_reqs = global_metrics.successful_requests.load();
    uint64_t failed_reqs = global_metrics.failed_requests.load();
    uint64_t total_latency = global_metrics.total_latency_ns.load();
    
    std::cout << "\n=== TEST RESULTS ===" << std::endl;
    std::cout << "Total Requests: " << total_reqs << std::endl;
    std::cout << "Successful: " << successful_reqs << " (" 
              << (100.0 * successful_reqs / total_reqs) << "%)" << std::endl;
    std::cout << "Failed: " << failed_reqs << " (" 
              << (100.0 * failed_reqs / total_reqs) << "%)" << std::endl;
    
    std::cout << "\n=== PERFORMANCE ===" << std::endl;
    std::cout << "Total Duration: " << total_duration.count() << " ms" << std::endl;
    std::cout << "Throughput: " << (successful_reqs * 1000.0 / total_duration.count()) 
              << " req/s" << std::endl;
    
    if (successful_reqs > 0) {
        std::cout << "\n=== LATENCY ===" << std::endl;
        std::cout << "Average: " << (total_latency / successful_reqs / 1000) << " μs" << std::endl;
        std::cout << "Min: " << (global_metrics.min_latency_ns.load() / 1000) << " μs" << std::endl;
        std::cout << "Max: " << (global_metrics.max_latency_ns.load() / 1000) << " μs" << std::endl;
        
        // Calculate percentiles from samples
        {
            std::lock_guard<std::mutex> lock(global_metrics.latency_mutex);
            if (!global_metrics.latency_samples.empty()) {
                std::vector<uint64_t> samples = global_metrics.latency_samples;
                std::sort(samples.begin(), samples.end());
                
                size_t p50_idx = samples.size() * 0.5;
                size_t p95_idx = samples.size() * 0.95;
                size_t p99_idx = samples.size() * 0.99;
                
                std::cout << "P50: " << (samples[p50_idx] / 1000) << " μs" << std::endl;
                std::cout << "P95: " << (samples[p95_idx] / 1000) << " μs" << std::endl;
                std::cout << "P99: " << (samples[p99_idx] / 1000) << " μs" << std::endl;
            }
        }
    }
}

void save_results_to_file(const std::string& filename, int threads, int requests_per_thread,
                          std::chrono::milliseconds duration) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not open file " << filename << " for writing" << std::endl;
        return;
    }
    
    file << "# UniqueID Service Test Results" << std::endl;
    file << "# Timestamp: " << std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() << std::endl;
    file << "threads," << threads << std::endl;
    file << "requests_per_thread," << requests_per_thread << std::endl;
    file << "total_requests," << global_metrics.total_requests.load() << std::endl;
    file << "successful_requests," << global_metrics.successful_requests.load() << std::endl;
    file << "failed_requests," << global_metrics.failed_requests.load() << std::endl;
    file << "duration_ms," << duration.count() << std::endl;
    file << "throughput_rps," << (global_metrics.successful_requests.load() * 1000.0 / duration.count()) << std::endl;
    
    if (global_metrics.successful_requests.load() > 0) {
        file << "avg_latency_us," << (global_metrics.total_latency_ns.load() / global_metrics.successful_requests.load() / 1000) << std::endl;
        file << "min_latency_us," << (global_metrics.min_latency_ns.load() / 1000) << std::endl;
        file << "max_latency_us," << (global_metrics.max_latency_ns.load() / 1000) << std::endl;
    }
    
    file.close();
    std::cout << "Results saved to " << filename << std::endl;
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --host <host>       Server host (default: localhost)" << std::endl;
    std::cout << "  -p, --port <port>       Server port (default: 9090)" << std::endl;
    std::cout << "  -t, --threads <num>     Number of client threads (default: 4)" << std::endl;
    std::cout << "  -r, --requests <num>    Requests per thread (default: 1000)" << std::endl;
    std::cout << "  -w, --warmup <num>      Warmup requests per thread (default: 100)" << std::endl;
    std::cout << "  -v, --verbose           Verbose output" << std::endl;
    std::cout << "  -o, --output <file>     Save results to file" << std::endl;
    std::cout << "  --help                  Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    // Default parameters
    std::string server_host = "localhost";
    int server_port = 9090;
    int num_threads = 4;
    int requests_per_thread = 1000;
    int warmup_requests = 100;
    bool verbose = false;
    std::string output_file;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--host") {
            if (i + 1 < argc) server_host = argv[++i];
        } else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) server_port = std::stoi(argv[++i]);
        } else if (arg == "-t" || arg == "--threads") {
            if (i + 1 < argc) num_threads = std::stoi(argv[++i]);
        } else if (arg == "-r" || arg == "--requests") {
            if (i + 1 < argc) requests_per_thread = std::stoi(argv[++i]);
        } else if (arg == "-w" || arg == "--warmup") {
            if (i + 1 < argc) warmup_requests = std::stoi(argv[++i]);
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) output_file = argv[++i];
        } else if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    std::cout << "=== UniqueID Service Client Test ===" << std::endl;
    std::cout << "Server: " << server_host << ":" << server_port << std::endl;
    std::cout << "Threads: " << num_threads << std::endl;
    std::cout << "Requests per thread: " << requests_per_thread << std::endl;
    std::cout << "Warmup requests per thread: " << warmup_requests << std::endl;
    std::cout << "Total requests: " << (num_threads * requests_per_thread) << std::endl;
    std::cout << std::endl;
    
    // Create and launch client threads
    std::vector<std::thread> threads;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(client_thread, i, server_host, server_port, 
                           requests_per_thread, warmup_requests, verbose);
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Print results
    print_results(num_threads, requests_per_thread, duration);
    
    // Save results to file if requested
    if (!output_file.empty()) {
        save_results_to_file(output_file, num_threads, requests_per_thread, duration);
    }
    
    return 0;
}
