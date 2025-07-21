---
id: unit-test-framework-setup
level: task
title: "Unit test framework setup"
created_at: 2025-07-19T23:03:28.602234+00:00
updated_at: 2025-07-19T23:03:28.602234+00:00
parent: foundation-layer
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
---

# Unit test framework setup

## Parent Initiative

[[foundation-layer]]

## Objective

Establish a comprehensive unit testing framework for GraphQLite development that provides automated testing, performance validation, memory leak detection, and continuous integration support. The testing framework must validate all core functionality while maintaining fast execution times for rapid development cycles.

This framework ensures code quality, prevents regressions, and validates that all performance targets established in Phase Zero are maintained throughout development.

## Acceptance Criteria

- [ ] **Testing Framework**: Complete unit test framework with assertion macros and test discovery
- [ ] **Performance Tests**: Automated validation of performance targets (sub-5ms operations, 10K+ nodes/sec)
- [ ] **Memory Testing**: Memory leak detection and resource cleanup validation
- [ ] **Coverage Reporting**: Code coverage analysis with minimum 90% coverage target
- [ ] **CI Integration**: Automated test execution in continuous integration pipeline
- [ ] **Test Isolation**: Each test runs in isolated environment with clean database state
- [ ] **Parallel Execution**: Support for parallel test execution to minimize test suite runtime
- [ ] **Regression Testing**: Comprehensive test suite to prevent regressions in core functionality

## Implementation Notes

**Test Framework Architecture:**

```c
// Test framework core structures
typedef struct {
    char *name;
    void (*test_func)(void);
    bool should_run;
    double execution_time_ms;
    bool passed;
    char *failure_message;
} test_case_t;

typedef struct {
    char *suite_name;
    test_case_t *tests;
    size_t test_count;
    size_t tests_passed;
    size_t tests_failed;
    double total_time_ms;
} test_suite_t;

typedef struct {
    test_suite_t *suites;
    size_t suite_count;
    size_t total_tests;
    size_t total_passed;
    size_t total_failed;
    bool verbose_output;
    bool stop_on_failure;
} test_runner_t;
```

**Test Macros and Assertions:**

```c
// Basic assertion macros
#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            test_fail(__FILE__, __LINE__, "Expected true but got false: " #condition); \
            return; \
        } \
    } while(0)

#define ASSERT_FALSE(condition) \
    do { \
        if (condition) { \
            test_fail(__FILE__, __LINE__, "Expected false but got true: " #condition); \
            return; \
        } \
    } while(0)

#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            char msg[256]; \
            snprintf(msg, sizeof(msg), "Expected %ld but got %ld", (long)(expected), (long)(actual)); \
            test_fail(__FILE__, __LINE__, msg); \
            return; \
        } \
    } while(0)

#define ASSERT_STR_EQ(expected, actual) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            char msg[512]; \
            snprintf(msg, sizeof(msg), "Expected '%s' but got '%s'", (expected), (actual)); \
            test_fail(__FILE__, __LINE__, msg); \
            return; \
        } \
    } while(0)

#define ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            test_fail(__FILE__, __LINE__, "Expected NULL pointer"); \
            return; \
        } \
    } while(0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            test_fail(__FILE__, __LINE__, "Expected non-NULL pointer"); \
            return; \
        } \
    } while(0)

// Performance assertion macros
#define ASSERT_PERFORMANCE(operation, max_time_ms) \
    do { \
        struct timespec start, end; \
        clock_gettime(CLOCK_MONOTONIC, &start); \
        operation; \
        clock_gettime(CLOCK_MONOTONIC, &end); \
        double elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0 + \
                           (end.tv_nsec - start.tv_nsec) / 1000000.0; \
        if (elapsed_ms > (max_time_ms)) { \
            char msg[256]; \
            snprintf(msg, sizeof(msg), "Operation took %.2fms, expected < %.2fms", \
                    elapsed_ms, (double)(max_time_ms)); \
            test_fail(__FILE__, __LINE__, msg); \
            return; \
        } \
    } while(0)

// Memory leak detection macros
#define ASSERT_NO_MEMORY_LEAKS(operation) \
    do { \
        size_t initial_memory = get_current_memory_usage(); \
        operation; \
        size_t final_memory = get_current_memory_usage(); \
        if (final_memory > initial_memory + MEMORY_TOLERANCE) { \
            char msg[256]; \
            snprintf(msg, sizeof(msg), "Memory leak detected: %zu bytes", \
                    final_memory - initial_memory); \
            test_fail(__FILE__, __LINE__, msg); \
            return; \
        } \
    } while(0)
```

