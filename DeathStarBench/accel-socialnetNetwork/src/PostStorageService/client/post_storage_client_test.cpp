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
#include <inttypes.h>

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TUDPSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/transport/TBufferTransports.h>

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
    std::atomic<uint64_t> store_operations{0};
    std::atomic<uint64_t> read_operations{0};
    std::atomic<uint64_t> read_multi_operations{0};
    
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

// Helper function to create a sample post with fixed-length strings
Post createSamplePost(int64_t post_id, int64_t req_id, int thread_id) {
    Post post;
    post.post_id = post_id;
    post.req_id = req_id;
    post.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
 
    // Fixed-length text (64 chars)
    char text_buf[65];
    snprintf(text_buf, sizeof(text_buf), "Sample post text from thread %03d with post_id %010ld      ",
             (int)(thread_id % 1000), post_id);
    std::string text_str(text_buf);
    text_str.resize(64, ' ');
    post.text = text_str;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> post_type_dist(0, 3);
    post.post_type = static_cast<PostType::type>(post_type_dist(gen)); //PostType::POST;
 
    // Creator with fixed-length username (16 chars)
    post.creator.user_id = thread_id + 1000;
    char username_buf[17];
    snprintf(username_buf, sizeof(username_buf), "user_%011d", (int)(thread_id % 100000000000LL));
    std::string username_str(username_buf);
    username_str.resize(16, ' ');
    post.creator.username = username_str;
 
    // Sample URLs with fixed lengths
    Url url;
    char short_buf[33], expanded_buf[65];
    snprintf(short_buf, sizeof(short_buf), "http://short.ly/%010" PRId64, (int64_t)(post_id % 10000000000LL));
    snprintf(expanded_buf, sizeof(expanded_buf), "http://example.com/full_url/%020" PRId64, (int64_t)(post_id % 100000000000000LL));
    
    std::string short_str(short_buf);
    std::string expanded_str(expanded_buf);
    short_str.resize(32, ' ');
    expanded_str.resize(64, ' ');
    
    url.shortened_url = short_str;
    url.expanded_url = expanded_str;
    post.urls.push_back(url);
 
    // Sample user mentions with fixed-length username (16 chars)
    UserMention mention;
    mention.user_id = (thread_id + 1) * 1000;
    char mention_buf[17];
    snprintf(mention_buf, sizeof(mention_buf), "mentioned_%05d", (int)((thread_id + 1) % 100000));
    std::string mention_str(mention_buf);
    mention_str.resize(16, ' ');
    mention.username = mention_str;
    post.user_mentions.push_back(mention);
 
    // Sample media with fixed-length type (8 chars)
    Media media;
    media.media_id = (post_id * 10) & 0x7FFFFFFFFFFFFFFF;
    const char* media_types[] = {
        "image   ",  // 8 chars
        "video   ",  // 8 chars
        "audio   ",  // 8 chars
        "document",  // 8 chars
        "gif     "   // 8 chars
    };
    media.media_type = media_types[post_id % 5];
    post.media.push_back(media);
 
    return post;
 }

// Variables for generating unique post_ids
static std::mutex id_generation_mutex;
static int64_t current_timestamp = -1;
static int counter = 0;

// Helper function to generate unique ID (similar to UniqueID service)
uint64_t generateUniquePostId(int thread_id) {
    std::lock_guard<std::mutex> lock(id_generation_mutex);
    
    // Get current timestamp in milliseconds
    int64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Handle counter logic like UniqueID service
    if (current_timestamp > timestamp) {
        // Time went backwards - this shouldn't happen normally
        timestamp = current_timestamp;
    }
    
    if (current_timestamp == timestamp) {
        counter++;
    } else {
        current_timestamp = timestamp;
        counter = 0;
    }
    
    // Combine timestamp, thread_id, and counter for uniqueness
    // Format: timestamp (40 bits) + thread_id (8 bits) + counter (16 bits)
    int64_t unique_id = (timestamp << 24) | ((thread_id & 0xFF) << 16) | (counter & 0xFFFF);
    
    return unique_id & 0x7FFFFFFFFFFFFFFF;
}


