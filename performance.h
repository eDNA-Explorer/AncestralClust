/*
 * performance.h
 * 
 * Comprehensive Performance Monitoring System for AncestralClust
 * Provides high-resolution timing, memory tracking, CPU monitoring, and thread-safe logging
 * 
 * Author: Performance Monitoring System
 * Compatible with: C99, OpenMP, pthread
 * Dependencies: time.h, stdio.h, stdatomic.h (C11), sys/time.h, unistd.h
 */

#ifndef PERFORMANCE_H
#define PERFORMANCE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>

#ifdef _OPENMP
#include <omp.h>
#endif

/* Feature detection for atomic operations */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
#include <stdatomic.h>
#define PERF_HAS_ATOMICS 1
#else
#define PERF_HAS_ATOMICS 0
#endif

/* Platform-specific includes */
#ifdef __linux__
#include <sys/sysinfo.h>
#endif

#ifdef __MACH__
#include <mach/mach.h>
#include <mach/task.h>
#include <mach/mach_init.h>
#endif

/* Compatibility macros for different compilers */
#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

/* ========================================================================
 * CONFIGURATION CONSTANTS
 * ======================================================================== */

#define PERF_MAX_LABEL_LEN      64
#define PERF_MAX_FILENAME_LEN   256
#define PERF_MAX_LOG_ENTRIES    10000
#define PERF_BUFFER_SIZE        8192
#define PERF_MAX_THREADS        256
#define PERF_SAMPLING_INTERVAL  100000  /* microseconds */

/* Output format options */
typedef enum {
    PERF_FORMAT_HUMAN = 0,
    PERF_FORMAT_CSV,
    PERF_FORMAT_JSON,
    PERF_FORMAT_TSV
} perf_output_format_t;

/* Logging levels */
typedef enum {
    PERF_LOG_ERROR = 0,
    PERF_LOG_WARN,
    PERF_LOG_INFO,
    PERF_LOG_DEBUG,
    PERF_LOG_TRACE
} perf_log_level_t;

/* Granularity levels for performance monitoring */
typedef enum {
    PERF_GRANULARITY_COARSE = 0,  /* Only major milestones */
    PERF_GRANULARITY_MEDIUM,      /* Function-level tracking */
    PERF_GRANULARITY_FINE,        /* Loop and operation level */
    PERF_GRANULARITY_DEBUG        /* Extremely detailed tracking */
} perf_granularity_t;

/* ========================================================================
 * MILESTONE DEFINITIONS FOR ANCESTRALCLUST
 * ======================================================================== */

