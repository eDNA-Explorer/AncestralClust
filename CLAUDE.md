# AncestralClust - Claude Code Configuration

## Project Overview
AncestralClust is a bioinformatics tool for clustering divergent nucleotide sequences using phylogenetic trees and ancestral sequence reconstruction.

## Build & Test Commands
```bash
# Build the project (standard)
make

# Build with performance logging enabled
make performance

# Debug build
make debug

# Clean build artifacts
make clean
```

## Project Structure
- `ancestralclust.c` - Main program entry point (instrumented with performance logging)
- `performance.h/performance.c` - Comprehensive performance monitoring system
- `kalign/` - Multiple sequence alignment library
- `WFA2/` - Wavefront alignment algorithm implementation
- `needleman_wunsch.c` - Pairwise alignment implementation
- `hashmap.c` - Hashmap implementation for taxonomy handling

## Key Dependencies
- GCC compiler with OpenMP support (-fopenmp)
- Math library (-lm)
- pthread library
- zlib (-lz)
- Real-time library (-lrt) for performance monitoring

## Usage Notes
- Requires FASTA input files with sequences on single lines (no line breaks)
- Supports only A, G, C, T, N nucleotides (no ambiguous bases or gaps)
- Maximum sequence length: 6000bp (configurable in global.h)
- Minimum sequence length: 100bp (configurable in global.h)

## Development Environment
- Language: C
- Compiler: GCC
- Build system: Make
- Threading: OpenMP

## Performance Monitoring

### Enabling Performance Logging
```bash
# Build with performance logging
make performance

# Run with performance monitoring
./ancestralclust -i sequences.fasta -r 500 -b 10

# Performance output files generated:
# - ancestralclust_performance.csv (detailed metrics)
# - Console output with performance summary
```

### Performance Metrics Tracked
- **Timing**: All major workflow phases (initialization, FASTA loading, distance matrix, tree construction, clustering, alignment, output)
- **Memory**: RSS, VmSize, VmPeak tracking at key milestones and allocations
- **CPU**: Per-thread and total CPU utilization during compute-intensive phases
- **Threading**: pthread and OpenMP parallel region performance
- **Convergence**: Clustering iteration progress and convergence metrics
- **I/O**: File reading/writing performance

### Performance Output Format
CSV output includes columns:
`timestamp,milestone,duration_ms,memory_rss_kb,memory_virt_kb,thread_count,iteration,convergence_metric,cpu_percent,label,context`

### Configuration
Performance logging granularity can be controlled through compile-time flags:
- `PERFORMANCE_LOGGING_ENABLED` - Enable performance monitoring
- Default configuration provides balanced monitoring with minimal overhead (<5%)

## Specialized Subagents

### Performance Logging Agent
**Type:** `performance-logger`
**Purpose:** Add comprehensive performance monitoring and logging to track clustering performance, memory usage, and algorithmic efficiency.

**Key Responsibilities:**
- Implement timing instrumentation for major operations (tree construction, alignment, clustering)
- Add memory usage tracking for large data structures (sequences, distance matrices, trees)
- Create performance logging infrastructure with configurable verbosity levels
- Add OpenMP thread performance monitoring
- Implement CSV/JSON output formats for performance metrics
- Track algorithmic convergence and iteration statistics

**Target Areas:**
- `ancestralclust.c:main()` - Overall runtime tracking
- Tree construction phases in clustering algorithm
- Alignment operations (kalign, WFA2, needleman-wunsch)
- Memory allocation patterns for large structures
- OpenMP parallel sections performance
- Distance matrix computation timing

**Implementation Strategy:**
- Create `performance.h/performance.c` module
- Add timer macros for easy instrumentation
- Implement memory tracking hooks
- Add performance report generation
- Integrate with existing OpenMP threading

### Crash/Debug Logging Agent  
**Type:** `crash-debug-logger`
**Purpose:** Implement robust error handling, crash recovery, and detailed debugging infrastructure for production and development use.

**Key Responsibilities:**
- Add comprehensive error handling with detailed error codes and messages
- Implement signal handlers for graceful crash recovery
- Create structured logging system with multiple log levels (ERROR, WARN, INFO, DEBUG, TRACE)
- Add input validation and bounds checking for all major data structures
- Implement stack trace generation on crashes
- Add memory leak detection and buffer overflow protection
- Create checkpoint/recovery mechanism for long-running clustering jobs

**Target Areas:**
- `global.h` - Add error code definitions and logging macros
- `ancestralclust.c` - Add main error handling framework
- Memory allocation/deallocation throughout codebase
- Array bounds checking in critical loops
- File I/O error handling
- Thread safety and race condition detection
- Input validation for FASTA parsing and parameter validation

**Implementation Strategy:**
- Create `debug.h/debug.c` and `logging.h/logging.c` modules
- Add signal handlers for SIGSEGV, SIGABRT, SIGFPE
- Implement structured logging with timestamps and thread IDs
- Add assertion macros with detailed failure information
- Create memory debugging wrapper functions
- Add input sanitization functions

### Unit Testing Agent
**Type:** `unit-tester`
**Purpose:** Establish comprehensive unit testing framework covering core algorithms, data structures, and edge cases.

**Key Responsibilities:**
- Set up C unit testing framework (e.g., Unity, Check, or custom)
- Create tests for core data structures (node, Options, mystruct, etc.)
- Test mathematical functions (distance calculations, tree operations)
- Add integration tests for major workflows
- Create test data generators for FASTA files and edge cases
- Implement continuous testing with various input sizes and edge cases
- Add regression tests for bug fixes

**Target Areas:**
- `math.c` - Test all mathematical functions (eigenvalue, matrix operations)
- `hashmap.c` - Test hashmap operations and edge cases
- `alignment*.c` - Test alignment algorithms with known inputs/outputs
- FASTA parsing functions - Test with malformed inputs
- Tree construction and manipulation functions
- Distance matrix calculations
- Memory management functions
- OpenMP thread safety

**Implementation Strategy:**
- Create `tests/` directory with organized test suites
- Add test runner and reporting infrastructure
- Create mock data generators for reproducible testing
- Implement property-based testing for algorithmic functions
- Add performance regression testing
- Create automated test execution in Docker environment
- Add code coverage analysis

## Subagent Usage Guidelines

When working with these subagents, use the Task tool with the appropriate subagent type:

```
Task tool with subagent_type: "performance-logger"
Task tool with subagent_type: "crash-debug-logger" 
Task tool with subagent_type: "unit-tester"
```

Each subagent should be used proactively when:
- Adding new features that affect performance, reliability, or correctness
- Debugging issues in the corresponding domain
- Implementing new algorithms or data structures
- Preparing for production deployment or performance optimization