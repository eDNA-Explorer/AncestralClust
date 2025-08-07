# the compiler: gcc for C program
CC = gcc

#compiler flags:
# -g adds debugging information to the executable file
# -Wall turns on most, but not all, compiler warnings
CFLAGS = -w -pg -std=c11
DBGCFLAGS = -g -w -fopenmp -std=c11
PERFCFLAGS = -O2 -w -fopenmp -std=gnu11 -DPERFORMANCE_LOGGING_ENABLED -D_GNU_SOURCE
# -lm links the math library
#LIBS = -lm -lpthread -lz
LIBS = -lm -pthread -lz -lrt
OPENMP = -fopenmp
OPTIMIZATION = -O3 -march=native

# Test-specific configuration
TESTCFLAGS = -g -w -std=c11 -DPERFORMANCE_LOGGING_ENABLED -D_GNU_SOURCE
UNITY_DIR = tests/unity
UNITY_SOURCES = $(UNITY_DIR)/unity.c
# Platform-specific test libraries (macOS doesn't have librt)
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    TEST_LIBS = -lm -pthread -lz
else
    TEST_LIBS = $(LIBS)
endif
#sources
SOURCES = ancestralclust.c options.c math.c opt.c performance.c
NEEDLEMANWUNSCH = needleman_wunsch.c alignment.c alignment_scoring.c
HASHMAP = hashmap.c
KALIGN = kalign/run_kalign.c kalign/tlmisc.c kalign/tldevel.c kalign/parameters.c kalign/rwalign.c kalign/alignment_parameters.c kalign/idata.c kalign/aln_task.c kalign/bisectingKmeans.c kalign/esl_stopwatch.c kalign/aln_run.c kalign/alphabet.c kalign/pick_anchor.c kalign/sequence_distance.c kalign/euclidean_dist.c kalign/aln_mem.c kalign/tlrng.c kalign/aln_setup.c kalign/aln_controller.c kalign/weave_alignment.c kalign/aln_seqseq.c kalign/aln_profileprofile.c kalign/aln_seqprofile.c
WFA2 = WFA2/wavefront_aligner.c WFA2/wavefront_bialigner.c WFA2/mm_allocator.c WFA2/wavefront_align.c WFA2/wavefront_attributes.c WFA2/wavefront_debug.c WFA2/profiler_timer.c WFA2/profiler_counter.c WFA2/cigar.c WFA2/wavefront_compute.c WFA2/wavefront_slab.c WFA2/wavefront.c WFA2/vector.c WFA2/wavefront_components.c WFA2/bitmap.c WFA2/wavefront_backtrace_buffer.c WFA2/wavefront_pcigar.c WFA2/wavefront_heuristic.c WFA2/wavefront_penalties.c WFA2/wavefront_unialign.c WFA2/wavefront_plot.c WFA2/heatmap.c WFA2/wavefront_backtrace.c WFA2/wavefront_extend.c WFA2/wavefront_compute_affine2p.c WFA2/wavefront_backtrace_offload.c WFA2/wavefront_compute_affine.c WFA2/wavefront_compute_linear.c WFA2/wavefront_compute_edit.c WFA2/string_padded.c WFA2/wavefront_bialign.c
OBJECTS = (SOURCES: .c = .o)
# the build target executable:
TARGET = ancestralclust

all: $(TARGET)
$(TARGET): $(SOURCES) $(NEEDLEMANWUNSCH) $(HASHMAP) $(KALIGN) $(WFA2)
	$(CC) $(OPENMP) $(CFLAGS) -o $(TARGET) $(NEEDLEMANWUNSCH) $(HASHMAP) $(KALIGN) $(WFA2) $(SOURCES) $(LIBS)

debug: $(SOURCES) $(NEEDLEMANWUNSCH) $(HASHMAP) $(KALIGN) $(WFA2)
	$(CC) $(DBGCFLAGS) -o $(TARGET) $(NEEDLEMANWUNSCH) $(HASHMAP) $(KALIGN) $(WFA2) $(SOURCES) $(LIBS)