typedef enum {
    /* Program lifecycle milestones */
    PERF_MILESTONE_PROGRAM_START = 0,
    PERF_MILESTONE_PROGRAM_END,
    PERF_MILESTONE_OPTION_PARSING,
    PERF_MILESTONE_INITIALIZATION,
    PERF_MILESTONE_CLEANUP,
    
    /* File I/O milestones */
    PERF_MILESTONE_FASTA_LOAD_START,
    PERF_MILESTONE_FASTA_LOAD_END,
    PERF_MILESTONE_FASTA_PARSE,
    PERF_MILESTONE_TAXONOMY_LOAD,
    PERF_MILESTONE_OUTPUT_WRITE,
    
    /* Distance matrix computation */
    PERF_MILESTONE_DISTANCE_MATRIX_START,
    PERF_MILESTONE_DISTANCE_MATRIX_END,
    PERF_MILESTONE_DISTANCE_CALCULATION,
    PERF_MILESTONE_DISTANCE_PTHREAD_SECTION,
    PERF_MILESTONE_DISTANCE_AVERAGE_CALC,
    
    /* Tree construction phases */
    PERF_MILESTONE_TREE_CONSTRUCTION_START,
    PERF_MILESTONE_TREE_CONSTRUCTION_END,
    PERF_MILESTONE_TREE_NODE_CREATION,
    PERF_MILESTONE_TREE_BRANCH_LENGTH_CALC,
    
    /* Clustering algorithm phases */
    PERF_MILESTONE_CLUSTERING_START,
    PERF_MILESTONE_CLUSTERING_END,
    PERF_MILESTONE_CLUSTERING_ITERATION,
    PERF_MILESTONE_CLUSTER_ASSIGNMENT,
    PERF_MILESTONE_CLUSTER_CENTROID_UPDATE,
    PERF_MILESTONE_CLUSTER_CONVERGENCE_CHECK,
    PERF_MILESTONE_CLUSTER_INITIALIZATION,
    
    /* Alignment operations */
    PERF_MILESTONE_ALIGNMENT_START,
    PERF_MILESTONE_ALIGNMENT_END,
    PERF_MILESTONE_KALIGN_EXECUTION,
    PERF_MILESTONE_WFA2_EXECUTION,
    PERF_MILESTONE_NEEDLEMAN_WUNSCH,
    PERF_MILESTONE_SEQUENCE_ALIGNMENT,
    PERF_MILESTONE_MSA_CONSTRUCTION,
    
    /* OpenMP parallel regions */
    PERF_MILESTONE_OMP_PARALLEL_START,
    PERF_MILESTONE_OMP_PARALLEL_END,
    PERF_MILESTONE_OMP_THREAD_SPAWN,
    PERF_MILESTONE_OMP_THREAD_JOIN,
    PERF_MILESTONE_OMP_BARRIER,
    
    /* Memory management events */
    PERF_MILESTONE_MEMORY_ALLOC,
    PERF_MILESTONE_MEMORY_FREE,
    PERF_MILESTONE_MEMORY_REALLOC,
    PERF_MILESTONE_LARGE_ALLOCATION,
    
    /* Custom user-defined milestones */
    PERF_MILESTONE_USER_1,
    PERF_MILESTONE_USER_2,
    PERF_MILESTONE_USER_3,
    PERF_MILESTONE_USER_4,
    PERF_MILESTONE_USER_5,
    
    PERF_MILESTONE_COUNT  /* Must be last */
} perf_milestone_t;

/* ========================================================================
 * CORE DATA STRUCTURES
 * ======================================================================== */

/* High-resolution timestamp structure */
typedef struct {
    struct timespec ts;
    uint64_t cycles;    /* CPU cycles if available */
} perf_timestamp_t;

/* Memory usage metrics */
typedef struct {
    size_t rss_kb;          /* Resident Set Size in KB */
    size_t virt_kb;         /* Virtual memory size in KB */
    size_t peak_rss_kb;     /* Peak RSS in KB */
    size_t heap_allocated;  /* Tracked heap allocations */
    size_t heap_freed;      /* Tracked heap deallocations */
    size_t allocation_count;/* Number of allocations */
    size_t free_count;      /* Number of frees */
} perf_memory_t;

/* CPU usage metrics */
typedef struct {
    double cpu_percent;     /* CPU usage percentage */
    double user_time;       /* User CPU time */
    double system_time;     /* System CPU time */
    uint64_t context_switches; /* Context switches */
    uint64_t cache_misses;     /* Cache misses (if available) */
} perf_cpu_t;

/* Thread-specific performance data */
typedef struct {
    int thread_id;
    int omp_thread_num;
    perf_timestamp_t start_time;
    perf_timestamp_t end_time;
    perf_memory_t memory;
    perf_cpu_t cpu;
    uint64_t operations_count;
    char label[PERF_MAX_LABEL_LEN];
} perf_thread_data_t;

/* Performance metrics for a single measurement */
typedef struct {
    perf_milestone_t milestone;
    perf_timestamp_t timestamp;
    double duration_ms;         /* Duration in milliseconds */
    perf_memory_t memory;
    perf_cpu_t cpu;
    int thread_count;
    int iteration_number;       /* For iterative algorithms */
    double convergence_metric;  /* Algorithm-specific convergence */
    char label[PERF_MAX_LABEL_LEN];
    char context[PERF_MAX_LABEL_LEN];
} perf_metrics_t;

/* Performance configuration */
typedef struct {
    int enabled;                        /* Global enable/disable */
    perf_granularity_t granularity;     /* Monitoring granularity */
    perf_log_level_t log_level;         /* Minimum log level */
    perf_output_format_t output_format; /* Output format */
    FILE *output_file;                  /* Output destination */
    char output_filename[PERF_MAX_FILENAME_LEN];
    int flush_immediately;              /* Flush after each write */
    int track_memory;                   /* Enable memory tracking */
    int track_cpu;                      /* Enable CPU tracking */
    int track_threads;                  /* Enable thread tracking */
    uint64_t sampling_interval_us;      /* Sampling interval in microseconds */
} perf_config_t;

