#!/bin/bash

# UniqueID Service Test Runner Script
# This script automates testing of the UniqueID service with various configurations

set -e  # Exit on any error

# Configuration
SERVER_HOST=${SERVER_HOST:-localhost}
SERVER_PORT=${SERVER_PORT:-9090}
CLIENT_BINARY=${CLIENT_BINARY:-./uid_client_test}
RESULTS_DIR=${RESULTS_DIR:-test_results}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

echo_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

echo_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_server() {
    echo_info "Checking if server is running on ${SERVER_HOST}:${SERVER_PORT}"
    if ! nc -z "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null; then
        echo_error "Server is not reachable at ${SERVER_HOST}:${SERVER_PORT}"
        echo_error "Please make sure the UniqueID service is running"
        exit 1
    fi
    echo_info "Server is running ✓"
}

check_client() {
    if [[ ! -f "$CLIENT_BINARY" ]]; then
        echo_error "Client binary not found: $CLIENT_BINARY"
        echo_error "Please build the client first with: make"
        exit 1
    fi
    echo_info "Client binary found ✓"
}

setup_results_dir() {
    mkdir -p "$RESULTS_DIR"
    echo_info "Results will be saved to: $RESULTS_DIR"
}

run_test() {
    local test_name="$1"
    local threads="$2"
    local requests="$3"
    local warmup="$4"
    local extra_args="$5"
    
    local timestamp=$(date +"%Y%m%d_%H%M%S")
    local output_file="${RESULTS_DIR}/${test_name}_${timestamp}.csv"
    
    echo_info "Running test: $test_name"
    echo_info "  Threads: $threads, Requests per thread: $requests, Warmup: $warmup"
    
    if $CLIENT_BINARY -h "$SERVER_HOST" -p "$SERVER_PORT" \
                     -t "$threads" -r "$requests" -w "$warmup" \
                     -o "$output_file" $extra_args; then
        echo_info "Test completed successfully ✓"
        echo_info "Results saved to: $output_file"
    else
        echo_error "Test failed ✗"
        return 1
    fi
    
    echo ""
}

run_baseline_tests() {
    echo_info "=== Running Baseline Performance Tests ==="
    
    # Single thread baseline
    run_test "baseline_1thread" 1 1000 100 ""
    
    # Multi-thread scaling tests
    run_test "baseline_2threads" 2 1000 100 ""
    run_test "baseline_4threads" 4 1000 100 ""
    run_test "baseline_8threads" 8 1000 100 ""
    
    # High load test
    run_test "baseline_high_load" 8 5000 200 ""
}

run_stress_tests() {
    echo_info "=== Running Stress Tests ==="
    
    # Stress test with many threads
    run_test "stress_16threads" 16 2000 200 ""
    
    # Very high request rate
    run_test "stress_burst" 4 10000 500 ""
    
    # Sustained load
    run_test "stress_sustained" 8 20000 1000 ""
}

run_latency_tests() {
    echo_info "=== Running Latency Analysis Tests ==="
    
    # Low concurrency for latency measurement
    run_test "latency_1thread" 1 5000 100 "-v"
    run_test "latency_2threads" 2 5000 100 "-v"
    
    # Medium concurrency
    run_test "latency_4threads" 4 5000 100 "-v"
}

analyze_results() {
    echo_info "=== Analyzing Results ==="
    
    local summary_file="${RESULTS_DIR}/test_summary_$(date +"%Y%m%d_%H%M%S").txt"
    
    echo "UniqueID Service Test Summary" > "$summary_file"
    echo "Generated: $(date)" >> "$summary_file"
    echo "Server: ${SERVER_HOST}:${SERVER_PORT}" >> "$summary_file"
    echo "" >> "$summary_file"
    
    for csv_file in "${RESULTS_DIR}"/*.csv; do
        if [[ -f "$csv_file" ]]; then
            echo "=== $(basename "$csv_file") ===" >> "$summary_file"
            
            # Extract key metrics
            local throughput=$(grep "throughput_rps" "$csv_file" | cut -d',' -f2)
            local avg_latency=$(grep "avg_latency_us" "$csv_file" | cut -d',' -f2)
            local successful=$(grep "successful_requests" "$csv_file" | cut -d',' -f2)
            local failed=$(grep "failed_requests" "$csv_file" | cut -d',' -f2)
            
            echo "Throughput: ${throughput} req/s" >> "$summary_file"
            echo "Avg Latency: ${avg_latency} μs" >> "$summary_file"
            echo "Success Rate: ${successful}/${successful} ($(echo "scale=2; $successful * 100 / ($successful + $failed)" | bc -l)%)" >> "$summary_file"
            echo "" >> "$summary_file"
        fi
    done
    
    echo_info "Summary saved to: $summary_file"
}

# Main execution
print_usage() {
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  baseline  - Run baseline performance tests"
    echo "  stress    - Run stress tests"
    echo "  latency   - Run latency analysis tests"
    echo "  all       - Run all tests"
    echo "  check     - Check server connectivity and client binary"
    echo ""
    echo "Environment variables:"
    echo "  SERVER_HOST - Server hostname (default: localhost)"
    echo "  SERVER_PORT - Server port (default: 9090)"
    echo "  CLIENT_BINARY - Path to client binary (default: ./uid_client_test)"
    echo "  RESULTS_DIR - Results directory (default: test_results)"
}

case "${1:-all}" in
    "check")
        check_server
        check_client
        ;;
    "baseline")
        check_server
        check_client
        setup_results_dir
        run_baseline_tests
        analyze_results
        ;;
    "stress")
        check_server
        check_client
        setup_results_dir
        run_stress_tests
        analyze_results
        ;;
    "latency")
        check_server
        check_client
        setup_results_dir
        run_latency_tests
        analyze_results
        ;;
    "all")
        check_server
        check_client
        setup_results_dir
        run_baseline_tests
        run_stress_tests
        run_latency_tests
        analyze_results
        ;;
    "help"|"-h"|"--help")
        print_usage
        ;;
    *)
        echo_error "Unknown command: $1"
        print_usage
        exit 1
        ;;
esac

echo_info "Test run completed!"
