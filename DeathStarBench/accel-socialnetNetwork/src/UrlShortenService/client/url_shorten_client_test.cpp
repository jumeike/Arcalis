#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <map>
#include <random>
#include <mutex>
#include <algorithm>

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/transport/TBufferTransports.h>

#include "../../../gen-cpp/UrlShortenService.h"
#include "../../../gen-cpp/social_network_types.h"

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace social_network;

struct TestMetrics {
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> successful_requests{0};
    std::atomic<uint64_t> failed_requests{0};
    std::atomic<uint64_t> total_latency_ns{0};
    std::atomic<uint64_t> min_latency_ns{UINT64_MAX};
    std::atomic<uint64_t> max_latency_ns{0};
    std::vector<uint64_t> latency_samples;
    std::mutex latency_mutex;
    
    std::atomic<uint64_t> compose_operations{0};
    std::atomic<uint64_t> get_extended_operations{0};
    
    void record_latency(uint64_t latency_ns) {
        total_latency_ns += latency_ns;
        
        uint64_t current_min = min_latency_ns.load();
        while (latency_ns < current_min && 
               !min_latency_ns.compare_exchange_weak(current_min, latency_ns));
               
        uint64_t current_max = max_latency_ns.load();
        while (latency_ns > current_max && 
               !max_latency_ns.compare_exchange_weak(current_max, latency_ns));
        
        if (total_requests % 100 == 0) {
            std::lock_guard<std::mutex> lock(latency_mutex);
            latency_samples.push_back(latency_ns);
        }
    }
};

TestMetrics global_metrics;

void client_thread(int thread_id, const std::string& server_host, int server_port,
                   int operations_per_thread, int warmup_operations, bool verbose) {
    try {
        std::shared_ptr<TTransport> socket(new TSocket(server_host, server_port));
        std::shared_ptr<TTransport> transport(new TBufferedTransport(socket, 2048));
        std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
        UrlShortenServiceClient client(protocol);
        
        transport->open();
        
        if (verbose) {
            std::cout << "Thread " << thread_id << " connected to URL shorten server" << std::endl;
        }
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> url_count_dist(1, 5);
        std::uniform_real_distribution<float> operation_dist(0.0f, 1.0f);
        
        std::vector<std::string> shortened_urls;
        
        // Warmup phase - compose URLs
        for (int i = 0; i < warmup_operations; i++) {
            std::map<std::string, std::string> carrier;
            carrier["trace-id"] = "warmup-0000";
            carrier["span-id"] = "00";
            
            try {
                std::vector<std::string> urls;
                int url_count = url_count_dist(gen);
                for (int j = 0; j < url_count; j++) {
                    urls.push_back("https://example.com/page" + std::to_string(thread_id * 1000 + i * 10 + j));
                }
                
                std::vector<Url> result;
                client.ComposeUrls(result, i, urls, carrier);
                
                for (const auto& url : result) {
                    shortened_urls.push_back(url.shortened_url);
                }
            } catch (const TException& e) {
                if (verbose) {
                    std::cerr << "Warmup error in thread " << thread_id << ": " << e.what() << std::endl;
                }
            }
        }
        
        if (verbose) {
            std::cout << "Thread " << thread_id << " completed warmup, created " 
                      << shortened_urls.size() << " shortened URLs" << std::endl;
        }
        
        // Measurement phase
        for (int i = 0; i < operations_per_thread; i++) {
            global_metrics.total_requests++;
            
            std::map<std::string, std::string> carrier;
            carrier["trace-id"] = "test-0000-0000";
            carrier["span-id"] = "00";
            
            float op_rand = operation_dist(gen);
            bool compose_op = (op_rand < 0.5f) || shortened_urls.empty();
            
            auto start_time = std::chrono::high_resolution_clock::now();
            
            try {
                if (compose_op) {
                    // ComposeUrls operation
                    std::vector<std::string> urls;
                    int url_count = url_count_dist(gen);
                    for (int j = 0; j < url_count; j++) {
                        urls.push_back("https://example.com/test" + std::to_string(thread_id * 100000 + i * 10 + j));
                    }
                    
                    std::vector<Url> result;
                    client.ComposeUrls(result, thread_id * 10000 + i, urls, carrier);
                    
                    for (const auto& url : result) {
                        shortened_urls.push_back(url.shortened_url);
                    }
                    
                    global_metrics.compose_operations++;
                    
                    if (verbose && i % 100 == 0) {
                        std::cout << "Thread " << thread_id << " composed " << url_count << " URLs" << std::endl;
                    }
                } else {
                    // GetExtendedUrls operation
                    int count = std::min(3, (int)shortened_urls.size());
                    std::vector<std::string> query_urls;
                    
                    std::set<int> indices;
                    while (indices.size() < (size_t)count) {
                        indices.insert(gen() % shortened_urls.size());
                    }
                    
                    for (int idx : indices) {
                        query_urls.push_back(shortened_urls[idx]);
                    }
                    
                    std::vector<std::string> result;
                    client.GetExtendedUrls(result, thread_id * 10000 + i, query_urls, carrier);
                    
                    global_metrics.get_extended_operations++;
                    
                    if (verbose && i % 100 == 0) {
                        std::cout << "Thread " << thread_id << " retrieved " << result.size() << " URLs" << std::endl;
                    }
                }
                
                auto end_time = std::chrono::high_resolution_clock::now();
                uint64_t latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
                
                global_metrics.successful_requests++;
                global_metrics.record_latency(latency_ns);
                
            } catch (const TException& e) {
                auto end_time = std::chrono::high_resolution_clock::now();
                uint64_t latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
                
                global_metrics.failed_requests++;
                global_metrics.record_latency(latency_ns);
                
                if (verbose) {
                    std::cerr << "Thread " << thread_id << " - Operation " << i 
                              << " failed: " << e.what() << std::endl;
                }
            }
        }
        
        transport->close();
        
        if (verbose) {
            std::cout << "Thread " << thread_id << " completed" << std::endl;
        }
        
    } catch (const TException& e) {
        std::cerr << "Thread " << thread_id << " connection error: " << e.what() << std::endl;
    }
}

