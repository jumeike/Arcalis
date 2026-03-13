#!/bin/bash

# PostStorage Service Test Runner Script
# This script automates testing of the PostStorage service with various configurations

set -e  # Exit on any error

# Configuration
SERVER_HOST=${SERVER_HOST:-localhost}
SERVER_PORT=${SERVER_PORT:-9091}
CLIENT_BINARY=${CLIENT_BINARY:-./post_storage_client_test}
SERVER_BINARY=${SERVER_BINARY:-""}
RESULTS_DIR=${RESULTS_DIR:-test_results}
SERVER_PID=""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

echo_section() {
    echo -e "${BLUE}[SECTION]${NC} $1"
}

kill_server() {
    echo_info "Stopping PostStorage server..."
    
    # Find and kill PostStorage server process
    local server_pids=$(pgrep -f "PostStorageService" 2>/dev/null || echo "")
    
    if [[ -n "$server_pids" ]]; then
        echo_info "Found PostStorage server process(es): $server_pids"
        kill $server_pids 2>/dev/null || true
        sleep 2
        
        # Force kill if still running
        local remaining_pids=$(pgrep -f "PostStorageService" 2>/dev/null || echo "")
        if [[ -n "$remaining_pids" ]]; then
            echo_warn "Force killing remaining processes: $remaining_pids"
            kill -9 $remaining_pids 2>/dev/null || true
        fi
        
        echo_info "PostStorage server stopped ✓"
    else
        echo_warn "No PostStorage server process found"
    fi
}

check_server() {
    echo_info "Checking if PostStorage server is running on ${SERVER_HOST}:${SERVER_PORT}"
    if ! nc -z "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null; then
        echo_error "PostStorage server is not reachable at ${SERVER_HOST}:${SERVER_PORT}"
        echo_error "Please make sure the PostStorage service is running"
        echo_error "Or set SERVER_BINARY to auto-start: SERVER_BINARY=path/to/PostStorageService $0 basic"
        exit 1
    fi
    echo_info "PostStorage server is running ✓"
}

check_client() {
    if [[ ! -f "$CLIENT_BINARY" ]]; then
        echo_error "Client binary not found: $CLIENT_BINARY"
        echo_error "Please build the client first with: make"
        exit 1
    fi
    echo_info "Client binary found ✓"
}

check_dependencies() {
    echo_info "Checking dependencies..."
    
    # Check if docker is running (for memcached/mongodb)
    if ! docker ps >/dev/null 2>&1; then
        echo_warn "Docker not running - using local databases"
    else
        echo_info "Docker is running ✓"
    fi
    
    # Check if MongoDB is accessible
    if nc -z localhost 27017 2>/dev/null; then
        echo_info "MongoDB accessible on localhost:27017 ✓"
    else
        echo_warn "MongoDB not accessible on localhost:27017"
    fi
    
    # Check if Memcached is accessible
    if nc -z localhost 11211 2>/dev/null; then
        echo_info "Memcached accessible on localhost:11211 ✓"
    elif nc -z localhost 11212 2>/dev/null; then
        echo_info "Memcached accessible on localhost:11212 ✓"
    else
        echo_warn "Memcached not accessible on standard ports"
    fi
}

setup_results_dir() {
    mkdir -p "$RESULTS_DIR"
    echo_info "Results will be saved to: $RESULTS_DIR"
}

run_test() {
    local test_name="$1"
    local threads="$2"
    local operations="$3"
    local warmup="$4"
    local extra_args="$5"
    
    local timestamp=$(date +"%Y%m%d_%H%M%S")
    local output_file="${RESULTS_DIR}/${test_name}_${timestamp}.log"
    
    echo_info "Running test: $test_name"
    echo_info "  Config: $threads threads, $operations ops/thread, $warmup warmup"
    
    # Kill any existing server and start fresh
    if [[ -n "$SERVER_BINARY" ]]; then
        kill_server
        start_server
    fi
    
    if $CLIENT_BINARY -h "$SERVER_HOST" -p "$SERVER_PORT" \
                     -t "$threads" -o "$operations" -w "$warmup" \
                     $extra_args 2>&1 | tee "$output_file"; then
        echo_info "Test completed successfully ✓"
        echo_info "Results saved to: $output_file"
    else
        echo_error "Test failed ✗"
        return 1
    fi
    
    # Kill server after test to clean MongoDB
    if [[ -n "$SERVER_BINARY" ]]; then
        kill_server
        sleep 1
    fi
    
    echo ""
}

run_basic_tests() {
    echo_section "=== Running Basic Functionality Tests ==="
    
    # Single thread, basic operations
    run_test "basic_single_thread" 1 10 5 "-v"
    
    # Test each operation type
    run_test "basic_store_only" 1 20 10 ""
    
    # Small multi-thread test
    run_test "basic_multi_thread" 2 10 5 "-v"
}

run_performance_tests() {
    echo_section "=== Running Performance Tests ==="
    
    # Scaling tests
    run_test "perf_2threads" 2 100 20 ""
    run_test "perf_4threads" 4 100 20 ""
    run_test "perf_8threads" 8 100 20 ""
    
    # High load test
    run_test "perf_high_load" 4 500 50 ""
    
    # Sustained load
    run_test "perf_sustained" 8 1000 100 ""
}

run_stress_tests() {
    echo_section "=== Running Stress Tests ==="
    
    # High thread count
    run_test "stress_many_threads" 16 200 50 ""
    
    # High operation count
    run_test "stress_many_ops" 8 2000 200 ""
    
    # Burst test
    run_test "stress_burst" 12 500 100 ""
}

