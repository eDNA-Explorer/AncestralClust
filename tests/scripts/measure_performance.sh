#!/bin/bash

# measure_performance.sh
# Performance Overhead Measurement Script for AncestralClust
#
# This script performs detailed performance analysis of the performance logging system:
# - Measures execution time overhead of performance logging
# - Analyzes memory usage impact
# - Tests performance across different workload sizes
# - Generates performance benchmark reports
# - Validates that overhead remains below acceptable thresholds
#
# Usage: ./tests/scripts/measure_performance.sh [options]
# Options:
#   --quick         Quick performance test (small dataset only)
#   --comprehensive Full performance analysis (multiple datasets)
#   --baseline      Establish performance baseline (no logging)
#   --overhead      Measure logging overhead only
#   --report        Generate performance report
#   --threshold N   Set overhead threshold percentage (default: 5)
#   --iterations N  Number of test iterations (default: 3)
#   --verbose       Enable verbose output

set -e

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TEST_DIR="$PROJECT_ROOT/tests"
RESULTS_DIR="$TEST_DIR/results/performance"
BINARY="$PROJECT_ROOT/ancestralclust"

# Test datasets
SMALL_DATASET="$TEST_DIR/fixtures/small_test.fasta"
MEDIUM_DATASET="$TEST_DIR/fixtures/medium_test.fasta"
LARGE_DATASET="$TEST_DIR/fixtures/large_test.fasta"

# Performance configuration
OVERHEAD_THRESHOLD=5.0  # 5% maximum overhead
DEFAULT_ITERATIONS=3
TIMEOUT_SECONDS=300     # 5 minutes per test

# Test modes
RUN_QUICK=false
RUN_COMPREHENSIVE=false
RUN_BASELINE=false
RUN_OVERHEAD=false
GENERATE_REPORT=false
VERBOSE=false
ITERATIONS=$DEFAULT_ITERATIONS

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Usage information
show_help() {
    echo "AncestralClust Performance Measurement Tool"
    echo ""
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  --quick         Quick performance test (small dataset only)"
    echo "  --comprehensive Full performance analysis (multiple datasets)"
    echo "  --baseline      Establish performance baseline (no logging)"
    echo "  --overhead      Measure logging overhead only"
    echo "  --report        Generate performance report"
    echo "  --threshold N   Set overhead threshold percentage (default: 5)"
    echo "  --iterations N  Number of test iterations (default: 3)"
    echo "  --verbose       Enable verbose output"
    echo "  --help          Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 --quick                    # Quick performance test"
    echo "  $0 --comprehensive --report   # Full analysis with report"
    echo "  $0 --overhead --threshold 3   # Test 3% overhead threshold"
    echo "  $0 --baseline                 # Establish baseline performance"
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --quick)
                RUN_QUICK=true
                shift
                ;;
            --comprehensive)
                RUN_COMPREHENSIVE=true
                shift
                ;;
            --baseline)
                RUN_BASELINE=true
                shift
                ;;
            --overhead)
                RUN_OVERHEAD=true
                shift
                ;;
            --report)
                GENERATE_REPORT=true
                shift
                ;;
            --threshold)
                OVERHEAD_THRESHOLD="$2"
                shift 2
                ;;
            --iterations)
                ITERATIONS="$2"
                shift 2
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
    
    # Set default mode if none specified
    if [[ "$RUN_QUICK" == "false" && "$RUN_COMPREHENSIVE" == "false" && 
          "$RUN_BASELINE" == "false" && "$RUN_OVERHEAD" == "false" ]]; then
        RUN_QUICK=true
    fi
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

