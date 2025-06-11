#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <map>
#include <fstream>
#include <iomanip>
#include <random>
#include <mutex>

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/transport/TBufferTransports.h>

#include "../../../gen-cpp/UserTimelineService.h"
#include "../../../gen-cpp/PostStorageService.h"
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
    
    // Operation-specific metrics
    std::atomic<uint64_t> write_operations{0};
    std::atomic<uint64_t> read_operations{0};
    std::atomic<uint64_t> post_store_operations{0};
    
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

// Helper function to create a sample post
Post createSamplePost(int64_t post_id, int64_t req_id, int thread_id) {
    Post post;
    post.post_id = post_id;
    post.req_id = req_id;
    post.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    post.text = "Sample timeline post from thread " + std::to_string(thread_id) + 
                " with post_id " + std::to_string(post_id);
    post.post_type = PostType::POST;
    
    // Creator
    post.creator.user_id = thread_id + 1000;
    post.creator.username = "user_" + std::to_string(thread_id);
    
    // Sample URLs
    if (post_id % 3 == 0) {
        Url url;
        url.shortened_url = "http://short.ly/" + std::to_string(post_id);
        url.expanded_url = "http://example.com/full_url/" + std::to_string(post_id);
        post.urls.push_back(url);
    }
    
    // Sample user mentions
    if (post_id % 4 == 0) {
        UserMention mention;
        mention.user_id = (thread_id + 1) * 1000;
        mention.username = "mentioned_user_" + std::to_string(thread_id + 1);
        post.user_mentions.push_back(mention);
    }
    
    // Sample media
    if (post_id % 5 == 0) {
        Media media;
        media.media_id = post_id * 10;
        media.media_type = "image";
        post.media.push_back(media);
    }
    
    return post;
}

TestMetrics global_metrics;

