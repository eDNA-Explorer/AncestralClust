/*
 * thread_test_helpers.h
 * 
 * Utility functions and helpers for thread safety testing in the AncestralClust
 * performance logging system. Provides synchronization primitives, performance
 * measurement utilities, and thread safety validation functions.
 * 
 * These helpers simplify the creation of deterministic, repeatable thread safety
 * tests while providing comprehensive validation of concurrent behavior.
 */

#ifndef THREAD_TEST_HELPERS_H
#define THREAD_TEST_HELPERS_H

#include "portable_barrier.h"
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <stdatomic.h>

/* Maximum number of threads supported in tests */
#define THREAD_TEST_MAX_THREADS 256
#define THREAD_TEST_MAX_BARRIERS 32
#define THREAD_TEST_MAX_MEASUREMENTS 10000

/* Thread test result codes */
typedef enum {
    THREAD_TEST_SUCCESS = 0,
    THREAD_TEST_TIMEOUT,
    THREAD_TEST_THREAD_CREATION_FAILED,
    THREAD_TEST_SYNCHRONIZATION_FAILED,
    THREAD_TEST_VALIDATION_FAILED,
    THREAD_TEST_PERFORMANCE_FAILED
} thread_test_result_t;

/* Thread execution phases */
typedef enum {
    THREAD_PHASE_INIT = 0,
    THREAD_PHASE_SETUP,
    THREAD_PHASE_EXECUTION,
    THREAD_PHASE_CLEANUP,
    THREAD_PHASE_COMPLETE
} thread_phase_t;

/* Thread synchronization structure */
typedef struct {
    pthread_barrier_t start_barrier;
    pthread_barrier_t phase_barriers[4];
    pthread_barrier_t end_barrier;
    pthread_mutex_t data_mutex;
    volatile bool should_stop;
    volatile int active_threads;
    atomic_int operations_completed;
    atomic_int errors_detected;
    struct timespec test_start_time;
    struct timespec test_end_time;
    int total_threads;
    bool barriers_initialized;
} thread_test_sync_t;

/* Thread-specific test data */
typedef struct {
    int thread_id;
    int thread_index;
    void *test_data;
    void *results;
    thread_test_sync_t *sync;
    struct timespec start_time;
    struct timespec end_time;
    int operations_count;
    int errors_count;
    thread_phase_t current_phase;
} thread_test_context_t;

/* Performance measurement structure */
typedef struct {
    double operation_times[THREAD_TEST_MAX_MEASUREMENTS];
    int measurement_count;
    double min_time_ms;
    double max_time_ms;
    double avg_time_ms;
    double std_dev_ms;
    double percentile_95_ms;
    double percentile_99_ms;
    uint64_t total_operations;
    uint64_t operations_per_second;
} thread_performance_stats_t;

/* Thread safety validation results */
typedef struct {
    bool data_integrity_valid;
    bool memory_consistency_valid;
    bool atomic_operations_valid;
    bool no_race_conditions;
    bool no_deadlocks;
    bool performance_acceptable;
    double max_acceptable_overhead_percent;
    double measured_overhead_percent;
    char validation_details[256];
} thread_safety_validation_t;

/* Thread test configuration */
typedef struct {
    int num_threads;
    int operations_per_thread;
    int timeout_seconds;
    bool enable_performance_measurement;
    bool enable_detailed_validation;
    bool randomize_execution_order;
    double max_acceptable_overhead_percent;
    void *custom_config;
} thread_test_config_t;

/* Function pointer types for thread test callbacks */
typedef void* (*thread_test_function_t)(void *context);
typedef bool (*thread_validation_function_t)(thread_test_context_t *contexts, int num_threads);
typedef void (*thread_cleanup_function_t)(thread_test_context_t *context);

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * THREAD TEST FRAMEWORK FUNCTIONS
 * ======================================================================== */

/* Initialize and cleanup thread test framework */
thread_test_result_t thread_test_init(void);
void thread_test_cleanup(void);

/* Create and configure thread test synchronization */
thread_test_sync_t* thread_test_create_sync(int num_threads);
void thread_test_destroy_sync(thread_test_sync_t *sync);
thread_test_result_t thread_test_reset_sync(thread_test_sync_t *sync);

/* Thread test execution framework */
thread_test_result_t thread_test_run(
    thread_test_function_t test_function,
    thread_test_config_t *config,
    thread_validation_function_t validator,
    thread_cleanup_function_t cleanup
);

/* Advanced thread test execution with custom setup */
thread_test_result_t thread_test_run_advanced(
    thread_test_function_t test_function,
    thread_test_config_t *config,
    thread_validation_function_t validator,
    thread_cleanup_function_t cleanup,
    void *setup_data,
    size_t context_data_size
);

/* ========================================================================
 * SYNCHRONIZATION HELPER FUNCTIONS
 * ======================================================================== */

/* Barrier synchronization helpers */
thread_test_result_t thread_test_wait_start_barrier(thread_test_context_t *context);
thread_test_result_t thread_test_wait_phase_barrier(thread_test_context_t *context, thread_phase_t phase);
thread_test_result_t thread_test_wait_end_barrier(thread_test_context_t *context);

/* Advanced synchronization patterns */
thread_test_result_t thread_test_staggered_start(thread_test_context_t *context, int delay_ms);
thread_test_result_t thread_test_coordinated_stop(thread_test_sync_t *sync);
bool thread_test_wait_for_condition(volatile bool *condition, bool expected_value, int timeout_ms);
bool thread_test_wait_for_counter(atomic_int *counter, int expected_value, int timeout_ms);

