/*
 * portable_barrier.h
 * 
 * Portable pthread barrier implementation for systems that don't support
 * pthread barriers natively (like macOS).
 * 
 * This provides a simple barrier implementation using mutexes and condition variables.
 */

#ifndef PORTABLE_BARRIER_H
#define PORTABLE_BARRIER_H

#include <pthread.h>

/* Define the constant that's used by both the portable implementation and mock framework */
#define PORTABLE_BARRIER_SERIAL_THREAD 1

#ifdef __APPLE__
/* macOS doesn't have pthread barriers, so we implement our own */

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
    int waiting;
    int generation;
} portable_barrier_t;

typedef void* portable_barrierattr_t;

/* Function declarations */
int portable_barrier_init(portable_barrier_t *barrier, const portable_barrierattr_t *attr, unsigned count);
int portable_barrier_destroy(portable_barrier_t *barrier);
int portable_barrier_wait(portable_barrier_t *barrier);

/* Map to portable versions */
#define pthread_barrier_t portable_barrier_t
#define pthread_barrierattr_t portable_barrierattr_t
#define pthread_barrier_init portable_barrier_init
#define pthread_barrier_destroy portable_barrier_destroy
#define pthread_barrier_wait portable_barrier_wait
#define PTHREAD_BARRIER_SERIAL_THREAD PORTABLE_BARRIER_SERIAL_THREAD

#else
/* Use system pthread barriers */
/* No mapping needed, use system types directly */
/* But we still need the constant for the mock framework */
#ifndef PTHREAD_BARRIER_SERIAL_THREAD
#define PTHREAD_BARRIER_SERIAL_THREAD PORTABLE_BARRIER_SERIAL_THREAD
#endif
#endif

#endif /* PORTABLE_BARRIER_H */