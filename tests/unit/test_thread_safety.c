/*
 * test_thread_safety.c
 * 
 * Comprehensive thread safety tests for the AncestralClust performance logging system
 * Tests concurrent initialization, milestone tracking, event logging, and thread management
 * 
 * These tests verify that the performance monitoring system is thread-safe under
 * concurrent access patterns typical in parallel clustering operations.
 */

#include "../unity/unity.h"
#include "../../performance.h"
#include "../utils/portable_barrier.h"
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdatomic.h>
#include <time.h>

/* Test configuration constants */
#define MAX_TEST_THREADS 32
#define OPERATIONS_PER_THREAD 1000
#define MILESTONE_ITERATIONS 100
#define THREAD_BARRIER_TIMEOUT_SEC 10

/* Thread synchronization structures */
typedef struct {
    pthread_barrier_t start_barrier;
    pthread_barrier_t end_barrier;
    volatile int should_stop;
    atomic_int error_count;
    atomic_int completed_threads;
    atomic_int operations_completed;
} thread_sync_t;

/* Thread test data structure */
typedef struct {
    int thread_id;
    int num_operations;
    perf_milestone_t milestone;
    thread_sync_t *sync;
    char label[64];
    double *results;
    int result_count;
} thread_test_data_t;

/* Global thread synchronization */
static thread_sync_t g_thread_sync;

/* Test fixture setup and teardown */
void setUp(void) {
    perf_cleanup();
    perf_init();
    
    /* Initialize thread synchronization */
    memset(&g_thread_sync, 0, sizeof(g_thread_sync));
    atomic_init(&g_thread_sync.error_count, 0);
    atomic_init(&g_thread_sync.completed_threads, 0);
    atomic_init(&g_thread_sync.operations_completed, 0);
}

void tearDown(void) {
    g_thread_sync.should_stop = 1;
    perf_cleanup();
}

/* Helper function to wait with timeout */
static int wait_for_condition(volatile int *condition, int expected_value, int timeout_sec) {
    struct timespec start, current;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    while (*condition != expected_value) {
        clock_gettime(CLOCK_MONOTONIC, &current);
        double elapsed = (current.tv_sec - start.tv_sec) + 
                        (current.tv_nsec - start.tv_nsec) / 1e9;
        
        if (elapsed > timeout_sec) {
            return 0; /* Timeout */
        }
        
        usleep(1000); /* Sleep 1ms */
    }
    return 1; /* Success */
}

/* ========================================================================
 * CONCURRENT INITIALIZATION AND CLEANUP TESTS
 * ======================================================================== */

static void* concurrent_init_thread(void *arg) {
    thread_test_data_t *data = (thread_test_data_t*)arg;
    
    /* Wait for all threads to be ready */
    pthread_barrier_wait(&data->sync->start_barrier);
    
    for (int i = 0; i < data->num_operations && !data->sync->should_stop; i++) {
        /* Concurrent initialization attempts */
        int init_result = perf_init();
        if (init_result != 0) {
            atomic_fetch_add(&data->sync->error_count, 1);
        }
        
        /* Brief operations between init/cleanup */
        perf_config_t *config = perf_get_config();
        if (config == NULL) {
            atomic_fetch_add(&data->sync->error_count, 1);
        }
        
        /* Concurrent cleanup attempts */
        perf_cleanup();
        
        atomic_fetch_add(&data->sync->operations_completed, 1);
    }
    
    atomic_fetch_add(&data->sync->completed_threads, 1);
    pthread_barrier_wait(&data->sync->end_barrier);
    return NULL;
}

