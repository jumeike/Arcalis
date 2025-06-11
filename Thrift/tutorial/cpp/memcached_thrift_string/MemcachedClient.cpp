#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include "gen-cpp/MemcachedService.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <random>
#include <thread>
#include <atomic>
#include <iomanip>
#include <unordered_map>
#include <mutex>

using namespace std;
using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace thrift_memcached;

class BenchmarkStats {
public:
    BenchmarkStats(int valueSize = 128) : valueSize(valueSize) {} 

    void addLatency(double latency, bool isSet, bool success) {
        latencies.push_back(latency);
        if (isSet) {
            setCount++;
            if (success) setSuccessCount++;
        } else {
            getCount++;
            if (success) getSuccessCount++;
        }
    }

    void calculate() {
        sort(latencies.begin(), latencies.end());
        size_t size = latencies.size();
        // Calculate percentiles
        p50 = latencies[size * 0.5];     // Median (50th percentile)
        p90 = latencies[size * 0.9];     // 90th percentile
        p99 = latencies[size * 0.99];    // 99th percentile
        p999 = latencies[size * 0.999];  // 99.9th percentile
        p9999 = latencies[size * 0.9999];// 99.99th percentile
        
        double sum = 0;
        for (const auto& latency : latencies) {
            sum += latency;
        }
        average = sum / size;
    }

    double elapsed_time; // in seconds

    void print() const {
        cout << fixed << setprecision(3);
        cout << "Elapsed time: " << elapsed_time << " seconds" << endl;
        cout << "Average latency: " << average << " ms" << endl;
        cout << "P50 latency: " << p50 << " ms" << endl;
        cout << "P90 latency: " << p90 << " ms" << endl;
        cout << "P99 latency: " << p99 << " ms" << endl;
        cout << "P999 latency: " << p999 << " ms" << endl;
        cout << "P9999 latency: " << p9999 << " ms" << endl;
        cout << "Throughput: " << throughput << " ops/sec(RPS)" << endl;
        cout << setprecision(2);  // Change to 2 decimal places for percentages
        double setSuccessRate = setCount > 0 ? (100.0 * setSuccessCount / setCount) : 0.0;
        double getSuccessRate = getCount > 0 ? (100.0 * getSuccessCount / getCount) : 0.0;
        cout << "Set operations: " << setCount << " (Success: " << setSuccessRate << "%)" << endl;
        cout << "Get operations: " << getCount << " (Success: " << getSuccessRate << "%)" << endl;
        
        // Calculate network throughput
        double bytes_per_get = 35;
        if (getCount > 0){
            bytes_per_get += (valueSize * (getSuccessCount / static_cast<double>(getCount)));
        }
        double bytes_per_set = 35 + valueSize;  // key + value for SET operations
        
        double total_bytes_per_sec = throughput * 
            ((setCount > 0 ? (bytes_per_set * setCount) : 0) + 
            (getCount > 0 ? (bytes_per_get * getCount) : 0)) / 
            (setCount + getCount);
            
        double gbps = (total_bytes_per_sec * 8) / (1024 * 1024 * 1024);
        
        cout << "Network Throughput: " << fixed << setprecision(3) 
            << gbps << " Gbps (" 
            << gbps * 1000 << " Mbps)" << endl;
    }

    vector<double> latencies;
    double p50, p90, p99, p999, p9999, average;
    double throughput;
    int setCount = 0, getCount = 0, setSuccessCount = 0, getSuccessCount = 0;
    int valueSize;
};

class MemcachedClient {
public:
    MemcachedClient(const string& host, int port) 
        : socket(new TSocket(host, port)),
          transport(new TBufferedTransport(socket)),
          protocol(new TBinaryProtocol(transport)),
          client(protocol) {}

    void connect() {
        transport->open();
    }

    void close() {
        transport->close();
    }

    bool set(const string& key, const string& value) {
        lock_guard<mutex> lock(client_mutex);  // Lock during operation
        return client.setRequest(key, value);
    }

