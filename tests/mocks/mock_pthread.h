/*
 * mock_pthread.h
 * 
 * Mock framework for pthread functions to enable controlled threading tests
 * for the AncestralClust performance logging system.
 * 
 * This mock framework allows testing of thread creation failure scenarios,
 * synchronization edge cases, and mutex behavior under controlled conditions.
 */

#ifndef MOCK_PTHREAD_H
#define MOCK_PTHREAD_H

#include "../utils/portable_barrier.h"
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* Mock control flags */
extern bool mock_pthread_enabled;
extern bool mock_pthread_create_should_fail;
extern bool mock_pthread_mutex_lock_should_fail;
extern bool mock_pthread_mutex_unlock_should_fail;
extern bool mock_pthread_barrier_should_fail;
extern int mock_pthread_create_failure_count;
extern int mock_pthread_mutex_failure_count;

/* Mock statistics */
typedef struct {
    int pthread_create_calls;
    int pthread_join_calls;
    int pthread_mutex_lock_calls;
    int pthread_mutex_unlock_calls;
    int pthread_mutex_init_calls;
    int pthread_mutex_destroy_calls;
    int pthread_barrier_init_calls;
    int pthread_barrier_wait_calls;
    int pthread_barrier_destroy_calls;
    int pthread_key_create_calls;
    int pthread_setspecific_calls;
    int pthread_getspecific_calls;
    int failed_operations;
} mock_pthread_stats_t;

extern mock_pthread_stats_t mock_pthread_stats;

/* Mock thread data structure */
typedef struct mock_thread_data {
    pthread_t original_thread;
    void *(*start_routine)(void*);
    void *arg;
    void *return_value;
    bool is_joined;
    bool is_cancelled;
    struct mock_thread_data *next;
} mock_thread_data_t;

/* Mock mutex data structure */
typedef struct mock_mutex_data {
    pthread_mutex_t original_mutex;
    bool is_locked;
    pthread_t owner_thread;
    int lock_count;
    struct timespec lock_time;
    struct mock_mutex_data *next;
} mock_mutex_data_t;

/* Mock barrier data structure */
typedef struct mock_barrier_data {
    pthread_barrier_t original_barrier;
    int count;
    int waiting_threads;
    bool is_destroyed;
    struct mock_barrier_data *next;
} mock_barrier_data_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * MOCK FRAMEWORK CONTROL FUNCTIONS
 * ======================================================================== */

/* Initialize and cleanup mock framework */
void mock_pthread_init(void);
void mock_pthread_cleanup(void);
void mock_pthread_reset_stats(void);

/* Enable/disable mocking */
void mock_pthread_enable(bool enable);

/* Configure failure scenarios */
void mock_pthread_set_create_failure(bool should_fail, int failure_count);
void mock_pthread_set_mutex_failure(bool should_fail, int failure_count);
void mock_pthread_set_barrier_failure(bool should_fail);

/* Get mock statistics */
mock_pthread_stats_t* mock_pthread_get_stats(void);

/* ========================================================================
 * MOCK PTHREAD FUNCTION DECLARATIONS
 * ======================================================================== */

/* Thread management mocks */
int mock_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                       void *(*start_routine)(void*), void *arg);
int mock_pthread_join(pthread_t thread, void **retval);
int mock_pthread_cancel(pthread_t thread);
void mock_pthread_exit(void *retval);
pthread_t mock_pthread_self(void);
int mock_pthread_equal(pthread_t t1, pthread_t t2);

/* Mutex mocks */
int mock_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int mock_pthread_mutex_destroy(pthread_mutex_t *mutex);
int mock_pthread_mutex_lock(pthread_mutex_t *mutex);
int mock_pthread_mutex_trylock(pthread_mutex_t *mutex);
int mock_pthread_mutex_unlock(pthread_mutex_t *mutex);

/* Barrier mocks */
int mock_pthread_barrier_init(pthread_barrier_t *barrier, 
                             const pthread_barrierattr_t *attr, unsigned count);
