/*
 * performance.c
 * 
 * Implementation of the comprehensive performance monitoring system for AncestralClust
 * 
 * This file provides the core implementation for high-resolution timing, memory tracking,
 * CPU monitoring, and thread-safe logging capabilities defined in performance.h
 */

#include "performance.h"
#include <math.h>
#include <pthread.h>

/* ========================================================================
 * GLOBAL PERFORMANCE CONTEXT
 * ======================================================================== */

perf_context_t g_perf_context = {0};

/* Thread-local storage for thread-specific data */
#ifdef _OPENMP
static __thread int tls_thread_id = -1;
#else
static pthread_key_t tls_thread_key;
static pthread_once_t tls_key_once = PTHREAD_ONCE_INIT;
#endif

/* Mutex for thread-safe operations when atomics are not available */
#if !PERF_HAS_ATOMICS
static pthread_mutex_t perf_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/* ========================================================================
 * MILESTONE NAME MAPPING
 * ======================================================================== */

static const char* milestone_names[PERF_MILESTONE_COUNT] = {
    "PROGRAM_START", "PROGRAM_END", "OPTION_PARSING", "INITIALIZATION", "CLEANUP",
    "FASTA_LOAD_START", "FASTA_LOAD_END", "FASTA_PARSE", "TAXONOMY_LOAD", "OUTPUT_WRITE",
    "DISTANCE_MATRIX_START", "DISTANCE_MATRIX_END", "DISTANCE_CALCULATION", 
    "DISTANCE_PTHREAD_SECTION", "DISTANCE_AVERAGE_CALC",
    "TREE_CONSTRUCTION_START", "TREE_CONSTRUCTION_END", "TREE_NODE_CREATION", "TREE_BRANCH_LENGTH_CALC",
    "CLUSTERING_START", "CLUSTERING_END", "CLUSTERING_ITERATION", "CLUSTER_ASSIGNMENT",
    "CLUSTER_CENTROID_UPDATE", "CLUSTER_CONVERGENCE_CHECK", "CLUSTER_INITIALIZATION",
    "ALIGNMENT_START", "ALIGNMENT_END", "KALIGN_EXECUTION", "WFA2_EXECUTION",
    "NEEDLEMAN_WUNSCH", "SEQUENCE_ALIGNMENT", "MSA_CONSTRUCTION",
    "OMP_PARALLEL_START", "OMP_PARALLEL_END", "OMP_THREAD_SPAWN", "OMP_THREAD_JOIN", "OMP_BARRIER",
    "MEMORY_ALLOC", "MEMORY_FREE", "MEMORY_REALLOC", "LARGE_ALLOCATION",
    "USER_1", "USER_2", "USER_3", "USER_4", "USER_5"
};

/* ========================================================================
 * THREAD-LOCAL STORAGE MANAGEMENT
 * ======================================================================== */

#ifndef _OPENMP
static void tls_key_create(void) {
    pthread_key_create(&tls_thread_key, NULL);
}

static int get_thread_id(void) {
    pthread_once(&tls_key_once, tls_key_create);
    int *thread_id = (int*)pthread_getspecific(tls_thread_key);
    if (!thread_id) {
        thread_id = malloc(sizeof(int));
        *thread_id = -1;
        pthread_setspecific(tls_thread_key, thread_id);
    }
    return *thread_id;
}

static void set_thread_id(int id) {
    pthread_once(&tls_key_once, tls_key_create);
    int *thread_id = (int*)pthread_getspecific(tls_thread_key);
    if (!thread_id) {
        thread_id = malloc(sizeof(int));
        pthread_setspecific(tls_thread_key, thread_id);
    }
    *thread_id = id;
}
#endif

/* ========================================================================
 * INITIALIZATION AND CLEANUP
 * ======================================================================== */

int perf_init(void) {
    perf_config_t default_config = {
        .enabled = 1,
        .granularity = PERF_GRANULARITY_MEDIUM,
        .log_level = PERF_LOG_INFO,
        .output_format = PERF_FORMAT_HUMAN,
        .output_file = stderr,
        .flush_immediately = 0,
        .track_memory = 1,
        .track_cpu = 1,
        .track_threads = 1,
        .sampling_interval_us = PERF_SAMPLING_INTERVAL
    };
    strcpy(default_config.output_filename, "performance.log");
    
    return perf_init_with_config(&default_config);
}