    bool get(string& value, const string& key) {
        lock_guard<mutex> lock(client_mutex);  // Lock during operation
        client.getRequest(value, key);
        return !value.empty() && value != "Key not found";
    }

private:
    shared_ptr<TTransport> socket;
    shared_ptr<TTransport> transport;
    shared_ptr<TProtocol> protocol;
    MemcachedServiceClient client;
    mutex client_mutex;  // Mutex for thread safety
};

class KeyValueGenerator {
public:
    enum class KeyDistribution {
        SEQUENTIAL,    // key0, key1, key2...
        POOL,         // Reuse from a fixed pool of keys
        RANDOM,       // Random keys
        ZIPFIAN       // Some keys are more frequently accessed than others
    };

    KeyValueGenerator(KeyDistribution dist, int poolSize, int valueSize) 
        : distribution(dist), 
          poolSize(poolSize), 
          valueSize(valueSize),
          rng(std::random_device{}()),
          zipfian_dist(1.5) // Zipf parameter, higher = more skewed
    {
        // Pre-generate the value string template
        valueTemplate = string(valueSize, 'x');
    }

    string generateKey(int operation) {
        switch (distribution) {
            case KeyDistribution::SEQUENTIAL:
                return "key" + to_string(operation);
            
            case KeyDistribution::POOL:
                return "key" + to_string(operation % poolSize);
            
            case KeyDistribution::RANDOM:
                return "key" + to_string(uniform_dist(rng) % poolSize);
            
            case KeyDistribution::ZIPFIAN:
                // Zipfian distribution for more realistic cache access patterns
                return "key" + to_string(static_cast<int>(zipfian_dist(rng) * poolSize));
            
            default:
                return "key" + to_string(operation % poolSize);
        }
    }

    string generateValue(int operation) {
        // Create a value with a predictable pattern including the operation number
        string value = valueTemplate;
        string opStr = to_string(operation);
        // Insert the operation number at the start of the value
        value.replace(0, opStr.length(), opStr);
        return value;
    }

private:
    KeyDistribution distribution;
    int poolSize;
    int valueSize;
    string valueTemplate;
    mt19937 rng;
    uniform_int_distribution<> uniform_dist;
    
    // Zipfian distribution implementation
    class ZipfianDist {
    public:
        ZipfianDist(double alpha) : alpha(alpha) {}
        
        double operator()(mt19937& rng) {
            uniform_real_distribution<> uniform(0.0, 1.0);
            double u = uniform(rng);
            return pow(1.0 - u, 1.0 / alpha);
        }
    private:
        double alpha;
    } zipfian_dist;
};

void runBenchmark(MemcachedClient& client, 
                  int numOperations, 
                  double setRatio, 
                  BenchmarkStats& stats,
                  KeyValueGenerator& kvGen) {
    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<> dis(0.0, 1.0);

    auto start = chrono::high_resolution_clock::now();

    for (int i = 0; i < numOperations; ++i) {
        string key = kvGen.generateKey(i);
        string value = kvGen.generateValue(i);

        auto opStart = chrono::high_resolution_clock::now();
        bool success;
        bool isSet = dis(gen) < setRatio;
        
        if (isSet) {
            success = client.set(key, value);
        } else {
            success = client.get(value, key);
        }

        auto opEnd = chrono::high_resolution_clock::now();
        double latency = chrono::duration<double, milli>(opEnd - opStart).count();
        stats.addLatency(latency, isSet, success);
    }

    auto end = chrono::high_resolution_clock::now();
    stats.elapsed_time = chrono::duration<double>(end - start).count();
    stats.throughput = numOperations / stats.elapsed_time;
}

unordered_map<string, string> parseArgs(int argc, char** argv) {
    unordered_map<string, string> args;
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 < argc) {
            string key = argv[i];
            if (key.substr(0, 2) == "--") {
                args[key.substr(2)] = argv[i + 1];
            }
        }
    }
    return args;
}

KeyValueGenerator::KeyDistribution parseDistribution(const string& dist) {
    if (dist == "sequential") return KeyValueGenerator::KeyDistribution::SEQUENTIAL;
    if (dist == "pool") return KeyValueGenerator::KeyDistribution::POOL;
    if (dist == "random") return KeyValueGenerator::KeyDistribution::RANDOM;
    if (dist == "zipfian") return KeyValueGenerator::KeyDistribution::ZIPFIAN;
    // Default to POOL if unrecognized
    return KeyValueGenerator::KeyDistribution::POOL;
}