void test_concurrent_initialization_cleanup(void) {
    const int num_threads = 8;
    pthread_t threads[num_threads];
    thread_test_data_t thread_data[num_threads];
    
    /* Initialize barriers */
    pthread_barrier_init(&g_thread_sync.start_barrier, NULL, num_threads);
    pthread_barrier_init(&g_thread_sync.end_barrier, NULL, num_threads);
    
    /* Create and start threads */
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].num_operations = 50;
        thread_data[i].sync = &g_thread_sync;
        
        int result = pthread_create(&threads[i], NULL, concurrent_init_thread, &thread_data[i]);
        TEST_ASSERT_EQUAL(0, result);
    }
    
    /* Wait for all threads to complete */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Verify no errors occurred */
    int total_errors = atomic_load(&g_thread_sync.error_count);
    TEST_ASSERT_EQUAL(0, total_errors);
    
    /* Verify all operations completed */
    int total_ops = atomic_load(&g_thread_sync.operations_completed);
    TEST_ASSERT_EQUAL(num_threads * 50, total_ops);
    
    /* Cleanup barriers */
    pthread_barrier_destroy(&g_thread_sync.start_barrier);
    pthread_barrier_destroy(&g_thread_sync.end_barrier);
    
    /* Reinitialize for subsequent tests */
    perf_init();
}

/* ========================================================================
 * CONCURRENT MILESTONE TRACKING TESTS
 * ======================================================================== */

static void* concurrent_milestone_thread(void *arg) {
    thread_test_data_t *data = (thread_test_data_t*)arg;
    
    /* Register thread for tracking */
    perf_register_thread();
    
    /* Wait for all threads to be ready */
    pthread_barrier_wait(&data->sync->start_barrier);
    
    for (int i = 0; i < data->num_operations && !data->sync->should_stop; i++) {
        char label[64];
        snprintf(label, sizeof(label), "thread_%d_op_%d", data->thread_id, i);
        
        /* Start milestone */
        perf_start_milestone_labeled(data->milestone, label);
        
        /* Simulate work with small delay */
        usleep(100 + (i % 500)); /* 0.1-0.6ms variable delay */
        
        /* End milestone */
        perf_end_milestone_labeled(data->milestone, label);
        
        atomic_fetch_add(&data->sync->operations_completed, 1);
    }
    
    /* Unregister thread */
    perf_unregister_thread();
    
    atomic_fetch_add(&data->sync->completed_threads, 1);
    pthread_barrier_wait(&data->sync->end_barrier);
    return NULL;
}

void test_concurrent_milestone_tracking(void) {
    const int num_threads = 16;
    pthread_t threads[num_threads];
    thread_test_data_t thread_data[num_threads];
    
    /* Initialize barriers */
    pthread_barrier_init(&g_thread_sync.start_barrier, NULL, num_threads);
    pthread_barrier_init(&g_thread_sync.end_barrier, NULL, num_threads);
    
    /* Create and start threads with different milestones */
    perf_milestone_t milestones[] = {
        PERF_MILESTONE_DISTANCE_CALCULATION,
        PERF_MILESTONE_CLUSTERING_ITERATION,
        PERF_MILESTONE_ALIGNMENT_START,
        PERF_MILESTONE_MEMORY_ALLOC
    };
    
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].num_operations = MILESTONE_ITERATIONS;
        thread_data[i].milestone = milestones[i % 4];
        thread_data[i].sync = &g_thread_sync;
        
        int result = pthread_create(&threads[i], NULL, concurrent_milestone_thread, &thread_data[i]);
        TEST_ASSERT_EQUAL(0, result);
    }
    
    /* Wait for all threads to complete */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Verify no errors and all operations completed */
    int total_errors = atomic_load(&g_thread_sync.error_count);
    TEST_ASSERT_EQUAL(0, total_errors);
    
    int completed_threads = atomic_load(&g_thread_sync.completed_threads);
    TEST_ASSERT_EQUAL(num_threads, completed_threads);
    
    int total_ops = atomic_load(&g_thread_sync.operations_completed);
    TEST_ASSERT_EQUAL(num_threads * MILESTONE_ITERATIONS, total_ops);
    
    /* Cleanup barriers */
    pthread_barrier_destroy(&g_thread_sync.start_barrier);
    pthread_barrier_destroy(&g_thread_sync.end_barrier);
}

/* ========================================================================
 * CONCURRENT EVENT LOGGING TESTS
 * ======================================================================== */

