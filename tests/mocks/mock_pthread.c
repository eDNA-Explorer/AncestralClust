/*
 * mock_pthread.c
 * 
 * Implementation of the mock pthread framework for controlled threading tests
 * in the AncestralClust performance logging system.
 * 
 * This implementation provides controllable pthread function behavior for testing
 * thread creation failures, synchronization edge cases, and performance analysis.
 */

#include "../utils/portable_barrier.h"
#include "mock_pthread.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <assert.h>

/* ========================================================================
 * GLOBAL MOCK CONTROL VARIABLES
 * ======================================================================== */

bool mock_pthread_enabled = false;
bool mock_pthread_create_should_fail = false;
bool mock_pthread_mutex_lock_should_fail = false;
bool mock_pthread_mutex_unlock_should_fail = false;
bool mock_pthread_barrier_should_fail = false;
int mock_pthread_create_failure_count = 0;
int mock_pthread_mutex_failure_count = 0;

/* Mock statistics */
mock_pthread_stats_t mock_pthread_stats = {0};

/* Internal data structures */
static mock_thread_data_t *mock_threads = NULL;
static mock_mutex_data_t *mock_mutexes = NULL;
static mock_barrier_data_t *mock_barriers = NULL;
static pthread_mutex_t mock_internal_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Thread-local storage simulation */
#define MAX_TLS_KEYS 256
static void *tls_data[MAX_TLS_KEYS];
static void (*tls_destructors[MAX_TLS_KEYS])(void*);
static bool tls_key_used[MAX_TLS_KEYS];
static int next_tls_key = 0;

/* Performance measurement data */
static mock_pthread_perf_stats_t perf_stats = {0};
static bool force_contention = false;
static bool simulate_resource_exhaustion = false;

/* ========================================================================
 * INTERNAL HELPER FUNCTIONS
 * ======================================================================== */

static void lock_mock_internal(void) {
    pthread_mutex_lock(&mock_internal_mutex);
}

static void unlock_mock_internal(void) {
    pthread_mutex_unlock(&mock_internal_mutex);
}