**Test Registration and Discovery:**

```c
// Test registration macro
#define REGISTER_TEST(suite_name, test_name) \
    static void test_##test_name(void); \
    static void __attribute__((constructor)) register_##test_name(void) { \
        register_test_case(#suite_name, #test_name, test_##test_name); \
    } \
    static void test_##test_name(void)

// Test suite registration
void register_test_case(const char *suite_name, const char *test_name, 
                       void (*test_func)(void)) {
    test_runner_t *runner = get_global_test_runner();
    
    // Find or create test suite
    test_suite_t *suite = find_or_create_suite(runner, suite_name);
    
    // Add test case to suite
    suite->tests = realloc(suite->tests, 
                          (suite->test_count + 1) * sizeof(test_case_t));
    
    test_case_t *test = &suite->tests[suite->test_count];
    test->name = strdup(test_name);
    test->test_func = test_func;
    test->should_run = true;
    test->passed = false;
    test->failure_message = NULL;
    
    suite->test_count++;
    runner->total_tests++;
}
```

**Database Test Fixtures:**

```c
// Test database management
typedef struct {
    graphqlite_db_t *db;
    char *temp_db_path;
    bool setup_complete;
} test_fixture_t;

test_fixture_t* create_test_fixture(void) {
    test_fixture_t *fixture = malloc(sizeof(test_fixture_t));
    
    // Create temporary database file
    char temp_path[] = "/tmp/graphqlite_test_XXXXXX";
    int fd = mkstemp(temp_path);
    close(fd);
    unlink(temp_path);  // Remove file, we just need the path
    
    fixture->temp_db_path = strdup(temp_path);
    
    // Open database and initialize schema
    fixture->db = graphqlite_open(fixture->temp_db_path);
    if (!fixture->db) {
        free(fixture->temp_db_path);
        free(fixture);
        return NULL;
    }
    
    // Create schema
    int rc = graphqlite_create_schema(fixture->db);
    if (rc != SQLITE_OK) {
        graphqlite_close(fixture->db);
        free(fixture->temp_db_path);
        free(fixture);
        return NULL;
    }
    
    fixture->setup_complete = true;
    return fixture;
}

void destroy_test_fixture(test_fixture_t *fixture) {
    if (fixture) {
        if (fixture->db) {
            graphqlite_close(fixture->db);
        }
        if (fixture->temp_db_path) {
            unlink(fixture->temp_db_path);  // Clean up temp file
            free(fixture->temp_db_path);
        }
        free(fixture);
    }
}

// Test fixture macros
#define SETUP_TEST_DB() \
    test_fixture_t *fixture = create_test_fixture(); \
    ASSERT_NOT_NULL(fixture); \
    graphqlite_db_t *db = fixture->db

#define TEARDOWN_TEST_DB() \
    destroy_test_fixture(fixture)
```

**Performance Test Framework:**