/* Global performance context */
typedef struct {
    perf_config_t config;
    perf_metrics_t *log_entries;
    volatile size_t log_count;
    size_t log_capacity;
    perf_thread_data_t thread_data[PERF_MAX_THREADS];
    perf_timestamp_t program_start_time;
    
#if PERF_HAS_ATOMICS
    atomic_size_t allocation_counter;
    atomic_size_t bytes_allocated;
    atomic_size_t bytes_freed;
    atomic_int active_threads;
#else
    size_t allocation_counter;
    size_t bytes_allocated;
    size_t bytes_freed;
    int active_threads;
#endif

    /* Milestone timing tracking */
    perf_timestamp_t milestone_starts[PERF_MILESTONE_COUNT];
    int milestone_active[PERF_MILESTONE_COUNT];
    
    /* Statistics */
    double total_runtime_ms;
    size_t peak_memory_kb;
    int max_threads_used;
} perf_context_t;

/* ========================================================================
 * GLOBAL PERFORMANCE CONTEXT
 * ======================================================================== */

extern perf_context_t g_perf_context;

/* ========================================================================
 * CORE FUNCTION DECLARATIONS
 * ======================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/* Initialization and cleanup */
int perf_init(void);
int perf_init_with_config(const perf_config_t *config);
void perf_cleanup(void);
void perf_reset(void);

/* Configuration management */
void perf_set_config(const perf_config_t *config);
perf_config_t* perf_get_config(void);
void perf_set_enabled(int enabled);
void perf_set_granularity(perf_granularity_t granularity);
void perf_set_output_file(const char *filename);
void perf_set_output_format(perf_output_format_t format);

/* High-resolution timing functions */
void perf_get_timestamp(perf_timestamp_t *ts);
double perf_timestamp_diff_ms(const perf_timestamp_t *start, const perf_timestamp_t *end);
uint64_t perf_timestamp_diff_ns(const perf_timestamp_t *start, const perf_timestamp_t *end);

/* Memory tracking functions */
int perf_get_memory_usage(perf_memory_t *memory);
void perf_track_allocation(void *ptr, size_t size);
void perf_track_deallocation(void *ptr);
size_t perf_get_current_rss_kb(void);
size_t perf_get_peak_rss_kb(void);

/* CPU monitoring functions */
int perf_get_cpu_usage(perf_cpu_t *cpu);
double perf_get_cpu_percent(void);

/* Thread management */
int perf_register_thread(void);
void perf_unregister_thread(void);
int perf_get_thread_count(void);
perf_thread_data_t* perf_get_thread_data(int thread_id);

/* Milestone tracking */
void perf_start_milestone(perf_milestone_t milestone);
void perf_end_milestone(perf_milestone_t milestone);
void perf_start_milestone_labeled(perf_milestone_t milestone, const char *label);
void perf_end_milestone_labeled(perf_milestone_t milestone, const char *label);

/* Custom event logging */
void perf_log_event(const char *label, double value);
void perf_log_event_with_context(const char *label, double value, const char *context);
void perf_log_iteration(int iteration, double convergence_metric);
void perf_log_algorithm_step(const char *algorithm, const char *step, double metric);

/* Output and reporting */
void perf_flush_logs(void);
void perf_print_summary(void);
void perf_print_detailed_report(void);
void perf_export_csv(const char *filename);
void perf_export_json(const char *filename);

