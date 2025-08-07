#!/bin/bash

# run_all_tests.sh
# Comprehensive Test Suite Execution Script for AncestralClust
# 
# This script runs the complete test suite including:
# - Unit tests for performance system
# - Integration tests with real clustering operations  
# - Stress tests for multi-threading and memory
# - Performance overhead validation
# - Test coverage analysis
#
# Usage: ./tests/scripts/run_all_tests.sh [options]
# Options:
#   --unit-only     Run only unit tests
#   --integration   Run only integration tests  
#   --stress        Run only stress tests
#   --coverage      Generate test coverage report
#   --performance   Measure performance overhead
#   --verbose       Enable verbose output
#   --help          Show this help message

set -e  # Exit on any error

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TEST_DIR="$PROJECT_ROOT/tests"
RESULTS_DIR="$TEST_DIR/results"
BINARY="$PROJECT_ROOT/ancestralclust"

# Test configuration
UNIT_TESTS=("test_performance" "test_thread_safety")
STRESS_TESTS=("test_performance_stress")
INTEGRATION_TESTS=("test_ancestralclust_integration")
PERFORMANCE_THRESHOLD=0.05  # 5% maximum overhead

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Flags
RUN_UNIT=true
RUN_INTEGRATION=true
RUN_STRESS=true
GENERATE_COVERAGE=false
MEASURE_PERFORMANCE=false
VERBOSE=false

# Usage information
show_help() {
    echo "AncestralClust Comprehensive Test Suite"
    echo ""
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  --unit-only     Run only unit tests"
    echo "  --integration   Run only integration tests"
    echo "  --stress        Run only stress tests"
    echo "  --coverage      Generate test coverage report"
    echo "  --performance   Measure performance overhead"
    echo "  --verbose       Enable verbose output"
    echo "  --help          Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                     # Run all tests"
    echo "  $0 --unit-only         # Run only unit tests"
    echo "  $0 --coverage          # Run all tests with coverage"
    echo "  $0 --performance       # Include performance measurements"
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --unit-only)
                RUN_UNIT=true
                RUN_INTEGRATION=false
                RUN_STRESS=false
                shift
                ;;
            --integration)
                RUN_UNIT=false
                RUN_INTEGRATION=true
                RUN_STRESS=false
                shift
                ;;
            --stress)
                RUN_UNIT=false
                RUN_INTEGRATION=false
                RUN_STRESS=true
                shift
                ;;
            --coverage)
                GENERATE_COVERAGE=true
                shift
                ;;
            --performance)
                MEASURE_PERFORMANCE=true
                shift
                ;;
            --verbose)
                VERBOSE=true
                shift
                ;;
            --help)
                show_help
                exit 0
                ;;
            *)
                echo "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done
}

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_verbose() {
    if [[ "$VERBOSE" == "true" ]]; then
        echo -e "${BLUE}[VERBOSE]${NC} $1"
    fi
}

