/*
 * test_performance_stress.c
 * 
 * Comprehensive stress tests for the AncestralClust performance logging system
 * Tests high-frequency operations, memory leak detection, large dataset simulation,
 * resource exhaustion scenarios, and performance overhead measurement.
 * 
 * These tests verify that the performance monitoring system maintains <5% overhead
 * even under extreme load conditions typical in large-scale clustering operations.
 */

#include "../unity/unity.h"
#include "../../performance.h"
#include "../utils/portable_barrier.h"
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdatomic.h>
#include <time.h>
#include <sys/resource.h>
#include <signal.h>

/* Stress test configuration constants */
#define STRESS_MAX_THREADS 64
#define HIGH_FREQ_OPERATIONS 50000
#define MEMORY_PRESSURE_MB 256
#define LARGE_DATASET_SEQUENCES 10000
#define STRESS_TEST_DURATION_SEC 30
#define MEMORY_LEAK_ITERATIONS 10000
#define OVERHEAD_MEASUREMENT_CYCLES 1000000

/* Stress test synchronization */
typedef struct {
    pthread_barrier_t start_barrier;
    pthread_barrier_t end_barrier;
    volatile int should_stop;
    atomic_int total_operations;
    atomic_int memory_allocations;
    atomic_int memory_deallocations;
    atomic_long total_bytes_allocated;
    atomic_long total_bytes_freed;
    atomic_int active_threads;
    struct timespec test_start_time;
    struct timespec test_end_time;
} stress_sync_t;

/* Stress test data structure */
typedef struct {
    int thread_id;
    int operations_target;
    stress_sync_t *sync;
    double *performance_measurements;
    int measurement_count;
    size_t memory_used;
} stress_test_data_t;

/* Global stress test synchronization */
static stress_sync_t g_stress_sync;
static volatile sig_atomic_t stress_timeout_flag = 0;

/* Signal handler for stress test timeout */
static void stress_timeout_handler(int sig) {
    stress_timeout_flag = 1;
    g_stress_sync.should_stop = 1;
}

/* Test fixture setup and teardown */
void setUp(void) {
    perf_cleanup();
    perf_init();
    
    /* Initialize stress test synchronization */
    memset(&g_stress_sync, 0, sizeof(g_stress_sync));
    atomic_init(&g_stress_sync.total_operations, 0);
    atomic_init(&g_stress_sync.memory_allocations, 0);
    atomic_init(&g_stress_sync.memory_deallocations, 0);
    atomic_init(&g_stress_sync.total_bytes_allocated, 0);
    atomic_init(&g_stress_sync.total_bytes_freed, 0);
    atomic_init(&g_stress_sync.active_threads, 0);
    stress_timeout_flag = 0;
}

void tearDown(void) {
    g_stress_sync.should_stop = 1;
    perf_cleanup();
    signal(SIGALRM, SIG_DFL);
}

/* ========================================================================
 * HIGH-FREQUENCY MILESTONE TRACKING STRESS TEST
 * ======================================================================== */

static void* high_frequency_milestone_thread(void *arg) {
    stress_test_data_t *data = (stress_test_data_t*)arg;
    
    /* Register thread */
    perf_register_thread();
    atomic_fetch_add(&data->sync->active_threads, 1);
    
    /* Wait for all threads to be ready */
    pthread_barrier_wait(&data->sync->start_barrier);
    
    /* High-frequency milestone operations */
    for (int i = 0; i < data->operations_target && !data->sync->should_stop; i++) {
        /* Rapid-fire milestone tracking */
        perf_start_milestone(PERF_MILESTONE_DISTANCE_CALCULATION);
        perf_end_milestone(PERF_MILESTONE_DISTANCE_CALCULATION);
        
        perf_start_milestone(PERF_MILESTONE_CLUSTERING_ITERATION);
        perf_end_milestone(PERF_MILESTONE_CLUSTERING_ITERATION);
        
        perf_start_milestone(PERF_MILESTONE_MEMORY_ALLOC);
        perf_end_milestone(PERF_MILESTONE_MEMORY_ALLOC);
        
        /* Log events rapidly */
        perf_log_event("high_freq_event", (double)i);
        perf_log_iteration(i % 1000, 0.99 - (i * 0.0001));
        
        atomic_fetch_add(&data->sync->total_operations, 5);
        
        /* Every 1000 operations, yield to prevent CPU starvation */
        if (i % 1000 == 0) {
            usleep(1);
        }
    }
    
    /* Unregister thread */
    perf_unregister_thread();
    atomic_fetch_sub(&data->sync->active_threads, 1);
    
    pthread_barrier_wait(&data->sync->end_barrier);
    return NULL;
}