static void* concurrent_event_logging_thread(void *arg) {
    thread_test_data_t *data = (thread_test_data_t*)arg;
    
    /* Wait for all threads to be ready */
    pthread_barrier_wait(&data->sync->start_barrier);
    
    for (int i = 0; i < data->num_operations && !data->sync->should_stop; i++) {
        char label[64];
        char context[64];
        snprintf(label, sizeof(label), "event_%d_%d", data->thread_id, i);
        snprintf(context, sizeof(context), "thread_%d_context", data->thread_id);
        
        /* Log various types of events */
        double value = (double)(i * data->thread_id + 1);
        perf_log_event_with_context(label, value, context);
        
        /* Log iteration if applicable */
        if (i % 10 == 0) {
            perf_log_iteration(i, value / 100.0);
        }
        
        /* Log algorithm step */
        perf_log_algorithm_step("concurrent_test", "step", value);
        
        atomic_fetch_add(&data->sync->operations_completed, 1);
    }
    
    atomic_fetch_add(&data->sync->completed_threads, 1);
    pthread_barrier_wait(&data->sync->end_barrier);
    return NULL;
}

void test_concurrent_event_logging(void) {
    const int num_threads = 12;
    pthread_t threads[num_threads];
    thread_test_data_t thread_data[num_threads];
    
    /* Initialize barriers */
    pthread_barrier_init(&g_thread_sync.start_barrier, NULL, num_threads);
    pthread_barrier_init(&g_thread_sync.end_barrier, NULL, num_threads);
    
    /* Create and start threads */
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].num_operations = 200;
        thread_data[i].sync = &g_thread_sync;
        
        int result = pthread_create(&threads[i], NULL, concurrent_event_logging_thread, &thread_data[i]);
        TEST_ASSERT_EQUAL(0, result);
    }
    
    /* Wait for all threads to complete */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Verify no errors and all operations completed */
    int total_errors = atomic_load(&g_thread_sync.error_count);
    TEST_ASSERT_EQUAL(0, total_errors);
    
    int completed_threads = atomic_load(&g_thread_sync.completed_threads);
    TEST_ASSERT_EQUAL(num_threads, completed_threads);
    
    int total_ops = atomic_load(&g_thread_sync.operations_completed);
    TEST_ASSERT_EQUAL(num_threads * 200, total_ops);
    
    /* Cleanup barriers */
    pthread_barrier_destroy(&g_thread_sync.start_barrier);
    pthread_barrier_destroy(&g_thread_sync.end_barrier);
}

/* ========================================================================
 * THREAD-LOCAL STORAGE VALIDATION TESTS
 * ======================================================================== */

static void* thread_local_validation_thread(void *arg) {
    thread_test_data_t *data = (thread_test_data_t*)arg;
    
    /* Register thread and get ID */
    int thread_id = perf_register_thread();
    
    /* Wait for all threads to be ready */
    pthread_barrier_wait(&data->sync->start_barrier);
    
    /* Verify thread ID consistency */
    for (int i = 0; i < 100 && !data->sync->should_stop; i++) {
        perf_thread_data_t *thread_data = perf_get_thread_data(thread_id);
        
        if (thread_data == NULL) {
            atomic_fetch_add(&data->sync->error_count, 1);
            break;
        }
        
        /* Verify thread ID remains consistent */
        if (thread_data->thread_id != thread_id) {
            atomic_fetch_add(&data->sync->error_count, 1);
        }
        
        /* Update thread-specific data */
        thread_data->operations_count++;
        snprintf(thread_data->label, sizeof(thread_data->label), 
                "thread_%d_op_%d", data->thread_id, i);
        
        usleep(10); /* Small delay to increase chance of race conditions */
    }
    
    /* Unregister thread */
    perf_unregister_thread();
    
    atomic_fetch_add(&data->sync->completed_threads, 1);
    pthread_barrier_wait(&data->sync->end_barrier);
    return NULL;
}

