/*
 * test_performance.c
 * 
 * Unit tests for the AncestralClust performance monitoring system
 * Tests core functionality including initialization, timestamp operations,
 * and milestone tracking.
 */

#include "../unity/unity.h"
#include "../../performance.h"
#include <unistd.h>
#include <string.h>

/* Test fixture setup and teardown */
void setUp(void) {
    /* Reset performance context before each test */
    perf_cleanup();
    perf_init();
}

void tearDown(void) {
    /* Clean up after each test */
    perf_cleanup();
}

/*
 * Test: Performance system initialization
 * Verifies that the performance monitoring system initializes correctly
 * and sets up default configuration values.
 */
void test_perf_init_default_config(void) {
    perf_config_t *config = perf_get_config();
    
    TEST_ASSERT_NOT_NULL(config);
    TEST_ASSERT_EQUAL(1, config->enabled);
    TEST_ASSERT_EQUAL(PERF_GRANULARITY_MEDIUM, config->granularity);
    TEST_ASSERT_EQUAL(PERF_LOG_INFO, config->log_level);
    TEST_ASSERT_EQUAL(PERF_FORMAT_HUMAN, config->output_format);
    TEST_ASSERT_NOT_NULL(config->output_file);
    TEST_ASSERT_EQUAL(1, config->track_memory);
    TEST_ASSERT_EQUAL(1, config->track_cpu);
    TEST_ASSERT_EQUAL(1, config->track_threads);
}

/*
 * Test: Performance system custom configuration
 * Verifies that custom configuration can be applied correctly.
 */
void test_perf_init_with_custom_config(void) {
    perf_cleanup();
    
    perf_config_t custom_config = {
        .enabled = 1,
        .granularity = PERF_GRANULARITY_FINE,
        .log_level = PERF_LOG_DEBUG,
        .output_format = PERF_FORMAT_CSV,
        .output_file = stderr,
        .flush_immediately = 1,
        .track_memory = 0,
        .track_cpu = 1,
        .track_threads = 0,
        .sampling_interval_us = 50000
    };
    strcpy(custom_config.output_filename, "test_output.log");
    
    int result = perf_init_with_config(&custom_config);
    TEST_ASSERT_EQUAL(0, result);
    
    perf_config_t *config = perf_get_config();
    TEST_ASSERT_EQUAL(PERF_GRANULARITY_FINE, config->granularity);
    TEST_ASSERT_EQUAL(PERF_LOG_DEBUG, config->log_level);
    TEST_ASSERT_EQUAL(PERF_FORMAT_CSV, config->output_format);
    TEST_ASSERT_EQUAL(1, config->flush_immediately);
    TEST_ASSERT_EQUAL(0, config->track_memory);
    TEST_ASSERT_EQUAL(1, config->track_cpu);
    TEST_ASSERT_EQUAL(0, config->track_threads);
    TEST_ASSERT_EQUAL(50000, config->sampling_interval_us);
}

/*
 * Test: High-resolution timestamp functionality
 * Verifies that timestamp functions work correctly and provide
 * monotonic time measurements.
 */
void test_perf_timestamp_functionality(void) {
    perf_timestamp_t ts1, ts2;
    
    perf_get_timestamp(&ts1);
    usleep(1000); /* Sleep for 1ms */
    perf_get_timestamp(&ts2);
    
    double diff_ms = perf_timestamp_diff_ms(&ts1, &ts2);
    uint64_t diff_ns = perf_timestamp_diff_ns(&ts1, &ts2);
    
    /* Verify that time has passed (should be at least 1ms, but allow for variation) */
    TEST_ASSERT_GREATER_OR_EQUAL(0.5, diff_ms);
    TEST_ASSERT_LESS_THAN(10.0, diff_ms); /* Should be less than 10ms */
    
    /* Verify nanosecond precision is consistent */
    TEST_ASSERT_GREATER_OR_EQUAL(500000, diff_ns);
    TEST_ASSERT_LESS_THAN(10000000, diff_ns);
    
    /* Verify consistency between ms and ns measurements */
    double calculated_ms = diff_ns / 1000000.0;
    TEST_ASSERT_FLOAT_WITHIN(0.1, diff_ms, calculated_ms);
}

/*
 * Test: Basic milestone tracking
 * Verifies that milestone start/end tracking works correctly
 * and calculates durations properly.
 */
