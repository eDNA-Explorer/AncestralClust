# AncestralClust Comprehensive Testing Framework

This directory contains a complete testing framework for AncestralClust, providing comprehensive validation of the performance monitoring system through unit tests, integration tests, stress tests, and automated CI/CD integration.

## Directory Structure

```
tests/
├── README.md                           # This comprehensive documentation
├── test_runner.c                       # Main test runner executable
├── unity/                              # Unity testing framework v2.6.2
│   ├── unity.c                        # Unity framework implementation
│   ├── unity.h                        # Unity framework header
│   └── unity_internals.h              # Unity framework internals
├── unit/                               # Unit tests
│   ├── test_performance.c             # Core performance system tests
│   └── test_thread_safety.c           # Thread safety validation tests
├── integration/                        # End-to-end integration tests
│   └── test_ancestralclust_integration.c  # Full workflow validation
├── stress/                             # Stress and load testing
│   └── test_performance_stress.c      # High-load performance validation
├── fixtures/                           # Test data and expected outputs
│   ├── small_test.fasta               # Small dataset (5 sequences)
│   ├── medium_test.fasta               # Medium dataset (20 sequences)
│   ├── test_config.txt                # Test configuration file
│   └── expected_csv_pattern.txt       # Expected CSV output documentation
├── scripts/                            # Test automation and CI/CD scripts
│   ├── run_all_tests.sh               # Comprehensive test suite runner
│   ├── measure_performance.sh         # Performance overhead measurement
│   └── validate_coverage.sh           # Test coverage analysis
├── results/                            # Test execution results and reports
├── mocks/                              # Mock objects for testing
│   ├── mock_pthread.c                 # Pthread mocking for thread tests
│   └── mock_pthread.h                 # Pthread mock headers
└── utils/                              # Test utilities and helpers
    ├── thread_test_helpers.c           # Thread testing utilities
    ├── thread_test_helpers.h           # Thread testing headers
    ├── portable_barrier.c             # Cross-platform barrier implementation
    └── portable_barrier.h             # Barrier headers
```

## Quick Start

### Prerequisites

- GCC with C11 support and OpenMP
- pthread support
- zlib development libraries
- Optional: lcov/gcov for coverage analysis
- Optional: Docker for containerized testing

### Building the Performance-Enabled Binary

```bash
# Build AncestralClust with performance logging enabled
make performance

# Verify the binary was built successfully
./ancestralclust --help
```

### Building Tests

```bash
# Build all test suites
make tests

# Build specific test categories
make unit-tests          # Unit tests only
make stress-tests         # Stress tests only

# Build integration tests
make test_ancestralclust_integration

# Clean test executables
make clean-tests

# Clean everything including main binary
make clean-all
```

### Running Tests

#### Quick Test Execution

```bash
# Run all tests with automation script (recommended)
./tests/scripts/run_all_tests.sh

# Run specific test categories
./tests/scripts/run_all_tests.sh --unit-only
./tests/scripts/run_all_tests.sh --integration
./tests/scripts/run_all_tests.sh --stress

# Run with coverage analysis
./tests/scripts/run_all_tests.sh --coverage

# Run with performance measurement
./tests/scripts/run_all_tests.sh --performance
```

#### Manual Test Execution

```bash
# Run individual test suites
./tests/unit/test_performance
./tests/unit/test_thread_safety
./tests/integration/test_ancestralclust_integration
./tests/stress/test_performance_stress

# Run using Makefile targets
make run-unit-tests
make run-stress-tests
make run-tests            # All tests

# Run specific test
./tests/test_runner "Performance Tests"

# Show help
./tests/test_runner --help
```

### Performance Analysis

```bash
# Measure performance overhead
./tests/scripts/measure_performance.sh --quick

# Comprehensive performance analysis
./tests/scripts/measure_performance.sh --comprehensive --report

# Set custom overhead threshold (default 5%)
./tests/scripts/measure_performance.sh --threshold 3

# Generate performance baseline
./tests/scripts/measure_performance.sh --baseline
```

### Coverage Analysis

```bash
# Generate test coverage report
./tests/scripts/validate_coverage.sh --generate --report

# Focus on performance system coverage
./tests/scripts/validate_coverage.sh --performance --threshold 90

# Generate coverage for specific test types
./tests/scripts/validate_coverage.sh --unit --integration
```

## Test Coverage Summary

### Unit Tests (`tests/unit/`)

#### Performance System Tests (`test_performance.c`)
**11 tests** covering core performance monitoring functionality:

- ✅ **Initialization & Configuration**: Default and custom configuration setup
- ✅ **High-Resolution Timing**: Timestamp operations and duration calculations  
- ✅ **Milestone Tracking**: Basic and labeled milestone start/end tracking
- ✅ **Event Logging**: Custom event logging with context support
- ✅ **Memory Monitoring**: Memory usage tracking and validation
- ✅ **Utility Functions**: Milestone names, formatting, enable/disable

#### Thread Safety Tests (`test_thread_safety.c`)
**8 tests** validating thread-safe performance logging:

- ✅ **Concurrent Access**: Multiple threads accessing performance system simultaneously
- ✅ **Atomic Operations**: Thread-safe counters and memory tracking
- ✅ **Thread Registration**: Registration and cleanup in multi-threaded environments
- ✅ **Data Integrity**: Consistent data under concurrent access
- ✅ **OpenMP Integration**: Performance logging in OpenMP parallel regions

### Integration Tests (`tests/integration/`)

#### End-to-End Workflow Tests (`test_ancestralclust_integration.c`)
**8 comprehensive tests** validating performance logging during actual clustering:

- ✅ **Basic Integration**: Performance logging during real clustering operations
- ✅ **CSV Output Validation**: Proper CSV format and content verification
- ✅ **Granularity Levels**: Different logging granularity configurations
- ✅ **Multi-threaded Execution**: Thread safety during parallel clustering
- ✅ **Performance Overhead**: <5% execution time overhead validation
- ✅ **Memory Tracking**: Accurate memory usage monitoring
- ✅ **Algorithm Milestones**: Logical sequence of clustering milestones
- ✅ **Error Handling**: Graceful performance logging during error conditions

### Stress Tests (`tests/stress/`)

#### High-Load Performance Tests (`test_performance_stress.c`)
**6 stress tests** validating system behavior under extreme conditions:

- ✅ **High-Frequency Logging**: Rapid milestone logging performance
- ✅ **Memory Pressure**: Performance under memory constraints
- ✅ **Thread Scalability**: Behavior with high thread counts
- ✅ **Long-Running Operations**: Extended execution stability
- ✅ **Resource Exhaustion**: Graceful degradation under resource limits
- ✅ **Recovery Testing**: System recovery after stress conditions

### Test Data and Fixtures

#### FASTA Test Datasets
- **`small_test.fasta`**: 5 sequences, ~75bp each - Quick validation
- **`medium_test.fasta`**: 20 sequences, ~100bp each - Integration testing
- **`large_test.fasta`**: 50 sequences, ~200bp each - Performance testing (auto-generated)

#### Expected Output Patterns
- **CSV Format Validation**: Header format and field validation
- **Milestone Sequences**: Expected algorithmic milestone ordering
- **Performance Thresholds**: Acceptable overhead and resource usage ranges

### Automation Scripts

#### `run_all_tests.sh` - Master Test Runner
**Features:**
- Complete test suite execution (unit + integration + stress)
- Selective test category execution
- Performance overhead measurement (≤5% threshold)
- Test coverage analysis and reporting
- CI/CD pipeline integration
- Comprehensive result reporting

**Usage Examples:**
```bash
./tests/scripts/run_all_tests.sh                    # All tests
./tests/scripts/run_all_tests.sh --unit-only        # Unit tests only
./tests/scripts/run_all_tests.sh --coverage         # With coverage
./tests/scripts/run_all_tests.sh --performance      # With overhead measurement
```

#### `measure_performance.sh` - Performance Analysis
**Features:**
- Execution time overhead measurement
- Memory usage impact analysis  
- Multi-dataset performance validation
- Baseline performance establishment
- Performance regression detection
- HTML performance reports

**Usage Examples:**
```bash
./tests/scripts/measure_performance.sh --quick              # Quick test
./tests/scripts/measure_performance.sh --comprehensive     # Full analysis
./tests/scripts/measure_performance.sh --baseline          # Establish baseline
./tests/scripts/measure_performance.sh --threshold 3       # Custom threshold
```

#### `validate_coverage.sh` - Coverage Analysis
**Features:**
- Line, function, and branch coverage analysis
- Performance system focused coverage (90% threshold)
- HTML coverage report generation
- Coverage threshold validation
- Exclusion of third-party dependencies
- CI/CD integration support

**Usage Examples:**
```bash
./tests/scripts/validate_coverage.sh --generate --report   # Full analysis
./tests/scripts/validate_coverage.sh --performance         # Focus on perf system
./tests/scripts/validate_coverage.sh --threshold 85        # Custom threshold
```

### Test Statistics and Metrics