void test_thread_local_storage_validation(void) {
    const int num_threads = 20;
    pthread_t threads[num_threads];
    thread_test_data_t thread_data[num_threads];
    
    /* Initialize barriers */
    pthread_barrier_init(&g_thread_sync.start_barrier, NULL, num_threads);
    pthread_barrier_init(&g_thread_sync.end_barrier, NULL, num_threads);
    
    /* Create and start threads */
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].sync = &g_thread_sync;
        
        int result = pthread_create(&threads[i], NULL, thread_local_validation_thread, &thread_data[i]);
        TEST_ASSERT_EQUAL(0, result);
    }
    
    /* Wait for all threads to complete */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Verify no errors occurred */
    int total_errors = atomic_load(&g_thread_sync.error_count);
    TEST_ASSERT_EQUAL(0, total_errors);
    
    int completed_threads = atomic_load(&g_thread_sync.completed_threads);
    TEST_ASSERT_EQUAL(num_threads, completed_threads);
    
    /* Cleanup barriers */
    pthread_barrier_destroy(&g_thread_sync.start_barrier);
    pthread_barrier_destroy(&g_thread_sync.end_barrier);
}

/* ========================================================================
 * ATOMIC COUNTER VERIFICATION TESTS
 * ======================================================================== */

static void* atomic_counter_thread(void *arg) {
    thread_test_data_t *data = (thread_test_data_t*)arg;
    
    /* Wait for all threads to be ready */
    pthread_barrier_wait(&data->sync->start_barrier);
    
    for (int i = 0; i < data->num_operations && !data->sync->should_stop; i++) {
        /* Simulate memory allocations */
        size_t alloc_size = 1024 + (i % 4096);
        void *fake_ptr = (void*)(uintptr_t)(data->thread_id * 1000000 + i);
        
        perf_track_allocation(fake_ptr, alloc_size);
        
        /* Small delay to increase contention */
        if (i % 100 == 0) {
            usleep(1);
        }
        
        /* Simulate deallocation */
        perf_track_deallocation(fake_ptr);
        
        atomic_fetch_add(&data->sync->operations_completed, 1);
    }
    
    atomic_fetch_add(&data->sync->completed_threads, 1);
    pthread_barrier_wait(&data->sync->end_barrier);
    return NULL;
}

void test_atomic_counter_verification(void) {
    const int num_threads = 16;
    const int ops_per_thread = 500;
    pthread_t threads[num_threads];
    thread_test_data_t thread_data[num_threads];
    
    /* Initialize barriers */
    pthread_barrier_init(&g_thread_sync.start_barrier, NULL, num_threads);
    pthread_barrier_init(&g_thread_sync.end_barrier, NULL, num_threads);
    
    /* Create and start threads */
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].num_operations = ops_per_thread;
        thread_data[i].sync = &g_thread_sync;
        
        int result = pthread_create(&threads[i], NULL, atomic_counter_thread, &thread_data[i]);
        TEST_ASSERT_EQUAL(0, result);
    }
    
    /* Wait for all threads to complete */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Verify operations completed correctly */
    int total_ops = atomic_load(&g_thread_sync.operations_completed);
    TEST_ASSERT_EQUAL(num_threads * ops_per_thread, total_ops);
    
    int completed_threads = atomic_load(&g_thread_sync.completed_threads);
    TEST_ASSERT_EQUAL(num_threads, completed_threads);
    
    /* Cleanup barriers */
    pthread_barrier_destroy(&g_thread_sync.start_barrier);
    pthread_barrier_destroy(&g_thread_sync.end_barrier);
}

/* ========================================================================
 * MIXED WORKLOAD STRESS TEST
 * ======================================================================== */

