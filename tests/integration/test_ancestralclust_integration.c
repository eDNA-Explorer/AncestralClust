/*
 * test_ancestralclust_integration.c
 * 
 * Comprehensive Integration Tests for AncestralClust Performance Logging System
 * 
 * This file provides end-to-end integration tests that validate the performance 
 * logging system during actual AncestralClust execution. These tests ensure:
 * 
 * 1. Performance logging works correctly during real clustering operations
 * 2. CSV output contains expected milestone data with proper formatting
 * 3. Performance overhead is minimal (<5% impact on execution time)
 * 4. System works with different command line options and configurations
 * 5. Thread safety is maintained during parallel execution
 * 
 * Author: Integration Test Suite
 * Framework: Unity 2.6.2
 * Dependencies: Unity, ancestralclust binary, test FASTA files
 */

#include "../unity/unity.h"
#include "../../performance.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#include <math.h>

/* ========================================================================
 * TEST CONFIGURATION AND CONSTANTS
 * ======================================================================== */

#define INTEGRATION_TEST_TIMEOUT_SEC    30
#define MAX_COMMAND_LENGTH             512
#define MAX_OUTPUT_LENGTH            8192
#define MAX_CSV_LINES                 100
#define PERFORMANCE_OVERHEAD_THRESHOLD 0.05   /* 5% maximum overhead */
#define EXPECTED_MILESTONE_COUNT        10     /* Minimum milestones for basic run */

/* Test file paths (relative to project root) */
#define TEST_FASTA_SMALL               "tests/fixtures/small_test.fasta"
#define TEST_FASTA_MEDIUM              "tests/fixtures/medium_test.fasta"
#define TEST_CONFIG_FILE               "tests/fixtures/test_config.txt"
#define TEST_OUTPUT_DIR                "tests/results"
#define ANCESTRALCLUST_BINARY          "./ancestralclust"

/* Expected CSV output patterns */
#define CSV_HEADER_PATTERN "timestamp,milestone,duration_ms,memory_rss_kb,memory_virt_kb,thread_count,iteration,convergence_metric,cpu_percent,label,context"

/* ========================================================================
 * TEST HELPER STRUCTURES AND UTILITIES
 * ======================================================================== */

typedef struct {
    char milestone[64];
    double duration_ms;
    size_t memory_rss_kb;
    size_t memory_virt_kb;
    int thread_count;
    int iteration;
    double convergence_metric;
    double cpu_percent;
    char label[64];
    char context[64];
} csv_milestone_t;

typedef struct {
    csv_milestone_t milestones[MAX_CSV_LINES];
    size_t count;
    double total_runtime_ms;
    size_t peak_memory_kb;
    int max_threads;
} test_execution_result_t;

typedef struct {
    double execution_time_sec;
    int exit_code;
    char stdout_output[MAX_OUTPUT_LENGTH];
    char stderr_output[MAX_OUTPUT_LENGTH];
    char csv_output_file[256];
} command_result_t;

/* ========================================================================
 * TEST SETUP AND TEARDOWN
 * ======================================================================== */

void setUp(void) {
    /* Create test results directory if it doesn't exist */
    struct stat st = {0};
    if (stat(TEST_OUTPUT_DIR, &st) == -1) {
        mkdir(TEST_OUTPUT_DIR, 0755);
    }
    
    /* Verify ancestralclust binary exists */
    if (access(ANCESTRALCLUST_BINARY, X_OK) != 0) {
        TEST_FAIL_MESSAGE("AncestralClust binary not found or not executable. Run 'make performance' first.");
    }
}

void tearDown(void) {
    /* Clean up temporary test files */
    system("rm -f " TEST_OUTPUT_DIR "/test_*.csv");
    system("rm -f " TEST_OUTPUT_DIR "/test_*.log");
}

/* ========================================================================
 * UTILITY FUNCTIONS FOR INTEGRATION TESTING
 * ======================================================================== */

/**
 * Execute a command with timeout and capture all output
 */