static mock_thread_data_t* find_mock_thread(pthread_t thread) {
    mock_thread_data_t *current = mock_threads;
    while (current) {
        if (pthread_equal(current->original_thread, thread)) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static mock_mutex_data_t* find_mock_mutex(pthread_mutex_t *mutex) {
    mock_mutex_data_t *current = mock_mutexes;
    while (current) {
        if (&current->original_mutex == mutex) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static mock_barrier_data_t* find_mock_barrier(pthread_barrier_t *barrier) {
    mock_barrier_data_t *current = mock_barriers;
    while (current) {
        if (&current->original_barrier == barrier) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static double get_time_diff_ms(struct timespec *start, struct timespec *end) {
    return (end->tv_sec - start->tv_sec) * 1000.0 + 
           (end->tv_nsec - start->tv_nsec) / 1e6;
}

/* ========================================================================
 * MOCK FRAMEWORK CONTROL FUNCTIONS
 * ======================================================================== */

void mock_pthread_init(void) {
    lock_mock_internal();
    
    mock_pthread_enabled = false;
    mock_pthread_create_should_fail = false;
    mock_pthread_mutex_lock_should_fail = false;
    mock_pthread_mutex_unlock_should_fail = false;
    mock_pthread_barrier_should_fail = false;
    mock_pthread_create_failure_count = 0;
    mock_pthread_mutex_failure_count = 0;
    
    memset(&mock_pthread_stats, 0, sizeof(mock_pthread_stats));
    memset(&perf_stats, 0, sizeof(perf_stats));
    memset(tls_data, 0, sizeof(tls_data));
    memset(tls_destructors, 0, sizeof(tls_destructors));
    memset(tls_key_used, 0, sizeof(tls_key_used));
    next_tls_key = 0;
    
    force_contention = false;
    simulate_resource_exhaustion = false;
    
    unlock_mock_internal();
}

void mock_pthread_cleanup(void) {
    lock_mock_internal();
    
    /* Clean up thread data */
    while (mock_threads) {
        mock_thread_data_t *next = mock_threads->next;
        free(mock_threads);
        mock_threads = next;
    }
    
    /* Clean up mutex data */
    while (mock_mutexes) {
        mock_mutex_data_t *next = mock_mutexes->next;
        free(mock_mutexes);
        mock_mutexes = next;
    }
    
    /* Clean up barrier data */
    while (mock_barriers) {
        mock_barrier_data_t *next = mock_barriers->next;
        free(mock_barriers);
        mock_barriers = next;
    }
    
    /* Clean up TLS data */
    for (int i = 0; i < MAX_TLS_KEYS; i++) {
        if (tls_key_used[i] && tls_destructors[i] && tls_data[i]) {
            tls_destructors[i](tls_data[i]);
        }
        tls_data[i] = NULL;
        tls_destructors[i] = NULL;
        tls_key_used[i] = false;
    }
    
    mock_pthread_enabled = false;
    
    unlock_mock_internal();
}

void mock_pthread_reset_stats(void) {
    lock_mock_internal();
    memset(&mock_pthread_stats, 0, sizeof(mock_pthread_stats));
    memset(&perf_stats, 0, sizeof(perf_stats));
    unlock_mock_internal();
}

void mock_pthread_enable(bool enable) {
    lock_mock_internal();
    mock_pthread_enabled = enable;
    unlock_mock_internal();
}

void mock_pthread_set_create_failure(bool should_fail, int failure_count) {
    lock_mock_internal();
    mock_pthread_create_should_fail = should_fail;
    mock_pthread_create_failure_count = failure_count;
    unlock_mock_internal();
}

void mock_pthread_set_mutex_failure(bool should_fail, int failure_count) {
    lock_mock_internal();
    mock_pthread_mutex_lock_should_fail = should_fail;
    mock_pthread_mutex_unlock_should_fail = should_fail;
    mock_pthread_mutex_failure_count = failure_count;
    unlock_mock_internal();
}

void mock_pthread_set_barrier_failure(bool should_fail) {
    lock_mock_internal();
    mock_pthread_barrier_should_fail = should_fail;
    unlock_mock_internal();
}

mock_pthread_stats_t* mock_pthread_get_stats(void) {
    return &mock_pthread_stats;
}

/* ========================================================================
 * MOCK THREAD MANAGEMENT FUNCTIONS
 * ======================================================================== */

int mock_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                       void *(*start_routine)(void*), void *arg) {
    lock_mock_internal();
    mock_pthread_stats.pthread_create_calls++;
    
    /* Simulate creation failure if configured */
    if (mock_pthread_create_should_fail) {
        if (mock_pthread_create_failure_count > 0) {
            mock_pthread_create_failure_count--;
            mock_pthread_stats.failed_operations++;
            unlock_mock_internal();
            return EAGAIN; /* Resource temporarily unavailable */
        }
    }
    
    /* Simulate resource exhaustion */
    if (simulate_resource_exhaustion) {
        mock_pthread_stats.failed_operations++;
        unlock_mock_internal();
        return ENOMEM; /* Out of memory */
    }
    
    /* Create actual thread */
    int result = pthread_create(thread, attr, start_routine, arg);
    
    if (result == 0) {
        /* Track thread data */
        mock_thread_data_t *thread_data = malloc(sizeof(mock_thread_data_t));
        if (thread_data) {
            thread_data->original_thread = *thread;
            thread_data->start_routine = start_routine;
            thread_data->arg = arg;
            thread_data->return_value = NULL;
            thread_data->is_joined = false;
            thread_data->is_cancelled = false;
            thread_data->next = mock_threads;
            mock_threads = thread_data;
        }
    } else {
        mock_pthread_stats.failed_operations++;
    }
    
    unlock_mock_internal();
    return result;
}

int mock_pthread_join(pthread_t thread, void **retval) {
    lock_mock_internal();
    mock_pthread_stats.pthread_join_calls++;
    
    mock_thread_data_t *thread_data = find_mock_thread(thread);
    if (thread_data) {
        thread_data->is_joined = true;
    }
    
    unlock_mock_internal();
    
    /* Perform actual join */
    return pthread_join(thread, retval);
}

int mock_pthread_cancel(pthread_t thread) {
    lock_mock_internal();
    
    mock_thread_data_t *thread_data = find_mock_thread(thread);
    if (thread_data) {
        thread_data->is_cancelled = true;
    }
    
    unlock_mock_internal();
    
    return pthread_cancel(thread);
}

void mock_pthread_exit(void *retval) {
    lock_mock_internal();
    
    pthread_t self = pthread_self();
    mock_thread_data_t *thread_data = find_mock_thread(self);
    if (thread_data) {
        thread_data->return_value = retval;
    }
    
    unlock_mock_internal();
    
    pthread_exit(retval);
}

pthread_t mock_pthread_self(void) {
    return pthread_self();
}

int mock_pthread_equal(pthread_t t1, pthread_t t2) {
    return pthread_equal(t1, t2);
}

/* ========================================================================
 * MOCK MUTEX FUNCTIONS
 * ======================================================================== */

int mock_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    lock_mock_internal();
    mock_pthread_stats.pthread_mutex_init_calls++;
    
    int result = pthread_mutex_init(mutex, attr);
    
    if (result == 0) {
        /* Track mutex data */
        mock_mutex_data_t *mutex_data = malloc(sizeof(mock_mutex_data_t));
        if (mutex_data) {
            mutex_data->original_mutex = *mutex;
            mutex_data->is_locked = false;
            mutex_data->owner_thread = 0;
            mutex_data->lock_count = 0;
            memset(&mutex_data->lock_time, 0, sizeof(mutex_data->lock_time));
            mutex_data->next = mock_mutexes;
            mock_mutexes = mutex_data;
        }
    }
    
    unlock_mock_internal();
    return result;
}

int mock_pthread_mutex_destroy(pthread_mutex_t *mutex) {
    lock_mock_internal();
    mock_pthread_stats.pthread_mutex_destroy_calls++;
    
    /* Remove from tracking */
    mock_mutex_data_t **current = &mock_mutexes;
    while (*current) {
        if (&(*current)->original_mutex == mutex) {
            mock_mutex_data_t *to_remove = *current;
            *current = (*current)->next;
            free(to_remove);
            break;
        }
        current = &(*current)->next;
    }
    
    unlock_mock_internal();
    
    return pthread_mutex_destroy(mutex);
}

int mock_pthread_mutex_lock(pthread_mutex_t *mutex) {
    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    lock_mock_internal();
    mock_pthread_stats.pthread_mutex_lock_calls++;
    
    /* Simulate lock failure if configured */
    if (mock_pthread_mutex_lock_should_fail) {
        if (mock_pthread_mutex_failure_count > 0) {
            mock_pthread_mutex_failure_count--;
            mock_pthread_stats.failed_operations++;
            perf_stats.failed_locks++;
            unlock_mock_internal();
            return EINVAL; /* Invalid mutex */
        }
    }
    
    mock_mutex_data_t *mutex_data = find_mock_mutex(mutex);
    unlock_mock_internal();
    
    /* Simulate contention if configured */
    if (force_contention) {
        usleep(1000); /* 1ms delay to simulate contention */
        perf_stats.total_contentions++;
    }
    
    /* Perform actual lock */
    int result = pthread_mutex_lock(mutex);
    
    lock_mock_internal();
    if (result == 0) {
        if (mutex_data) {
            mutex_data->is_locked = true;
            mutex_data->owner_thread = pthread_self();
            mutex_data->lock_count++;
            mutex_data->lock_time = start_time;
        }
        perf_stats.successful_locks++;
        
        /* Update performance statistics */
        struct timespec end_time;
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        double lock_time_ms = get_time_diff_ms(&start_time, &end_time);
        
        if (lock_time_ms > perf_stats.max_lock_time_ms) {
            perf_stats.max_lock_time_ms = lock_time_ms;
        }
        
        perf_stats.avg_lock_time_ms = 
            (perf_stats.avg_lock_time_ms * (perf_stats.successful_locks - 1) + lock_time_ms) /
            perf_stats.successful_locks;
    } else {
        mock_pthread_stats.failed_operations++;
        perf_stats.failed_locks++;
    }
    unlock_mock_internal();
    
    return result;
}

int mock_pthread_mutex_trylock(pthread_mutex_t *mutex) {
    lock_mock_internal();
    
    mock_mutex_data_t *mutex_data = find_mock_mutex(mutex);
    
    unlock_mock_internal();
    
    int result = pthread_mutex_trylock(mutex);
    
    lock_mock_internal();
    if (result == 0 && mutex_data) {
        mutex_data->is_locked = true;
        mutex_data->owner_thread = pthread_self();
        mutex_data->lock_count++;
        clock_gettime(CLOCK_MONOTONIC, &mutex_data->lock_time);
    }
    unlock_mock_internal();
    
    return result;
}

int mock_pthread_mutex_unlock(pthread_mutex_t *mutex) {
    lock_mock_internal();
    mock_pthread_stats.pthread_mutex_unlock_calls++;
    
    /* Simulate unlock failure if configured */
    if (mock_pthread_mutex_unlock_should_fail) {
        if (mock_pthread_mutex_failure_count > 0) {
            mock_pthread_mutex_failure_count--;
            mock_pthread_stats.failed_operations++;
            unlock_mock_internal();
            return EPERM; /* Operation not permitted */
        }
    }
    
    mock_mutex_data_t *mutex_data = find_mock_mutex(mutex);
    if (mutex_data) {
        mutex_data->is_locked = false;
        mutex_data->owner_thread = 0;
    }
    
    unlock_mock_internal();
    
    return pthread_mutex_unlock(mutex);
}

/* ========================================================================
 * MOCK BARRIER FUNCTIONS
 * ======================================================================== */

int mock_pthread_barrier_init(pthread_barrier_t *barrier, 
                             const pthread_barrierattr_t *attr, unsigned count) {
    lock_mock_internal();
    mock_pthread_stats.pthread_barrier_init_calls++;
    
    if (mock_pthread_barrier_should_fail) {
        mock_pthread_stats.failed_operations++;
        unlock_mock_internal();
        return EINVAL;
    }
    
    /* Use the appropriate barrier implementation based on platform */
#ifdef __APPLE__
    int result = portable_barrier_init(barrier, attr, count);
#else
    int result = pthread_barrier_init(barrier, attr, count);
#endif
    
    if (result == 0) {
        /* Track barrier data */
        mock_barrier_data_t *barrier_data = malloc(sizeof(mock_barrier_data_t));
        if (barrier_data) {
            barrier_data->original_barrier = *barrier;
            barrier_data->count = count;
            barrier_data->waiting_threads = 0;
            barrier_data->is_destroyed = false;
            barrier_data->next = mock_barriers;
            mock_barriers = barrier_data;
        }
    }
    
    unlock_mock_internal();
    return result;
}

int mock_pthread_barrier_destroy(pthread_barrier_t *barrier) {
    lock_mock_internal();
    mock_pthread_stats.pthread_barrier_destroy_calls++;
    
    /* Remove from tracking */
    mock_barrier_data_t **current = &mock_barriers;
    while (*current) {
        if (&(*current)->original_barrier == barrier) {
            mock_barrier_data_t *to_remove = *current;
            to_remove->is_destroyed = true;
            *current = (*current)->next;
            free(to_remove);
            break;
        }
        current = &(*current)->next;
    }
    
    unlock_mock_internal();
    
    /* Use the appropriate barrier implementation based on platform */
#ifdef __APPLE__
    return portable_barrier_destroy(barrier);
#else
    return pthread_barrier_destroy(barrier);
#endif
}

int mock_pthread_barrier_wait(pthread_barrier_t *barrier) {
    lock_mock_internal();
    mock_pthread_stats.pthread_barrier_wait_calls++;
    
    if (mock_pthread_barrier_should_fail) {
        mock_pthread_stats.failed_operations++;
        unlock_mock_internal();
        return EINVAL;
    }
    
    mock_barrier_data_t *barrier_data = find_mock_barrier(barrier);
    if (barrier_data) {
        barrier_data->waiting_threads++;
    }
    
    unlock_mock_internal();
    
    /* Use the appropriate barrier implementation based on platform */
#ifdef __APPLE__
    int result = portable_barrier_wait(barrier);
#else
    int result = pthread_barrier_wait(barrier);
#endif
    
    lock_mock_internal();
    /* Check for serial thread return value (platform-specific) */
#ifdef __APPLE__
    if (barrier_data && result == PORTABLE_BARRIER_SERIAL_THREAD) {
#else
    if (barrier_data && result == PTHREAD_BARRIER_SERIAL_THREAD) {
#endif
        barrier_data->waiting_threads = 0; /* Reset for next cycle */
    }
    unlock_mock_internal();
    
    return result;
}

/* ========================================================================
 * MOCK THREAD-LOCAL STORAGE FUNCTIONS
 * ======================================================================== */

int mock_pthread_key_create(pthread_key_t *key, void (*destructor)(void*)) {
    lock_mock_internal();
    mock_pthread_stats.pthread_key_create_calls++;
    
    /* Find next available key */
    int key_index = -1;
    for (int i = 0; i < MAX_TLS_KEYS; i++) {
        if (!tls_key_used[i]) {
            key_index = i;
            break;
        }
    }
    
    if (key_index == -1) {
        mock_pthread_stats.failed_operations++;
        unlock_mock_internal();
        return EAGAIN; /* No more keys available */
    }
    
    tls_key_used[key_index] = true;
    tls_destructors[key_index] = destructor;
    *key = (pthread_key_t)key_index;
    
    unlock_mock_internal();
    return 0;
}

int mock_pthread_key_delete(pthread_key_t key) {
    lock_mock_internal();
    
    int key_index = (int)key;
    if (key_index >= 0 && key_index < MAX_TLS_KEYS && tls_key_used[key_index]) {
        tls_key_used[key_index] = false;
        tls_destructors[key_index] = NULL;
        tls_data[key_index] = NULL;
        unlock_mock_internal();
        return 0;
    }
    
    unlock_mock_internal();
    return EINVAL;
}

int mock_pthread_setspecific(pthread_key_t key, const void *value) {
    lock_mock_internal();
    mock_pthread_stats.pthread_setspecific_calls++;
    
    int key_index = (int)key;
    if (key_index >= 0 && key_index < MAX_TLS_KEYS && tls_key_used[key_index]) {
        tls_data[key_index] = (void*)value;
        unlock_mock_internal();
        return 0;
    }
    
    mock_pthread_stats.failed_operations++;
    unlock_mock_internal();
    return EINVAL;
}

void* mock_pthread_getspecific(pthread_key_t key) {
    lock_mock_internal();
    mock_pthread_stats.pthread_getspecific_calls++;
    
    int key_index = (int)key;
    if (key_index >= 0 && key_index < MAX_TLS_KEYS && tls_key_used[key_index]) {
        void *result = tls_data[key_index];
        unlock_mock_internal();
        return result;
    }
    
    unlock_mock_internal();
    return NULL;
}

/* ========================================================================
 * HELPER FUNCTIONS FOR TESTING
 * ======================================================================== */

void mock_pthread_simulate_delay(int milliseconds) {
    usleep(milliseconds * 1000);
}

void mock_pthread_force_contention(bool enable) {
    lock_mock_internal();
    force_contention = enable;
    unlock_mock_internal();
}

void mock_pthread_simulate_resource_exhaustion(bool enable) {
    lock_mock_internal();
    simulate_resource_exhaustion = enable;
    unlock_mock_internal();
}

int mock_pthread_get_execution_order(pthread_t thread) {
    /* Simple implementation - return thread pointer as order indicator */
    return (int)((uintptr_t)thread & 0xFFFF);
}

bool mock_pthread_wait_for_barrier_threads(pthread_barrier_t *barrier, 
                                          int expected_count, int timeout_ms) {
    lock_mock_internal();
    mock_barrier_data_t *barrier_data = find_mock_barrier(barrier);
    unlock_mock_internal();
    
    if (!barrier_data) {
        return false;
    }
    
    struct timespec start, current;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    while (timeout_ms > 0) {
        lock_mock_internal();
        int waiting = barrier_data->waiting_threads;
        unlock_mock_internal();
        
        if (waiting >= expected_count) {
            return true;
        }
        
        usleep(1000); /* Sleep 1ms */
        
        clock_gettime(CLOCK_MONOTONIC, &current);
        int elapsed_ms = (int)get_time_diff_ms(&start, &current);
        timeout_ms -= elapsed_ms;
        start = current;
    }
    
    return false;
}

bool mock_pthread_validate_mutex_state(pthread_mutex_t *mutex) {
    lock_mock_internal();
    mock_mutex_data_t *mutex_data = find_mock_mutex(mutex);
    unlock_mock_internal();
    
    if (!mutex_data) {
        return true; /* Unknown mutex, assume valid */
    }
    
    /* Basic validation - ensure consistent state */
    return (mutex_data->is_locked == (mutex_data->owner_thread != 0));
}

bool mock_pthread_validate_no_deadlocks(void) {
    /* Simple deadlock detection - check for circular dependencies */
    lock_mock_internal();
    
    /* For now, just verify that no thread owns multiple mutexes */
    /* This is a simplified check - real deadlock detection would be more complex */
    mock_mutex_data_t *current = mock_mutexes;
    pthread_t owner_threads[256];
    int owner_count = 0;
    
    while (current && owner_count < 256) {
        if (current->is_locked) {
            for (int i = 0; i < owner_count; i++) {
                if (pthread_equal(owner_threads[i], current->owner_thread)) {
                    unlock_mock_internal();
                    return false; /* Potential deadlock - same thread owns multiple mutexes */
                }
            }
            owner_threads[owner_count++] = current->owner_thread;
        }
        current = current->next;
    }
    
    unlock_mock_internal();
    return true;
}

mock_pthread_perf_stats_t* mock_pthread_get_performance_stats(void) {
    return &perf_stats;
}