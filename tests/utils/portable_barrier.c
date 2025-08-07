/*
 * portable_barrier.c
 * 
 * Implementation of portable pthread barrier for systems that don't support
 * pthread barriers natively (like macOS).
 */

#include "portable_barrier.h"
#include <errno.h>

#ifdef __APPLE__

int portable_barrier_init(portable_barrier_t *barrier, const portable_barrierattr_t *attr, unsigned count) {
    if (!barrier || count == 0) {
        return EINVAL;
    }
    
    int result = pthread_mutex_init(&barrier->mutex, NULL);
    if (result != 0) {
        return result;
    }
    
    result = pthread_cond_init(&barrier->cond, NULL);
    if (result != 0) {
        pthread_mutex_destroy(&barrier->mutex);
        return result;
    }
    
    barrier->count = count;
    barrier->waiting = 0;
    barrier->generation = 0;
    
    return 0;
}

int portable_barrier_destroy(portable_barrier_t *barrier) {
    if (!barrier) {
        return EINVAL;
    }
    
    pthread_mutex_destroy(&barrier->mutex);
    pthread_cond_destroy(&barrier->cond);
    
    return 0;
}

int portable_barrier_wait(portable_barrier_t *barrier) {
    if (!barrier) {
        return EINVAL;
    }
    
    pthread_mutex_lock(&barrier->mutex);
    
    int generation = barrier->generation;
    barrier->waiting++;
    
    if (barrier->waiting == barrier->count) {
        /* Last thread to reach barrier */
        barrier->waiting = 0;
        barrier->generation++;
        pthread_cond_broadcast(&barrier->cond);
        pthread_mutex_unlock(&barrier->mutex);
        return PORTABLE_BARRIER_SERIAL_THREAD;
    } else {
        /* Wait for all threads to reach barrier */
        while (generation == barrier->generation) {
            pthread_cond_wait(&barrier->cond, &barrier->mutex);
        }
        pthread_mutex_unlock(&barrier->mutex);
        return 0;
    }
}

#endif /* __APPLE__ */