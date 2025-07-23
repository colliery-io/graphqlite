#include <CUnit/CUnit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/core/graphqlite_internal.h"
#include "../src/cypher/cypher_executor.h"

// Test database instance
static graphqlite_db_t *db = NULL;

// ============================================================================
// Setup and Teardown Functions
// ============================================================================

int setup_minimal_executor_test(void) {
    // Create in-memory database
    db = graphqlite_open(":memory:", 0);
    if (!db) {
        return -1;
    }
    
    // Create a couple of test nodes
    int64_t node1 = graphqlite_create_node(db);
    int64_t node2 = graphqlite_create_node(db);
    
    if (node1 <= 0 || node2 <= 0) {
        return -1;
    }
    
    return 0;
}

int teardown_minimal_executor_test(void) {
    if (db) {
        graphqlite_close(db);
        db = NULL;
    }
    return 0;
}

// ============================================================================
// Basic Executor Tests
// ============================================================================

void test_executor_basic_interface(void) {
    printf("Testing basic executor interface...\n");
    
    // Test 1: Execute a query (should return empty result, no crash)
    const char *query = "MATCH (n) RETURN n";
    printf("About to call cypher_execute_query...\n");
    
    cypher_result_t *result = cypher_execute_query(db, query);
    printf("cypher_execute_query returned: %p\n", (void*)result);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        // Should not have errors
        CU_ASSERT_FALSE(cypher_result_has_error(result));
        
        // Should have 1 column named 'n'
        size_t col_count = cypher_result_get_column_count(result);
        CU_ASSERT_EQUAL(col_count, 1);
        
        if (col_count > 0) {
            const char *col_name = cypher_result_get_column_name(result, 0);
            CU_ASSERT_PTR_NOT_NULL(col_name);
            if (col_name) {
                CU_ASSERT_STRING_EQUAL(col_name, "n");
            }
        }
        
        // Should have 0 rows (executor doesn't actually query yet)
        size_t row_count = cypher_result_get_row_count(result);
        CU_ASSERT_EQUAL(row_count, 0);
        
        printf("Query executed successfully: %zu columns, %zu rows\n", col_count, row_count);
        
        cypher_result_destroy(result);
    }
}

void test_executor_error_handling(void) {
    printf("Testing executor error handling...\n");
    
    // Test 1: NULL database
    cypher_result_t *result = cypher_execute_query(NULL, "MATCH (n) RETURN n");
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_TRUE(cypher_result_has_error(result));
        const char *error = cypher_result_get_error(result);
        CU_ASSERT_PTR_NOT_NULL(error);
        printf("Expected error with NULL db: %s\n", error);
        cypher_result_destroy(result);
    }
    
    // Test 2: NULL query
    result = cypher_execute_query(db, NULL);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_TRUE(cypher_result_has_error(result));
        const char *error = cypher_result_get_error(result);
        CU_ASSERT_PTR_NOT_NULL(error);
        printf("Expected error with NULL query: %s\n", error);
        cypher_result_destroy(result);
    }
}

void test_result_memory_management(void) {
    printf("Testing result memory management...\n");
    
    // Create and destroy multiple results to test for leaks
    for (int i = 0; i < 10; i++) {
        cypher_result_t *result = cypher_execute_query(db, "MATCH (n) RETURN n");
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            CU_ASSERT_FALSE(cypher_result_has_error(result));
            cypher_result_destroy(result);
        }
    }
    
    printf("Memory management test completed\n");
}

// ============================================================================
// Test Suite Registration
// ============================================================================

int add_minimal_executor_tests(void) {
    CU_pSuite suite = CU_add_suite("Minimal Executor Tests", 
                                   setup_minimal_executor_test, 
                                   teardown_minimal_executor_test);
    if (!suite) {
        return -1;
    }
    
    if (!CU_add_test(suite, "Basic executor interface", test_executor_basic_interface) ||
        !CU_add_test(suite, "Error handling", test_executor_error_handling) ||
        !CU_add_test(suite, "Memory management", test_result_memory_management)) {
        return -1;
    }
    
    return 0;
}