int perf_init_with_config(const perf_config_t *config) {
    if (!config) return -1;
    
    memset(&g_perf_context, 0, sizeof(perf_context_t));
    g_perf_context.config = *config;
    
    /* Allocate log entries buffer */
    g_perf_context.log_capacity = PERF_MAX_LOG_ENTRIES;
    g_perf_context.log_entries = malloc(sizeof(perf_metrics_t) * g_perf_context.log_capacity);
    if (!g_perf_context.log_entries) {
        return -1;
    }
    
    /* Initialize atomic counters */
#if PERF_HAS_ATOMICS
    atomic_store(&g_perf_context.allocation_counter, 0);
    atomic_store(&g_perf_context.bytes_allocated, 0);
    atomic_store(&g_perf_context.bytes_freed, 0);
    atomic_store(&g_perf_context.active_threads, 0);
#else
    g_perf_context.allocation_counter = 0;
    g_perf_context.bytes_allocated = 0;
    g_perf_context.bytes_freed = 0;
    g_perf_context.active_threads = 0;
#endif
    
    /* Record program start time */
    perf_get_timestamp(&g_perf_context.program_start_time);
    
    /* Initialize milestone tracking arrays */
    for (int i = 0; i < PERF_MILESTONE_COUNT; i++) {
        g_perf_context.milestone_active[i] = 0;
    }
    
    return 0;
}

void perf_cleanup(void) {
    if (g_perf_context.config.enabled) {
        perf_flush_logs();
        
        if (g_perf_context.config.output_file && 
            g_perf_context.config.output_file != stdout && 
            g_perf_context.config.output_file != stderr) {
            fclose(g_perf_context.config.output_file);
        }
    }
    
    free(g_perf_context.log_entries);
    memset(&g_perf_context, 0, sizeof(perf_context_t));
}

void perf_reset(void) {
    if (!g_perf_context.config.enabled) return;
    
    g_perf_context.log_count = 0;
    g_perf_context.total_runtime_ms = 0.0;
    g_perf_context.peak_memory_kb = 0;
    g_perf_context.max_threads_used = 0;
    
    perf_get_timestamp(&g_perf_context.program_start_time);
    
    for (int i = 0; i < PERF_MILESTONE_COUNT; i++) {
        g_perf_context.milestone_active[i] = 0;
    }
}

/* ========================================================================
 * CONFIGURATION MANAGEMENT
 * ======================================================================== */

void perf_set_config(const perf_config_t *config) {
    if (config) {
        g_perf_context.config = *config;
    }
}

perf_config_t* perf_get_config(void) {
    return &g_perf_context.config;
}

void perf_set_enabled(int enabled) {
    g_perf_context.config.enabled = enabled;
}

void perf_set_granularity(perf_granularity_t granularity) {
    g_perf_context.config.granularity = granularity;
}

void perf_set_output_file(const char *filename) {
    if (!filename) return;
    
    strncpy(g_perf_context.config.output_filename, filename, PERF_MAX_FILENAME_LEN - 1);
    g_perf_context.config.output_filename[PERF_MAX_FILENAME_LEN - 1] = '\0';
    
    if (g_perf_context.config.output_file && 
        g_perf_context.config.output_file != stdout && 
        g_perf_context.config.output_file != stderr) {
        fclose(g_perf_context.config.output_file);
    }
    
    g_perf_context.config.output_file = fopen(filename, "w");
    if (!g_perf_context.config.output_file) {
        g_perf_context.config.output_file = stderr;
    }
}

void perf_set_output_format(perf_output_format_t format) {
    g_perf_context.config.output_format = format;
}

/* ========================================================================
 * HIGH-RESOLUTION TIMING FUNCTIONS
 * ======================================================================== */

void perf_get_timestamp(perf_timestamp_t *ts) {
    if (!ts) return;
    
#ifdef CLOCK_MONOTONIC_RAW
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts->ts);
#elif defined(CLOCK_MONOTONIC)
    clock_gettime(CLOCK_MONOTONIC, &ts->ts);
#else
    clock_gettime(CLOCK_REALTIME, &ts->ts);
#endif
    
    /* TODO: Add CPU cycle counter support for even higher precision */
    ts->cycles = 0;
}