performance: $(SOURCES) $(NEEDLEMANWUNSCH) $(HASHMAP) $(KALIGN) $(WFA2)
	$(CC) $(PERFCFLAGS) $(OPENMP) -o $(TARGET) $(NEEDLEMANWUNSCH) $(HASHMAP) $(KALIGN) $(WFA2) $(SOURCES) $(LIBS)

clean:
	$(RM) $(TARGET)

# Test targets
# Thread safety and stress test configuration
ifeq ($(UNAME_S),Darwin)
    THREAD_TEST_CFLAGS = $(TESTCFLAGS) -DENABLE_PTHREAD_MOCKING
else
    THREAD_TEST_CFLAGS = $(TESTCFLAGS) -DENABLE_PTHREAD_MOCKING -fopenmp
endif
THREAD_UTILS = tests/utils/thread_test_helpers.c tests/utils/portable_barrier.c
MOCK_PTHREAD = tests/mocks/mock_pthread.c

test-runner: tests/test_runner.c
	$(CC) $(TESTCFLAGS) -o tests/test_runner tests/test_runner.c

test_performance: tests/unit/test_performance.c performance.c $(UNITY_SOURCES)
	$(CC) $(TESTCFLAGS) -I$(UNITY_DIR) -o tests/unit/test_performance \
		tests/unit/test_performance.c performance.c $(UNITY_SOURCES) $(TEST_LIBS)

test_thread_safety: tests/unit/test_thread_safety.c performance.c $(UNITY_SOURCES) $(THREAD_UTILS) $(MOCK_PTHREAD)
	$(CC) $(THREAD_TEST_CFLAGS) -I$(UNITY_DIR) -I tests/utils -I tests/mocks \
		-o tests/unit/test_thread_safety \
		tests/unit/test_thread_safety.c performance.c $(UNITY_SOURCES) \
		$(THREAD_UTILS) $(MOCK_PTHREAD) $(TEST_LIBS)

test_performance_stress: tests/stress/test_performance_stress.c performance.c $(UNITY_SOURCES) $(THREAD_UTILS)
	$(CC) $(THREAD_TEST_CFLAGS) -I$(UNITY_DIR) -I tests/utils \
		-o tests/stress/test_performance_stress \
		tests/stress/test_performance_stress.c performance.c $(UNITY_SOURCES) \
		$(THREAD_UTILS) $(TEST_LIBS)

# Unit tests
unit-tests: test_performance test_thread_safety
	@echo "Unit tests compiled successfully"

# Stress tests  
stress-tests: test_performance_stress
	@echo "Stress tests compiled successfully"

# Integration tests
test_ancestralclust_integration: tests/integration/test_ancestralclust_integration.c performance.c $(UNITY_SOURCES)
	$(CC) $(TESTCFLAGS) -I$(UNITY_DIR) -o tests/integration/test_ancestralclust_integration \
		tests/integration/test_ancestralclust_integration.c performance.c $(UNITY_SOURCES) $(TEST_LIBS)

# Integration test suite
integration-tests: test_ancestralclust_integration
	@echo "Integration tests compiled successfully"

# All tests
tests: test-runner unit-tests stress-tests integration-tests
	@echo "All tests compiled successfully"

# Run specific test suites
run-unit-tests: unit-tests
	@echo "Running unit tests..."
	./tests/unit/test_performance
	./tests/unit/test_thread_safety

run-integration-tests: integration-tests
	@echo "Running integration tests..."
	./tests/integration/test_ancestralclust_integration

run-stress-tests: stress-tests
	@echo "Running stress tests..."
	./tests/stress/test_performance_stress

run-thread-safety-tests: test_thread_safety
	@echo "Running thread safety tests..."
	./tests/unit/test_thread_safety

# Comprehensive test execution
run-tests: tests
	@echo "Running comprehensive test suite..."
	@echo "=== Test Runner ==="
	./tests/test_runner
	@echo "=== Unit Tests ==="
	./tests/unit/test_performance
	./tests/unit/test_thread_safety
	@echo "=== Integration Tests ==="
	./tests/integration/test_ancestralclust_integration
	@echo "=== Stress Tests ==="
	./tests/stress/test_performance_stress

# Automated test suite with scripts
run-automated-tests: tests
	@echo "Running automated test suite..."
	./tests/scripts/run_all_tests.sh