run_cache_tests() {
    echo_section "=== Running Cache Behavior Tests ==="
    
    # Test cache hit behavior
    echo_info "Testing cache hit patterns..."
    run_test "cache_warmup" 2 100 50 "-v"
    
    # Immediate read test (should have high cache hit rate)
    echo_info "Testing immediate reads for cache hits..."
    run_test "cache_hits" 4 200 100 ""
}

analyze_results() {
    echo_section "=== Analyzing Results ==="
    
    local summary_file="${RESULTS_DIR}/test_summary_$(date +"%Y%m%d_%H%M%S").txt"
    
    echo "PostStorage Service Test Summary" > "$summary_file"
    echo "Generated: $(date)" >> "$summary_file"
    echo "Server: ${SERVER_HOST}:${SERVER_PORT}" >> "$summary_file"
    echo "" >> "$summary_file"
    
    echo_info "Analyzing log files..."
    
    for log_file in "${RESULTS_DIR}"/*.log; do
        if [[ -f "$log_file" ]]; then
            echo "=== $(basename "$log_file") ===" >> "$summary_file"
            
            # Extract key metrics from log
            local throughput=$(grep "Throughput:" "$log_file" | tail -1 | awk '{print $2}')
            local avg_latency=$(grep "Average:" "$log_file" | tail -1 | awk '{print $2}')
            local success_rate=$(grep "Successful:" "$log_file" | tail -1 | awk '{print $2 " " $3}')
            local total_ops=$(grep "Total Requests:" "$log_file" | tail -1 | awk '{print $3}')
            
            if [[ -n "$throughput" ]]; then
                echo "Throughput: ${throughput} req/s" >> "$summary_file"
            fi
            if [[ -n "$avg_latency" ]]; then
                echo "Avg Latency: ${avg_latency} μs" >> "$summary_file"
            fi
            if [[ -n "$success_rate" ]]; then
                echo "Success Rate: ${success_rate}" >> "$summary_file"
            fi
            if [[ -n "$total_ops" ]]; then
                echo "Total Operations: ${total_ops}" >> "$summary_file"
            fi
            
            # Check for errors
            local error_count=$(grep -c "failed:" "$log_file" 2>/dev/null || echo "0")
            echo "Errors: ${error_count}" >> "$summary_file"
            
            echo "" >> "$summary_file"
        fi
    done
    
    echo_info "Summary saved to: $summary_file"
    
    # Display quick summary
    echo_section "=== Quick Summary ==="
    local total_tests=$(ls "${RESULTS_DIR}"/*.log 2>/dev/null | wc -l)
    local failed_tests=$(grep -l "failed:" "${RESULTS_DIR}"/*.log 2>/dev/null | wc -l)
    local passed_tests=$((total_tests - failed_tests))
    
    echo_info "Total tests: $total_tests"
    echo_info "Passed: $passed_tests"
    if [[ $failed_tests -gt 0 ]]; then
        echo_warn "Failed: $failed_tests"
    fi
}

cleanup_on_exit() {
    echo_info "Cleaning up..."
    if [[ -n "$SERVER_BINARY" ]]; then
        kill_server
    fi
}

print_usage() {
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  basic     - Run basic functionality tests"
    echo "  perf      - Run performance tests"
    echo "  stress    - Run stress tests"
    echo "  cache     - Run cache behavior tests"
    echo "  all       - Run all tests"
    echo "  check     - Check server connectivity and dependencies"
    echo ""
    echo "Environment variables:"
    echo "  SERVER_HOST     - Server hostname (default: localhost)"
    echo "  SERVER_PORT     - Server port (default: 9091)"
    echo "  CLIENT_BINARY   - Path to client binary (default: ./post_storage_client_test)"
    echo "  SERVER_BINARY   - Path to server binary (optional, for auto-restart)"
    echo "  RESULTS_DIR     - Results directory (default: test_results)"
    echo ""
    echo "Examples:"
    echo "  $0 basic                                              # Run basic tests (manual server)"
    echo "  SERVER_BINARY=../server/build/PostStorageService $0 perf  # Auto-restart server"
    echo "  SERVER_PORT=9090 $0 perf                             # Different port"
    echo "  $0 check                                             # Check connectivity"
}

# Set up cleanup on exit
trap cleanup_on_exit EXIT

# Main execution
case "${1:-all}" in
    "check")
        if [[ -n "$SERVER_BINARY" ]]; then
            kill_server
            start_server
            kill_server
        else
            check_server
        fi
        check_client
        check_dependencies
        ;;
    "basic")
        if [[ -z "$SERVER_BINARY" ]]; then
            check_server
        fi
        check_client
        setup_results_dir
        run_basic_tests
        analyze_results
        ;;
    "perf")
        if [[ -z "$SERVER_BINARY" ]]; then
            check_server
        fi
        check_client
        setup_results_dir
        run_performance_tests
        analyze_results
        ;;
    "stress")
        if [[ -z "$SERVER_BINARY" ]]; then
            check_server
        fi
        check_client
        setup_results_dir
        run_stress_tests
        analyze_results
        ;;
    "cache")
        if [[ -z "$SERVER_BINARY" ]]; then
            check_server
        fi
        check_client
        setup_results_dir
        run_cache_tests
        analyze_results
        ;;
    "all")
        if [[ -z "$SERVER_BINARY" ]]; then
            check_server
        fi
        check_client
        check_dependencies
        setup_results_dir
        run_basic_tests
        run_performance_tests
        run_cache_tests
        run_stress_tests
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