#### Overall Test Coverage
- **Total Test Cases**: 33 tests across all categories
- **Unit Tests**: 19 tests (performance system + thread safety)
- **Integration Tests**: 8 tests (end-to-end validation)
- **Stress Tests**: 6 tests (high-load scenarios)

#### Performance Validation
- **Overhead Threshold**: ≤5% execution time impact
- **Memory Overhead**: Minimal RSS increase during logging
- **Thread Safety**: Validated up to 64 concurrent threads
- **Logging Throughput**: >10,000 milestones/second capability

#### Code Coverage Targets
- **Line Coverage**: ≥80% overall, ≥90% performance system
- **Function Coverage**: ≥85% overall, ≥95% performance system  
- **Branch Coverage**: Informational (reported but not enforced)

#### Platform Support
- **Primary**: Ubuntu 20.04 LTS (Docker CI)
- **Development**: macOS (local testing)
- **Compilers**: GCC 9+, Clang 10+
- **Framework**: Unity 2.6.2

## Docker Support

The testing framework provides complete Docker integration for consistent testing across environments:

### Standard Docker Testing

```bash
# Build and run tests in Docker environment
docker build -f Dockerfile.test -t ancestralclust-test .

# This automatically:
# 1. Sets up Ubuntu 20.04 LTS environment  
# 2. Installs build dependencies (GCC, OpenMP, zlib)
# 3. Compiles AncestralClust with performance logging
# 4. Builds and executes all test suites
# 5. Validates Linux compatibility
```

### CI/CD Docker Integration

```bash
# Run comprehensive test suite in Docker
docker run --rm ancestralclust-test make run-tests

# Run with volume mounting for result collection
docker run --rm -v $(pwd)/test-results:/app/tests/results \
    ancestralclust-test ./tests/scripts/run_all_tests.sh --coverage

# Performance testing in controlled environment
docker run --rm ancestralclust-test \
    ./tests/scripts/measure_performance.sh --comprehensive --report
```

## Makefile Targets

### Test-Specific Targets
- `tests`: Compile all test executables
- `run-tests`: Compile and run all tests
- `test-runner`: Compile the test runner only
- `test_performance`: Compile performance tests only
- `clean-tests`: Remove test executables
- `clean-all`: Remove all executables including tests

### Platform Support
The Makefile automatically detects the platform and adjusts library linking:
- **macOS**: Uses `-lm -pthread -lz` (no librt)
- **Linux**: Uses `-lm -pthread -lz -lrt` (includes librt)

## Test Design Philosophy

### Comprehensive Coverage
Tests are designed to validate:
- **Correctness**: Functions behave as expected
- **Robustness**: Handle edge cases and error conditions  
- **Performance**: Verify timing accuracy and memory tracking
- **Platform Compatibility**: Work across macOS and Linux

### Boundary Value Analysis
- Memory limits tested with reasonable bounds for different platforms
- Timing precision validated with microsecond-level accuracy
- Configuration options tested at boundary values

### Regression Prevention
- Tests capture current behavior to prevent future breakage
- Memory usage patterns validated to catch leaks
- Performance characteristics monitored for degradation

## Future Expansion

The framework is designed for easy extension:

1. **Additional Unit Tests**: Add new test files in `unit/`
2. **Integration Tests**: Add end-to-end tests in `integration/`
3. **Mock Objects**: Create mocks for external dependencies in `mocks/`
4. **Test Utilities**: Add helper functions in `utils/`
5. **Test Data**: Store fixtures and test data in `fixtures/`

### Adding New Tests

1. Create new test file: `tests/unit/test_newmodule.c`
2. Follow Unity test conventions:
   ```c
   #include "../unity/unity.h"
   #include "../../your_module.h"
   
   void setUp(void) { /* setup */ }
   void tearDown(void) { /* cleanup */ }
   
   void test_your_function(void) {
       TEST_ASSERT_EQUAL(expected, actual);
   }
   
   int main(void) {
       UNITY_BEGIN();
       RUN_TEST(test_your_function);
       return UNITY_END();
   }
   ```
3. Add to Makefile test targets
4. Update test runner executable list

## Performance Monitoring Integration

The tests validate the performance monitoring system which provides:

- **High-resolution timing** with nanosecond precision
- **Memory tracking** for RSS and virtual memory usage
- **Milestone tracking** for algorithmic phases
- **Thread-safe logging** for parallel execution
- **Multiple output formats** (human-readable, CSV, JSON)
- **Configurable granularity** levels

## CI/CD Integration Guidelines

### GitHub Actions Integration

The testing framework is designed for seamless CI/CD integration:

```yaml
# Example GitHub Actions workflow
name: AncestralClust Tests
on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    - name: Build and Test
      run: |
        make performance
        ./tests/scripts/run_all_tests.sh --coverage --performance
    - name: Upload Coverage
      uses: codecov/codecov-action@v3
      with:
        file: ./tests/results/coverage/coverage.xml
```

### Jenkins Integration

```groovy
pipeline {
    agent any
    stages {
        stage('Build') {
            steps {
                sh 'make performance'
            }
        }
        stage('Test') {
            steps {
                sh './tests/scripts/run_all_tests.sh --coverage'
            }
            post {
                always {
                    publishHTML([
                        allowMissing: false,
                        alwaysLinkToLastBuild: true,
                        keepAll: true,
                        reportDir: 'tests/results/coverage/html',
                        reportFiles: 'index.html',
                        reportName: 'Coverage Report'
                    ])
                }
            }
        }
        stage('Performance') {
            steps {
                sh './tests/scripts/measure_performance.sh --comprehensive --report'
            }
        }
    }
}
```

### Performance Regression Detection

The testing framework includes automated performance regression detection:

- **Baseline Establishment**: Automatic baseline performance measurement
- **Regression Thresholds**: Configurable performance degradation limits  
- **Trend Analysis**: Historical performance tracking
- **Alert Integration**: Automated notifications for performance issues

## Production Deployment Validation

### Performance Verification Checklist

Before deploying to production, verify:

- ✅ **Unit Tests**: All 19 unit tests pass
- ✅ **Integration Tests**: All 8 integration tests pass  
- ✅ **Stress Tests**: All 6 stress tests pass
- ✅ **Performance Overhead**: ≤5% execution time impact
- ✅ **Memory Impact**: ≤10% RSS increase during logging
- ✅ **Thread Safety**: Validated up to production thread counts
- ✅ **Coverage**: ≥80% overall, ≥90% performance system
- ✅ **Platform Testing**: Both development and target platforms tested

### Production Configuration Recommendations

```c
// Recommended production performance configuration
perf_config_t production_config = {
    .enabled = 1,
    .granularity = PERF_GRANULARITY_COARSE,  // Minimal overhead
    .output_format = PERF_FORMAT_CSV,
    .track_memory = 1,
    .track_cpu = 0,                          // Disable for minimal overhead
    .track_threads = 1,
    .flush_immediately = 0,                  // Batch for performance
    .sampling_interval_us = 1000000          // 1 second sampling
};
```

### Monitoring and Alerting

The performance logging system supports production monitoring:

- **Real-time Metrics**: Live performance data streaming
- **Anomaly Detection**: Automatic detection of performance anomalies
- **Resource Monitoring**: Memory and CPU usage tracking
- **Performance Baselines**: Automatic baseline establishment and comparison

## Troubleshooting

### Common Issues and Solutions

#### Test Failures

**Issue**: Unit tests fail with "Binary not found"  
**Solution**: Run `make performance` before executing tests

**Issue**: Integration tests timeout  
**Solution**: Increase timeout or use smaller test datasets

**Issue**: Coverage analysis fails  
**Solution**: Install lcov/gcov: `sudo apt-get install lcov`

#### Performance Issues

**Issue**: Overhead exceeds 5% threshold  
**Solution**: Reduce granularity or disable CPU tracking

**Issue**: Memory usage grows during long runs  
**Solution**: Enable periodic log flushing or reduce log buffer size

**Issue**: Thread safety failures  
**Solution**: Verify atomic operations support and pthread linking

#### Docker Issues

**Issue**: Docker build fails  
**Solution**: Ensure Docker has sufficient memory (≥2GB recommended)

**Issue**: Permission errors in Docker  
**Solution**: Use `--user $(id -u):$(id -g)` flag

### Debug Mode

Enable verbose debugging for detailed test analysis:

```bash
# Enable verbose output for all tests
export UNITY_VERBOSE=1
./tests/scripts/run_all_tests.sh --verbose

# Debug specific test categories
./tests/scripts/validate_coverage.sh --verbose
./tests/scripts/measure_performance.sh --verbose
```

### Support and Maintenance

This comprehensive testing framework ensures the AncestralClust performance monitoring system:

- **Maintains Production Quality**: Rigorous validation of all functionality
- **Prevents Regressions**: Automated detection of performance and functional regressions
- **Supports Development**: Fast feedback cycle for development changes
- **Enables Continuous Deployment**: Automated validation pipeline for releases
- **Provides Observability**: Detailed performance insights for optimization

The framework is actively maintained and continuously improved to support the evolving needs of the AncestralClust project while ensuring the performance logging system remains reliable, efficient, and production-ready.