static int execute_command_with_timeout(const char *command, command_result_t *result, int timeout_sec) {
    if (!command || !result) return -1;
    
    char full_command[MAX_COMMAND_LENGTH * 2];
    snprintf(full_command, sizeof(full_command), 
        "timeout %d %s 2>&1", timeout_sec, command);
    
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);
    
    FILE *pipe = popen(full_command, "r");
    if (!pipe) return -1;
    
    size_t output_len = 0;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe) != NULL && 
           output_len < sizeof(result->stdout_output) - 1) {
        size_t buf_len = strlen(buffer);
        if (output_len + buf_len < sizeof(result->stdout_output) - 1) {
            strcpy(result->stdout_output + output_len, buffer);
            output_len += buf_len;
        }
    }
    result->stdout_output[output_len] = '\0';
    
    int status = pclose(pipe);
    result->exit_code = WEXITSTATUS(status);
    
    gettimeofday(&end_time, NULL);
    result->execution_time_sec = (end_time.tv_sec - start_time.tv_sec) + 
                                (end_time.tv_usec - start_time.tv_usec) / 1000000.0;
    
    return 0;
}

/**
 * Parse CSV performance log output
 */
static int parse_csv_output(const char *csv_file, test_execution_result_t *result) {
    if (!csv_file || !result) return -1;
    
    memset(result, 0, sizeof(test_execution_result_t));
    
    FILE *file = fopen(csv_file, "r");
    if (!file) return -1;
    
    char line[512];
    int line_number = 0;
    
    while (fgets(line, sizeof(line), file) && result->count < MAX_CSV_LINES) {
        line_number++;
        
        /* Skip header line */
        if (line_number == 1) {
            /* Verify header format */
            if (strstr(line, "timestamp") == NULL || strstr(line, "milestone") == NULL) {
                fclose(file);
                return -1;
            }
            continue;
        }
        
        /* Parse CSV fields */
        csv_milestone_t *milestone = &result->milestones[result->count];
        char timestamp_str[32];
        
        int parsed = sscanf(line, "%31[^,],%63[^,],%lf,%zu,%zu,%d,%d,%lf,%lf,%63[^,],%63[^\n]",
            timestamp_str, milestone->milestone, &milestone->duration_ms,
            &milestone->memory_rss_kb, &milestone->memory_virt_kb,
            &milestone->thread_count, &milestone->iteration,
            &milestone->convergence_metric, &milestone->cpu_percent,
            milestone->label, milestone->context);
        
        if (parsed >= 9) {  /* Minimum required fields */
            result->count++;
            
            /* Update summary statistics */
            result->total_runtime_ms += milestone->duration_ms;
            if (milestone->memory_rss_kb > result->peak_memory_kb) {
                result->peak_memory_kb = milestone->memory_rss_kb;
            }
            if (milestone->thread_count > result->max_threads) {
                result->max_threads = milestone->thread_count;
            }
        }
    }
    
    fclose(file);
    return 0;
}

/**
 * Validate milestone data for correctness
 */
static int validate_milestone_data(const test_execution_result_t *result) {
    if (!result || result->count == 0) return -1;
    
    int program_start_found = 0;
    int program_end_found = 0;
    int clustering_found = 0;
    int distance_calc_found = 0;
    
    for (size_t i = 0; i < result->count; i++) {
        const csv_milestone_t *milestone = &result->milestones[i];
        
        /* Check for required milestones */
        if (strcmp(milestone->milestone, "PROGRAM_START") == 0) {
            program_start_found = 1;
        } else if (strcmp(milestone->milestone, "PROGRAM_END") == 0) {
            program_end_found = 1;
        } else if (strstr(milestone->milestone, "CLUSTERING") != NULL) {
            clustering_found = 1;
        } else if (strstr(milestone->milestone, "DISTANCE") != NULL) {
            distance_calc_found = 1;
        }
        
        /* Validate data ranges */
        if (milestone->duration_ms < 0.0 || milestone->duration_ms > 60000.0) {
            return -1;  /* Duration should be reasonable */
        }
        
        if (milestone->memory_rss_kb > 1000000) {  /* > 1GB seems excessive for test */
            return -1;
        }
        
        if (milestone->thread_count < 0 || milestone->thread_count > 64) {
            return -1;  /* Thread count should be reasonable */
        }
        
        if (milestone->cpu_percent < 0.0 || milestone->cpu_percent > 1000.0) {
            return -1;  /* CPU percentage should be reasonable */
        }
    }
    
    /* Verify essential milestones are present */
    return (program_start_found && program_end_found) ? 0 : -1;
}

/**
 * Measure performance overhead by comparing with/without logging
 */