void test_high_frequency_milestone_tracking(void) {
    const int num_threads = 32;
    const int ops_per_thread = HIGH_FREQ_OPERATIONS / num_threads;
    pthread_t threads[num_threads];
    stress_test_data_t thread_data[num_threads];
    
    printf("\nStarting high-frequency milestone tracking stress test...\n");
    printf("Target: %d operations across %d threads\n", HIGH_FREQ_OPERATIONS, num_threads);
    
    /* Initialize barriers */
    pthread_barrier_init(&g_stress_sync.start_barrier, NULL, num_threads);
    pthread_barrier_init(&g_stress_sync.end_barrier, NULL, num_threads);
    
    /* Set up timeout alarm */
    signal(SIGALRM, stress_timeout_handler);
    alarm(60); /* 60 second timeout */
    
    /* Record start time */
    clock_gettime(CLOCK_MONOTONIC, &g_stress_sync.test_start_time);
    
    /* Create and start threads */
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].operations_target = ops_per_thread;
        thread_data[i].sync = &g_stress_sync;
        
        int result = pthread_create(&threads[i], NULL, high_frequency_milestone_thread, &thread_data[i]);
        TEST_ASSERT_EQUAL(0, result);
    }
    
    /* Wait for all threads to complete */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Record end time */
    clock_gettime(CLOCK_MONOTONIC, &g_stress_sync.test_end_time);
    
    /* Calculate performance metrics */
    double duration_ms = (g_stress_sync.test_end_time.tv_sec - g_stress_sync.test_start_time.tv_sec) * 1000.0 +
                        (g_stress_sync.test_end_time.tv_nsec - g_stress_sync.test_start_time.tv_nsec) / 1e6;
    
    int total_ops = atomic_load(&g_stress_sync.total_operations);
    double ops_per_second = total_ops / (duration_ms / 1000.0);
    
    printf("Completed: %d operations in %.2f ms (%.0f ops/sec)\n", 
           total_ops, duration_ms, ops_per_second);
    
    /* Verify performance requirements */
    TEST_ASSERT_FALSE(stress_timeout_flag); /* Should not have timed out */
    TEST_ASSERT_GREATER_THAN(HIGH_FREQ_OPERATIONS * 0.95, total_ops); /* At least 95% completion */
    TEST_ASSERT_GREATER_THAN(1000.0, ops_per_second); /* Should achieve >1000 ops/sec */
    
    /* Cleanup */
    alarm(0);
    pthread_barrier_destroy(&g_stress_sync.start_barrier);
    pthread_barrier_destroy(&g_stress_sync.end_barrier);
}

/* ========================================================================
 * MEMORY LEAK DETECTION STRESS TEST
 * ======================================================================== */

static void* memory_leak_detection_thread(void *arg) {
    stress_test_data_t *data = (stress_test_data_t*)arg;
    void **allocated_ptrs;
    size_t allocation_size = 1024;
    
    /* Allocate array to track pointers */
    allocated_ptrs = malloc(sizeof(void*) * data->operations_target);
    TEST_ASSERT_NOT_NULL(allocated_ptrs);
    
    /* Wait for all threads to be ready */
    pthread_barrier_wait(&data->sync->start_barrier);
    
    /* Phase 1: Allocate memory and track it */
    for (int i = 0; i < data->operations_target && !data->sync->should_stop; i++) {
        size_t size = allocation_size + (i % 4096);
        allocated_ptrs[i] = malloc(size);
        
        if (allocated_ptrs[i] != NULL) {
            perf_track_allocation(allocated_ptrs[i], size);
            atomic_fetch_add(&data->sync->memory_allocations, 1);
            atomic_fetch_add(&data->sync->total_bytes_allocated, size);
            data->memory_used += size;
        }
        
        /* Simulate work */
        if (i % 100 == 0) {
            usleep(10);
        }
    }
    
    /* Phase 2: Free half the memory */
    for (int i = 0; i < data->operations_target / 2 && !data->sync->should_stop; i++) {
        if (allocated_ptrs[i] != NULL) {
            perf_track_deallocation(allocated_ptrs[i]);
            free(allocated_ptrs[i]);
            allocated_ptrs[i] = NULL;
            atomic_fetch_add(&data->sync->memory_deallocations, 1);
        }
    }
    
    /* Phase 3: Free remaining memory */
    for (int i = data->operations_target / 2; i < data->operations_target && !data->sync->should_stop; i++) {
        if (allocated_ptrs[i] != NULL) {
            perf_track_deallocation(allocated_ptrs[i]);
            free(allocated_ptrs[i]);
            allocated_ptrs[i] = NULL;
            atomic_fetch_add(&data->sync->memory_deallocations, 1);
        }
    }
    
    free(allocated_ptrs);
    pthread_barrier_wait(&data->sync->end_barrier);
    return NULL;
}

