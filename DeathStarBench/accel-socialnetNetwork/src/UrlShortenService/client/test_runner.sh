#!/bin/bash

# UrlShorten Service Test Runner

set -e

SERVER_HOST=${SERVER_HOST:-localhost}
SERVER_PORT=${SERVER_PORT:-9090}
CLIENT_BINARY=${CLIENT_BINARY:-./url_shorten_client_test}
RESULTS_DIR=${RESULTS_DIR:-test_results}

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
echo_error() { echo -e "${RED}[ERROR]${NC} $1"; }
echo_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }

check_server() {
    echo_info "Checking server at ${SERVER_HOST}:${SERVER_PORT}"
    if ! nc -z "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null; then
        echo_error "Server not reachable"
        echo_error "Start the service: ./src/UrlShortenService/server/build/UrlShortenService"
        exit 1
    fi
    echo_info "Server running ✓"
}

check_client() {
    if [[ ! -f "$CLIENT_BINARY" ]]; then
        echo_error "Client not found: $CLIENT_BINARY"
        echo_error "Build with: make"
        exit 1
    fi
    echo_info "Client found ✓"
}

run_test() {
    local test_name="$1"
    local threads="$2"
    local operations="$3"
    local warmup="$4"
    
    local timestamp=$(date +"%Y%m%d_%H%M%S")
    local output_file="${RESULTS_DIR}/${test_name}_${timestamp}.log"
    
    mkdir -p "$RESULTS_DIR"
    
    echo_info "Running: $test_name ($threads threads, $operations ops)"
    
    if $CLIENT_BINARY -h "$SERVER_HOST" -p "$SERVER_PORT" \
                     -t "$threads" -o "$operations" -w "$warmup" \
                     2>&1 | tee "$output_file"; then
        echo_info "Test completed ✓"
        echo_info "Results: $output_file"
    else
        echo_error "Test failed ✗"
        return 1
    fi
    echo ""
}

run_basic_tests() {
    echo_info "=== Basic Tests ==="
    run_test "basic_single" 1 50 10
    run_test "basic_multi" 2 100 20
}

run_perf_tests() {
    echo_info "=== Performance Tests ==="
    run_test "perf_2t" 2 200 50
    run_test "perf_4t" 4 200 50
    run_test "perf_8t" 8 200 50
}

print_usage() {
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  basic   - Basic functionality tests"
    echo "  perf    - Performance tests"
    echo "  all     - All tests"
    echo "  check   - Check connectivity"
    echo ""
    echo "Environment:"
    echo "  SERVER_HOST   - Server host (default: localhost)"
    echo "  SERVER_PORT   - Server port (default: 9090)"
    echo "  CLIENT_BINARY - Client path (default: ./url_shorten_client_test)"
    echo ""
    echo "Examples:"
    echo "  $0 basic"
    echo "  SERVER_PORT=9091 $0 perf"
}

case "${1:-basic}" in
    "check")
        check_server
        check_client
        ;;
    "basic")
        check_server
        check_client
        run_basic_tests
        ;;
    "perf")
        check_server
        check_client
        run_perf_tests
        ;;
    "all")
        check_server
        check_client
        run_basic_tests
        run_perf_tests
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

echo_info "Done!"
