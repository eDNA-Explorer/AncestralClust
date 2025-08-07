/*
 * test_runner.c
 * 
 * Simple test runner for AncestralClust Unity tests
 * This runner can execute individual test files or all tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* Test executable paths */
static const char* test_executables[] = {
    "./tests/unit/test_performance",
    "./tests/unit/test_thread_safety",
    "./tests/stress/test_performance_stress",
    NULL  /* Sentinel */
};

/* Test names for reporting */
static const char* test_names[] = {
    "Performance Tests",
    "Thread Safety Tests",
    "Performance Stress Tests",
    NULL  /* Sentinel */
};

/*
 * Execute a single test executable and return its exit status
 */
int run_single_test(const char* executable, const char* test_name) {
    printf("\n============================================================\n");
    printf("Running: %s\n", test_name);
    printf("============================================================\n");
    
    pid_t pid = fork();
    if (pid == 0) {
        /* Child process - execute the test */
        execl(executable, executable, (char*)NULL);
        /* If execl returns, there was an error */
        perror("execl failed");
        exit(1);
    } else if (pid > 0) {
        /* Parent process - wait for child to complete */
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            if (exit_code == 0) {
                printf("\n✓ %s PASSED\n", test_name);
            } else {
                printf("\n✗ %s FAILED (exit code: %d)\n", test_name, exit_code);
            }
            return exit_code;
        } else {
            printf("\n✗ %s FAILED (abnormal termination)\n", test_name);
            return 1;
        }
    } else {
        /* Fork failed */
        perror("fork failed");
        return 1;
    }
}

/*
 * Check if a test executable exists and is executable
 */
int test_executable_exists(const char* executable) {
    return access(executable, X_OK) == 0;
}

/*
 * Run all available tests
 */
int run_all_tests(void) {
    int total_tests = 0;
    int passed_tests = 0;
    int failed_tests = 0;
    
    printf("AncestralClust Unity Test Runner\n");
    printf("================================\n");
    
    /* Count total tests first */
    for (int i = 0; test_executables[i] != NULL; i++) {
        if (test_executable_exists(test_executables[i])) {
            total_tests++;
        }
    }
    
    if (total_tests == 0) {
        printf("No test executables found. Please compile tests first.\n");
        printf("Use: make tests\n");
        return 1;
    }
    
    printf("Found %d test executable(s)\n", total_tests);
    
    /* Run each test */
    for (int i = 0; test_executables[i] != NULL; i++) {
        if (test_executable_exists(test_executables[i])) {
            int result = run_single_test(test_executables[i], test_names[i]);
            if (result == 0) {
                passed_tests++;
            } else {
                failed_tests++;
            }
        } else {
            printf("Warning: Test executable not found: %s\n", test_executables[i]);
        }
    }
    
    /* Print summary */
    printf("\n============================================================\n");
    printf("TEST SUMMARY\n");
    printf("============================================================\n");
    printf("Total Tests:  %d\n", total_tests);
    printf("Passed:       %d\n", passed_tests);
    printf("Failed:       %d\n", failed_tests);
    
    if (failed_tests == 0) {
        printf("\n🎉 ALL TESTS PASSED!\n");
        return 0;
    } else {
        printf("\n❌ %d TEST(S) FAILED\n", failed_tests);
        return 1;
    }
}

/*
 * Display usage information
 */
void print_usage(const char* program_name) {
    printf("Usage: %s [OPTIONS] [TEST_NAME]\n", program_name);
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help     Show this help message\n");
    printf("  -l, --list     List available tests\n");
    printf("\n");
    printf("Test Names:\n");
    for (int i = 0; test_names[i] != NULL; i++) {
        printf("  %s\n", test_names[i]);
    }
    printf("\n");
    printf("If no test name is specified, all tests will be run.\n");
}

/*
 * List available tests
 */
void list_tests(void) {
    printf("Available Tests:\n");
    printf("================\n");
    
    for (int i = 0; test_names[i] != NULL; i++) {
        const char* status = test_executable_exists(test_executables[i]) ? "✓" : "✗";
        printf("%s %s (%s)\n", status, test_names[i], test_executables[i]);
    }
}

/*
 * Find and run a specific test by name
 */
int run_specific_test(const char* test_name) {
    for (int i = 0; test_names[i] != NULL; i++) {
        if (strcmp(test_names[i], test_name) == 0) {
            if (test_executable_exists(test_executables[i])) {
                return run_single_test(test_executables[i], test_names[i]);
            } else {
                printf("Error: Test executable not found: %s\n", test_executables[i]);
                printf("Please compile tests first with: make tests\n");
                return 1;
            }
        }
    }
    
    printf("Error: Test '%s' not found.\n", test_name);
    printf("Use --list to see available tests.\n");
    return 1;
}

/*
 * Main entry point
 */
int main(int argc, char* argv[]) {
    /* Parse command line arguments */
    if (argc >= 2) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[1], "-l") == 0 || strcmp(argv[1], "--list") == 0) {
            list_tests();
            return 0;
        } else {
            /* Run specific test */
            return run_specific_test(argv[1]);
        }
    }
    
    /* No arguments - run all tests */
    return run_all_tests();
}