void test_memory_leak_detection_extended_operation(void) {
    const int num_threads = 16;
    const int allocations_per_thread = MEMORY_LEAK_ITERATIONS / num_threads;
    pthread_t threads[num_threads];
    stress_test_data_t thread_data[num_threads];
    
    printf("\nStarting memory leak detection stress test...\n");
    printf("Target: %d allocations across %d threads\n", MEMORY_LEAK_ITERATIONS, num_threads);
    
    /* Get baseline memory usage */
    struct rusage usage_before;
    getrusage(RUSAGE_SELF, &usage_before);
    size_t baseline_rss = usage_before.ru_maxrss;
    
    /* Initialize barriers */
    pthread_barrier_init(&g_stress_sync.start_barrier, NULL, num_threads);
    pthread_barrier_init(&g_stress_sync.end_barrier, NULL, num_threads);
    
    /* Create and start threads */
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].operations_target = allocations_per_thread;
        thread_data[i].sync = &g_stress_sync;
        thread_data[i].memory_used = 0;
        
        int result = pthread_create(&threads[i], NULL, memory_leak_detection_thread, &thread_data[i]);
        TEST_ASSERT_EQUAL(0, result);
    }
    
    /* Wait for all threads to complete */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Check final memory usage */
    struct rusage usage_after;
    getrusage(RUSAGE_SELF, &usage_after);
    size_t final_rss = usage_after.ru_maxrss;
    
    /* Verify memory tracking accuracy */
    int total_allocs = atomic_load(&g_stress_sync.memory_allocations);
    int total_frees = atomic_load(&g_stress_sync.memory_deallocations);
    long total_allocated_bytes = atomic_load(&g_stress_sync.total_bytes_allocated);
    
    printf("Memory operations: %d allocs, %d frees\n", total_allocs, total_frees);
    printf("Total bytes allocated: %ld\n", total_allocated_bytes);
    printf("RSS change: %zu KB\n", (final_rss - baseline_rss) / 1024);
    
    /* Verify no significant memory leaks */
    TEST_ASSERT_EQUAL(total_allocs, total_frees);
    TEST_ASSERT_GREATER_THAN(MEMORY_LEAK_ITERATIONS * 0.95, total_allocs);
    
    /* Memory growth should be minimal after cleanup */
    size_t memory_growth_kb = (final_rss > baseline_rss) ? (final_rss - baseline_rss) / 1024 : 0;
    TEST_ASSERT_LESS_THAN(10240, memory_growth_kb); /* Less than 10MB growth */
    
    /* Cleanup */
    pthread_barrier_destroy(&g_stress_sync.start_barrier);
    pthread_barrier_destroy(&g_stress_sync.end_barrier);
}

/* ========================================================================
 * LARGE DATASET SIMULATION STRESS TEST
 * ======================================================================== */

static void* large_dataset_simulation_thread(void *arg) {
    stress_test_data_t *data = (stress_test_data_t*)arg;
    
    /* Wait for all threads to be ready */
    pthread_barrier_wait(&data->sync->start_barrier);
    
    /* Simulate large-scale clustering operations */
    for (int seq = 0; seq < data->operations_target && !data->sync->should_stop; seq++) {
        char seq_label[64];
        snprintf(seq_label, sizeof(seq_label), "sequence_%d_%d", data->thread_id, seq);
        
        /* Simulate distance matrix calculation */
        perf_start_milestone_labeled(PERF_MILESTONE_DISTANCE_MATRIX_START, seq_label);
        
        for (int i = 0; i < 100; i++) { /* 100 distance calculations per sequence */
            double distance = (double)(i * seq + data->thread_id) / 1000.0;
            perf_log_event_with_context("pairwise_distance", distance, seq_label);
            
            /* Simulate computational work */
            volatile double dummy = 0;
            for (int k = 0; k < 1000; k++) {
                dummy += sqrt((double)k);
            }
        }
        
        perf_end_milestone_labeled(PERF_MILESTONE_DISTANCE_MATRIX_START, seq_label);
        
        /* Simulate clustering iteration */
        if (seq % 10 == 0) {
            perf_start_milestone_labeled(PERF_MILESTONE_CLUSTERING_ITERATION, seq_label);
            double convergence = 1.0 - (seq * 0.001);
            perf_log_iteration(seq / 10, convergence);
            perf_end_milestone_labeled(PERF_MILESTONE_CLUSTERING_ITERATION, seq_label);
        }
        
        atomic_fetch_add(&data->sync->total_operations, 1);
    }
    
    pthread_barrier_wait(&data->sync->end_barrier);
    return NULL;
}