void client_thread(int thread_id, const std::string& server_host, int server_port, 
                   int operations_per_thread, int warmup_operations, bool verbose) {
    try {
        // Connect to UserTimeline service
        std::shared_ptr<TTransport> timeline_socket(new TSocket(server_host, server_port));
        std::shared_ptr<TTransport> timeline_transport(new TFramedTransport(timeline_socket));
        std::shared_ptr<TProtocol> timeline_protocol(new TBinaryProtocol(timeline_transport));
        UserTimelineServiceClient timeline_client(timeline_protocol);
        
        // Connect to PostStorage service (assuming port 9091)
        std::shared_ptr<TTransport> post_socket(new TSocket(server_host, 9091));
        std::shared_ptr<TTransport> post_transport(new TFramedTransport(post_socket));
        std::shared_ptr<TProtocol> post_protocol(new TBinaryProtocol(post_transport));
        PostStorageServiceClient post_client(post_protocol);
        
        timeline_transport->open();
        post_transport->open();
        
        if (verbose) {
            std::cout << "Thread " << thread_id << " connected to both services" << std::endl;
        }
        
        // Random number generators
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> operation_dist(0, 1); // 0=write, 1=read
        std::uniform_int_distribution<> user_dist(1, 100); // User IDs 1-100
        std::uniform_int_distribution<> read_size_dist(5, 20); // Timeline read size
        
        std::vector<std::pair<int64_t, int64_t>> written_posts; // (user_id, post_id)
        
        // Warmup phase - create posts and timeline entries
        for (int i = 0; i < warmup_operations; i++) {
            std::map<std::string, std::string> carrier;
            carrier["trace-id"] = "warmup-" + std::to_string(thread_id) + "-" + std::to_string(i);
            
            try {
                int64_t user_id = user_dist(gen);
                int64_t post_id = thread_id * 100000 + i;
                int64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                
                // First, create the post in PostStorage
                Post post = createSamplePost(post_id, i, thread_id);
                post_client.StorePost(i, post, carrier);
                
                // Then add to user's timeline
                timeline_client.WriteUserTimeline(i, post_id, user_id, timestamp, carrier);
                written_posts.emplace_back(user_id, post_id);
            } catch (const TException& e) {
                if (verbose) {
                    std::cerr << "Warmup error in thread " << thread_id << ": " << e.what() << std::endl;
                }
            }
        }
        
        if (verbose) {
            std::cout << "Thread " << thread_id << " completed warmup, created " 
                      << written_posts.size() << " posts and timeline entries" << std::endl;
        }
        
        // Measurement phase
        for (int i = 0; i < operations_per_thread; i++) {
            global_metrics.total_requests++;
            
            std::map<std::string, std::string> carrier;
            carrier["trace-id"] = "test-" + std::to_string(thread_id) + "-" + std::to_string(i);
            carrier["span-id"] = std::to_string(thread_id * 10000 + i);
            
            int operation = operation_dist(gen);
            auto start_time = std::chrono::high_resolution_clock::now();
            
            try {
                if (operation == 0 || written_posts.empty()) {
                    // Write timeline operation (create post first)
                    int64_t user_id = user_dist(gen);
                    int64_t post_id = thread_id * 100000 + warmup_operations + i;
                    int64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    
                    // Create post in PostStorage first
                    Post post = createSamplePost(post_id, thread_id * 10000 + i, thread_id);
                    post_client.StorePost(thread_id * 10000 + i, post, carrier);
                    global_metrics.post_store_operations++;
                    
                    // Then add to timeline
                    timeline_client.WriteUserTimeline(thread_id * 10000 + i, post_id, user_id, timestamp, carrier);
                    written_posts.emplace_back(user_id, post_id);
                    global_metrics.write_operations++;
                    
                    if (verbose && i % 100 == 0) {
                        std::cout << "Thread " << thread_id << " created post " << post_id 
                                  << " and added to timeline for user " << user_id << std::endl;
                    }
                    
                } else {
                    // Read timeline operation
                    int idx = gen() % written_posts.size();
                    int64_t user_id = written_posts[idx].first;
                    int start = 0;
                    int stop = read_size_dist(gen);
                    
                    std::vector<Post> results;
                    timeline_client.ReadUserTimeline(results, thread_id * 100000 + i, user_id, start, stop, carrier);
                    global_metrics.read_operations++;
                    
                    if (verbose && i % 100 == 0) {
                        std::cout << "Thread " << thread_id << " read timeline for user " << user_id 
                                  << ", got " << results.size() << " posts" << std::endl;
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
        
        timeline_transport->close();
        post_transport->close();
        
        if (verbose) {
            std::cout << "Thread " << thread_id << " completed, created " 
                      << written_posts.size() << " total posts and timeline entries" << std::endl;
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
    std::cout << "Write operations: " << global_metrics.write_operations.load() << std::endl;
    std::cout << "Read operations: " << global_metrics.read_operations.load() << std::endl;
    std::cout << "Post store operations: " << global_metrics.post_store_operations.load() << std::endl;
    
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
    std::cout << "  -p, --port <port>       Server port (default: 9092)" << std::endl;
    std::cout << "  -t, --threads <num>     Number of client threads (default: 4)" << std::endl;
    std::cout << "  -o, --operations <num>  Operations per thread (default: 500)" << std::endl;
    std::cout << "  -w, --warmup <num>      Warmup operations per thread (default: 50)" << std::endl;
    std::cout << "  -v, --verbose           Verbose output" << std::endl;
    std::cout << "  --help                  Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string server_host = "localhost";
    int server_port = 9092;
    int num_threads = 4;
    int operations_per_thread = 500;
    int warmup_operations = 50;
    bool verbose = false;
    
    // Parse command line arguments
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
    
    std::cout << "=== UserTimeline Service Client Test ===" << std::endl;
    std::cout << "Server: " << server_host << ":" << server_port << std::endl;
    std::cout << "Threads: " << num_threads << std::endl;
    std::cout << "Operations per thread: " << operations_per_thread << std::endl;
    std::cout << "Warmup operations per thread: " << warmup_operations << std::endl;
    std::cout << "Mix: Write/Read timeline operations" << std::endl;
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