```c
// Performance test utilities
typedef struct {
    double min_time_ms;
    double max_time_ms;
    double avg_time_ms;
    size_t iterations;
    bool target_met;
} performance_result_t;

performance_result_t measure_performance(void (*operation)(void *), 
                                       void *data, 
                                       size_t iterations) {
    performance_result_t result = {0};
    result.iterations = iterations;
    result.min_time_ms = DBL_MAX;
    result.max_time_ms = 0.0;
    
    double total_time = 0.0;
    
    for (size_t i = 0; i < iterations; i++) {
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        operation(data);
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        double elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                           (end.tv_nsec - start.tv_nsec) / 1000000.0;
        
        total_time += elapsed_ms;
        
        if (elapsed_ms < result.min_time_ms) {
            result.min_time_ms = elapsed_ms;
        }
        if (elapsed_ms > result.max_time_ms) {
            result.max_time_ms = elapsed_ms;
        }
    }
    
    result.avg_time_ms = total_time / iterations;
    return result;
}

// Throughput measurement
typedef struct {
    size_t operations_completed;
    double total_time_ms;
    double operations_per_second;
    bool target_met;
} throughput_result_t;

throughput_result_t measure_throughput(void (*batch_operation)(void *, size_t),
                                      void *data,
                                      size_t target_ops_per_sec,
                                      double test_duration_ms) {
    throughput_result_t result = {0};
    
    struct timespec start, current;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    size_t batch_size = 1000;
    
    while (true) {
        clock_gettime(CLOCK_MONOTONIC, &current);
        double elapsed_ms = (current.tv_sec - start.tv_sec) * 1000.0 +
                           (current.tv_nsec - start.tv_nsec) / 1000000.0;
        
        if (elapsed_ms >= test_duration_ms) break;
        
        batch_operation(data, batch_size);
        result.operations_completed += batch_size;
    }
    
    result.total_time_ms = (current.tv_sec - start.tv_sec) * 1000.0 +
                          (current.tv_nsec - start.tv_nsec) / 1000000.0;
    
    result.operations_per_second = 
        (result.operations_completed * 1000.0) / result.total_time_ms;
    
    result.target_met = (result.operations_per_second >= target_ops_per_sec);
    
    return result;
}
```

**Example Test Cases:**

```c
// Node operation tests
REGISTER_TEST(node_operations, create_node) {
    SETUP_TEST_DB();
    
    int64_t node_id = graphqlite_create_node(db);
    ASSERT_TRUE(node_id > 0);
    
    bool exists = graphqlite_node_exists(db, node_id);
    ASSERT_TRUE(exists);
    
    TEARDOWN_TEST_DB();
}

REGISTER_TEST(node_operations, create_node_performance) {
    SETUP_TEST_DB();
    
    // Test single node creation performance
    ASSERT_PERFORMANCE({
        int64_t node_id = graphqlite_create_node(db);
        ASSERT_TRUE(node_id > 0);
    }, 5.0);  // Must complete in under 5ms
    
    TEARDOWN_TEST_DB();
}

REGISTER_TEST(node_operations, bulk_node_creation_throughput) {
    SETUP_TEST_DB();
    
    // Switch to interactive mode for throughput test
    graphqlite_switch_to_interactive_mode(db);
    
    typedef struct {
        graphqlite_db_t *db;
    } batch_data_t;
    
    batch_data_t batch_data = {.db = db};
    
    throughput_result_t result = measure_throughput(
        [](void *data, size_t count) {
            batch_data_t *bd = (batch_data_t*)data;
            for (size_t i = 0; i < count; i++) {
                int64_t node_id = graphqlite_create_node(bd->db);
                ASSERT_TRUE(node_id > 0);
            }
        },
        &batch_data,
        10000,  // Target: 10,000 nodes/second
        5000.0  // Test for 5 seconds
    );
    
    ASSERT_TRUE(result.target_met);
    
    TEARDOWN_TEST_DB();
}

// Property management tests
REGISTER_TEST(property_management, set_and_get_properties) {
    SETUP_TEST_DB();
    
    int64_t node_id = graphqlite_create_node(db);
    ASSERT_TRUE(node_id > 0);
    
    // Test integer property
    property_value_t int_value = {.type = PROP_INT, .value = {.int_val = 42}};
    int rc = graphqlite_set_property(db, ENTITY_NODE, node_id, "age", &int_value);
    ASSERT_EQ(SQLITE_OK, rc);
    
    property_value_t retrieved_value;
    rc = graphqlite_get_property(db, ENTITY_NODE, node_id, "age", &retrieved_value);
    ASSERT_EQ(SQLITE_OK, rc);
    ASSERT_EQ(PROP_INT, retrieved_value.type);
    ASSERT_EQ(42, retrieved_value.value.int_val);
    
    // Test text property
    property_value_t text_value = {.type = PROP_TEXT, .value = {.text_val = "John Doe"}};
    rc = graphqlite_set_property(db, ENTITY_NODE, node_id, "name", &text_value);
    ASSERT_EQ(SQLITE_OK, rc);
    
    rc = graphqlite_get_property(db, ENTITY_NODE, node_id, "name", &retrieved_value);
    ASSERT_EQ(SQLITE_OK, rc);
    ASSERT_EQ(PROP_TEXT, retrieved_value.type);
    ASSERT_STR_EQ("John Doe", retrieved_value.value.text_val);
    
    TEARDOWN_TEST_DB();
}

// Memory leak tests
REGISTER_TEST(memory_management, no_leaks_in_node_operations) {
    SETUP_TEST_DB();
    
    ASSERT_NO_MEMORY_LEAKS({
        for (int i = 0; i < 1000; i++) {
            int64_t node_id = graphqlite_create_node(db);
            ASSERT_TRUE(node_id > 0);
        }
    });
    
    TEARDOWN_TEST_DB();
}
```