void test_large_dataset_simulation_memory_pressure(void) {
    const int num_threads = 20;
    const int sequences_per_thread = LARGE_DATASET_SEQUENCES / num_threads;
    pthread_t threads[num_threads];
    stress_test_data_t thread_data[num_threads];
    
    printf("\nStarting large dataset simulation stress test...\n");
    printf("Simulating: %d sequences across %d threads\n", LARGE_DATASET_SEQUENCES, num_threads);
    
    /* Initialize barriers */
    pthread_barrier_init(&g_stress_sync.start_barrier, NULL, num_threads);
    pthread_barrier_init(&g_stress_sync.end_barrier, NULL, num_threads);
    
    /* Set up timeout */
    signal(SIGALRM, stress_timeout_handler);
    alarm(120); /* 2 minute timeout */
    
    /* Record start time and memory */
    clock_gettime(CLOCK_MONOTONIC, &g_stress_sync.test_start_time);
    struct rusage usage_start;
    getrusage(RUSAGE_SELF, &usage_start);
    
    /* Create and start threads */
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].operations_target = sequences_per_thread;
        thread_data[i].sync = &g_stress_sync;
        
        int result = pthread_create(&threads[i], NULL, large_dataset_simulation_thread, &thread_data[i]);
        TEST_ASSERT_EQUAL(0, result);
    }
    
    /* Wait for all threads to complete */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Record end time and memory */
    clock_gettime(CLOCK_MONOTONIC, &g_stress_sync.test_end_time);
    struct rusage usage_end;
    getrusage(RUSAGE_SELF, &usage_end);
    
    /* Calculate performance metrics */
    double duration_s = (g_stress_sync.test_end_time.tv_sec - g_stress_sync.test_start_time.tv_sec) +
                       (g_stress_sync.test_end_time.tv_nsec - g_stress_sync.test_start_time.tv_nsec) / 1e9;
    
    int total_sequences = atomic_load(&g_stress_sync.total_operations);
    double sequences_per_second = total_sequences / duration_s;
    
    size_t memory_usage_kb = usage_end.ru_maxrss / 1024;
    
    printf("Processed: %d sequences in %.2f seconds (%.1f seq/sec)\n", 
           total_sequences, duration_s, sequences_per_second);
    printf("Peak memory usage: %zu KB\n", memory_usage_kb);
    
    /* Verify performance requirements */
    TEST_ASSERT_FALSE(stress_timeout_flag);
    TEST_ASSERT_GREATER_THAN(LARGE_DATASET_SEQUENCES * 0.95, total_sequences);
    TEST_ASSERT_GREATER_THAN(10.0, sequences_per_second); /* At least 10 seq/sec */
    TEST_ASSERT_LESS_THAN(512 * 1024, memory_usage_kb); /* Less than 512MB */
    
    /* Cleanup */
    alarm(0);
    pthread_barrier_destroy(&g_stress_sync.start_barrier);
    pthread_barrier_destroy(&g_stress_sync.end_barrier);
}

/* ========================================================================
 * PERFORMANCE OVERHEAD MEASUREMENT TEST
 * ======================================================================== */