double perf_timestamp_diff_ms(const perf_timestamp_t *start, const perf_timestamp_t *end) {
    if (!start || !end) return 0.0;
    
    double start_ms = start->ts.tv_sec * 1000.0 + start->ts.tv_nsec / 1000000.0;
    double end_ms = end->ts.tv_sec * 1000.0 + end->ts.tv_nsec / 1000000.0;
    
    return end_ms - start_ms;
}

uint64_t perf_timestamp_diff_ns(const perf_timestamp_t *start, const perf_timestamp_t *end) {
    if (!start || !end) return 0;
    
    uint64_t start_ns = start->ts.tv_sec * 1000000000ULL + start->ts.tv_nsec;
    uint64_t end_ns = end->ts.tv_sec * 1000000000ULL + end->ts.tv_nsec;
    
    return end_ns - start_ns;
}

/* ========================================================================
 * MEMORY TRACKING FUNCTIONS
 * ======================================================================== */

int perf_get_memory_usage(perf_memory_t *memory) {
    if (!memory) return -1;
    
    memset(memory, 0, sizeof(perf_memory_t));
    
#ifdef __linux__
    FILE *status_file = fopen("/proc/self/status", "r");
    if (!status_file) return -1;
    
    char line[256];
    while (fgets(line, sizeof(line), status_file)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line, "VmRSS: %zu kB", &memory->rss_kb);
        } else if (strncmp(line, "VmSize:", 7) == 0) {
            sscanf(line, "VmSize: %zu kB", &memory->virt_kb);
        } else if (strncmp(line, "VmPeak:", 7) == 0) {
            sscanf(line, "VmPeak: %zu kB", &memory->peak_rss_kb);
        }
    }
    fclose(status_file);
    
#elif defined(__MACH__)
    struct task_basic_info info;
    mach_msg_type_number_t info_count = TASK_BASIC_INFO_COUNT;
    
    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &info_count) == KERN_SUCCESS) {
        memory->rss_kb = info.resident_size / 1024;
        memory->virt_kb = info.virtual_size / 1024;
    }
    
#else
    /* Fallback using getrusage */
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        memory->rss_kb = usage.ru_maxrss / 1024;  /* ru_maxrss is in KB on Linux, bytes on macOS */
    }
#endif

    /* Add tracked allocation data */
#if PERF_HAS_ATOMICS
    memory->heap_allocated = atomic_load(&g_perf_context.bytes_allocated);
    memory->heap_freed = atomic_load(&g_perf_context.bytes_freed);
    memory->allocation_count = atomic_load(&g_perf_context.allocation_counter);
#else
    memory->heap_allocated = g_perf_context.bytes_allocated;
    memory->heap_freed = g_perf_context.bytes_freed;
    memory->allocation_count = g_perf_context.allocation_counter;
#endif
    
    return 0;
}

void perf_track_allocation(void *ptr, size_t size) {
    if (!ptr || !g_perf_context.config.enabled || !g_perf_context.config.track_memory) return;
    
#if PERF_HAS_ATOMICS
    atomic_fetch_add(&g_perf_context.allocation_counter, 1);
    atomic_fetch_add(&g_perf_context.bytes_allocated, size);
#else
    pthread_mutex_lock(&perf_mutex);
    g_perf_context.allocation_counter++;
    g_perf_context.bytes_allocated += size;
    pthread_mutex_unlock(&perf_mutex);
#endif
}

void perf_track_deallocation(void *ptr) {
    if (!ptr || !g_perf_context.config.enabled || !g_perf_context.config.track_memory) return;
    
    /* Note: We can't easily track the size being freed without additional bookkeeping */
#if PERF_HAS_ATOMICS
    /* Could implement a hash table to track allocation sizes if needed */
#else
    pthread_mutex_lock(&perf_mutex);
    /* Could implement a hash table to track allocation sizes if needed */
    pthread_mutex_unlock(&perf_mutex);
#endif
}

size_t perf_get_current_rss_kb(void) {
    perf_memory_t memory;
    if (perf_get_memory_usage(&memory) == 0) {
        return memory.rss_kb;
    }
    return 0;
}

size_t perf_get_peak_rss_kb(void) {
    perf_memory_t memory;
    if (perf_get_memory_usage(&memory) == 0) {
        return memory.peak_rss_kb;
    }
    return 0;
}