int main(int argc, char** argv) {
    auto args = parseArgs(argc, argv);

    // Check required arguments
    if (args.find("host") == args.end() || 
        args.find("port") == args.end() ||
        args.find("num-operations") == args.end() || 
        args.find("num-threads") == args.end() ||
        args.find("set-ratio") == args.end()) {
        
        cerr << "Usage: " << argv[0] 
             << " --host <host> --port <port>"
             << " --num-operations <num_operations>"
             << " --num-threads <num_threads>"
             << " --set-ratio <set_ratio>"
             << "\nOptional arguments:"
             << "\n --pool-size <pool_size> (default: 1000)"
             << "\n --value-size <value_size> (default: 128)"
             << "\n --distribution <sequential|pool|random|zipfian> (default: pool)"
             << endl;
        return 1;
    }

    // Parse required arguments
    string host = args["host"];
    int port = stoi(args["port"]);
    int numOperations = stoi(args["num-operations"]);
    int numThreads = stoi(args["num-threads"]);
    double setRatio = stod(args["set-ratio"]);

    // Parse optional arguments with defaults
    int poolSize = args.find("pool-size") != args.end() ? 
                  stoi(args["pool-size"]) : 1000;
    
    int valueSize = args.find("value-size") != args.end() ? 
                   stoi(args["value-size"]) : 128;
    
    auto distribution = args.find("distribution") != args.end() ? 
                       parseDistribution(args["distribution"]) : 
                       KeyValueGenerator::KeyDistribution::POOL;

    // Print configuration
    cout << "------------------------" << endl
         << "Benchmark Configuration:" << endl
         << "------------------------" << endl
         << "Host: " << host << endl
         << "Port: " << port << endl
         << "Total operations: " << numOperations << endl
         << "Number of threads: " << numThreads << endl
         << "Set Ratio: " << setRatio << endl
         << "Pool Size: " << poolSize << endl
         << "Value Size: " << valueSize << endl
         << "Distribution: " << args.find("distribution")->second << endl;

    // Create a single shared client
    MemcachedClient sharedClient(host, port);
    try {
        sharedClient.connect();
    } catch (TException& tx) {
        cerr << "ERROR: Failed to connect: " << tx.what() << endl;
        return 1;
    }
    std::vector<std::thread> threads;
    vector<BenchmarkStats> threadStats(numThreads, BenchmarkStats(valueSize));

    auto total_start = chrono::high_resolution_clock::now();

    auto benchmarkFunc = [&](int threadId) {
        // MemcachedClient client(host, port);
        try {
            // Create KeyValueGenerator for this thread
            KeyValueGenerator kvGen(
                distribution,
                poolSize,
                valueSize
            );
            
            // Pass shared client to runBenchmark
            runBenchmark(sharedClient, 
                        numOperations / numThreads, 
                        setRatio, 
                        threadStats[threadId], 
                        kvGen);

        } catch (TException& tx) {
            cerr << "ERROR: " << tx.what() << endl;
        }
    };

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(benchmarkFunc, i);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto total_end = chrono::high_resolution_clock::now();
    sharedClient.close();

    BenchmarkStats totalStats(valueSize);
    totalStats.elapsed_time = chrono::duration<double>(total_end - total_start).count();

    for (const auto& stats : threadStats) {
        totalStats.latencies.insert(totalStats.latencies.end(), stats.latencies.begin(), stats.latencies.end());
        totalStats.throughput += stats.throughput;
        totalStats.setCount += stats.setCount;
        totalStats.getCount += stats.getCount;
        totalStats.setSuccessCount += stats.setSuccessCount;
        totalStats.getSuccessCount += stats.getSuccessCount;
    }

    totalStats.calculate();
    
    cout << "------------------" << endl;
    cout << "Benchmark Results:" << endl;
    cout << "------------------" << endl;
    totalStats.print();

    return 0;
}
