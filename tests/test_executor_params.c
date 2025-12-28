/**
 * Tests for parameterized Cypher queries
 *
 * Tests parameter substitution in:
 * - MATCH clause property filters
 * - CREATE clause property values
 * - WHERE clause conditions
 * - SET clause property updates
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <sqlite3.h>

#include "parser/cypher_parser.h"
#include "parser/cypher_ast.h"
#include "transform/cypher_transform.h"
#include "executor/cypher_executor.h"
#include "executor/cypher_schema.h"

/* Test database handle */
static sqlite3 *test_db = NULL;
static cypher_executor *executor = NULL;

/* Helper to execute a parameterized query and check success */
static cypher_result* exec_params(const char *query, const char *params_json)
{
    return cypher_executor_execute_params(executor, query, params_json);
}

/* Helper to execute a query without params */
static cypher_result* exec(const char *query)
{
    return cypher_executor_execute(executor, query);
}

/* Helper to find a value in result by column name */
static int result_contains_value(cypher_result *result, const char *col_name, const char *expected)
{
    if (!result || !result->success || result->row_count == 0) return 0;

    /* Find column index */
    int col_idx = -1;
    for (int c = 0; c < result->column_count; c++) {
        if (result->column_names[c] && strcmp(result->column_names[c], col_name) == 0) {
            col_idx = c;
            break;
        }
    }
    if (col_idx < 0) return 0;

    /* Search all rows for the expected value */
    for (int r = 0; r < result->row_count; r++) {
        if (result->data[r][col_idx] && strcmp(result->data[r][col_idx], expected) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Helper to get row count */
static int get_row_count(cypher_result *result)
{
    if (!result || !result->success) return 0;
    return result->row_count;
}

/* Helper to get a specific cell value */
static const char* get_cell(cypher_result *result, int row, const char *col_name)
{
    if (!result || !result->success || row >= result->row_count) return NULL;

    for (int c = 0; c < result->column_count; c++) {
        if (result->column_names[c] && strcmp(result->column_names[c], col_name) == 0) {
            return result->data[row][c];
        }
    }
    return NULL;
}

/* Setup - create database and test data */
static int setup_params_suite(void)
{
    int rc = sqlite3_open(":memory:", &test_db);
    if (rc != SQLITE_OK) return -1;

    cypher_schema_manager *schema_mgr = cypher_schema_create_manager(test_db);
    if (!schema_mgr) return -1;

    if (cypher_schema_initialize(schema_mgr) < 0) {
        cypher_schema_free_manager(schema_mgr);
        return -1;
    }
    cypher_schema_free_manager(schema_mgr);

    executor = cypher_executor_create(test_db);
    if (!executor) return -1;

    /* Create test data with literal values */
    cypher_result *r;
    r = exec("CREATE (:Person {name: \"Alice\", age: 30})");
    if (r) cypher_result_free(r);

    r = exec("CREATE (:Person {name: \"Bob\", age: 25})");
    if (r) cypher_result_free(r);

    r = exec("CREATE (:Person {name: \"Charlie\", age: 35})");
    if (r) cypher_result_free(r);

    return 0;
}

/* Teardown */
static int teardown_params_suite(void)
{
    if (executor) {
        cypher_executor_free(executor);
        executor = NULL;
    }
    if (test_db) {
        sqlite3_close(test_db);
        test_db = NULL;
    }
    return 0;
}

/*
 * TEST: MATCH with literal property filter (baseline)
 * This should work and filter correctly
 */
static void test_match_literal_filter(void)
{
    cypher_result *result = exec("MATCH (p:Person {name: \"Alice\"}) RETURN p.name AS name");

    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_TRUE(result->success);

    /* Should return exactly 1 result - Alice */
    CU_ASSERT_EQUAL(get_row_count(result), 1);
    CU_ASSERT_TRUE(result_contains_value(result, "name", "Alice"));

    if (result) cypher_result_free(result);
}

/*
 * TEST: MATCH with parameter in property filter
 * MATCH (p:Person {name: $name}) should filter by the parameter value
 */
static void test_match_param_filter(void)
{
    cypher_result *result = exec_params(
        "MATCH (p:Person {name: $name}) RETURN p.name AS name",
        "{\"name\": \"Alice\"}"
    );

    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_TRUE(result->success);

    /* Should return exactly 1 result - only Alice */
    CU_ASSERT_EQUAL_FATAL(get_row_count(result), 1);  /* FATAL: This is the key bug test */

    CU_ASSERT_TRUE(result_contains_value(result, "name", "Alice"));
    CU_ASSERT_FALSE(result_contains_value(result, "name", "Bob"));
    CU_ASSERT_FALSE(result_contains_value(result, "name", "Charlie"));

    if (result) cypher_result_free(result);
}

/*
 * TEST: CREATE with parameter in property value
 * CREATE (p:Person {name: $name}) should set the property from parameter
 */
static void test_create_param_property(void)
{
    cypher_result *result = exec_params(
        "CREATE (p:Person {name: $name, age: $age})",
        "{\"name\": \"Diana\", \"age\": 28}"
    );

    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_TRUE(result->success);
    CU_ASSERT_TRUE(result->nodes_created > 0);

    if (result) cypher_result_free(result);

    /* Verify the node was created with correct property value */
    result = exec("MATCH (p:Person {name: \"Diana\"}) RETURN p.name AS name, p.age AS age");

    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_TRUE(result->success);

    /* Should find Diana with age 28 */
    CU_ASSERT_TRUE(result_contains_value(result, "name", "Diana"));

    if (result) cypher_result_free(result);
}

/*
 * TEST: Multiple parameters in MATCH
 */
static void test_match_multiple_params(void)
{
    cypher_result *result = exec_params(
        "MATCH (p:Person) WHERE p.name = $name AND p.age = $age RETURN p.name AS name",
        "{\"name\": \"Alice\", \"age\": 30}"
    );

    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_TRUE(result->success);

    /* Should return exactly Alice */
    CU_ASSERT_EQUAL(get_row_count(result), 1);
    CU_ASSERT_TRUE(result_contains_value(result, "name", "Alice"));

    if (result) cypher_result_free(result);
}

/*
 * TEST: Parameter in WHERE clause
 */
static void test_where_param(void)
{
    cypher_result *result = exec_params(
        "MATCH (p:Person) WHERE p.age > $min_age RETURN p.name AS name ORDER BY p.age",
        "{\"min_age\": 28}"
    );

    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_TRUE(result->success);

    /* Should return Alice (30) and Charlie (35), not Bob (25) */
    CU_ASSERT_TRUE(result_contains_value(result, "name", "Alice"));
    CU_ASSERT_TRUE(result_contains_value(result, "name", "Charlie"));
    CU_ASSERT_FALSE(result_contains_value(result, "name", "Bob"));

    if (result) cypher_result_free(result);
}

/*
 * TEST: Parameter in SET clause
 */
static void test_set_param(void)
{
    /* First create a test node */
    cypher_result *r = exec("CREATE (:TestNode {value: 0})");
    if (r) cypher_result_free(r);

    cypher_result *result = exec_params(
        "MATCH (n:TestNode) SET n.value = $new_value RETURN n.value AS value",
        "{\"new_value\": 42}"
    );

    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_TRUE(result->success);

    if (result) cypher_result_free(result);

    /* Verify value was updated */
    result = exec("MATCH (n:TestNode) RETURN n.value AS value");
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_TRUE(result->success);

    const char *val = get_cell(result, 0, "value");
    CU_ASSERT_PTR_NOT_NULL(val);
    CU_ASSERT_TRUE(val && strcmp(val, "42") == 0);

    if (result) cypher_result_free(result);
}

/*
 * TEST: Integer parameter
 */
static void test_integer_param(void)
{
    cypher_result *result = exec_params(
        "MATCH (p:Person {age: $age}) RETURN p.name AS name",
        "{\"age\": 30}"
    );

    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_TRUE(result->success);

    /* Should find Alice (age 30) */
    CU_ASSERT_EQUAL(get_row_count(result), 1);
    CU_ASSERT_TRUE(result_contains_value(result, "name", "Alice"));

    if (result) cypher_result_free(result);
}

/*
 * TEST: Boolean parameter
 */
static void test_boolean_param(void)
{
    /* Create test node with boolean */
    exec("CREATE (:Feature {name: \"test\", enabled: true})");

    cypher_result *result = exec_params(
        "MATCH (f:Feature {enabled: $enabled}) RETURN f.name AS name",
        "{\"enabled\": true}"
    );

    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_TRUE(result->success);

    CU_ASSERT_TRUE(result_contains_value(result, "name", "test"));

    if (result) cypher_result_free(result);
}

/*
 * TEST: Null handling in parameters
 */
static void test_null_param(void)
{
    /* Create a node with null property */
    /* Note: 'optional' is a reserved keyword, use 'extra' instead */
    cypher_result *r = exec("CREATE (:TestNull {name: \"has_null\", extra: null})");
    if (r) cypher_result_free(r);

    cypher_result *result = exec_params(
        "MATCH (n:TestNull {name: $name}) RETURN n.extra AS opt",
        "{\"name\": \"has_null\"}"
    );

    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_TRUE(result->success);

    if (result) cypher_result_free(result);
}

/* Register all tests */
int register_params_tests(void)
{
    CU_pSuite suite = CU_add_suite(
        "Parameterized Query Tests",
        setup_params_suite,
        teardown_params_suite
    );

    if (!suite) return -1;

    /* Baseline test */
    if (!CU_add_test(suite, "MATCH with literal filter", test_match_literal_filter))
        return -1;

    /* Core parameter tests */
    if (!CU_add_test(suite, "MATCH with parameter in property filter", test_match_param_filter))
        return -1;
    if (!CU_add_test(suite, "CREATE with parameter property", test_create_param_property))
        return -1;
    if (!CU_add_test(suite, "MATCH with multiple parameters", test_match_multiple_params))
        return -1;

    /* WHERE clause */
    if (!CU_add_test(suite, "WHERE clause with parameter", test_where_param))
        return -1;

    /* SET clause */
    if (!CU_add_test(suite, "SET clause with parameter", test_set_param))
        return -1;

    /* Type-specific tests */
    if (!CU_add_test(suite, "Integer parameter", test_integer_param))
        return -1;
    if (!CU_add_test(suite, "Boolean parameter", test_boolean_param))
        return -1;
    if (!CU_add_test(suite, "Null parameter", test_null_param))
        return -1;

    return 0;
}