int mock_pthread_barrier_destroy(pthread_barrier_t *barrier);
int mock_pthread_barrier_wait(pthread_barrier_t *barrier);

/* Thread-local storage mocks */
int mock_pthread_key_create(pthread_key_t *key, void (*destructor)(void*));
int mock_pthread_key_delete(pthread_key_t key);
int mock_pthread_setspecific(pthread_key_t key, const void *value);
void* mock_pthread_getspecific(pthread_key_t key);

/* ========================================================================
 * MOCK WRAPPER MACROS
 * ======================================================================== */

#ifdef ENABLE_PTHREAD_MOCKING

/* Redirect pthread calls to mock functions when mocking is enabled */
#define pthread_create(thread, attr, start_routine, arg) \
    (mock_pthread_enabled ? mock_pthread_create(thread, attr, start_routine, arg) : \
     pthread_create(thread, attr, start_routine, arg))

#define pthread_join(thread, retval) \
    (mock_pthread_enabled ? mock_pthread_join(thread, retval) : \
     pthread_join(thread, retval))

#define pthread_mutex_init(mutex, attr) \
    (mock_pthread_enabled ? mock_pthread_mutex_init(mutex, attr) : \
     pthread_mutex_init(mutex, attr))

#define pthread_mutex_destroy(mutex) \
    (mock_pthread_enabled ? mock_pthread_mutex_destroy(mutex) : \
     pthread_mutex_destroy(mutex))

#define pthread_mutex_lock(mutex) \
    (mock_pthread_enabled ? mock_pthread_mutex_lock(mutex) : \
     pthread_mutex_lock(mutex))

#define pthread_mutex_unlock(mutex) \
    (mock_pthread_enabled ? mock_pthread_mutex_unlock(mutex) : \
     pthread_mutex_unlock(mutex))

#define pthread_barrier_init(barrier, attr, count) \
    (mock_pthread_enabled ? mock_pthread_barrier_init(barrier, attr, count) : \
     pthread_barrier_init(barrier, attr, count))

#define pthread_barrier_destroy(barrier) \
    (mock_pthread_enabled ? mock_pthread_barrier_destroy(barrier) : \
     pthread_barrier_destroy(barrier))

#define pthread_barrier_wait(barrier) \
    (mock_pthread_enabled ? mock_pthread_barrier_wait(barrier) : \
     pthread_barrier_wait(barrier))

#define pthread_key_create(key, destructor) \
    (mock_pthread_enabled ? mock_pthread_key_create(key, destructor) : \
     pthread_key_create(key, destructor))

#define pthread_setspecific(key, value) \
    (mock_pthread_enabled ? mock_pthread_setspecific(key, value) : \
     pthread_setspecific(key, value))

#define pthread_getspecific(key) \
    (mock_pthread_enabled ? mock_pthread_getspecific(key) : \
     pthread_getspecific(key))

#endif /* ENABLE_PTHREAD_MOCKING */

/* ========================================================================
 * HELPER FUNCTIONS FOR TESTING
 * ======================================================================== */

/* Simulate thread execution delays */
void mock_pthread_simulate_delay(int milliseconds);

/* Force thread contention scenarios */
void mock_pthread_force_contention(bool enable);

/* Simulate resource exhaustion */
void mock_pthread_simulate_resource_exhaustion(bool enable);

/* Get thread execution order for deterministic testing */
int mock_pthread_get_execution_order(pthread_t thread);

/* Wait for specific number of threads to reach a barrier */
bool mock_pthread_wait_for_barrier_threads(pthread_barrier_t *barrier, 
                                          int expected_count, int timeout_ms);

/* Validate thread safety invariants */
bool mock_pthread_validate_mutex_state(pthread_mutex_t *mutex);
bool mock_pthread_validate_no_deadlocks(void);

/* Performance measurement helpers */
typedef struct {
    double avg_lock_time_ms;
    double max_lock_time_ms;
    int total_contentions;
    int successful_locks;
    int failed_locks;
} mock_pthread_perf_stats_t;

mock_pthread_perf_stats_t* mock_pthread_get_performance_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_PTHREAD_H */