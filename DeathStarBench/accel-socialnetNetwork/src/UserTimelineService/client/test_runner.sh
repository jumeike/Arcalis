#!/bin/bash

# UserTimeline Service Test Runner Script
# This script automates testing of the UserTimeline service with various configurations

set -e  # Exit on any error

# Configuration
SERVER_HOST=${SERVER_HOST:-localhost}
SERVER_PORT=${SERVER_PORT:-9092}
POST_STORAGE_PORT=${POST_STORAGE_PORT:-9091}
CLIENT_BINARY=${CLIENT_BINARY:-./user_timeline_client_test}
SERVER_BINARY=${SERVER_BINARY:-""}
POST_STORAGE_BINARY=${POST_STORAGE_BINARY:-""}
RESULTS_DIR=${RESULTS_DIR:-test_results}
SERVER_PID=""
POST_STORAGE_PID=""

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
    echo_info "Stopping UserTimeline server..."
    
    local server_pids=$(pgrep -f "UserTimelineService" 2>/dev/null || echo "")
    
    if [[ -n "$server_pids" ]]; then
        echo_info "Found UserTimeline server process(es): $server_pids"
        kill $server_pids 2>/dev/null || true
        sleep 2
        
        local remaining_pids=$(pgrep -f "UserTimelineService" 2>/dev/null || echo "")
        if [[ -n "$remaining_pids" ]]; then
            echo_warn "Force killing remaining processes: $remaining_pids"
            kill -9 $remaining_pids 2>/dev/null || true
        fi
        
        echo_info "UserTimeline server stopped ✓"
    else
        echo_warn "No UserTimeline server process found"
    fi
}