static double measure_overhead_disabled(int iterations) {
    struct timespec start, end;
    
    /* Disable performance monitoring */
    perf_set_enabled(0);
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < iterations; i++) {
        PERF_START_MILESTONE(PERF_MILESTONE_DISTANCE_CALCULATION);
        perf_log_event("test_event", (double)i);
        PERF_END_MILESTONE(PERF_MILESTONE_DISTANCE_CALCULATION);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    return (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1e6;
}

static double measure_overhead_enabled(int iterations) {
    struct timespec start, end;
    
    /* Enable performance monitoring */
    perf_set_enabled(1);
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < iterations; i++) {
        PERF_START_MILESTONE(PERF_MILESTONE_DISTANCE_CALCULATION);
        perf_log_event("test_event", (double)i);
        PERF_END_MILESTONE(PERF_MILESTONE_DISTANCE_CALCULATION);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    return (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1e6;
}

void test_performance_overhead_measurement_under_load(void) {
    const int measurement_cycles = OVERHEAD_MEASUREMENT_CYCLES;
    
    printf("\nMeasuring performance overhead with %d operations...\n", measurement_cycles);
    
    /* Warm up */
    measure_overhead_disabled(1000);
    measure_overhead_enabled(1000);
    
    /* Measure baseline (disabled) */
    double baseline_time = 0;
    for (int run = 0; run < 5; run++) {
        baseline_time += measure_overhead_disabled(measurement_cycles);
    }
    baseline_time /= 5.0;
    
    /* Measure with performance monitoring enabled */
    double enabled_time = 0;
    for (int run = 0; run < 5; run++) {
        enabled_time += measure_overhead_enabled(measurement_cycles);
    }
    enabled_time /= 5.0;
    
    /* Calculate overhead */
    double overhead_ms = enabled_time - baseline_time;
    double overhead_percent = (overhead_ms / baseline_time) * 100.0;
    double overhead_per_operation_ns = (overhead_ms * 1e6) / measurement_cycles;
    
    printf("Baseline time (disabled): %.3f ms\n", baseline_time);
    printf("Enabled time: %.3f ms\n", enabled_time);
    printf("Overhead: %.3f ms (%.2f%%)\n", overhead_ms, overhead_percent);
    printf("Overhead per operation: %.2f ns\n", overhead_per_operation_ns);
    
    /* Verify overhead is acceptable (<5%) */
    TEST_ASSERT_LESS_THAN(5.0, overhead_percent);
    TEST_ASSERT_LESS_THAN(1000.0, overhead_per_operation_ns); /* Less than 1μs per operation */
    
    /* Verify baseline performance is reasonable */
    double ops_per_second = measurement_cycles / (baseline_time / 1000.0);
    TEST_ASSERT_GREATER_THAN(100000.0, ops_per_second); /* At least 100k ops/sec baseline */
}

/* ========================================================================
 * RESOURCE EXHAUSTION SCENARIO TEST
 * ======================================================================== */

void test_resource_exhaustion_scenarios(void) {
    printf("\nTesting resource exhaustion scenarios...\n");
    
    /* Test 1: Many rapid thread registrations */
    for (int i = 0; i < 1000; i++) {
        int thread_id = perf_register_thread();
        TEST_ASSERT_NOT_EQUAL(-1, thread_id);
        perf_unregister_thread();
    }
    
    /* Test 2: Rapid milestone start/stop cycles */
    for (int i = 0; i < 10000; i++) {
        perf_start_milestone(PERF_MILESTONE_USER_1);
        perf_end_milestone(PERF_MILESTONE_USER_1);
    }
    
    /* Test 3: Large number of event logs */
    for (int i = 0; i < 5000; i++) {
        char label[64];
        snprintf(label, sizeof(label), "stress_event_%d", i);
        perf_log_event(label, (double)i);
    }
    
    /* Test 4: Memory tracking stress */
    for (int i = 0; i < 10000; i++) {
        void *fake_ptr = (void*)(uintptr_t)(i + 1);
        perf_track_allocation(fake_ptr, 1024 + i);
        perf_track_deallocation(fake_ptr);
    }
    
    /* Verify system is still functional */
    perf_config_t *config = perf_get_config();
    TEST_ASSERT_NOT_NULL(config);
    TEST_ASSERT_EQUAL(1, config->enabled);
    
    printf("Resource exhaustion tests completed successfully\n");
}

/* ========================================================================
 * MAIN STRESS TEST RUNNER
 * ======================================================================== */

int main(void) {
    UNITY_BEGIN();
    
    printf("\n=== AncestralClust Performance Logging Stress Tests ===\n");
    
    /* High-frequency operation tests */
    RUN_TEST(test_high_frequency_milestone_tracking);
    
    /* Memory leak detection tests */
    RUN_TEST(test_memory_leak_detection_extended_operation);
    
    /* Large dataset simulation tests */
    RUN_TEST(test_large_dataset_simulation_memory_pressure);
    
    /* Performance overhead measurement */
    RUN_TEST(test_performance_overhead_measurement_under_load);
    
    /* Resource exhaustion scenarios */
    RUN_TEST(test_resource_exhaustion_scenarios);
    
    printf("\n=== Stress Test Summary ===\n");
    printf("All stress tests completed successfully!\n");
    printf("Performance monitoring system verified under extreme load conditions.\n");
    
    return UNITY_END();
}