/* ========================================================================
 * CPU MONITORING FUNCTIONS
 * ======================================================================== */

int perf_get_cpu_usage(perf_cpu_t *cpu) {
    if (!cpu) return -1;
    
    memset(cpu, 0, sizeof(perf_cpu_t));
    
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        cpu->user_time = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1000000.0;
        cpu->system_time = usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1000000.0;
        
        /* Calculate approximate CPU percentage */
        static double last_user_time = 0.0, last_system_time = 0.0;
        static struct timespec last_wall_time = {0, 0};
        
        struct timespec current_wall_time;
        clock_gettime(CLOCK_MONOTONIC, &current_wall_time);
        
        if (last_wall_time.tv_sec > 0) {
            double wall_diff = (current_wall_time.tv_sec - last_wall_time.tv_sec) +
                              (current_wall_time.tv_nsec - last_wall_time.tv_nsec) / 1000000000.0;
            double cpu_diff = (cpu->user_time + cpu->system_time) - (last_user_time + last_system_time);
            
            if (wall_diff > 0) {
                cpu->cpu_percent = (cpu_diff / wall_diff) * 100.0;
            }
        }
        
        last_user_time = cpu->user_time;
        last_system_time = cpu->system_time;
        last_wall_time = current_wall_time;
    }
    
    return 0;
}

double perf_get_cpu_percent(void) {
    perf_cpu_t cpu;
    if (perf_get_cpu_usage(&cpu) == 0) {
        return cpu.cpu_percent;
    }
    return 0.0;
}

/* ========================================================================
 * THREAD MANAGEMENT
 * ======================================================================== */

int perf_register_thread(void) {
    if (!g_perf_context.config.enabled || !g_perf_context.config.track_threads) return -1;
    
#if PERF_HAS_ATOMICS
    int thread_count = atomic_fetch_add(&g_perf_context.active_threads, 1);
#else
    pthread_mutex_lock(&perf_mutex);
    int thread_count = g_perf_context.active_threads++;
    pthread_mutex_unlock(&perf_mutex);
#endif
    
    int thread_id = thread_count;
    
#ifdef _OPENMP
    tls_thread_id = thread_id;
    g_perf_context.thread_data[thread_id].omp_thread_num = omp_get_thread_num();
#else
    set_thread_id(thread_id);
    g_perf_context.thread_data[thread_id].omp_thread_num = -1;
#endif
    
    g_perf_context.thread_data[thread_id].thread_id = thread_id;
    perf_get_timestamp(&g_perf_context.thread_data[thread_id].start_time);
    
    /* Update max threads used */
    if (thread_count + 1 > g_perf_context.max_threads_used) {
        g_perf_context.max_threads_used = thread_count + 1;
    }
    
    return thread_id;
}

void perf_unregister_thread(void) {
    if (!g_perf_context.config.enabled || !g_perf_context.config.track_threads) return;
    
#ifdef _OPENMP
    int thread_id = tls_thread_id;
#else
    int thread_id = get_thread_id();
#endif
    
    if (thread_id >= 0 && thread_id < PERF_MAX_THREADS) {
        perf_get_timestamp(&g_perf_context.thread_data[thread_id].end_time);
    }
    
#if PERF_HAS_ATOMICS
    atomic_fetch_sub(&g_perf_context.active_threads, 1);
#else
    pthread_mutex_lock(&perf_mutex);
    g_perf_context.active_threads--;
    pthread_mutex_unlock(&perf_mutex);
#endif
}

int perf_get_thread_count(void) {
#if PERF_HAS_ATOMICS
    return atomic_load(&g_perf_context.active_threads);
#else
    return g_perf_context.active_threads;
#endif
}

perf_thread_data_t* perf_get_thread_data(int thread_id) {
    if (thread_id < 0 || thread_id >= PERF_MAX_THREADS) return NULL;
    return &g_perf_context.thread_data[thread_id];
}

/* ========================================================================
 * MILESTONE TRACKING
 * ======================================================================== */

void perf_start_milestone(perf_milestone_t milestone) {
    perf_start_milestone_labeled(milestone, "");
}

void perf_end_milestone(perf_milestone_t milestone) {
    perf_end_milestone_labeled(milestone, "");
}