void print_results(int total_threads, int operations_per_thread, 
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
    
    std::cout << "\n=== OPERATION BREAKDOWN ===" << std::endl;
    std::cout << "ComposeUrls operations: " << global_metrics.compose_operations.load() << std::endl;
    std::cout << "GetExtendedUrls operations: " << global_metrics.get_extended_operations.load() << std::endl;
    
    std::cout << "\n=== PERFORMANCE ===" << std::endl;
    std::cout << "Total Duration: " << total_duration.count() << " ms" << std::endl;
    std::cout << "Throughput: " << (successful_reqs * 1000.0 / total_duration.count()) 
              << " req/s" << std::endl;
    
    if (successful_reqs > 0) {
        std::cout << "\n=== LATENCY ===" << std::endl;
        std::cout << "Average: " << (total_latency / successful_reqs / 1000) << " μs" << std::endl;
        std::cout << "Min: " << (global_metrics.min_latency_ns.load() / 1000) << " μs" << std::endl;
        std::cout << "Max: " << (global_metrics.max_latency_ns.load() / 1000) << " μs" << std::endl;
        
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

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --host <host>       Server host (default: localhost)" << std::endl;
    std::cout << "  -p, --port <port>       Server port (default: 9090)" << std::endl;
    std::cout << "  -t, --threads <num>     Number of client threads (default: 4)" << std::endl;
    std::cout << "  -o, --operations <num>  Operations per thread (default: 500)" << std::endl;
    std::cout << "  -w, --warmup <num>      Warmup operations per thread (default: 50)" << std::endl;
    std::cout << "  -v, --verbose           Verbose output" << std::endl;
    std::cout << "  --help                  Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string server_host = "localhost";
    int server_port = 9090;
    int num_threads = 4;
    int operations_per_thread = 500;
    int warmup_operations = 50;
    bool verbose = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--host") {
            if (i + 1 < argc) server_host = argv[++i];
        } else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) server_port = std::stoi(argv[++i]);
        } else if (arg == "-t" || arg == "--threads") {
            if (i + 1 < argc) num_threads = std::stoi(argv[++i]);
        } else if (arg == "-o" || arg == "--operations") {
            if (i + 1 < argc) operations_per_thread = std::stoi(argv[++i]);
        } else if (arg == "-w" || arg == "--warmup") {
            if (i + 1 < argc) warmup_operations = std::stoi(argv[++i]);
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    std::cout << "=== UrlShorten Service Client Test ===" << std::endl;
    std::cout << "Server: " << server_host << ":" << server_port << std::endl;
    std::cout << "Threads: " << num_threads << std::endl;
    std::cout << "Operations per thread: " << operations_per_thread << std::endl;
    std::cout << "Warmup operations per thread: " << warmup_operations << std::endl;
    std::cout << std::endl;
    
    std::vector<std::thread> threads;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(client_thread, i, server_host, server_port, 
                           operations_per_thread, warmup_operations, verbose);
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    print_results(num_threads, operations_per_thread, duration);
    
    return 0;
}