kill_post_storage_server() {
    echo_info "Stopping PostStorage server..."
    
    local server_pids=$(pgrep -f "PostStorageService" 2>/dev/null || echo "")
    
    if [[ -n "$server_pids" ]]; then
        echo_info "Found PostStorage server process(es): $server_pids"
        kill $server_pids 2>/dev/null || true
        sleep 2
        
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

start_post_storage_server() {
    if [[ -n "$POST_STORAGE_BINARY" ]]; then
        echo_info "Starting PostStorage server: $POST_STORAGE_BINARY"
        $POST_STORAGE_BINARY &
        POST_STORAGE_PID=$!
        sleep 3
        
        if ! kill -0 $POST_STORAGE_PID 2>/dev/null; then
            echo_error "Failed to start PostStorage server"
            exit 1
        fi
        
        if ! nc -z "$SERVER_HOST" "$POST_STORAGE_PORT" 2>/dev/null; then
            echo_error "PostStorage server not listening on port $POST_STORAGE_PORT"
            exit 1
        fi
        
        echo_info "PostStorage server started successfully ✓"
    fi
}

start_server() {
    if [[ -n "$SERVER_BINARY" ]]; then
        echo_info "Starting UserTimeline server: $SERVER_BINARY"
        $SERVER_BINARY &
        SERVER_PID=$!
        sleep 3
        
        if ! kill -0 $SERVER_PID 2>/dev/null; then
            echo_error "Failed to start UserTimeline server"
            exit 1
        fi
        
        if ! nc -z "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null; then
            echo_error "UserTimeline server not listening on port $SERVER_PORT"
            exit 1
        fi
        
        echo_info "UserTimeline server started successfully ✓"
    fi
}

check_servers() {
    echo_info "Checking if PostStorage server is running on ${SERVER_HOST}:${POST_STORAGE_PORT}"
    if ! nc -z "$SERVER_HOST" "$POST_STORAGE_PORT" 2>/dev/null; then
        echo_error "PostStorage server is not reachable at ${SERVER_HOST}:${POST_STORAGE_PORT}"
        echo_error "Please make sure the PostStorage service is running"
        exit 1
    fi
    echo_info "PostStorage server is running ✓"
    
    echo_info "Checking if UserTimeline server is running on ${SERVER_HOST}:${SERVER_PORT}"
    if ! nc -z "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null; then
        echo_error "UserTimeline server is not reachable at ${SERVER_HOST}:${SERVER_PORT}"
        echo_error "Please make sure the UserTimeline service is running"
        exit 1
    fi
    echo_info "UserTimeline server is running ✓"
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
    
    if ! docker ps >/dev/null 2>&1; then
        echo_warn "Docker not running - using local databases"
    else
        echo_info "Docker is running ✓"
    fi
    
    # Check MongoDB
    if nc -z localhost 27017 2>/dev/null; then
        echo_info "MongoDB accessible on localhost:27017 ✓"
    else
        echo_warn "MongoDB not accessible on localhost:27017"
    fi
    
    # Check Redis
    if nc -z localhost 6379 2>/dev/null; then
        echo_info "Redis accessible on localhost:6379 ✓"
    else
        echo_warn "Redis not accessible on localhost:6379"
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
    
    # Restart servers if auto-managed
    if [[ -n "$SERVER_BINARY" ]] || [[ -n "$POST_STORAGE_BINARY" ]]; then
        kill_server
        kill_post_storage_server
        start_post_storage_server
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
    
    echo ""
}

run_basic_tests() {
    echo_section "=== Running Basic Functionality Tests ==="
    
    run_test "basic_single_thread" 1 10 5 "-v"
    run_test "basic_timeline_ops" 1 20 10 ""
    run_test "basic_multi_thread" 2 10 5 "-v"
}

run_performance_tests() {
    echo_section "=== Running Performance Tests ==="
    
    run_test "perf_2threads" 2 100 20 ""
    run_test "perf_4threads" 4 100 20 ""
    run_test "perf_8threads" 8 100 20 ""
    run_test "perf_high_load" 4 500 50 ""
    run_test "perf_sustained" 8 1000 100 ""
}

run_stress_tests() {
    echo_section "=== Running Stress Tests ==="
    
    run_test "stress_many_threads" 16 200 50 ""
    run_test "stress_many_ops" 8 2000 200 ""
    run_test "stress_burst" 12 500 100 ""
}

run_timeline_tests() {
    echo_section "=== Running Timeline-Specific Tests ==="
    
    echo_info "Testing timeline write/read patterns..."
    run_test "timeline_writes" 4 100 50 ""
    run_test "timeline_reads" 4 200 100 ""
    run_test "timeline_mixed" 6 300 100 ""
}

analyze_results() {
    echo_section "=== Analyzing Results ==="
    
    local summary_file="${RESULTS_DIR}/test_summary_$(date +"%Y%m%d_%H%M%S").txt"
    
    echo "UserTimeline Service Test Summary" > "$summary_file"
    echo "Generated: $(date)" >> "$summary_file"
    echo "UserTimeline Server: ${SERVER_HOST}:${SERVER_PORT}" >> "$summary_file"
    echo "PostStorage Server: ${SERVER_HOST}:${POST_STORAGE_PORT}" >> "$summary_file"
    echo "" >> "$summary_file"
    
    echo_info "Analyzing log files..."
    
    for log_file in "${RESULTS_DIR}"/*.log; do
        if [[ -f "$log_file" ]]; then
            echo "=== $(basename "$log_file") ===" >> "$summary_file"
            
            local throughput=$(grep "Throughput:" "$log_file" | tail -1 | awk '{print $2}')
            local avg_latency=$(grep "Average:" "$log_file" | tail -1 | awk '{print $2}')
            local success_rate=$(grep "Successful:" "$log_file" | tail -1 | awk '{print $2 " " $3}')
            local total_ops=$(grep "Total Requests:" "$log_file" | tail -1 | awk '{print $3}')
            local write_ops=$(grep "Write operations:" "$log_file" | tail -1 | awk '{print $3}')
            local read_ops=$(grep "Read operations:" "$log_file" | tail -1 | awk '{print $3}')
            
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
            if [[ -n "$write_ops" ]]; then
                echo "Write Operations: ${write_ops}" >> "$summary_file"
            fi
            if [[ -n "$read_ops" ]]; then
                echo "Read Operations: ${read_ops}" >> "$summary_file"
            fi
            
            local error_count=$(grep -c "failed:" "$log_file" 2>/dev/null || echo "0")
            echo "Errors: ${error_count}" >> "$summary_file"
            
            echo "" >> "$summary_file"
        fi
    done
    
    echo_info "Summary saved to: $summary_file"
    
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
    if [[ -n "$POST_STORAGE_BINARY" ]]; then
        kill_post_storage_server
    fi
}

print_usage() {
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  basic     - Run basic functionality tests"
    echo "  perf      - Run performance tests"
    echo "  stress    - Run stress tests"
    echo "  timeline  - Run timeline-specific tests"
    echo "  all       - Run all tests"
    echo "  check     - Check server connectivity and dependencies"
    echo ""
    echo "Environment variables:"
    echo "  SERVER_HOST           - Server hostname (default: localhost)"
    echo "  SERVER_PORT           - UserTimeline port (default: 9090)"
    echo "  POST_STORAGE_PORT     - PostStorage port (default: 9091)"
    echo "  CLIENT_BINARY         - Path to client binary (default: ./user_timeline_client_test)"
    echo "  SERVER_BINARY         - Path to UserTimeline binary (optional)"
    echo "  POST_STORAGE_BINARY   - Path to PostStorage binary (optional)"
    echo "  RESULTS_DIR           - Results directory (default: test_results)"
    echo ""
    echo "Examples:"
    echo "  $0 basic                                             # Manual server mode"
    echo "  SERVER_BINARY=../server/build/UserTimelineService $0 perf  # Auto-restart"
    echo "  $0 check                                            # Check connectivity"
}

# Set up cleanup on exit
trap cleanup_on_exit EXIT

# Main execution
case "${1:-all}" in
    "check")
        if [[ -n "$POST_STORAGE_BINARY" ]] || [[ -n "$SERVER_BINARY" ]]; then
            kill_server
            kill_post_storage_server
            start_post_storage_server
            start_server
            kill_server
            kill_post_storage_server
        else
            check_servers
        fi
        check_client
        check_dependencies
        ;;
    "basic")
        if [[ -z "$SERVER_BINARY" ]] && [[ -z "$POST_STORAGE_BINARY" ]]; then
            check_servers
        fi
        check_client
        setup_results_dir
        run_basic_tests
        analyze_results
        ;;
    "perf")
        if [[ -z "$SERVER_BINARY" ]] && [[ -z "$POST_STORAGE_BINARY" ]]; then
            check_servers
        fi
        check_client
        setup_results_dir
        run_performance_tests
        analyze_results
        ;;
    "stress")
        if [[ -z "$SERVER_BINARY" ]] && [[ -z "$POST_STORAGE_BINARY" ]]; then
            check_servers
        fi
        check_client
        setup_results_dir
        run_stress_tests
        analyze_results
        ;;
    "timeline")
        if [[ -z "$SERVER_BINARY" ]] && [[ -z "$POST_STORAGE_BINARY" ]]; then
            check_servers
        fi
        check_client
        setup_results_dir
        run_timeline_tests
        analyze_results
        ;;
    "all")
        if [[ -z "$SERVER_BINARY" ]] && [[ -z "$POST_STORAGE_BINARY" ]]; then
            check_servers
        fi
        check_client
        check_dependencies
        setup_results_dir
        run_basic_tests
        run_performance_tests
        run_timeline_tests
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