void client_thread(int thread_id, const std::string& server_host, int server_port, 
                   int operations_per_thread, int warmup_operations, bool verbose) {
    try {
        std::shared_ptr<TTransport> socket(new TUDPSocket(server_host, server_port));
        //std::shared_ptr<TTransport> transport(new TFramedTransport(socket));
        std::shared_ptr<TTransport> transport(new TBufferedTransport(socket, 2048));
        std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
        PostStorageServiceClient client(protocol);
        
        transport->open();
        
        if (verbose) {
            std::cout << "Thread " << thread_id << " connected to post storage server" << std::endl;
        }
        
        // Random number generators
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> operation_dist(0, 2); // 0=store, 1=read, 2=read_multi
        std::uniform_int_distribution<> post_count_dist(1, 5); // For read_multi
        
        std::vector<int64_t> stored_post_ids; // Track what we've stored
        
        // Warmup phase
        for (int i = 0; i < warmup_operations; i++) {
            std::map<std::string, std::string> carrier;
            //carrier["trace-id"] = "warmup-" + std::to_string(thread_id) + "-" + std::to_string(i);
            carrier["trace-id"] = "test-0000-0000";  // Fixed 14-char string
            carrier["span-id"] = "00";               // Fixed 2-char string
            
            try {
                //int64_t post_id = thread_id * 100000 + i;
                uint64_t post_id = generateUniquePostId(thread_id);
                Post post = createSamplePost(post_id, i, thread_id);
                client.StorePost(i, post, carrier);
                stored_post_ids.push_back(post_id);
            } catch (const TException& e) {
                if (verbose) {
                    std::cerr << "Warmup error in thread " << thread_id << ": " << e.what() << std::endl;
                }
            }
        }
        
        if (verbose) {
            std::cout << "Thread " << thread_id << " completed warmup, stored " 
                      << stored_post_ids.size() << " posts" << std::endl;
        }
        
        // Measurement phase
        for (int i = 0; i < operations_per_thread; i++) {
            global_metrics.total_requests++;
            
            std::map<std::string, std::string> carrier;
            //carrier["trace-id"] = "test-" + std::to_string(thread_id) + "-" + std::to_string(i);
            //carrier["span-id"] = std::to_string(thread_id * 10000 + i);
            
            carrier["trace-id"] = "test-0000-0000";  // Fixed 14-char string
            carrier["span-id"] = "00";               // Fixed 2-char string

            int operation = operation_dist(gen);
            //int operation = i % 3;
            auto start_time = std::chrono::high_resolution_clock::now();
            
            try {
                //if (operation == 0 || stored_post_ids.empty()) {
                if (operation == 0) {
                    // Store operation
                    //int64_t post_id = thread_id * 100000 + warmup_operations + i;
                    int64_t post_id = generateUniquePostId(thread_id);
                    Post post = createSamplePost(post_id, thread_id * 10000 + i, thread_id);
                    
                    client.StorePost(thread_id * 10000 + i, post, carrier);
                    stored_post_ids.push_back(post_id);
                    global_metrics.store_operations++;
                    
                    if (verbose && i % 100 == 0) {
                        std::cout << "Thread " << thread_id << " stored post " << post_id << std::endl;
                    }
                    
                } else if (operation == 2) {
                    // Read single post
                    int idx = gen() % stored_post_ids.size();
                    int64_t post_id = stored_post_ids[idx];
                    // int64_t post_id = stored_post_ids[0];
                    Post result;
                    
                    client.ReadPost(result, thread_id * 1000000 + i, post_id, carrier);
                    global_metrics.read_operations++;
                    
                    if (verbose && i % 100 == 0) {
                        std::cout << "Thread " << thread_id << " read post " << post_id 
                                  << " (text: " << result.text.substr(0, 30) << "...)" << std::endl;
                    }
                    
                } else {
                    // Read multiple posts
                    int count = 3;
                    
                    std::set<int> unique_indices;
                    while (unique_indices.size() < (uint) count && unique_indices.size() < stored_post_ids.size()) {
                       int idx = gen() % stored_post_ids.size();
                       unique_indices.insert(idx);
                    }

                    std::vector<int64_t> post_ids;
                    for (int idx : unique_indices) {
                       post_ids.push_back(stored_post_ids[idx]);
                    }

                    std::vector<Post> results;
                    client.ReadPosts(results, thread_id * 1000000 + i, post_ids, carrier);
                    global_metrics.read_multi_operations++;
                    
                    if (verbose && i % 100 == 0) {
                        std::cout << "Thread " << thread_id << " read " << results.size() 
                                  << " posts in bulk" << std::endl;
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
                    // return;  // Exit on error
                }
            }
        }
        
        transport->close();
        
        if (verbose) {
            std::cout << "Thread " << thread_id << " completed, stored " 
                      << stored_post_ids.size() << " total posts" << std::endl;
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
    std::cout << "Store operations: " << global_metrics.store_operations.load() << std::endl;
    std::cout << "Read operations: " << global_metrics.read_operations.load() << std::endl;
    std::cout << "Read multi operations: " << global_metrics.read_multi_operations.load() << std::endl;
    
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
    std::cout << "  -p, --port <port>       Server port (default: 9091)" << std::endl;
    std::cout << "  -t, --threads <num>     Number of client threads (default: 4)" << std::endl;
    std::cout << "  -o, --operations <num>  Operations per thread (default: 500)" << std::endl;
    std::cout << "  -w, --warmup <num>      Warmup operations per thread (default: 50)" << std::endl;
    std::cout << "  -v, --verbose           Verbose output" << std::endl;
    std::cout << "  --help                  Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string server_host = "localhost";
    int server_port = 9091;
    int num_threads = 4;
    int operations_per_thread = 500;
    int warmup_operations = 10;
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

    // Check warmup operations
    if (warmup_operations == 0) {
        warmup_operations = 10;
        std::cout << "<Warning>: Warmup cannot be 0, set to default 10" << std::endl;
    }
    
    std::cout << "=== PostStorage Service Client Test ===" << std::endl;
    std::cout << "Server: " << server_host << ":" << server_port << std::endl;
    std::cout << "Threads: " << num_threads << std::endl;
    std::cout << "Operations per thread: " << operations_per_thread << std::endl;
    std::cout << "Warmup operations per thread: " << warmup_operations << std::endl;
    std::cout << "Mix: Store/Read/ReadMulti operations" << std::endl;
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