**Test Runner Implementation:**

```c
int run_all_tests(test_runner_t *runner) {
    printf("Running %zu tests across %zu suites...\n\n", 
           runner->total_tests, runner->suite_count);
    
    for (size_t i = 0; i < runner->suite_count; i++) {
        test_suite_t *suite = &runner->suites[i];
        printf("Suite: %s\n", suite->suite_name);
        
        for (size_t j = 0; j < suite->test_count; j++) {
            test_case_t *test = &suite->tests[j];
            
            if (!test->should_run) continue;
            
            printf("  %s ... ", test->name);
            fflush(stdout);
            
            struct timespec start, end;
            clock_gettime(CLOCK_MONOTONIC, &start);
            
            // Run test in protected environment
            current_test = test;
            test->test_func();
            
            clock_gettime(CLOCK_MONOTONIC, &end);
            test->execution_time_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                                    (end.tv_nsec - start.tv_nsec) / 1000000.0;
            
            if (test->failure_message) {
                printf("FAILED (%.2fms)\n", test->execution_time_ms);
                printf("    %s\n", test->failure_message);
                suite->tests_failed++;
                runner->total_failed++;
                
                if (runner->stop_on_failure) {
                    return 1;
                }
            } else {
                printf("PASSED (%.2fms)\n", test->execution_time_ms);
                test->passed = true;
                suite->tests_passed++;
                runner->total_passed++;
            }
        }
        
        printf("\n");
    }
    
    // Print summary
    printf("Test Results:\n");
    printf("  Total: %zu\n", runner->total_tests);
    printf("  Passed: %zu\n", runner->total_passed);
    printf("  Failed: %zu\n", runner->total_failed);
    printf("  Success Rate: %.1f%%\n", 
           (double)runner->total_passed / runner->total_tests * 100.0);
    
    return (runner->total_failed == 0) ? 0 : 1;
}
```

**Build Integration:**

```makefile
# Test targets in Makefile
test: $(TEST_OBJECTS)
	$(CC) $(CFLAGS) -o test_runner $(TEST_OBJECTS) $(LDFLAGS) -lsqlite3 -lpthread

test-run: test
	./test_runner

test-coverage: test
	gcov $(TEST_SOURCES)
	lcov --capture --directory . --output-file coverage.info
	genhtml coverage.info --output-directory coverage_html

test-memory: test
	valgrind --leak-check=full --show-leak-kinds=all ./test_runner

test-performance: test
	./test_runner --performance-only

.PHONY: test test-run test-coverage test-memory test-performance
```

**CI/CD Integration:**
- Automated test execution on every commit
- Performance regression detection
- Memory leak detection with Valgrind
- Code coverage reporting with minimum thresholds
- Parallel test execution for faster feedback

## Status Updates

*To be added during implementation*