# Performance analysis
run-performance-analysis: tests
	@echo "Running performance analysis..."
	./tests/scripts/measure_performance.sh --quick

# Coverage analysis
run-coverage-analysis: tests
	@echo "Running coverage analysis..."
	./tests/scripts/validate_coverage.sh --generate --report

# Complete validation (for CI/CD)
run-complete-validation: tests
	@echo "Running complete validation suite..."
	./tests/scripts/run_all_tests.sh --coverage --performance

# Docker test targets
docker-test-build:
	docker build -f Dockerfile.test -t ancestralclust-test .

docker-ci-build:
	docker build -f Dockerfile.ci -t ancestralclust-ci .

docker-run-tests: docker-test-build
	docker run --rm ancestralclust-test make run-tests

docker-run-stress-tests: docker-test-build
	docker run --rm ancestralclust-test make run-stress-tests

# Docker CI targets
docker-ci-quick: docker-ci-build
	docker run --rm ancestralclust-ci quick

docker-ci-full: docker-ci-build
	docker run --rm -v $(shell pwd)/ci-results:/app/results ancestralclust-ci full

docker-ci-performance: docker-ci-build
	docker run --rm -v $(shell pwd)/ci-results:/app/results ancestralclust-ci performance

docker-ci-coverage: docker-ci-build
	docker run --rm -v $(shell pwd)/ci-results:/app/results ancestralclust-ci coverage

# Cleanup targets
clean-tests:
	$(RM) tests/test_runner tests/unit/test_performance tests/unit/test_thread_safety tests/stress/test_performance_stress tests/integration/test_ancestralclust_integration

clean-results:
	$(RM) -rf tests/results/* ci-results/*

clean-coverage:
	find . -name "*.gcda" -delete
	find . -name "*.gcno" -delete
	$(RM) -rf tests/results/coverage/*

clean-all: clean clean-tests clean-results clean-coverage

# Help target
help:
	@echo "AncestralClust Testing Framework"
	@echo "================================"
	@echo ""
	@echo "Build Targets:"
	@echo "  performance                 Build AncestralClust with performance logging"
	@echo "  debug                      Build with debug symbols"
	@echo "  clean                      Clean main binary"
	@echo ""
	@echo "Test Build Targets:"
	@echo "  tests                      Build all test suites"
	@echo "  unit-tests                 Build unit tests only"
	@echo "  integration-tests          Build integration tests only"
	@echo "  stress-tests               Build stress tests only"
	@echo ""
	@echo "Test Execution Targets:"
	@echo "  run-tests                  Run comprehensive test suite"
	@echo "  run-unit-tests             Run unit tests only"
	@echo "  run-integration-tests      Run integration tests only"
	@echo "  run-stress-tests           Run stress tests only"
	@echo "  run-automated-tests        Run with automation scripts"
	@echo "  run-performance-analysis   Run performance overhead analysis"
	@echo "  run-coverage-analysis      Run test coverage analysis"
	@echo "  run-complete-validation    Run full CI/CD validation"
	@echo ""
	@echo "Docker Targets:"
	@echo "  docker-test-build          Build standard Docker test image"
	@echo "  docker-ci-build            Build CI Docker image"
	@echo "  docker-ci-quick            Run quick CI tests in Docker"
	@echo "  docker-ci-full             Run comprehensive CI tests in Docker"
	@echo "  docker-ci-performance      Run performance analysis in Docker"
	@echo "  docker-ci-coverage         Run coverage analysis in Docker"
	@echo ""
	@echo "Cleanup Targets:"
	@echo "  clean-tests                Clean test executables"
	@echo "  clean-results              Clean test results"
	@echo "  clean-coverage             Clean coverage data"
	@echo "  clean-all                  Clean everything"
	@echo ""
	@echo "For detailed usage instructions, see tests/README.md"

.PHONY: all debug performance clean tests unit-tests integration-tests stress-tests \
        run-tests run-unit-tests run-integration-tests run-stress-tests \
        run-automated-tests run-performance-analysis run-coverage-analysis \
        run-complete-validation docker-test-build docker-ci-build \
        docker-run-tests docker-ci-quick docker-ci-full docker-ci-performance \
        docker-ci-coverage clean-tests clean-results clean-coverage clean-all help