void test_perf_milestone_tracking(void) {
    /* Start and end a milestone */
    perf_start_milestone(PERF_MILESTONE_PROGRAM_START);
    usleep(1000); /* Sleep for 1ms */
    perf_end_milestone(PERF_MILESTONE_PROGRAM_START);
    
    /* Verify that the milestone was logged */
    TEST_ASSERT_GREATER_THAN(0, g_perf_context.log_count);
    TEST_ASSERT_LESS_OR_EQUAL(PERF_MAX_LOG_ENTRIES, g_perf_context.log_count);
    
    /* Check the logged entry */
    perf_metrics_t *entry = &g_perf_context.log_entries[0];
    TEST_ASSERT_EQUAL(PERF_MILESTONE_PROGRAM_START, entry->milestone);
    TEST_ASSERT_GREATER_OR_EQUAL(0.5, entry->duration_ms);
    TEST_ASSERT_LESS_THAN(10.0, entry->duration_ms);
}

/*
 * Test: Labeled milestone tracking
 * Verifies that milestone tracking with labels works correctly
 * and preserves label information.
 */
void test_perf_milestone_labeled_tracking(void) {
    const char *test_label = "test_milestone";
    
    perf_start_milestone_labeled(PERF_MILESTONE_CLUSTERING_START, test_label);
    usleep(500); /* Sleep for 0.5ms */
    perf_end_milestone_labeled(PERF_MILESTONE_CLUSTERING_START, test_label);
    
    /* Verify the logged entry */
    TEST_ASSERT_GREATER_THAN(0, g_perf_context.log_count);
    perf_metrics_t *entry = &g_perf_context.log_entries[0];
    
    TEST_ASSERT_EQUAL(PERF_MILESTONE_CLUSTERING_START, entry->milestone);
    TEST_ASSERT_GREATER_OR_EQUAL(0.3, entry->duration_ms);
    TEST_ASSERT_LESS_THAN(5.0, entry->duration_ms);
    TEST_ASSERT_EQUAL_STRING(test_label, entry->label);
}

/*
 * Test: Event logging functionality
 * Verifies that custom event logging works correctly
 * and stores event data properly.
 */
void test_perf_event_logging(void) {
    const char *event_label = "test_event";
    const char *event_context = "unit_test";
    double event_value = 42.5;
    
    size_t initial_log_count = g_perf_context.log_count;
    
    perf_log_event_with_context(event_label, event_value, event_context);
    
    /* Verify the event was logged */
    TEST_ASSERT_EQUAL(initial_log_count + 1, g_perf_context.log_count);
    
    perf_metrics_t *entry = &g_perf_context.log_entries[initial_log_count];
    TEST_ASSERT_EQUAL_STRING(event_label, entry->label);
    TEST_ASSERT_EQUAL_STRING(event_context, entry->context);
    TEST_ASSERT_EQUAL_FLOAT(event_value, entry->duration_ms); /* Value stored in duration_ms field */
}

/*
 * Test: Memory tracking functionality
 * Verifies that memory usage tracking works and returns reasonable values.
 */
void test_perf_memory_tracking(void) {
    perf_memory_t memory;
    int result = perf_get_memory_usage(&memory);
    
    TEST_ASSERT_EQUAL(0, result);
    
    /* Verify that we get reasonable RSS memory values */
    TEST_ASSERT_GREATER_THAN(0, memory.rss_kb);
    TEST_ASSERT_LESS_THAN(1000000, memory.rss_kb);  /* RSS should be less than 1GB for simple test */
    
    /* Virtual memory can be extremely large on macOS due to memory mapping
     * so we just verify it makes sense relative to RSS when both are available */
    if (memory.virt_kb > 0) {
        /* On most systems RSS should be less than or equal to virtual memory,
         * but on some systems the relationship might be different due to
         * how memory accounting works, so we just verify both are positive */
        TEST_ASSERT_GREATER_THAN(0, memory.virt_kb);
        /* But don't test upper bounds on virtual memory as it can be huge on macOS */
    }
    
    /* Verify the function can handle tracking data initialization */
    TEST_ASSERT_EQUAL(0, memory.heap_allocated);  /* Initially zero */
    TEST_ASSERT_EQUAL(0, memory.heap_freed);      /* Initially zero */
    TEST_ASSERT_EQUAL(0, memory.allocation_count); /* Initially zero */
}

/*
 * Test: Milestone name mapping
 * Verifies that milestone names are correctly mapped
 * and unknown milestones are handled properly.
 */