/* Utility functions */
const char* perf_milestone_name(perf_milestone_t milestone);
const char* perf_format_duration(double duration_ms);
void perf_format_memory_size(size_t bytes, char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

/* ========================================================================
 * PERFORMANCE TRACKING MACROS
 * ======================================================================== */

/* Core milestone macros */
#define PERF_START_MILESTONE(milestone) \
    do { \
        if (unlikely(g_perf_context.config.enabled)) { \
            perf_start_milestone(milestone); \
        } \
    } while(0)

#define PERF_END_MILESTONE(milestone) \
    do { \
        if (unlikely(g_perf_context.config.enabled)) { \
            perf_end_milestone(milestone); \
        } \
    } while(0)

#define PERF_START_MILESTONE_LABELED(milestone, label) \
    do { \
        if (unlikely(g_perf_context.config.enabled)) { \
            perf_start_milestone_labeled(milestone, label); \
        } \
    } while(0)

#define PERF_END_MILESTONE_LABELED(milestone, label) \
    do { \
        if (unlikely(g_perf_context.config.enabled)) { \
            perf_end_milestone_labeled(milestone, label); \
        } \
    } while(0)

/* Scoped milestone tracking with automatic cleanup */
#define PERF_SCOPE_MILESTONE(milestone, label) \
    perf_scoped_milestone_t _perf_scope_##__LINE__ = {0}; \
    if (unlikely(g_perf_context.config.enabled)) { \
        _perf_scope_##__LINE__.milestone = milestone; \
        strncpy(_perf_scope_##__LINE__.label, label, PERF_MAX_LABEL_LEN-1); \
        perf_start_milestone_labeled(milestone, label); \
    }

/* Memory tracking macros */
#define PERF_LOG_MEMORY(label) \
    do { \
        if (unlikely(g_perf_context.config.enabled && g_perf_context.config.track_memory)) { \
            perf_memory_t mem; \
            if (perf_get_memory_usage(&mem) == 0) { \
                perf_log_event_with_context("memory_rss_kb", (double)mem.rss_kb, label); \
            } \
        } \
    } while(0)

#define PERF_LOG_CPU(label) \
    do { \
        if (unlikely(g_perf_context.config.enabled && g_perf_context.config.track_cpu)) { \
            perf_cpu_t cpu; \
            if (perf_get_cpu_usage(&cpu) == 0) { \
                perf_log_event_with_context("cpu_percent", cpu.cpu_percent, label); \
            } \
        } \
    } while(0)

#define PERF_TRACK_ALLOC(ptr, size) \
    do { \
        if (unlikely(g_perf_context.config.enabled && g_perf_context.config.track_memory)) { \
            perf_track_allocation(ptr, size); \
        } \
    } while(0)

#define PERF_TRACK_FREE(ptr) \
    do { \
        if (unlikely(g_perf_context.config.enabled && g_perf_context.config.track_memory)) { \
            perf_track_deallocation(ptr); \
        } \
    } while(0)

/* Clustering-specific macros */
#define PERF_LOG_ITERATION(iteration, convergence) \
    do { \
        if (unlikely(g_perf_context.config.enabled)) { \
            perf_log_iteration(iteration, convergence); \
        } \
    } while(0)

#define PERF_LOG_CLUSTER_ASSIGNMENT(cluster_id, sequence_count) \
    do { \
        if (unlikely(g_perf_context.config.enabled)) { \
            char context[64]; \
            snprintf(context, sizeof(context), "cluster_%d", cluster_id); \
            perf_log_event_with_context("sequence_count", (double)sequence_count, context); \
        } \
    } while(0)

#define PERF_LOG_DISTANCE_CALCULATION(distance, seq1, seq2) \
    do { \
        if (unlikely(g_perf_context.config.enabled && g_perf_context.config.granularity >= PERF_GRANULARITY_FINE)) { \
            char context[128]; \
            snprintf(context, sizeof(context), "%s_vs_%s", seq1, seq2); \
            perf_log_event_with_context("distance", distance, context); \
        } \
    } while(0)

/* Alignment-specific macros */
#define PERF_LOG_ALIGNMENT_SCORE(score, algorithm) \
    do { \
        if (unlikely(g_perf_context.config.enabled)) { \
            perf_log_event_with_context("alignment_score", (double)score, algorithm); \
        } \
    } while(0)

/* Thread management macros */
#define PERF_REGISTER_THREAD() \
    do { \
        if (unlikely(g_perf_context.config.enabled && g_perf_context.config.track_threads)) { \
            perf_register_thread(); \
        } \
    } while(0)

#define PERF_UNREGISTER_THREAD() \
    do { \
        if (unlikely(g_perf_context.config.enabled && g_perf_context.config.track_threads)) { \
            perf_unregister_thread(); \
        } \
    } while(0)

/* OpenMP-specific macros */
#ifdef _OPENMP
#define PERF_OMP_PARALLEL_START() PERF_START_MILESTONE(PERF_MILESTONE_OMP_PARALLEL_START)
#define PERF_OMP_PARALLEL_END() PERF_END_MILESTONE(PERF_MILESTONE_OMP_PARALLEL_END)
#else
#define PERF_OMP_PARALLEL_START() do {} while(0)
#define PERF_OMP_PARALLEL_END() do {} while(0)
#endif

/* Conditional compilation macros for different granularity levels */
#define PERF_COARSE(code) \
    do { \
        if (unlikely(g_perf_context.config.enabled && g_perf_context.config.granularity >= PERF_GRANULARITY_COARSE)) { \
            code; \
        } \
    } while(0)

#define PERF_MEDIUM(code) \
    do { \
        if (unlikely(g_perf_context.config.enabled && g_perf_context.config.granularity >= PERF_GRANULARITY_MEDIUM)) { \
            code; \
        } \
    } while(0)

#define PERF_FINE(code) \
    do { \
        if (unlikely(g_perf_context.config.enabled && g_perf_context.config.granularity >= PERF_GRANULARITY_FINE)) { \
            code; \
        } \
    } while(0)

#define PERF_DEBUG(code) \
    do { \
        if (unlikely(g_perf_context.config.enabled && g_perf_context.config.granularity >= PERF_GRANULARITY_DEBUG)) { \
            code; \
        } \
    } while(0)

/* ========================================================================
 * RAII-STYLE SCOPED PERFORMANCE TRACKING
 * ======================================================================== */

/* Scoped milestone structure for automatic cleanup */
typedef struct {
    perf_milestone_t milestone;
    char label[PERF_MAX_LABEL_LEN];
    int active;
} perf_scoped_milestone_t;

/* Destructor function for scoped milestones */
static inline void perf_scoped_milestone_cleanup(perf_scoped_milestone_t *scope) {
    if (scope && scope->active && g_perf_context.config.enabled) {
        perf_end_milestone_labeled(scope->milestone, scope->label);
    }
}

/* GCC-specific cleanup attribute for automatic scope management */
#ifdef __GNUC__
#define PERF_SCOPED_MILESTONE(milestone, label) \
    perf_scoped_milestone_t _perf_scope_##__LINE__ __attribute__((cleanup(perf_scoped_milestone_cleanup))) = {0}; \
    if (unlikely(g_perf_context.config.enabled)) { \
        _perf_scope_##__LINE__.milestone = milestone; \
        _perf_scope_##__LINE__.active = 1; \
        strncpy(_perf_scope_##__LINE__.label, label, PERF_MAX_LABEL_LEN-1); \
        perf_start_milestone_labeled(milestone, label); \
    }
#else
/* Fallback for compilers without cleanup attribute */
#define PERF_SCOPED_MILESTONE(milestone, label) PERF_START_MILESTONE_LABELED(milestone, label)
#endif

/* ========================================================================
 * PERFORMANCE STATISTICS AND ANALYSIS
 * ======================================================================== */

/* Statistical analysis structure */
typedef struct {
    double min;
    double max;
    double mean;
    double std_dev;
    double median;
    double percentile_95;
    double percentile_99;
    size_t sample_count;
} perf_statistics_t;

/* Function to calculate statistics for a given milestone */
int perf_get_milestone_statistics(perf_milestone_t milestone, perf_statistics_t *stats);

/* Function to get overall performance summary */
typedef struct {
    double total_runtime_ms;
    size_t peak_memory_kb;
    double avg_cpu_percent;
    int max_threads;
    size_t total_allocations;
    size_t total_bytes_allocated;
    perf_statistics_t timing_stats[PERF_MILESTONE_COUNT];
} perf_summary_t;

int perf_get_summary(perf_summary_t *summary);

/* ========================================================================
 * CSV OUTPUT FORMAT SPECIFICATION
 * ======================================================================== */

/*
 * CSV Header Format:
 * timestamp,milestone,duration_ms,memory_rss_kb,memory_virt_kb,thread_count,
 * iteration,convergence_metric,cpu_percent,label,context
 * 
 * Example CSV output:
 * 2024-01-01T10:00:00.123,CLUSTERING_START,0.0,1024,2048,4,0,0.0,15.2,"K-means clustering","initialization"
 * 2024-01-01T10:00:00.456,CLUSTERING_ITERATION,125.3,1056,2048,4,1,0.85,18.7,"K-means clustering","iteration_1"
 */

/* ========================================================================
 * JSON OUTPUT FORMAT SPECIFICATION  
 * ======================================================================== */

/*
 * JSON Structure:
 * {
 *   "performance_log": {
 *     "metadata": {
 *       "program": "AncestralClust",
 *       "start_time": "2024-01-01T10:00:00.000Z",
 *       "end_time": "2024-01-01T10:05:30.456Z",
 *       "total_runtime_ms": 330456.0,
 *       "config": { ... }
 *     },
 *     "milestones": [
 *       {
 *         "timestamp": "2024-01-01T10:00:00.123Z",
 *         "milestone": "CLUSTERING_START",
 *         "duration_ms": 0.0,
 *         "memory": {
 *           "rss_kb": 1024,
 *           "virt_kb": 2048,
 *           "peak_rss_kb": 1024
 *         },
 *         "cpu": {
 *           "percent": 15.2,
 *           "user_time": 0.1,
 *           "system_time": 0.05
 *         },
 *         "threading": {
 *           "thread_count": 4,
 *           "omp_threads": 4
 *         },
 *         "algorithm": {
 *           "iteration": 0,
 *           "convergence_metric": 0.0
 *         },
 *         "label": "K-means clustering",
 *         "context": "initialization"
 *       }
 *     ],
 *     "summary": {
 *       "statistics": { ... }
 *     }
 *   }
 * }
 */

/* ========================================================================
 * INTEGRATION GUIDELINES AND USAGE EXAMPLES
 * ======================================================================== */

/*
 * BASIC INTEGRATION EXAMPLE:
 * 
 * int main(int argc, char **argv) {
 *     // Initialize performance monitoring
 *     perf_init();
 *     PERF_START_MILESTONE(PERF_MILESTONE_PROGRAM_START);
 *     
 *     // Your existing code...
 *     Options opt;
 *     PERF_START_MILESTONE(PERF_MILESTONE_OPTION_PARSING);
 *     read_options(argc, argv, &opt);
 *     PERF_END_MILESTONE(PERF_MILESTONE_OPTION_PARSING);
 *     
 *     // Clustering operations
 *     PERF_START_MILESTONE(PERF_MILESTONE_CLUSTERING_START);
 *     for (int iter = 0; iter < max_iterations; iter++) {
 *         PERF_START_MILESTONE_LABELED(PERF_MILESTONE_CLUSTERING_ITERATION, "k-means");
 *         double convergence = perform_clustering_iteration();
 *         PERF_LOG_ITERATION(iter, convergence);
 *         PERF_END_MILESTONE_LABELED(PERF_MILESTONE_CLUSTERING_ITERATION, "k-means");
 *         
 *         if (convergence < threshold) break;
 *     }
 *     PERF_END_MILESTONE(PERF_MILESTONE_CLUSTERING_START);
 *     
 *     PERF_END_MILESTONE(PERF_MILESTONE_PROGRAM_START);
 *     perf_print_summary();
 *     perf_cleanup();
 *     return 0;
 * }
 * 
 * OPENMP INTEGRATION EXAMPLE:
 * 
 * #pragma omp parallel for
 * for (int i = 0; i < num_sequences; i++) {
 *     PERF_REGISTER_THREAD();
 *     PERF_START_MILESTONE_LABELED(PERF_MILESTONE_DISTANCE_CALCULATION, "pairwise");
 *     
 *     double distance = calculate_distance(sequences[i], sequences[j]);
 *     PERF_LOG_DISTANCE_CALCULATION(distance, seq_names[i], seq_names[j]);
 *     
 *     PERF_END_MILESTONE_LABELED(PERF_MILESTONE_DISTANCE_CALCULATION, "pairwise");
 *     PERF_UNREGISTER_THREAD();
 * }
 * 
 * MEMORY TRACKING EXAMPLE:
 * 
 * void* my_malloc(size_t size) {
 *     void *ptr = malloc(size);
 *     PERF_TRACK_ALLOC(ptr, size);
 *     return ptr;
 * }
 * 
 * void my_free(void *ptr) {
 *     PERF_TRACK_FREE(ptr);
 *     free(ptr);
 * }
 */

#endif /* PERFORMANCE_H */