void perf_start_milestone_labeled(perf_milestone_t milestone, const char *label) {
    if (!g_perf_context.config.enabled || milestone >= PERF_MILESTONE_COUNT) return;
    
    perf_get_timestamp(&g_perf_context.milestone_starts[milestone]);
    g_perf_context.milestone_active[milestone] = 1;
}

void perf_end_milestone_labeled(perf_milestone_t milestone, const char *label) {
    if (!g_perf_context.config.enabled || milestone >= PERF_MILESTONE_COUNT) return;
    if (!g_perf_context.milestone_active[milestone]) return;
    
    perf_timestamp_t end_time;
    perf_get_timestamp(&end_time);
    
    double duration_ms = perf_timestamp_diff_ms(&g_perf_context.milestone_starts[milestone], &end_time);
    
    /* Log the milestone */
    if (g_perf_context.log_count < g_perf_context.log_capacity) {
        perf_metrics_t *entry = &g_perf_context.log_entries[g_perf_context.log_count];
        
        entry->milestone = milestone;
        entry->timestamp = end_time;
        entry->duration_ms = duration_ms;
        entry->thread_count = perf_get_thread_count();
        entry->iteration_number = 0;
        entry->convergence_metric = 0.0;
        
        if (label) {
            strncpy(entry->label, label, PERF_MAX_LABEL_LEN - 1);
            entry->label[PERF_MAX_LABEL_LEN - 1] = '\0';
        } else {
            entry->label[0] = '\0';
        }
        
        /* Get memory and CPU info if tracking is enabled */
        if (g_perf_context.config.track_memory) {
            perf_get_memory_usage(&entry->memory);
        }
        if (g_perf_context.config.track_cpu) {
            perf_get_cpu_usage(&entry->cpu);
        }
        
        g_perf_context.log_count++;
        
        if (g_perf_context.config.flush_immediately) {
            perf_flush_logs();
        }
    }
    
    g_perf_context.milestone_active[milestone] = 0;
}

/* ========================================================================
 * CUSTOM EVENT LOGGING
 * ======================================================================== */

void perf_log_event(const char *label, double value) {
    perf_log_event_with_context(label, value, "");
}

void perf_log_event_with_context(const char *label, double value, const char *context) {
    if (!g_perf_context.config.enabled || !label) return;
    
    if (g_perf_context.log_count < g_perf_context.log_capacity) {
        perf_metrics_t *entry = &g_perf_context.log_entries[g_perf_context.log_count];
        
        entry->milestone = PERF_MILESTONE_USER_1;  /* Use as generic event marker */
        perf_get_timestamp(&entry->timestamp);
        entry->duration_ms = value;  /* Repurpose as event value */
        entry->thread_count = perf_get_thread_count();
        
        strncpy(entry->label, label, PERF_MAX_LABEL_LEN - 1);
        entry->label[PERF_MAX_LABEL_LEN - 1] = '\0';
        
        if (context) {
            strncpy(entry->context, context, PERF_MAX_LABEL_LEN - 1);
            entry->context[PERF_MAX_LABEL_LEN - 1] = '\0';
        } else {
            entry->context[0] = '\0';
        }
        
        g_perf_context.log_count++;
    }
}

void perf_log_iteration(int iteration, double convergence_metric) {
    if (!g_perf_context.config.enabled) return;
    
    if (g_perf_context.log_count < g_perf_context.log_capacity) {
        perf_metrics_t *entry = &g_perf_context.log_entries[g_perf_context.log_count];
        
        entry->milestone = PERF_MILESTONE_CLUSTERING_ITERATION;
        perf_get_timestamp(&entry->timestamp);
        entry->duration_ms = 0.0;
        entry->thread_count = perf_get_thread_count();
        entry->iteration_number = iteration;
        entry->convergence_metric = convergence_metric;
        
        snprintf(entry->label, PERF_MAX_LABEL_LEN, "iteration_%d", iteration);
        snprintf(entry->context, PERF_MAX_LABEL_LEN, "convergence=%.6f", convergence_metric);
        
        g_perf_context.log_count++;
    }
}