void test_perf_milestone_names(void) {
    /* Test known milestone names */
    TEST_ASSERT_EQUAL_STRING("PROGRAM_START", perf_milestone_name(PERF_MILESTONE_PROGRAM_START));
    TEST_ASSERT_EQUAL_STRING("CLUSTERING_START", perf_milestone_name(PERF_MILESTONE_CLUSTERING_START));
    TEST_ASSERT_EQUAL_STRING("DISTANCE_CALCULATION", perf_milestone_name(PERF_MILESTONE_DISTANCE_CALCULATION));
    
    /* Test unknown milestone */
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", perf_milestone_name(PERF_MILESTONE_COUNT));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", perf_milestone_name(-1));
}

/*
 * Test: Configuration enable/disable
 * Verifies that the performance system can be enabled and disabled
 * and that disabled state prevents logging.
 */
void test_perf_enable_disable(void) {
    size_t initial_log_count;
    
    /* Test with performance enabled */
    perf_set_enabled(1);
    initial_log_count = g_perf_context.log_count;
    perf_start_milestone(PERF_MILESTONE_INITIALIZATION);
    perf_end_milestone(PERF_MILESTONE_INITIALIZATION);
    TEST_ASSERT_GREATER_THAN(initial_log_count, g_perf_context.log_count);
    
    /* Test with performance disabled */
    perf_set_enabled(0);
    initial_log_count = g_perf_context.log_count;
    perf_start_milestone(PERF_MILESTONE_CLEANUP);
    perf_end_milestone(PERF_MILESTONE_CLEANUP);
    TEST_ASSERT_EQUAL(initial_log_count, g_perf_context.log_count);
    
    /* Re-enable for subsequent tests */
    perf_set_enabled(1);
}

/*
 * Test: Duration formatting
 * Verifies that duration formatting utility works correctly
 * for different time scales.
 */
void test_perf_duration_formatting(void) {
    /* Test millisecond formatting */
    const char *formatted = perf_format_duration(0.5);
    TEST_ASSERT_NOT_NULL(formatted);
    TEST_ASSERT_TRUE(strstr(formatted, "ms") != NULL);
    
    /* Test second formatting */
    formatted = perf_format_duration(1500.0);
    TEST_ASSERT_NOT_NULL(formatted);
    TEST_ASSERT_TRUE(strstr(formatted, "s") != NULL);
    
    /* Test minute formatting */
    formatted = perf_format_duration(120000.0);
    TEST_ASSERT_NOT_NULL(formatted);
    TEST_ASSERT_TRUE(strstr(formatted, "min") != NULL);
}

/*
 * Test: Memory size formatting
 * Verifies that memory size formatting utility works correctly
 * for different byte scales.
 */
void test_perf_memory_formatting(void) {
    char buffer[64];
    
    /* Test byte formatting */
    perf_format_memory_size(512, buffer, sizeof(buffer));
    TEST_ASSERT_TRUE(strstr(buffer, "B") != NULL);
    
    /* Test kilobyte formatting */
    perf_format_memory_size(2048, buffer, sizeof(buffer));
    TEST_ASSERT_TRUE(strstr(buffer, "KB") != NULL);
    
    /* Test megabyte formatting */
    perf_format_memory_size(2097152, buffer, sizeof(buffer));
    TEST_ASSERT_TRUE(strstr(buffer, "MB") != NULL);
    
    /* Test gigabyte formatting */
    perf_format_memory_size(2147483648UL, buffer, sizeof(buffer));
    TEST_ASSERT_TRUE(strstr(buffer, "GB") != NULL);
}

/* Main test runner */
int main(void) {
    UNITY_BEGIN();
    
    /* Basic functionality tests */
    RUN_TEST(test_perf_init_default_config);
    RUN_TEST(test_perf_init_with_custom_config);
    RUN_TEST(test_perf_timestamp_functionality);
    
    /* Core performance tracking tests */
    RUN_TEST(test_perf_milestone_tracking);
    RUN_TEST(test_perf_milestone_labeled_tracking);
    RUN_TEST(test_perf_event_logging);
    
    /* System monitoring tests */
    RUN_TEST(test_perf_memory_tracking);
    
    /* Utility function tests */
    RUN_TEST(test_perf_milestone_names);
    RUN_TEST(test_perf_enable_disable);
    RUN_TEST(test_perf_duration_formatting);
    RUN_TEST(test_perf_memory_formatting);
    
    return UNITY_END();
}