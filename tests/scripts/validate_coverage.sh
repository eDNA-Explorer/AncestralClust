#!/bin/bash

# validate_coverage.sh
# Test Coverage Analysis Script for AncestralClust Performance Logging System
#
# This script provides comprehensive test coverage analysis including:
# - Line coverage analysis using gcov/lcov
# - Branch coverage measurement
# - Function coverage validation
# - Performance system specific coverage
# - HTML coverage report generation
# - Coverage threshold validation
# - Integration with CI/CD pipelines
#
# Usage: ./tests/scripts/validate_coverage.sh [options]
# Options:
#   --generate      Generate coverage data by running tests
#   --report        Generate HTML coverage report
#   --threshold N   Set minimum coverage threshold (default: 80%)
#   --performance   Focus on performance system coverage
#   --unit          Include unit test coverage
#   --integration   Include integration test coverage
#   --exclude-deps  Exclude third-party dependencies from coverage
#   --verbose       Enable verbose output

set -e

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TEST_DIR="$PROJECT_ROOT/tests"
COVERAGE_DIR="$TEST_DIR/results/coverage"
BINARY="$PROJECT_ROOT/ancestralclust"

# Coverage configuration
COVERAGE_THRESHOLD=80.0  # 80% minimum coverage
PERFORMANCE_THRESHOLD=90.0  # 90% minimum for performance system
DEFAULT_GCOV_FLAGS="--branch-counts --function-summaries"
DEFAULT_LCOV_FLAGS="--rc lcov_branch_coverage=1"

# Test modes
GENERATE_COVERAGE=false
GENERATE_REPORT=false
FOCUS_PERFORMANCE=false
INCLUDE_UNIT=true
INCLUDE_INTEGRATION=true
EXCLUDE_DEPS=true
VERBOSE=false

# Source files to analyze
PERFORMANCE_SOURCES=("performance.c" "performance.h")
CORE_SOURCES=("ancestralclust.c" "options.c" "math.c" "opt.c" "alignment.c" "alignment_scoring.c" "needleman_wunsch.c" "hashmap.c")
EXCLUDED_PATTERNS=("*/tests/*" "*/WFA2/*" "*/kalign/*" "/usr/*" "*/Unity/*")

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Usage information
show_help() {
    echo "AncestralClust Test Coverage Analysis Tool"
    echo ""
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  --generate      Generate coverage data by running tests"
    echo "  --report        Generate HTML coverage report"
    echo "  --threshold N   Set minimum coverage threshold (default: 80%)"
    echo "  --performance   Focus on performance system coverage"
    echo "  --unit          Include unit test coverage (default: true)"
    echo "  --integration   Include integration test coverage (default: true)"
    echo "  --exclude-deps  Exclude third-party dependencies (default: true)"
    echo "  --verbose       Enable verbose output"
    echo "  --help          Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 --generate --report           # Full coverage analysis"
    echo "  $0 --performance --threshold 90  # Focus on performance system"
    echo "  $0 --report                      # Generate report from existing data"
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --generate)
                GENERATE_COVERAGE=true
                shift
                ;;
            --report)
                GENERATE_REPORT=true
                shift
                ;;
            --threshold)
                COVERAGE_THRESHOLD="$2"
                shift 2
                ;;
            --performance)
                FOCUS_PERFORMANCE=true
                PERFORMANCE_THRESHOLD="$COVERAGE_THRESHOLD"
                shift
                ;;
            --unit)
                INCLUDE_UNIT=true
                shift
                ;;
            --integration)
                INCLUDE_INTEGRATION=true
                shift
                ;;
            --exclude-deps)
                EXCLUDE_DEPS=true
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
    
    # Set default mode if none specified
    if [[ "$GENERATE_COVERAGE" == "false" && "$GENERATE_REPORT" == "false" ]]; then
        GENERATE_COVERAGE=true
        GENERATE_REPORT=true
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