# Setup environment for performance testing
setup_environment() {
    log_info "Setting up performance testing environment..."
    
    # Create results directory
    mkdir -p "$RESULTS_DIR"
    
    # Clean previous results
    rm -f "$RESULTS_DIR"/*.log "$RESULTS_DIR"/*.csv "$RESULTS_DIR"/*.json
    
    # Verify binary exists
    if [[ ! -f "$BINARY" ]]; then
        log_info "Building AncestralClust binary..."
        cd "$PROJECT_ROOT"
        make performance || {
            log_error "Failed to build AncestralClust binary"
            exit 1
        }
    fi
    
    # Create large test dataset if it doesn't exist
    if [[ ! -f "$LARGE_DATASET" ]]; then
        log_info "Creating large test dataset..."
        create_large_test_dataset
    fi
    
    # Verify test datasets exist
    for dataset in "$SMALL_DATASET" "$MEDIUM_DATASET"; do
        if [[ ! -f "$dataset" ]]; then
            log_error "Test dataset not found: $dataset"
            exit 1
        fi
    done
    
    log_success "Environment setup complete"
}

# Create a larger test dataset for comprehensive testing
create_large_test_dataset() {
    local output="$LARGE_DATASET"
    local sequence_count=50
    local sequence_length=200
    
    log_verbose "Creating large test dataset with $sequence_count sequences..."
    
    cat > "$output" << 'EOF'
>large_seq_01
ATGCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGAT
>large_seq_02
ATGCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGAC
>large_seq_03
ATGCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGAG
>large_seq_04
ATGCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGAA
EOF

    # Generate more sequences programmatically
    for ((i=5; i<=sequence_count; i++)); do
        printf ">large_seq_%02d\n" "$i" >> "$output"
        
        # Generate sequence with slight variations
        local base_seq="ATGCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCGATCG"
        
        # Add some variation based on sequence number
        case $((i % 4)) in
            0) echo "${base_seq}AT" >> "$output" ;;
            1) echo "${base_seq}GC" >> "$output" ;;
            2) echo "${base_seq}TT" >> "$output" ;;
            3) echo "${base_seq}AA" >> "$output" ;;
        esac
    done
    
    log_verbose "Large test dataset created: $output"
}

# Execute a single performance test
run_performance_test() {
    local test_name="$1"
    local input_file="$2"
    local enable_logging="$3"
    local output_file="$4"
    local memory_file="$5"
    
    log_verbose "Running performance test: $test_name"
    
    # Prepare command
    local cmd="$BINARY -i $input_file"
    if [[ "$enable_logging" == "true" ]]; then
        cmd="$cmd -o $output_file --performance-format csv --performance-granularity medium"
    else
        cmd="$cmd --no-performance-logging 2>/dev/null"
    fi
    
    # Execute with time and memory measurement
    local start_time=$(date +%s.%N)
    
    # Use GNU time if available for detailed statistics
    if command -v /usr/bin/time >/dev/null 2>&1; then
        /usr/bin/time -v timeout $TIMEOUT_SECONDS $cmd > /dev/null 2> "$memory_file"
        local exit_code=$?
    else
        # Fallback to built-in time
        { timeout $TIMEOUT_SECONDS $cmd > /dev/null; } 2> "$memory_file"
        local exit_code=$?
    fi
    
    local end_time=$(date +%s.%N)
    local execution_time=$(echo "$end_time - $start_time" | bc -l)
    
    if [[ $exit_code -eq 0 ]]; then
        echo "$execution_time"
        return 0
    else
        echo "ERROR"
        return 1
    fi
}

# Get memory usage from time output
extract_memory_usage() {
    local memory_file="$1"
    
    if [[ -f "$memory_file" ]]; then
        # Try to extract maximum resident set size (GNU time format)
        local max_rss=$(grep "Maximum resident set size" "$memory_file" 2>/dev/null | awk '{print $NF}')
        if [[ -n "$max_rss" ]]; then
            echo "$max_rss"
            return 0
        fi
        
        # Fallback: estimate from other available information
        echo "0"
    else
        echo "0"
    fi
}

# Run baseline performance tests (without logging)
run_baseline_tests() {
    if [[ "$RUN_BASELINE" != "true" ]]; then
        return 0
    fi
    
    log_info "Running baseline performance tests (no logging)..."
    
    local baseline_results="$RESULTS_DIR/baseline_results.log"
    echo "# Baseline Performance Results" > "$baseline_results"
    echo "# Dataset,Iteration,ExecutionTime(s),MaxRSS(KB)" >> "$baseline_results"
    
    local datasets=("$SMALL_DATASET:small" "$MEDIUM_DATASET:medium")
    if [[ "$RUN_COMPREHENSIVE" == "true" && -f "$LARGE_DATASET" ]]; then
        datasets+=("$LARGE_DATASET:large")
    fi
    
    for dataset_entry in "${datasets[@]}"; do
        IFS=':' read -r dataset_path dataset_name <<< "$dataset_entry"
        
        log_verbose "Testing baseline performance with $dataset_name dataset..."
        
        local total_time=0
        local max_memory=0
        local successful_runs=0
        
        for ((i=1; i<=ITERATIONS; i++)); do
            local memory_file="$RESULTS_DIR/baseline_${dataset_name}_${i}.mem"
            local exec_time=$(run_performance_test "baseline_${dataset_name}_$i" "$dataset_path" "false" "" "$memory_file")
            
            if [[ "$exec_time" != "ERROR" ]]; then
                local memory_usage=$(extract_memory_usage "$memory_file")
                
                echo "$dataset_name,$i,$exec_time,$memory_usage" >> "$baseline_results"
                total_time=$(echo "$total_time + $exec_time" | bc -l)
                
                if [[ "$memory_usage" -gt "$max_memory" ]]; then
                    max_memory="$memory_usage"
                fi
                
                ((successful_runs++))
                log_verbose "  Iteration $i: ${exec_time}s, ${memory_usage}KB"
            else
                log_warning "  Iteration $i failed"
            fi
            
            rm -f "$memory_file"
        done
        
        if [[ $successful_runs -gt 0 ]]; then
            local avg_time=$(echo "scale=3; $total_time / $successful_runs" | bc -l)
            echo "$dataset_name,average,$avg_time,$max_memory" >> "$baseline_results"
            log_success "Baseline $dataset_name: ${avg_time}s average (${successful_runs}/$ITERATIONS runs)"
        else
            log_error "All baseline tests failed for $dataset_name"
        fi
    done
    
    log_success "Baseline tests completed"
}

# Run performance tests with logging enabled
run_logging_tests() {
    if [[ "$RUN_OVERHEAD" != "true" && "$RUN_QUICK" != "true" && "$RUN_COMPREHENSIVE" != "true" ]]; then
        return 0
    fi
    
    log_info "Running performance tests with logging enabled..."
    
    local logging_results="$RESULTS_DIR/logging_results.log"
    echo "# Performance Logging Results" > "$logging_results"
    echo "# Dataset,Iteration,ExecutionTime(s),MaxRSS(KB),LogFileSize(bytes)" >> "$logging_results"
    
    local datasets=("$SMALL_DATASET:small")
    if [[ "$RUN_COMPREHENSIVE" == "true" ]]; then
        datasets+=("$MEDIUM_DATASET:medium")
        if [[ -f "$LARGE_DATASET" ]]; then
            datasets+=("$LARGE_DATASET:large")
        fi
    fi
    
    for dataset_entry in "${datasets[@]}"; do
        IFS=':' read -r dataset_path dataset_name <<< "$dataset_entry"
        
        log_verbose "Testing logging performance with $dataset_name dataset..."
        
        local total_time=0
        local max_memory=0
        local successful_runs=0
        
        for ((i=1; i<=ITERATIONS; i++)); do
            local output_file="$RESULTS_DIR/logging_${dataset_name}_${i}.csv"
            local memory_file="$RESULTS_DIR/logging_${dataset_name}_${i}.mem"
            
            local exec_time=$(run_performance_test "logging_${dataset_name}_$i" "$dataset_path" "true" "$output_file" "$memory_file")
            
            if [[ "$exec_time" != "ERROR" ]]; then
                local memory_usage=$(extract_memory_usage "$memory_file")
                local log_size=0
                
                if [[ -f "$output_file" ]]; then
                    log_size=$(wc -c < "$output_file")
                fi
                
                echo "$dataset_name,$i,$exec_time,$memory_usage,$log_size" >> "$logging_results"
                total_time=$(echo "$total_time + $exec_time" | bc -l)
                
                if [[ "$memory_usage" -gt "$max_memory" ]]; then
                    max_memory="$memory_usage"
                fi
                
                ((successful_runs++))
                log_verbose "  Iteration $i: ${exec_time}s, ${memory_usage}KB, ${log_size}B log"
            else
                log_warning "  Iteration $i failed"
            fi
            
            rm -f "$memory_file"
        done
        
        if [[ $successful_runs -gt 0 ]]; then
            local avg_time=$(echo "scale=3; $total_time / $successful_runs" | bc -l)
            echo "$dataset_name,average,$avg_time,$max_memory,0" >> "$logging_results"
            log_success "Logging $dataset_name: ${avg_time}s average (${successful_runs}/$ITERATIONS runs)"
        else
            log_error "All logging tests failed for $dataset_name"
        fi
    done
    
    log_success "Logging tests completed"
}

# Calculate performance overhead
calculate_overhead() {
    local baseline_file="$RESULTS_DIR/baseline_results.log"
    local logging_file="$RESULTS_DIR/logging_results.log"
    local overhead_file="$RESULTS_DIR/overhead_analysis.log"
    
    if [[ ! -f "$baseline_file" || ! -f "$logging_file" ]]; then
        log_warning "Cannot calculate overhead: baseline or logging results missing"
        return 1
    fi
    
    log_info "Calculating performance overhead..."
    
    echo "# Performance Overhead Analysis" > "$overhead_file"
    echo "# Dataset,BaselineTime(s),LoggingTime(s),Overhead(%),MemoryOverhead(KB)" >> "$overhead_file"
    
    local overall_overhead=0
    local dataset_count=0
    local overhead_threshold_exceeded=false
    
    # Extract baseline averages
    while IFS=',' read -r dataset iteration time memory; do
        if [[ "$iteration" == "average" ]]; then
            local baseline_time="$time"
            local baseline_memory="$memory"
            
            # Find corresponding logging average
            local logging_line=$(grep "^$dataset,average," "$logging_file")
            if [[ -n "$logging_line" ]]; then
                IFS=',' read -r _ _ logging_time logging_memory _ <<< "$logging_line"
                
                # Calculate overhead
                local time_overhead_abs=$(echo "scale=6; $logging_time - $baseline_time" | bc -l)
                local time_overhead_pct=$(echo "scale=2; ($time_overhead_abs / $baseline_time) * 100" | bc -l)
                local memory_overhead=$(echo "$logging_memory - $baseline_memory" | bc -l)
                
                echo "$dataset,$baseline_time,$logging_time,$time_overhead_pct,$memory_overhead" >> "$overhead_file"
                
                overall_overhead=$(echo "scale=6; $overall_overhead + $time_overhead_pct" | bc -l)
                ((dataset_count++))
                
                log_verbose "  $dataset: ${time_overhead_pct}% time overhead, ${memory_overhead}KB memory overhead"
                
                # Check threshold
                local threshold_check=$(echo "$time_overhead_pct > $OVERHEAD_THRESHOLD" | bc -l)
                if [[ "$threshold_check" -eq 1 ]]; then
                    overhead_threshold_exceeded=true
                    log_warning "  $dataset exceeds ${OVERHEAD_THRESHOLD}% threshold"
                fi
            fi
        fi
    done < "$baseline_file"
    
    if [[ $dataset_count -gt 0 ]]; then
        local avg_overhead=$(echo "scale=2; $overall_overhead / $dataset_count" | bc -l)
        echo "overall,average,average,$avg_overhead,average" >> "$overhead_file"
        
        log_info "Average performance overhead: ${avg_overhead}%"
        
        if [[ "$overhead_threshold_exceeded" == "true" ]]; then
            log_error "Performance overhead exceeds ${OVERHEAD_THRESHOLD}% threshold for some datasets"
            return 1
        else
            log_success "Performance overhead within acceptable limits (${avg_overhead}% < ${OVERHEAD_THRESHOLD}%)"
        fi
    else
        log_error "Could not calculate overhead (no matching datasets)"
        return 1
    fi
}

# Generate comprehensive performance report
generate_performance_report() {
    if [[ "$GENERATE_REPORT" != "true" ]]; then
        return 0
    fi
    
    log_info "Generating performance report..."
    
    local report_file="$RESULTS_DIR/performance_report.html"
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    
    cat > "$report_file" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>AncestralClust Performance Analysis Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        h1, h2 { color: #333; }
        table { border-collapse: collapse; width: 100%; margin: 20px 0; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #f2f2f2; }
        .summary { background-color: #f9f9f9; padding: 20px; margin: 20px 0; }
        .pass { color: green; font-weight: bold; }
        .fail { color: red; font-weight: bold; }
        .warning { color: orange; font-weight: bold; }
    </style>
</head>
<body>
    <h1>AncestralClust Performance Analysis Report</h1>
    <p><strong>Generated:</strong> $timestamp</p>
    <p><strong>Test Configuration:</strong> $ITERATIONS iterations, ${OVERHEAD_THRESHOLD}% threshold</p>
    
    <div class="summary">
        <h2>Executive Summary</h2>
EOF

    # Add summary based on available results
    if [[ -f "$RESULTS_DIR/overhead_analysis.log" ]]; then
        local avg_overhead=$(grep "^overall,average," "$RESULTS_DIR/overhead_analysis.log" | cut -d',' -f4)
        if [[ -n "$avg_overhead" ]]; then
            local overhead_check=$(echo "$avg_overhead > $OVERHEAD_THRESHOLD" | bc -l)
            if [[ "$overhead_check" -eq 1 ]]; then
                echo '        <p class="fail">Performance overhead exceeds threshold: '"$avg_overhead"'% > '"$OVERHEAD_THRESHOLD"'%</p>' >> "$report_file"
            else
                echo '        <p class="pass">Performance overhead within acceptable limits: '"$avg_overhead"'% ≤ '"$OVERHEAD_THRESHOLD"'%</p>' >> "$report_file"
            fi
        fi
    fi
    
    cat >> "$report_file" << EOF
    </div>
    
    <h2>Baseline Performance Results</h2>
EOF

    # Add baseline results table
    if [[ -f "$RESULTS_DIR/baseline_results.log" ]]; then
        echo '    <table>' >> "$report_file"
        echo '        <tr><th>Dataset</th><th>Avg Time (s)</th><th>Max Memory (KB)</th></tr>' >> "$report_file"
        
        while IFS=',' read -r dataset iteration time memory; do
            if [[ "$iteration" == "average" ]]; then
                echo "        <tr><td>$dataset</td><td>$time</td><td>$memory</td></tr>" >> "$report_file"
            fi
        done < "$RESULTS_DIR/baseline_results.log"
        
        echo '    </table>' >> "$report_file"
    fi
    
    cat >> "$report_file" << EOF
    
    <h2>Performance Logging Results</h2>
EOF

    # Add logging results table
    if [[ -f "$RESULTS_DIR/logging_results.log" ]]; then
        echo '    <table>' >> "$report_file"
        echo '        <tr><th>Dataset</th><th>Avg Time (s)</th><th>Max Memory (KB)</th></tr>' >> "$report_file"
        
        while IFS=',' read -r dataset iteration time memory log_size; do
            if [[ "$iteration" == "average" ]]; then
                echo "        <tr><td>$dataset</td><td>$time</td><td>$memory</td></tr>" >> "$report_file"
            fi
        done < "$RESULTS_DIR/logging_results.log"
        
        echo '    </table>' >> "$report_file"
    fi
    
    cat >> "$report_file" << EOF
    
    <h2>Overhead Analysis</h2>
EOF

    # Add overhead analysis table
    if [[ -f "$RESULTS_DIR/overhead_analysis.log" ]]; then
        echo '    <table>' >> "$report_file"
        echo '        <tr><th>Dataset</th><th>Baseline (s)</th><th>With Logging (s)</th><th>Overhead (%)</th><th>Status</th></tr>' >> "$report_file"
        
        while IFS=',' read -r dataset baseline logging overhead memory_overhead; do
            if [[ "$dataset" != "#"* ]]; then
                local status_class="pass"
                local status_text="OK"
                
                if [[ "$dataset" != "overall" ]]; then
                    local threshold_check=$(echo "$overhead > $OVERHEAD_THRESHOLD" | bc -l 2>/dev/null || echo "0")
                    if [[ "$threshold_check" -eq 1 ]]; then
                        status_class="fail"
                        status_text="EXCEEDS THRESHOLD"
                    fi
                fi
                
                echo "        <tr><td>$dataset</td><td>$baseline</td><td>$logging</td><td>$overhead</td><td class=\"$status_class\">$status_text</td></tr>" >> "$report_file"
            fi
        done < "$RESULTS_DIR/overhead_analysis.log"
        
        echo '    </table>' >> "$report_file"
    fi
    
    cat >> "$report_file" << EOF
    
    <h2>Test Files Generated</h2>
    <ul>
EOF

    # List generated files
    for file in "$RESULTS_DIR"/*.{log,csv}; do
        if [[ -f "$file" ]]; then
            local filename=$(basename "$file")
            local filesize=$(wc -c < "$file")
            echo "        <li>$filename ($filesize bytes)</li>" >> "$report_file"
        fi
    done
    
    cat >> "$report_file" << EOF
    </ul>
    
    <h2>Recommendations</h2>
    <ul>
        <li>Performance logging overhead should remain below ${OVERHEAD_THRESHOLD}% for production use</li>
        <li>Monitor memory usage during long-running operations</li>
        <li>Consider adjusting granularity settings for performance-critical applications</li>
        <li>Regularly benchmark performance to detect regressions</li>
    </ul>
    
    <footer>
        <p><em>Report generated by AncestralClust Performance Measurement Tool</em></p>
    </footer>
</body>
</html>
EOF

    log_success "Performance report generated: $report_file"
}

# Main execution
main() {
    echo "========================================"
    echo "AncestralClust Performance Measurement"
    echo "========================================"
    echo ""
    
    parse_args "$@"
    
    log_info "Performance measurement configuration:"
    log_info "  Overhead threshold: ${OVERHEAD_THRESHOLD}%"
    log_info "  Test iterations: $ITERATIONS"
    log_info "  Test modes: Quick=$RUN_QUICK, Comprehensive=$RUN_COMPREHENSIVE"
    log_info "  Analysis: Baseline=$RUN_BASELINE, Overhead=$RUN_OVERHEAD"
    log_info "  Report generation: $GENERATE_REPORT"
    echo ""
    
    setup_environment
    
    local exit_code=0
    
    # Run baseline tests
    run_baseline_tests || exit_code=1
    
    # Run logging tests
    run_logging_tests || exit_code=1
    
    # Calculate overhead if both baseline and logging results exist
    if [[ -f "$RESULTS_DIR/baseline_results.log" && -f "$RESULTS_DIR/logging_results.log" ]]; then
        calculate_overhead || exit_code=1
    fi
    
    # Generate report
    generate_performance_report
    
    echo ""
    echo "========================================"
    if [[ $exit_code -eq 0 ]]; then
        log_success "Performance measurement completed successfully!"
    else
        log_error "Performance measurement completed with issues"
    fi
    echo "Results available in: $RESULTS_DIR"
    echo "========================================"
    
    exit $exit_code
}

# Execute main function with all arguments
main "$@"