/* Mutex and lock helpers */
thread_test_result_t thread_test_safe_lock(pthread_mutex_t *mutex, int timeout_ms);
thread_test_result_t thread_test_safe_unlock(pthread_mutex_t *mutex);

/* ========================================================================
 * PERFORMANCE MEASUREMENT FUNCTIONS
 * ======================================================================== */

/* Performance timing functions */
void thread_test_start_timing(thread_test_context_t *context);
void thread_test_end_timing(thread_test_context_t *context);
double thread_test_get_elapsed_ms(thread_test_context_t *context);

/* Performance statistics calculation */
thread_test_result_t thread_test_calculate_performance_stats(
    thread_test_context_t *contexts,
    int num_threads,
    thread_performance_stats_t *stats
);

/* Overhead measurement */
double thread_test_measure_overhead(
    thread_test_function_t baseline_function,
    thread_test_function_t test_function,
    thread_test_config_t *config
);

/* Performance validation */
bool thread_test_validate_performance(
    thread_performance_stats_t *stats,
    double max_overhead_percent,
    uint64_t min_ops_per_second
);

/* ========================================================================
 * THREAD SAFETY VALIDATION FUNCTIONS
 * ======================================================================== */

/* Memory consistency validation */
bool thread_test_validate_memory_consistency(
    thread_test_context_t *contexts,
    int num_threads,
    void *shared_data,
    size_t data_size
);

/* Atomic operations validation */
bool thread_test_validate_atomic_operations(
    atomic_int *counters,
    int num_counters,
    int expected_total
);

/* Race condition detection */
bool thread_test_detect_race_conditions(
    thread_test_context_t *contexts,
    int num_threads,
    const char *test_description
);

/* Deadlock detection */
bool thread_test_detect_deadlocks(thread_test_sync_t *sync);

/* Comprehensive thread safety validation */
thread_test_result_t thread_test_comprehensive_validation(
    thread_test_context_t *contexts,
    int num_threads,
    thread_safety_validation_t *validation_result
);

/* ========================================================================
 * UTILITY HELPER FUNCTIONS
 * ======================================================================== */

/* Thread creation and management */
thread_test_result_t thread_test_create_threads(
    pthread_t *threads,
    thread_test_context_t *contexts,
    thread_test_function_t test_function,
    thread_test_config_t *config,
    thread_test_sync_t *sync
);

thread_test_result_t thread_test_join_threads(
    pthread_t *threads,
    int num_threads,
    int timeout_seconds
);

/* Test data generation and validation */
void* thread_test_generate_test_data(size_t size, int pattern);
bool thread_test_validate_test_data(void *data, size_t size, int expected_pattern);

/* Random number generation for testing */
void thread_test_seed_random(uint32_t seed);
uint32_t thread_test_random(void);
uint32_t thread_test_random_range(uint32_t min, uint32_t max);

/* Thread affinity and priority helpers */
thread_test_result_t thread_test_set_thread_affinity(int cpu_id);
thread_test_result_t thread_test_set_thread_priority(int priority);

/* Memory allocation tracking for leak detection */
typedef struct {
    void *ptr;
    size_t size;
    struct timespec alloc_time;
    int thread_id;
} thread_test_allocation_t;

void thread_test_start_allocation_tracking(void);
void thread_test_stop_allocation_tracking(void);
void* thread_test_tracked_malloc(size_t size);
void thread_test_tracked_free(void *ptr);
bool thread_test_check_memory_leaks(void);
void thread_test_print_allocation_report(void);

/* ========================================================================
 * ERROR HANDLING AND REPORTING
 * ======================================================================== */

/* Error reporting functions */
void thread_test_report_error(thread_test_context_t *context, const char *error_msg);
void thread_test_report_warning(thread_test_context_t *context, const char *warning_msg);
const char* thread_test_result_to_string(thread_test_result_t result);

/* Test result analysis */
void thread_test_print_summary(
    thread_performance_stats_t *stats,
    thread_safety_validation_t *validation,
    thread_test_config_t *config
);

void thread_test_print_detailed_report(
    thread_test_context_t *contexts,
    int num_threads,
    thread_performance_stats_t *stats
);

/* ========================================================================
 * PREDEFINED TEST PATTERNS
 * ======================================================================== */

/* Common test patterns for performance logging validation */
thread_test_result_t thread_test_concurrent_initialization(int num_threads);
thread_test_result_t thread_test_concurrent_milestone_tracking(int num_threads, int operations_per_thread);
thread_test_result_t thread_test_concurrent_event_logging(int num_threads, int events_per_thread);
thread_test_result_t thread_test_mixed_workload_simulation(int num_threads, int duration_seconds);
thread_test_result_t thread_test_stress_memory_allocation(int num_threads, int allocations_per_thread);

/* High-level test suites */
thread_test_result_t thread_test_run_comprehensive_suite(void);
thread_test_result_t thread_test_run_performance_suite(void);
thread_test_result_t thread_test_run_stress_suite(void);

/* ========================================================================
 * CONFIGURATION HELPERS
 * ======================================================================== */

/* Create default test configurations */
thread_test_config_t thread_test_create_default_config(void);
thread_test_config_t thread_test_create_stress_config(void);
thread_test_config_t thread_test_create_performance_config(void);

/* Configuration validation */
bool thread_test_validate_config(thread_test_config_t *config);
void thread_test_adjust_config_for_system(thread_test_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* THREAD_TEST_HELPERS_H */