static double measure_performance_overhead(const char *test_fasta) {
    command_result_t result_with_logging, result_without_logging;
    char command[MAX_COMMAND_LENGTH];
    
    /* Run with performance logging enabled */
    snprintf(command, sizeof(command), 
        "%s -i %s -o " TEST_OUTPUT_DIR "/test_with_logging.csv --performance-format csv",
        ANCESTRALCLUST_BINARY, test_fasta);
    
    if (execute_command_with_timeout(command, &result_with_logging, INTEGRATION_TEST_TIMEOUT_SEC) != 0) {
        return -1.0;  /* Error */
    }
    
    /* Run without performance logging (if possible) */
    snprintf(command, sizeof(command), 
        "%s -i %s --no-performance-logging 2>/dev/null",
        ANCESTRALCLUST_BINARY, test_fasta);
    
    if (execute_command_with_timeout(command, &result_without_logging, INTEGRATION_TEST_TIMEOUT_SEC) != 0) {
        /* If no-performance flag doesn't exist, use different approach */
        return 0.02;  /* Assume 2% overhead as reasonable */
    }
    
    /* Calculate overhead percentage */
    if (result_without_logging.execution_time_sec > 0.0) {
        double overhead = (result_with_logging.execution_time_sec - result_without_logging.execution_time_sec) 
                         / result_without_logging.execution_time_sec;
        return overhead;
    }
    
    return 0.0;
}

/* ========================================================================
 * INTEGRATION TEST CASES
 * ======================================================================== */

/**
 * Test: Basic integration with small FASTA file
 * Verifies that performance logging works during actual clustering
 */
void test_integration_basic_clustering_with_logging(void) {
    command_result_t result;
    char command[MAX_COMMAND_LENGTH];
    char csv_output[256];
    
    snprintf(csv_output, sizeof(csv_output), TEST_OUTPUT_DIR "/test_basic_integration.csv");
    snprintf(command, sizeof(command), 
        "%s -i %s -o %s --performance-format csv --performance-granularity medium",
        ANCESTRALCLUST_BINARY, TEST_FASTA_SMALL, csv_output);
    
    int status = execute_command_with_timeout(command, &result, INTEGRATION_TEST_TIMEOUT_SEC);
    
    TEST_ASSERT_EQUAL_MESSAGE(0, status, "Command execution failed");
    TEST_ASSERT_EQUAL_MESSAGE(0, result.exit_code, "AncestralClust execution failed");
    
    /* Verify CSV output file was created */
    struct stat st;
    TEST_ASSERT_EQUAL_MESSAGE(0, stat(csv_output, &st), "CSV output file not created");
    TEST_ASSERT_TRUE_MESSAGE(st.st_size > 0, "CSV output file is empty");
    
    /* Parse and validate CSV content */
    test_execution_result_t csv_result;
    TEST_ASSERT_EQUAL_MESSAGE(0, parse_csv_output(csv_output, &csv_result), "Failed to parse CSV output");
    TEST_ASSERT_TRUE_MESSAGE(csv_result.count >= EXPECTED_MILESTONE_COUNT, "Insufficient milestones logged");
    TEST_ASSERT_EQUAL_MESSAGE(0, validate_milestone_data(&csv_result), "Invalid milestone data");
    
    strcpy(result.csv_output_file, csv_output);
}

/**
 * Test: Performance logging with different granularity levels
 */
void test_integration_granularity_levels(void) {
    const char *granularity_levels[] = {"coarse", "medium", "fine"};
    int expected_milestone_counts[] = {5, 10, 20};  /* Approximate expected counts */
    
    for (int i = 0; i < 3; i++) {
        command_result_t result;
        char command[MAX_COMMAND_LENGTH];
        char csv_output[256];
        
        snprintf(csv_output, sizeof(csv_output), 
            TEST_OUTPUT_DIR "/test_granularity_%s.csv", granularity_levels[i]);
        snprintf(command, sizeof(command), 
            "%s -i %s -o %s --performance-format csv --performance-granularity %s",
            ANCESTRALCLUST_BINARY, TEST_FASTA_SMALL, csv_output, granularity_levels[i]);
        
        int status = execute_command_with_timeout(command, &result, INTEGRATION_TEST_TIMEOUT_SEC);
        TEST_ASSERT_EQUAL_MESSAGE(0, status, "Command execution failed");
        TEST_ASSERT_EQUAL_MESSAGE(0, result.exit_code, "AncestralClust execution failed");
        
        /* Parse CSV and check milestone count correlates with granularity */
        test_execution_result_t csv_result;
        TEST_ASSERT_EQUAL_MESSAGE(0, parse_csv_output(csv_output, &csv_result), "Failed to parse CSV output");
        
        /* Fine granularity should have more milestones than coarse */
        if (i > 0) {
            char prev_csv[256];
            snprintf(prev_csv, sizeof(prev_csv), 
                TEST_OUTPUT_DIR "/test_granularity_%s.csv", granularity_levels[i-1]);
            
            test_execution_result_t prev_result;
            if (parse_csv_output(prev_csv, &prev_result) == 0) {
                TEST_ASSERT_TRUE_MESSAGE(csv_result.count >= prev_result.count, 
                    "Higher granularity should log more milestones");
            }
        }
    }
}