void perf_log_algorithm_step(const char *algorithm, const char *step, double metric) {
    if (!g_perf_context.config.enabled || !algorithm || !step) return;
    
    char label[PERF_MAX_LABEL_LEN];
    snprintf(label, PERF_MAX_LABEL_LEN, "%s_%s", algorithm, step);
    
    char context[PERF_MAX_LABEL_LEN];
    snprintf(context, PERF_MAX_LABEL_LEN, "metric=%.6f", metric);
    
    perf_log_event_with_context(label, metric, context);
}

/* ========================================================================
 * OUTPUT AND REPORTING
 * ======================================================================== */

void perf_flush_logs(void) {
    if (!g_perf_context.config.enabled || !g_perf_context.config.output_file) return;
    
    for (size_t i = 0; i < g_perf_context.log_count; i++) {
        perf_metrics_t *entry = &g_perf_context.log_entries[i];
        
        switch (g_perf_context.config.output_format) {
            case PERF_FORMAT_CSV:
                fprintf(g_perf_context.config.output_file,
                    "%ld.%09ld,%s,%.3f,%zu,%zu,%d,%d,%.6f,%.2f,%s,%s\n",
                    entry->timestamp.ts.tv_sec, entry->timestamp.ts.tv_nsec,
                    perf_milestone_name(entry->milestone),
                    entry->duration_ms,
                    entry->memory.rss_kb, entry->memory.virt_kb,
                    entry->thread_count, entry->iteration_number,
                    entry->convergence_metric, entry->cpu.cpu_percent,
                    entry->label, entry->context);
                break;
                
            case PERF_FORMAT_JSON:
                /* TODO: Implement JSON output */
                break;
                
            case PERF_FORMAT_HUMAN:
            default:
                fprintf(g_perf_context.config.output_file,
                    "[%ld.%09ld] %s: %.3f ms, RSS: %zu KB, Threads: %d, %s\n",
                    entry->timestamp.ts.tv_sec, entry->timestamp.ts.tv_nsec,
                    perf_milestone_name(entry->milestone),
                    entry->duration_ms,
                    entry->memory.rss_kb,
                    entry->thread_count,
                    entry->label);
                break;
        }
    }
    
    fflush(g_perf_context.config.output_file);
}

void perf_print_summary(void) {
    if (!g_perf_context.config.enabled) return;
    
    perf_timestamp_t current_time;
    perf_get_timestamp(&current_time);
    
    double total_runtime = perf_timestamp_diff_ms(&g_perf_context.program_start_time, &current_time);
    
    fprintf(stderr, "\n=== AncestralClust Performance Summary ===\n");
    fprintf(stderr, "Total Runtime: %.3f ms\n", total_runtime);
    fprintf(stderr, "Peak Memory Usage: %zu KB\n", perf_get_peak_rss_kb());
    fprintf(stderr, "Max Threads Used: %d\n", g_perf_context.max_threads_used);
    fprintf(stderr, "Total Log Entries: %zu\n", g_perf_context.log_count);
    
#if PERF_HAS_ATOMICS
    fprintf(stderr, "Total Allocations: %zu\n", atomic_load(&g_perf_context.allocation_counter));
    fprintf(stderr, "Total Bytes Allocated: %zu\n", atomic_load(&g_perf_context.bytes_allocated));
#else
    fprintf(stderr, "Total Allocations: %zu\n", g_perf_context.allocation_counter);
    fprintf(stderr, "Total Bytes Allocated: %zu\n", g_perf_context.bytes_allocated);
#endif
    
    fprintf(stderr, "==========================================\n\n");
}

void perf_print_detailed_report(void) {
    perf_print_summary();
    
    /* TODO: Implement detailed milestone statistics */
}

void perf_export_csv(const char *filename) {
    if (!filename) return;
    
    FILE *old_output = g_perf_context.config.output_file;
    perf_output_format_t old_format = g_perf_context.config.output_format;
    
    g_perf_context.config.output_file = fopen(filename, "w");
    if (!g_perf_context.config.output_file) {
        g_perf_context.config.output_file = old_output;
        return;
    }
    
    g_perf_context.config.output_format = PERF_FORMAT_CSV;
    
    /* Write CSV header */
    fprintf(g_perf_context.config.output_file,
        "timestamp,milestone,duration_ms,memory_rss_kb,memory_virt_kb,thread_count,"
        "iteration,convergence_metric,cpu_percent,label,context\n");
    
    perf_flush_logs();
    
    fclose(g_perf_context.config.output_file);
    g_perf_context.config.output_file = old_output;
    g_perf_context.config.output_format = old_format;
}