static void* mixed_workload_thread(void *arg) {
    thread_test_data_t *data = (thread_test_data_t*)arg;
    
    /* Register thread */
    perf_register_thread();
    
    /* Wait for all threads to be ready */
    pthread_barrier_wait(&data->sync->start_barrier);
    
    for (int i = 0; i < data->num_operations && !data->sync->should_stop; i++) {
        char label[64];
        snprintf(label, sizeof(label), "mixed_op_%d_%d", data->thread_id, i);
        
        /* Mixed operations simulating real clustering workload */
        switch (i % 4) {
            case 0:
                /* Distance calculation simulation */
                perf_start_milestone_labeled(PERF_MILESTONE_DISTANCE_CALCULATION, label);
                usleep(50);
                perf_log_event("distance_value", (double)(i * 0.1));
                perf_end_milestone_labeled(PERF_MILESTONE_DISTANCE_CALCULATION, label);
                break;
                
            case 1:
                /* Clustering iteration simulation */
                perf_start_milestone_labeled(PERF_MILESTONE_CLUSTERING_ITERATION, label);
                perf_log_iteration(i / 4, 0.95 - (i * 0.001));
                usleep(75);
                perf_end_milestone_labeled(PERF_MILESTONE_CLUSTERING_ITERATION, label);
                break;
                
            case 2:
                /* Memory allocation simulation */
                perf_start_milestone_labeled(PERF_MILESTONE_MEMORY_ALLOC, label);
                void *fake_ptr = (void*)(uintptr_t)(data->thread_id * 10000 + i);
                perf_track_allocation(fake_ptr, 2048);
                usleep(25);
                perf_track_deallocation(fake_ptr);
                perf_end_milestone_labeled(PERF_MILESTONE_MEMORY_ALLOC, label);
                break;
                
            case 3:
                /* Alignment simulation */
                perf_start_milestone_labeled(PERF_MILESTONE_SEQUENCE_ALIGNMENT, label);
                perf_log_algorithm_step("alignment", "score_calc", (double)(i * 2.5));
                usleep(100);
                perf_end_milestone_labeled(PERF_MILESTONE_SEQUENCE_ALIGNMENT, label);
                break;
        }
        
        atomic_fetch_add(&data->sync->operations_completed, 1);
    }
    
    /* Unregister thread */
    perf_unregister_thread();
    
    atomic_fetch_add(&data->sync->completed_threads, 1);
    pthread_barrier_wait(&data->sync->end_barrier);
    return NULL;
}

void test_mixed_workload_thread_safety(void) {
    const int num_threads = 24;
    const int ops_per_thread = 400;
    pthread_t threads[num_threads];
    thread_test_data_t thread_data[num_threads];
    
    /* Initialize barriers */
    pthread_barrier_init(&g_thread_sync.start_barrier, NULL, num_threads);
    pthread_barrier_init(&g_thread_sync.end_barrier, NULL, num_threads);
    
    /* Record start time */
    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    /* Create and start threads */
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].num_operations = ops_per_thread;
        thread_data[i].sync = &g_thread_sync;
        
        int result = pthread_create(&threads[i], NULL, mixed_workload_thread, &thread_data[i]);
        TEST_ASSERT_EQUAL(0, result);
    }
    
    /* Wait for all threads to complete */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Record end time and calculate duration */
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double duration_ms = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                        (end_time.tv_nsec - start_time.tv_nsec) / 1e6;
    
    /* Verify all operations completed successfully */
    int total_ops = atomic_load(&g_thread_sync.operations_completed);
    TEST_ASSERT_EQUAL(num_threads * ops_per_thread, total_ops);
    
    int completed_threads = atomic_load(&g_thread_sync.completed_threads);
    TEST_ASSERT_EQUAL(num_threads, completed_threads);
    
    /* Performance verification: should complete within reasonable time */
    /* With 24 threads * 400 ops = 9600 total operations */
    /* Should complete in under 10 seconds even on slow systems */
    TEST_ASSERT_LESS_THAN(10000.0, duration_ms);
    
    printf("\nMixed workload test: %d threads, %d ops each, %.2f ms total\n", 
           num_threads, ops_per_thread, duration_ms);
    
    /* Cleanup barriers */
    pthread_barrier_destroy(&g_thread_sync.start_barrier);
    pthread_barrier_destroy(&g_thread_sync.end_barrier);
}

/* ========================================================================
 * MAIN TEST RUNNER
 * ======================================================================== */

int main(void) {
    UNITY_BEGIN();
    
    /* Thread safety tests */
    RUN_TEST(test_concurrent_initialization_cleanup);
    RUN_TEST(test_concurrent_milestone_tracking);
    RUN_TEST(test_concurrent_event_logging);
    RUN_TEST(test_thread_local_storage_validation);
    RUN_TEST(test_atomic_counter_verification);
    RUN_TEST(test_mixed_workload_thread_safety);
    
    return UNITY_END();
}