# Check required tools
check_requirements() {
    log_info "Checking coverage analysis requirements..."
    
    local missing_tools=()
    
    # Check for gcov
    if ! command -v gcov &> /dev/null; then
        missing_tools+=("gcov")
    fi
    
    # Check for lcov
    if ! command -v lcov &> /dev/null; then
        missing_tools+=("lcov")
    fi
    
    # Check for genhtml
    if ! command -v genhtml &> /dev/null; then
        missing_tools+=("genhtml")
    fi
    
    if [[ ${#missing_tools[@]} -gt 0 ]]; then
        log_error "Missing required tools: ${missing_tools[*]}"
        log_info "Install with: sudo apt-get install lcov (Ubuntu) or brew install lcov (macOS)"
        exit 1
    fi
    
    log_success "All required tools available"
}

# Setup coverage environment
setup_coverage_environment() {
    log_info "Setting up coverage analysis environment..."
    
    # Create coverage directory
    mkdir -p "$COVERAGE_DIR"
    
    # Clean previous coverage data
    rm -rf "$COVERAGE_DIR"/*.info "$COVERAGE_DIR"/*.gcov "$COVERAGE_DIR"/html
    
    # Clean any existing gcov files in source directory
    find "$PROJECT_ROOT" -name "*.gcda" -delete
    find "$PROJECT_ROOT" -name "*.gcno" -delete
    
    log_success "Coverage environment prepared"
}

# Build project with coverage instrumentation
build_with_coverage() {
    log_info "Building project with coverage instrumentation..."
    
    cd "$PROJECT_ROOT"
    
    # Clean previous build
    make clean >/dev/null 2>&1
    
    # Build with coverage flags
    local coverage_flags="--coverage -O0 -g"
    log_verbose "Building with flags: $coverage_flags"
    
    if CFLAGS="$coverage_flags" make performance; then
        log_success "Project built with coverage instrumentation"
    else
        log_error "Failed to build project with coverage instrumentation"
        exit 1
    fi
    
    # Build test executables with coverage
    log_verbose "Building test executables with coverage..."
    if CFLAGS="$coverage_flags" make tests; then
        log_verbose "Test executables built with coverage"
    else
        log_warning "Some test executables failed to build with coverage"
    fi
}

# Run tests to generate coverage data
run_tests_for_coverage() {
    if [[ "$GENERATE_COVERAGE" != "true" ]]; then
        return 0
    fi
    
    log_info "Running tests to generate coverage data..."
    
    cd "$PROJECT_ROOT"
    
    # Run unit tests
    if [[ "$INCLUDE_UNIT" == "true" ]]; then
        log_verbose "Running unit tests for coverage..."
        
        local unit_tests=("test_performance" "test_thread_safety")
        for test in "${unit_tests[@]}"; do
            local test_path="$TEST_DIR/unit/$test"
            if [[ -f "$test_path" ]]; then
                log_verbose "  Running $test..."
                "$test_path" >/dev/null 2>&1 || log_warning "  $test failed or not found"
            fi
        done
    fi
    
    # Run integration tests
    if [[ "$INCLUDE_INTEGRATION" == "true" ]]; then
        log_verbose "Running integration tests for coverage..."
        
        local integration_test="$TEST_DIR/integration/test_ancestralclust_integration"
        if [[ -f "$integration_test" ]]; then
            log_verbose "  Running integration tests..."
            "$integration_test" >/dev/null 2>&1 || log_warning "  Integration tests failed or not found"
        fi
        
        # Run a simple integration test directly if test executable doesn't exist
        if [[ ! -f "$integration_test" ]]; then
            log_verbose "  Running simple integration test for coverage..."
            if [[ -f "$TEST_DIR/fixtures/small_test.fasta" ]]; then
                timeout 30 "$BINARY" -i "$TEST_DIR/fixtures/small_test.fasta" -o "$COVERAGE_DIR/coverage_test.csv" --performance-format csv >/dev/null 2>&1 || true
            fi
        fi
    fi
    
    # Run stress tests (optional)
    local stress_test="$TEST_DIR/stress/test_performance_stress"
    if [[ -f "$stress_test" ]]; then
        log_verbose "Running stress tests for coverage..."
        timeout 60 "$stress_test" >/dev/null 2>&1 || log_verbose "  Stress tests failed or timed out"
    fi
    
    log_success "Test execution for coverage completed"
}

# Generate raw coverage data
generate_coverage_data() {
    log_info "Generating coverage data..."
    
    cd "$PROJECT_ROOT"
    
    # Create base coverage info file
    local base_coverage="$COVERAGE_DIR/base_coverage.info"
    log_verbose "Creating base coverage file..."
    
    if lcov $DEFAULT_LCOV_FLAGS --capture --initial --directory . --output-file "$base_coverage" >/dev/null 2>&1; then
        log_verbose "Base coverage data captured"
    else
        log_warning "Failed to capture base coverage data"
    fi
    
    # Create test coverage info file
    local test_coverage="$COVERAGE_DIR/test_coverage.info"
    log_verbose "Creating test coverage file..."
    
    if lcov $DEFAULT_LCOV_FLAGS --capture --directory . --output-file "$test_coverage" >/dev/null 2>&1; then
        log_verbose "Test coverage data captured"
    else
        log_error "Failed to capture test coverage data"
        return 1
    fi
    
    # Combine base and test coverage
    local combined_coverage="$COVERAGE_DIR/combined_coverage.info"
    if [[ -f "$base_coverage" ]]; then
        log_verbose "Combining base and test coverage..."
        lcov $DEFAULT_LCOV_FLAGS --add-tracefile "$base_coverage" --add-tracefile "$test_coverage" --output-file "$combined_coverage" >/dev/null 2>&1
    else
        cp "$test_coverage" "$combined_coverage"
    fi
    
    log_success "Raw coverage data generated"
}

# Filter coverage data
filter_coverage_data() {
    log_info "Filtering coverage data..."
    
    local combined_coverage="$COVERAGE_DIR/combined_coverage.info"
    local filtered_coverage="$COVERAGE_DIR/filtered_coverage.info"
    
    if [[ ! -f "$combined_coverage" ]]; then
        log_error "Combined coverage file not found"
        return 1
    fi
    
    # Build exclude patterns
    local exclude_patterns=()
    if [[ "$EXCLUDE_DEPS" == "true" ]]; then
        for pattern in "${EXCLUDED_PATTERNS[@]}"; do
            exclude_patterns+=("$pattern")
        done
    fi
    
    # Apply filters
    if [[ ${#exclude_patterns[@]} -gt 0 ]]; then
        log_verbose "Excluding patterns: ${exclude_patterns[*]}"
        lcov $DEFAULT_LCOV_FLAGS --remove "$combined_coverage" "${exclude_patterns[@]}" --output-file "$filtered_coverage" >/dev/null 2>&1
    else
        cp "$combined_coverage" "$filtered_coverage"
    fi
    
    # Focus on performance system if requested
    if [[ "$FOCUS_PERFORMANCE" == "true" ]]; then
        local performance_coverage="$COVERAGE_DIR/performance_coverage.info"
        log_verbose "Filtering for performance system files only..."
        
        local performance_patterns=()
        for source in "${PERFORMANCE_SOURCES[@]}"; do
            performance_patterns+=("*/$source")
        done
        
        lcov $DEFAULT_LCOV_FLAGS --extract "$filtered_coverage" "${performance_patterns[@]}" --output-file "$performance_coverage" >/dev/null 2>&1
        filtered_coverage="$performance_coverage"
    fi
    
    log_success "Coverage data filtered"
    echo "$filtered_coverage"
}

# Analyze coverage statistics
analyze_coverage_statistics() {
    local coverage_file="$1"
    
    if [[ ! -f "$coverage_file" ]]; then
        log_error "Coverage file not found: $coverage_file"
        return 1
    fi
    
    log_info "Analyzing coverage statistics..."
    
    local stats_file="$COVERAGE_DIR/coverage_stats.txt"
    
    # Generate summary statistics
    lcov $DEFAULT_LCOV_FLAGS --summary "$coverage_file" > "$stats_file" 2>&1
    
    # Extract key metrics
    local line_coverage=$(grep "lines" "$stats_file" | grep -o '[0-9.]*%' | head -1 | tr -d '%')
    local function_coverage=$(grep "functions" "$stats_file" | grep -o '[0-9.]*%' | head -1 | tr -d '%')
    local branch_coverage=$(grep "branches" "$stats_file" | grep -o '[0-9.]*%' | head -1 | tr -d '%')
    
    # Default values if not found
    line_coverage=${line_coverage:-0.0}
    function_coverage=${function_coverage:-0.0}
    branch_coverage=${branch_coverage:-0.0}
    
    # Log results
    log_info "Coverage Statistics:"
    log_info "  Line Coverage: ${line_coverage}%"
    log_info "  Function Coverage: ${function_coverage}%"
    log_info "  Branch Coverage: ${branch_coverage}%"
    
    # Check thresholds
    local threshold_to_check="$COVERAGE_THRESHOLD"
    if [[ "$FOCUS_PERFORMANCE" == "true" ]]; then
        threshold_to_check="$PERFORMANCE_THRESHOLD"
    fi
    
    local line_check=$(echo "$line_coverage >= $threshold_to_check" | bc -l)
    local function_check=$(echo "$function_coverage >= $threshold_to_check" | bc -l)
    
    local overall_pass=true
    
    if [[ "$line_check" -eq 1 ]]; then
        log_success "Line coverage meets threshold (${line_coverage}% >= ${threshold_to_check}%)"
    else
        log_error "Line coverage below threshold (${line_coverage}% < ${threshold_to_check}%)"
        overall_pass=false
    fi
    
    if [[ "$function_check" -eq 1 ]]; then
        log_success "Function coverage meets threshold (${function_coverage}% >= ${threshold_to_check}%)"
    else
        log_error "Function coverage below threshold (${function_coverage}% < ${threshold_to_check}%)"
        overall_pass=false
    fi
    
    # Branch coverage is informational
    if [[ -n "$branch_coverage" && "$branch_coverage" != "0.0" ]]; then
        log_info "Branch coverage: ${branch_coverage}% (informational)"
    fi
    
    # Generate detailed file-by-file coverage
    log_verbose "Generating detailed file coverage..."
    lcov $DEFAULT_LCOV_FLAGS --list "$coverage_file" > "$COVERAGE_DIR/detailed_coverage.txt" 2>&1
    
    if [[ "$overall_pass" == "true" ]]; then
        log_success "Coverage analysis passed all thresholds"
        return 0
    else
        log_error "Coverage analysis failed to meet thresholds"
        return 1
    fi
}

# Generate HTML coverage report
generate_html_report() {
    if [[ "$GENERATE_REPORT" != "true" ]]; then
        return 0
    fi
    
    local coverage_file="$1"
    
    if [[ ! -f "$coverage_file" ]]; then
        log_warning "Coverage file not found, skipping HTML report generation"
        return 1
    fi
    
    log_info "Generating HTML coverage report..."
    
    local html_dir="$COVERAGE_DIR/html"
    mkdir -p "$html_dir"
    
    # Generate HTML report
    local genhtml_options="--branch-coverage --function-coverage --legend --sort"
    
    if genhtml $genhtml_options "$coverage_file" --output-directory "$html_dir" >/dev/null 2>&1; then
        log_success "HTML coverage report generated: $html_dir/index.html"
        
        # Generate a summary page
        generate_coverage_summary "$html_dir"
    else
        log_error "Failed to generate HTML coverage report"
        return 1
    fi
}

# Generate coverage summary page
generate_coverage_summary() {
    local html_dir="$1"
    local summary_file="$html_dir/summary.html"
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    
    cat > "$summary_file" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>AncestralClust Coverage Summary</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        h1, h2 { color: #333; }
        .summary { background-color: #f9f9f9; padding: 20px; margin: 20px 0; }
        .metric { display: inline-block; margin: 10px 20px; padding: 10px; border: 1px solid #ddd; }
        .pass { background-color: #d4edda; border-color: #c3e6cb; }
        .fail { background-color: #f8d7da; border-color: #f5c6cb; }
        .info { background-color: #d1ecf1; border-color: #bee5eb; }
        table { border-collapse: collapse; width: 100%; margin: 20px 0; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #f2f2f2; }
    </style>
</head>
<body>
    <h1>AncestralClust Test Coverage Summary</h1>
    <p><strong>Generated:</strong> $timestamp</p>
    
    <div class="summary">
        <h2>Coverage Metrics</h2>
EOF

    # Add coverage metrics from stats file
    if [[ -f "$COVERAGE_DIR/coverage_stats.txt" ]]; then
        local line_coverage=$(grep "lines" "$COVERAGE_DIR/coverage_stats.txt" | grep -o '[0-9.]*%' | head -1 | tr -d '%')
        local function_coverage=$(grep "functions" "$COVERAGE_DIR/coverage_stats.txt" | grep -o '[0-9.]*%' | head -1 | tr -d '%')
        local branch_coverage=$(grep "branches" "$COVERAGE_DIR/coverage_stats.txt" | grep -o '[0-9.]*%' | head -1 | tr -d '%')
        
        line_coverage=${line_coverage:-0.0}
        function_coverage=${function_coverage:-0.0}
        branch_coverage=${branch_coverage:-0.0}
        
        local threshold_to_check="$COVERAGE_THRESHOLD"
        if [[ "$FOCUS_PERFORMANCE" == "true" ]]; then
            threshold_to_check="$PERFORMANCE_THRESHOLD"
        fi
        
        local line_class="info"
        local function_class="info"
        
        local line_check=$(echo "$line_coverage >= $threshold_to_check" | bc -l)
        local function_check=$(echo "$function_coverage >= $threshold_to_check" | bc -l)
        
        if [[ "$line_check" -eq 1 ]]; then line_class="pass"; else line_class="fail"; fi
        if [[ "$function_check" -eq 1 ]]; then function_class="pass"; else function_class="fail"; fi
        
        cat >> "$summary_file" << EOF
        <div class="metric $line_class">
            <strong>Line Coverage:</strong> ${line_coverage}%<br>
            <small>Threshold: ${threshold_to_check}%</small>
        </div>
        <div class="metric $function_class">
            <strong>Function Coverage:</strong> ${function_coverage}%<br>
            <small>Threshold: ${threshold_to_check}%</small>
        </div>
        <div class="metric info">
            <strong>Branch Coverage:</strong> ${branch_coverage}%<br>
            <small>Informational</small>
        </div>
EOF
    fi
    
    cat >> "$summary_file" << EOF
    </div>
    
    <h2>Analysis Configuration</h2>
    <ul>
        <li>Coverage Threshold: ${COVERAGE_THRESHOLD}%</li>
        <li>Performance Focus: $FOCUS_PERFORMANCE</li>
        <li>Include Unit Tests: $INCLUDE_UNIT</li>
        <li>Include Integration Tests: $INCLUDE_INTEGRATION</li>
        <li>Exclude Dependencies: $EXCLUDE_DEPS</li>
    </ul>
    
    <h2>Reports</h2>
    <ul>
        <li><a href="index.html">Detailed Coverage Report</a></li>
        <li><a href="../coverage_stats.txt">Raw Coverage Statistics</a></li>
        <li><a href="../detailed_coverage.txt">File-by-File Coverage</a></li>
    </ul>
    
    <footer>
        <p><em>Generated by AncestralClust Coverage Analysis Tool</em></p>
    </footer>
</body>
</html>
EOF

    log_verbose "Coverage summary page generated: $summary_file"
}

# Main execution
main() {
    echo "========================================"
    echo "AncestralClust Test Coverage Analysis"
    echo "========================================"
    echo ""
    
    parse_args "$@"
    
    log_info "Coverage analysis configuration:"
    log_info "  Coverage threshold: ${COVERAGE_THRESHOLD}%"
    log_info "  Performance focus: $FOCUS_PERFORMANCE"
    log_info "  Generate coverage data: $GENERATE_COVERAGE"
    log_info "  Generate HTML report: $GENERATE_REPORT"
    log_info "  Include unit tests: $INCLUDE_UNIT"
    log_info "  Include integration tests: $INCLUDE_INTEGRATION"
    log_info "  Exclude dependencies: $EXCLUDE_DEPS"
    echo ""
    
    check_requirements
    setup_coverage_environment
    
    local exit_code=0
    local filtered_coverage=""
    
    if [[ "$GENERATE_COVERAGE" == "true" ]]; then
        build_with_coverage
        run_tests_for_coverage
        generate_coverage_data
        filtered_coverage=$(filter_coverage_data)
        
        if [[ -n "$filtered_coverage" ]]; then
            analyze_coverage_statistics "$filtered_coverage" || exit_code=1
        else
            log_error "Failed to generate filtered coverage data"
            exit_code=1
        fi
    else
        # Look for existing coverage data
        filtered_coverage="$COVERAGE_DIR/filtered_coverage.info"
        if [[ ! -f "$filtered_coverage" ]]; then
            log_warning "No existing coverage data found"
            exit_code=1
        fi
    fi
    
    # Generate HTML report
    if [[ -n "$filtered_coverage" && -f "$filtered_coverage" ]]; then
        generate_html_report "$filtered_coverage"
    fi
    
    echo ""
    echo "========================================"
    if [[ $exit_code -eq 0 ]]; then
        log_success "Coverage analysis completed successfully!"
    else
        log_error "Coverage analysis completed with issues"
    fi
    echo "Results available in: $COVERAGE_DIR"
    if [[ "$GENERATE_REPORT" == "true" && -f "$COVERAGE_DIR/html/summary.html" ]]; then
        echo "HTML report: $COVERAGE_DIR/html/summary.html"
    fi
    echo "========================================"
    
    exit $exit_code
}

# Execute main function with all arguments
main "$@"