void perf_export_json(const char *filename) {
    /* TODO: Implement JSON export */
}

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

const char* perf_milestone_name(perf_milestone_t milestone) {
    if (milestone >= 0 && milestone < PERF_MILESTONE_COUNT) {
        return milestone_names[milestone];
    }
    return "UNKNOWN";
}

const char* perf_format_duration(double duration_ms) {
    static char buffer[64];
    
    if (duration_ms < 1.0) {
        snprintf(buffer, sizeof(buffer), "%.3f ms", duration_ms);
    } else if (duration_ms < 1000.0) {
        snprintf(buffer, sizeof(buffer), "%.1f ms", duration_ms);
    } else if (duration_ms < 60000.0) {
        snprintf(buffer, sizeof(buffer), "%.2f s", duration_ms / 1000.0);
    } else {
        double minutes = duration_ms / 60000.0;
        snprintf(buffer, sizeof(buffer), "%.1f min", minutes);
    }
    
    return buffer;
}

void perf_format_memory_size(size_t bytes, char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return;
    
    if (bytes < 1024) {
        snprintf(buffer, buffer_size, "%zu B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.1f KB", bytes / 1024.0);
    } else if (bytes < 1024 * 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.1f MB", bytes / (1024.0 * 1024.0));
    } else {
        snprintf(buffer, buffer_size, "%.1f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    }
}

/* ========================================================================
 * STATISTICAL ANALYSIS FUNCTIONS
 * ======================================================================== */

int perf_get_milestone_statistics(perf_milestone_t milestone, perf_statistics_t *stats) {
    if (!stats || milestone >= PERF_MILESTONE_COUNT) return -1;
    
    memset(stats, 0, sizeof(perf_statistics_t));
    
    /* Collect all duration samples for this milestone */
    double *samples = malloc(g_perf_context.log_count * sizeof(double));
    if (!samples) return -1;
    
    size_t sample_count = 0;
    for (size_t i = 0; i < g_perf_context.log_count; i++) {
        if (g_perf_context.log_entries[i].milestone == milestone) {
            samples[sample_count++] = g_perf_context.log_entries[i].duration_ms;
        }
    }
    
    if (sample_count == 0) {
        free(samples);
        return -1;
    }
    
    /* Calculate basic statistics */
    stats->sample_count = sample_count;
    stats->min = samples[0];
    stats->max = samples[0];
    stats->mean = 0.0;
    
    for (size_t i = 0; i < sample_count; i++) {
        if (samples[i] < stats->min) stats->min = samples[i];
        if (samples[i] > stats->max) stats->max = samples[i];
        stats->mean += samples[i];
    }
    stats->mean /= sample_count;
    
    /* Calculate standard deviation */
    double variance = 0.0;
    for (size_t i = 0; i < sample_count; i++) {
        double diff = samples[i] - stats->mean;
        variance += diff * diff;
    }
    stats->std_dev = sqrt(variance / sample_count);
    
    /* TODO: Calculate median and percentiles (requires sorting) */
    
    free(samples);
    return 0;
}

int perf_get_summary(perf_summary_t *summary) {
    if (!summary) return -1;
    
    memset(summary, 0, sizeof(perf_summary_t));
    
    perf_timestamp_t current_time;
    perf_get_timestamp(&current_time);
    
    summary->total_runtime_ms = perf_timestamp_diff_ms(&g_perf_context.program_start_time, &current_time);
    summary->peak_memory_kb = perf_get_peak_rss_kb();
    summary->max_threads = g_perf_context.max_threads_used;
    
#if PERF_HAS_ATOMICS
    summary->total_allocations = atomic_load(&g_perf_context.allocation_counter);
    summary->total_bytes_allocated = atomic_load(&g_perf_context.bytes_allocated);
#else
    summary->total_allocations = g_perf_context.allocation_counter;
    summary->total_bytes_allocated = g_perf_context.bytes_allocated;
#endif
    
    /* Calculate statistics for each milestone */
    for (int i = 0; i < PERF_MILESTONE_COUNT; i++) {
        perf_get_milestone_statistics((perf_milestone_t)i, &summary->timing_stats[i]);
    }
    
    return 0;
}