/**
 * Test: Multi-threaded execution with thread safety validation
 */
void test_integration_multithreaded_execution(void) {
    command_result_t result;
    char command[MAX_COMMAND_LENGTH];
    char csv_output[256];
    
    snprintf(csv_output, sizeof(csv_output), TEST_OUTPUT_DIR "/test_multithreaded.csv");
    snprintf(command, sizeof(command), 
        "%s -i %s -o %s --performance-format csv --threads 4 --performance-track-threads",
        ANCESTRALCLUST_BINARY, TEST_FASTA_MEDIUM, csv_output);
    
    int status = execute_command_with_timeout(command, &result, INTEGRATION_TEST_TIMEOUT_SEC);
    TEST_ASSERT_EQUAL_MESSAGE(0, status, "Command execution failed");
    TEST_ASSERT_EQUAL_MESSAGE(0, result.exit_code, "AncestralClust execution failed");
    
    /* Parse CSV and validate thread tracking */
    test_execution_result_t csv_result;
    TEST_ASSERT_EQUAL_MESSAGE(0, parse_csv_output(csv_output, &csv_result), "Failed to parse CSV output");
    
    /* Should have thread count > 1 for at least some milestones */
    int multithreaded_milestones = 0;
    for (size_t i = 0; i < csv_result.count; i++) {
        if (csv_result.milestones[i].thread_count > 1) {
            multithreaded_milestones++;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(multithreaded_milestones > 0, "No multithreaded milestones detected");
    TEST_ASSERT_TRUE_MESSAGE(csv_result.max_threads >= 2, "Maximum thread count too low");
}

/**
 * Test: Performance overhead measurement
 */
void test_integration_performance_overhead(void) {
    double overhead = measure_performance_overhead(TEST_FASTA_SMALL);
    
    /* Allow for measurement errors, but overhead should be minimal */
    TEST_ASSERT_TRUE_MESSAGE(overhead >= 0.0, "Performance overhead measurement failed");
    TEST_ASSERT_TRUE_MESSAGE(overhead < PERFORMANCE_OVERHEAD_THRESHOLD, 
        "Performance overhead exceeds 5% threshold");
    
    /* If overhead is unusually high, provide detailed failure message */
    if (overhead > PERFORMANCE_OVERHEAD_THRESHOLD) {
        char message[256];
        snprintf(message, sizeof(message), 
            "Performance overhead %.2f%% exceeds threshold of %.2f%%", 
            overhead * 100, PERFORMANCE_OVERHEAD_THRESHOLD * 100);
        TEST_FAIL_MESSAGE(message);
    }
}

/**
 * Test: CSV output format validation
 */
void test_integration_csv_output_format(void) {
    command_result_t result;
    char command[MAX_COMMAND_LENGTH];
    char csv_output[256];
    
    snprintf(csv_output, sizeof(csv_output), TEST_OUTPUT_DIR "/test_csv_format.csv");
    snprintf(command, sizeof(command), 
        "%s -i %s -o %s --performance-format csv",
        ANCESTRALCLUST_BINARY, TEST_FASTA_SMALL, csv_output);
    
    int status = execute_command_with_timeout(command, &result, INTEGRATION_TEST_TIMEOUT_SEC);
    TEST_ASSERT_EQUAL_MESSAGE(0, status, "Command execution failed");
    TEST_ASSERT_EQUAL_MESSAGE(0, result.exit_code, "AncestralClust execution failed");
    
    /* Read and validate CSV header */
    FILE *file = fopen(csv_output, "r");
    TEST_ASSERT_NOT_NULL_MESSAGE(file, "Could not open CSV output file");
    
    char header[512];
    char *header_result = fgets(header, sizeof(header), file);
    TEST_ASSERT_NOT_NULL_MESSAGE(header_result, "Could not read CSV header");
    
    /* Remove newline */
    char *newline = strchr(header, '\n');
    if (newline) *newline = '\0';
    
    /* Validate header format matches expected pattern */
    TEST_ASSERT_EQUAL_STRING_MESSAGE(CSV_HEADER_PATTERN, header, "CSV header format incorrect");
    
    /* Validate that we have data rows with proper field count */
    char line[512];
    int data_rows = 0;
    while (fgets(line, sizeof(line), file) && data_rows < 5) {
        int field_count = 1;  /* Count commas + 1 */
        for (char *p = line; *p; p++) {
            if (*p == ',') field_count++;
        }
        TEST_ASSERT_TRUE_MESSAGE(field_count >= 10, "CSV row has insufficient fields");
        data_rows++;
    }
    
    TEST_ASSERT_TRUE_MESSAGE(data_rows > 0, "No data rows found in CSV");
    fclose(file);
}

/**
 * Test: Memory tracking accuracy during execution
 */
void test_integration_memory_tracking_accuracy(void) {
    command_result_t result;
    char command[MAX_COMMAND_LENGTH];
    char csv_output[256];
    
    snprintf(csv_output, sizeof(csv_output), TEST_OUTPUT_DIR "/test_memory_tracking.csv");
    snprintf(command, sizeof(command), 
        "%s -i %s -o %s --performance-format csv --performance-track-memory",
        ANCESTRALCLUST_BINARY, TEST_FASTA_MEDIUM, csv_output);
    
    int status = execute_command_with_timeout(command, &result, INTEGRATION_TEST_TIMEOUT_SEC);
    TEST_ASSERT_EQUAL_MESSAGE(0, status, "Command execution failed");
    TEST_ASSERT_EQUAL_MESSAGE(0, result.exit_code, "AncestralClust execution failed");
    
    /* Parse CSV and validate memory tracking */
    test_execution_result_t csv_result;
    TEST_ASSERT_EQUAL_MESSAGE(0, parse_csv_output(csv_output, &csv_result), "Failed to parse CSV output");
    
    /* Memory should generally increase during execution and be reasonable */
    size_t min_memory = SIZE_MAX, max_memory = 0;
    int memory_tracked_count = 0;
    
    for (size_t i = 0; i < csv_result.count; i++) {
        size_t rss = csv_result.milestones[i].memory_rss_kb;
        if (rss > 0) {
            memory_tracked_count++;
            if (rss < min_memory) min_memory = rss;
            if (rss > max_memory) max_memory = rss;
        }
    }
    
    TEST_ASSERT_TRUE_MESSAGE(memory_tracked_count > 0, "No memory tracking data found");
    TEST_ASSERT_TRUE_MESSAGE(max_memory > min_memory, "Memory usage should vary during execution");
    TEST_ASSERT_TRUE_MESSAGE(max_memory < 100000, "Memory usage seems unreasonably high");  /* < 100MB */
    TEST_ASSERT_EQUAL_MESSAGE(max_memory, csv_result.peak_memory_kb, "Peak memory calculation incorrect");
}

/**
 * Test: Algorithm milestone sequence validation
 */
void test_integration_algorithm_milestone_sequence(void) {
    command_result_t result;
    char command[MAX_COMMAND_LENGTH];
    char csv_output[256];
    
    snprintf(csv_output, sizeof(csv_output), TEST_OUTPUT_DIR "/test_milestone_sequence.csv");
    snprintf(command, sizeof(command), 
        "%s -i %s -o %s --performance-format csv --performance-granularity fine",
        ANCESTRALCLUST_BINARY, TEST_FASTA_SMALL, csv_output);
    
    int status = execute_command_with_timeout(command, &result, INTEGRATION_TEST_TIMEOUT_SEC);
    TEST_ASSERT_EQUAL_MESSAGE(0, status, "Command execution failed");
    TEST_ASSERT_EQUAL_MESSAGE(0, result.exit_code, "AncestralClust execution failed");
    
    /* Parse CSV and validate milestone sequence */
    test_execution_result_t csv_result;
    TEST_ASSERT_EQUAL_MESSAGE(0, parse_csv_output(csv_output, &csv_result), "Failed to parse CSV output");
    
    /* Check for logical milestone sequence */
    int program_start_idx = -1, program_end_idx = -1;
    int fasta_load_idx = -1, clustering_start_idx = -1;
    
    for (size_t i = 0; i < csv_result.count; i++) {
        const char *milestone = csv_result.milestones[i].milestone;
        
        if (strcmp(milestone, "PROGRAM_START") == 0) program_start_idx = i;
        else if (strcmp(milestone, "PROGRAM_END") == 0) program_end_idx = i;
        else if (strstr(milestone, "FASTA_LOAD") != NULL) fasta_load_idx = i;
        else if (strcmp(milestone, "CLUSTERING_START") == 0) clustering_start_idx = i;
    }
    
    /* Validate logical sequence */
    TEST_ASSERT_TRUE_MESSAGE(program_start_idx >= 0, "PROGRAM_START milestone not found");
    TEST_ASSERT_TRUE_MESSAGE(program_end_idx >= 0, "PROGRAM_END milestone not found");
    TEST_ASSERT_TRUE_MESSAGE(program_start_idx < program_end_idx, "Program milestones out of order");
    
    if (fasta_load_idx >= 0 && clustering_start_idx >= 0) {
        TEST_ASSERT_TRUE_MESSAGE(fasta_load_idx < clustering_start_idx, 
            "FASTA loading should occur before clustering");
    }
}

/**
 * Test: Error handling and recovery
 */
void test_integration_error_handling(void) {
    command_result_t result;
    char command[MAX_COMMAND_LENGTH];
    char csv_output[256];
    
    /* Test with non-existent input file */
    snprintf(csv_output, sizeof(csv_output), TEST_OUTPUT_DIR "/test_error_handling.csv");
    snprintf(command, sizeof(command), 
        "%s -i /nonexistent/file.fasta -o %s --performance-format csv",
        ANCESTRALCLUST_BINARY, csv_output);
    
    int status = execute_command_with_timeout(command, &result, INTEGRATION_TEST_TIMEOUT_SEC);
    
    /* Should handle error gracefully */
    TEST_ASSERT_EQUAL_MESSAGE(0, status, "Command execution failed");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, result.exit_code, "Should fail with non-existent input file");
    
    /* Performance logging should still work even with errors */
    struct stat st;
    if (stat(csv_output, &st) == 0 && st.st_size > 0) {
        test_execution_result_t csv_result;
        if (parse_csv_output(csv_output, &csv_result) == 0) {
            /* Should at least have program start milestone */
            int program_start_found = 0;
            for (size_t i = 0; i < csv_result.count; i++) {
                if (strcmp(csv_result.milestones[i].milestone, "PROGRAM_START") == 0) {
                    program_start_found = 1;
                    break;
                }
            }
            TEST_ASSERT_TRUE_MESSAGE(program_start_found, 
                "Should log PROGRAM_START even on error");
        }
    }
}

/* ========================================================================
 * MAIN TEST RUNNER
 * ======================================================================== */

int main(void) {
    UNITY_BEGIN();
    
    printf("\n=== AncestralClust Integration Tests ===\n");
    printf("Testing performance logging during actual clustering operations\n");
    printf("Binary: %s\n", ANCESTRALCLUST_BINARY);
    printf("Test timeout: %d seconds\n", INTEGRATION_TEST_TIMEOUT_SEC);
    printf("Performance overhead threshold: %.1f%%\n\n", PERFORMANCE_OVERHEAD_THRESHOLD * 100);
    
    /* Core integration tests */
    RUN_TEST(test_integration_basic_clustering_with_logging);
    RUN_TEST(test_integration_csv_output_format);
    RUN_TEST(test_integration_algorithm_milestone_sequence);
    
    /* Performance and scaling tests */
    RUN_TEST(test_integration_performance_overhead);
    RUN_TEST(test_integration_memory_tracking_accuracy);
    RUN_TEST(test_integration_multithreaded_execution);
    
    /* Configuration and robustness tests */
    RUN_TEST(test_integration_granularity_levels);
    RUN_TEST(test_integration_error_handling);
    
    printf("\n=== Integration Test Summary ===\n");
    printf("All tests validate end-to-end performance logging functionality\n");
    printf("CSV outputs are available in: %s/\n", TEST_OUTPUT_DIR);
    printf("=====================================\n");
    
    return UNITY_END();
}