# Environment setup
setup_environment() {
    log_info "Setting up test environment..."
    
    # Create results directory
    mkdir -p "$RESULTS_DIR"
    
    # Clean previous results
    rm -f "$RESULTS_DIR"/*.log "$RESULTS_DIR"/*.csv "$RESULTS_DIR"/*.html
    
    # Verify project structure
    if [[ ! -f "$PROJECT_ROOT/Makefile" ]]; then
        log_error "Makefile not found. Are you in the AncestralClust project root?"
        exit 1
    fi
    
    # Check if binary exists and build if necessary
    if [[ ! -f "$BINARY" ]]; then
        log_info "Building AncestralClust binary..."
        cd "$PROJECT_ROOT"
        make performance || {
            log_error "Failed to build AncestralClust binary"
            exit 1
        }
    fi
    
    log_success "Environment setup complete"
}

# Build all test executables
build_tests() {
    log_info "Building test executables..."
    
    cd "$PROJECT_ROOT"
    
    # Build unit tests
    if [[ "$RUN_UNIT" == "true" ]]; then
        log_verbose "Building unit tests..."
        make unit-tests || {
            log_error "Failed to build unit tests"
            exit 1
        }
    fi
    
    # Build stress tests
    if [[ "$RUN_STRESS" == "true" ]]; then
        log_verbose "Building stress tests..."
        make stress-tests || {
            log_error "Failed to build stress tests"
            exit 1
        }
    fi
    
    # Build integration tests
    if [[ "$RUN_INTEGRATION" == "true" ]]; then
        log_verbose "Building integration tests..."
        make test_ancestralclust_integration || {
            log_error "Failed to build integration tests"
            exit 1
        }
    fi
    
    log_success "Test executables built successfully"
}

# Run unit tests
run_unit_tests() {
    if [[ "$RUN_UNIT" != "true" ]]; then
        return 0
    fi
    
    log_info "Running unit tests..."
    
    local unit_results="$RESULTS_DIR/unit_test_results.log"
    local failed_tests=0
    
    for test in "${UNIT_TESTS[@]}"; do
        log_verbose "Running $test..."
        
        if [[ -f "$TEST_DIR/unit/$test" ]]; then
            if "$TEST_DIR/unit/$test" >> "$unit_results" 2>&1; then
                log_success "Unit test: $test"
            else
                log_error "Unit test failed: $test"
                ((failed_tests++))
            fi
        else
            log_warning "Unit test executable not found: $test"
            ((failed_tests++))
        fi
    done
    
    if [[ $failed_tests -eq 0 ]]; then
        log_success "All unit tests passed"
    else
        log_error "$failed_tests unit test(s) failed"
        return 1
    fi
}

# Run integration tests
run_integration_tests() {
    if [[ "$RUN_INTEGRATION" != "true" ]]; then
        return 0
    fi
    
    log_info "Running integration tests..."
    
    local integration_results="$RESULTS_DIR/integration_test_results.log"
    local failed_tests=0
    
    for test in "${INTEGRATION_TESTS[@]}"; do
        log_verbose "Running $test..."
        
        local test_executable="$TEST_DIR/integration/$test"
        if [[ -f "$test_executable" ]]; then
            if "$test_executable" >> "$integration_results" 2>&1; then
                log_success "Integration test: $test"
            else
                log_error "Integration test failed: $test"
                ((failed_tests++))
            fi
        else
            log_warning "Integration test executable not found: $test"
            ((failed_tests++))
        fi
    done
    
    if [[ $failed_tests -eq 0 ]]; then
        log_success "All integration tests passed"
    else
        log_error "$failed_tests integration test(s) failed"
        return 1
    fi
}

# Run stress tests
run_stress_tests() {
    if [[ "$RUN_STRESS" != "true" ]]; then
        return 0
    fi
    
    log_info "Running stress tests..."
    
    local stress_results="$RESULTS_DIR/stress_test_results.log"
    local failed_tests=0
    
    for test in "${STRESS_TESTS[@]}"; do
        log_verbose "Running $test..."
        
        if [[ -f "$TEST_DIR/stress/$test" ]]; then
            if timeout 300 "$TEST_DIR/stress/$test" >> "$stress_results" 2>&1; then
                log_success "Stress test: $test"
            else
                log_error "Stress test failed or timed out: $test"
                ((failed_tests++))
            fi
        else
            log_warning "Stress test executable not found: $test"
            ((failed_tests++))
        fi
    done
    
    if [[ $failed_tests -eq 0 ]]; then
        log_success "All stress tests passed"
    else
        log_error "$failed_tests stress test(s) failed"
        return 1
    fi
}

# Measure performance overhead
measure_performance_overhead() {
    if [[ "$MEASURE_PERFORMANCE" != "true" ]]; then
        return 0
    fi
    
    log_info "Measuring performance overhead..."
    
    local test_fasta="$TEST_DIR/fixtures/small_test.fasta"
    local results_file="$RESULTS_DIR/performance_overhead.log"
    
    if [[ ! -f "$test_fasta" ]]; then
        log_warning "Test FASTA file not found: $test_fasta"
        return 1
    fi
    
    # Run with performance logging
    log_verbose "Running with performance logging enabled..."
    local start_time=$(date +%s.%N)
    if timeout 60 "$BINARY" -i "$test_fasta" -o "$RESULTS_DIR/perf_test.csv" --performance-format csv >/dev/null 2>&1; then
        local end_time=$(date +%s.%N)
        local with_logging_time=$(echo "$end_time - $start_time" | bc -l)
    else
        log_warning "Performance test with logging failed"
        return 1
    fi
    
    # Run without performance logging (if supported)
    log_verbose "Running with performance logging disabled..."
    start_time=$(date +%s.%N)
    if timeout 60 "$BINARY" -i "$test_fasta" --no-performance >/dev/null 2>&1; then
        end_time=$(date +%s.%N)
        local without_logging_time=$(echo "$end_time - $start_time" | bc -l)
    else
        log_verbose "No-performance flag not supported, using fallback comparison"
        without_logging_time="$with_logging_time"
    fi
    
    # Calculate overhead
    if [[ "$without_logging_time" != "0" && "$without_logging_time" != "$with_logging_time" ]]; then
        local overhead=$(echo "scale=4; ($with_logging_time - $without_logging_time) / $without_logging_time" | bc -l)
        local overhead_percent=$(echo "scale=2; $overhead * 100" | bc -l)
        
        echo "Performance Overhead Analysis:" > "$results_file"
        echo "With logging: ${with_logging_time}s" >> "$results_file"
        echo "Without logging: ${without_logging_time}s" >> "$results_file"
        echo "Overhead: ${overhead_percent}%" >> "$results_file"
        
        log_info "Performance overhead: ${overhead_percent}%"
        
        # Check if overhead exceeds threshold
        local threshold_check=$(echo "$overhead > $PERFORMANCE_THRESHOLD" | bc -l)
        if [[ "$threshold_check" -eq 1 ]]; then
            log_warning "Performance overhead ${overhead_percent}% exceeds ${PERFORMANCE_THRESHOLD}% threshold"
            return 1
        else
            log_success "Performance overhead within acceptable limits"
        fi
    else
        log_info "Performance overhead measurement completed (baseline comparison not available)"
    fi
}

# Generate test coverage report
generate_coverage_report() {
    if [[ "$GENERATE_COVERAGE" != "true" ]]; then
        return 0
    fi
    
    log_info "Generating test coverage report..."
    
    # Check if gcov/lcov are available
    if ! command -v gcov &> /dev/null || ! command -v lcov &> /dev/null; then
        log_warning "gcov/lcov not available. Skipping coverage report."
        return 0
    fi
    
    local coverage_dir="$RESULTS_DIR/coverage"
    mkdir -p "$coverage_dir"
    
    # Rebuild with coverage flags
    cd "$PROJECT_ROOT"
    log_verbose "Rebuilding with coverage instrumentation..."
    make clean
    CFLAGS="--coverage -g" make performance || {
        log_warning "Failed to build with coverage instrumentation"
        return 1
    }
    
    # Re-run tests to generate coverage data
    log_verbose "Re-running tests for coverage analysis..."
    run_unit_tests >/dev/null 2>&1 || true
    run_integration_tests >/dev/null 2>&1 || true
    
    # Generate coverage report
    log_verbose "Processing coverage data..."
    lcov --capture --directory . --output-file "$coverage_dir/coverage.info" >/dev/null 2>&1
    lcov --remove "$coverage_dir/coverage.info" '/usr/*' '*/tests/*' '*/WFA2/*' '*/kalign/*' --output-file "$coverage_dir/coverage_filtered.info" >/dev/null 2>&1
    genhtml "$coverage_dir/coverage_filtered.info" --output-directory "$coverage_dir/html" >/dev/null 2>&1
    
    log_success "Coverage report generated: $coverage_dir/html/index.html"
}

# Generate final test report
generate_test_report() {
    log_info "Generating test summary report..."
    
    local report_file="$RESULTS_DIR/test_summary_report.txt"
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    
    cat > "$report_file" << EOF
AncestralClust Performance Logging System Test Report
Generated: $timestamp

=== Test Environment ===
Project Root: $PROJECT_ROOT
Test Results: $RESULTS_DIR
Binary: $BINARY

=== Test Configuration ===
Unit Tests: $RUN_UNIT
Integration Tests: $RUN_INTEGRATION  
Stress Tests: $RUN_STRESS
Coverage Analysis: $GENERATE_COVERAGE
Performance Measurement: $MEASURE_PERFORMANCE

=== Test Results Summary ===
EOF

    # Add unit test results
    if [[ "$RUN_UNIT" == "true" && -f "$RESULTS_DIR/unit_test_results.log" ]]; then
        echo "Unit Tests:" >> "$report_file"
        local unit_pass_count=$(grep -c "PASS" "$RESULTS_DIR/unit_test_results.log" 2>/dev/null || echo "0")
        local unit_fail_count=$(grep -c "FAIL" "$RESULTS_DIR/unit_test_results.log" 2>/dev/null || echo "0")
        echo "  Passed: $unit_pass_count" >> "$report_file"
        echo "  Failed: $unit_fail_count" >> "$report_file"
    fi
    
    # Add integration test results
    if [[ "$RUN_INTEGRATION" == "true" && -f "$RESULTS_DIR/integration_test_results.log" ]]; then
        echo "Integration Tests:" >> "$report_file"
        local int_pass_count=$(grep -c "PASS" "$RESULTS_DIR/integration_test_results.log" 2>/dev/null || echo "0")
        local int_fail_count=$(grep -c "FAIL" "$RESULTS_DIR/integration_test_results.log" 2>/dev/null || echo "0")
        echo "  Passed: $int_pass_count" >> "$report_file"
        echo "  Failed: $int_fail_count" >> "$report_file"
    fi
    
    # Add stress test results
    if [[ "$RUN_STRESS" == "true" && -f "$RESULTS_DIR/stress_test_results.log" ]]; then
        echo "Stress Tests:" >> "$report_file"
        local stress_pass_count=$(grep -c "PASS" "$RESULTS_DIR/stress_test_results.log" 2>/dev/null || echo "0")
        local stress_fail_count=$(grep -c "FAIL" "$RESULTS_DIR/stress_test_results.log" 2>/dev/null || echo "0")
        echo "  Passed: $stress_pass_count" >> "$report_file"
        echo "  Failed: $stress_fail_count" >> "$report_file"
    fi
    
    # Add performance overhead results
    if [[ -f "$RESULTS_DIR/performance_overhead.log" ]]; then
        echo "" >> "$report_file"
        echo "Performance Overhead:" >> "$report_file"
        cat "$RESULTS_DIR/performance_overhead.log" >> "$report_file"
    fi
    
    # Add available CSV outputs
    local csv_count=$(ls "$RESULTS_DIR"/*.csv 2>/dev/null | wc -l)
    echo "" >> "$report_file"
    echo "Generated CSV Files: $csv_count" >> "$report_file"
    
    # Coverage information
    if [[ "$GENERATE_COVERAGE" == "true" && -d "$RESULTS_DIR/coverage" ]]; then
        echo "Coverage Report: $RESULTS_DIR/coverage/html/index.html" >> "$report_file"
    fi
    
    echo "" >> "$report_file"
    echo "=== Files Generated ===" >> "$report_file"
    ls -la "$RESULTS_DIR"/ >> "$report_file"
    
    log_success "Test report generated: $report_file"
}

# Main execution
main() {
    echo "========================================"
    echo "AncestralClust Comprehensive Test Suite"
    echo "========================================"
    echo ""
    
    parse_args "$@"
    
    setup_environment
    build_tests
    
    local total_failures=0
    
    # Run test suites
    run_unit_tests || ((total_failures++))
    run_integration_tests || ((total_failures++))
    run_stress_tests || ((total_failures++))
    
    # Performance and coverage analysis
    measure_performance_overhead || ((total_failures++))
    generate_coverage_report || true  # Non-critical
    
    # Generate final report
    generate_test_report
    
    echo ""
    echo "========================================"
    if [[ $total_failures -eq 0 ]]; then
        log_success "All tests completed successfully!"
        echo "Results available in: $RESULTS_DIR"
        exit 0
    else
        log_error "$total_failures test suite(s) failed"
        echo "Check logs in: $RESULTS_DIR"
        exit 1
    fi
}

# Execute main function with all arguments
main "$@"