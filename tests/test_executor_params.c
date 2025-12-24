/*
 * Test parameterized query execution
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
static cypher_executor *test_executor = NULL;

/* Setup function - create test database with sample data */
static int setup_params_suite(void)
{
    int rc = sqlite3_open(":memory:", &test_db);
    if (rc != SQLITE_OK) {
        return -1;
    }

    test_executor = cypher_executor_create(test_db);
    if (!test_executor) {
        sqlite3_close(test_db);
        return -1;
    }

    /* Create test data */
    cypher_result *result;

    result = cypher_executor_execute(test_executor, "CREATE (n:Person {name: 'Alice', age: 30})");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(test_executor, "CREATE (n:Person {name: 'Bob', age: 25})");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(test_executor, "CREATE (n:Person {name: 'Charlie', age: 35})");
    if (result) cypher_result_free(result);

    return 0;
}

/* Teardown function */
static int teardown_params_suite(void)
{
    if (test_executor) {
        cypher_executor_free(test_executor);
        test_executor = NULL;
    }
    if (test_db) {
        sqlite3_close(test_db);
        test_db = NULL;
    }
    return 0;
}

/* Test parameterized query with string parameter */
static void test_param_string(void)
{
    const char *query = "MATCH (n:Person) WHERE n.name = $name RETURN n.name, n.age";
    const char *params = "{\"name\": \"Alice\"}";

    cypher_result *result = cypher_executor_execute_params(test_executor, query, params);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 1);

        if (result->row_count == 1 && result->data && result->data[0]) {
            /* Check that we got Alice */
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "Alice");
            CU_ASSERT_STRING_EQUAL(result->data[0][1], "30");
        }

        cypher_result_free(result);
    }
}

/* Test parameterized query with integer parameter */
static void test_param_integer(void)
{
    const char *query = "MATCH (n:Person) WHERE n.age > $min_age RETURN n.name, n.age ORDER BY n.age";
    const char *params = "{\"min_age\": 28}";

    cypher_result *result = cypher_executor_execute_params(test_executor, query, params);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 2);  /* Alice (30) and Charlie (35) */

        if (result->row_count == 2 && result->data) {
            /* With ORDER BY n.age, should be Alice (30) then Charlie (35) */
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "Alice");
            CU_ASSERT_STRING_EQUAL(result->data[1][0], "Charlie");
        }

        cypher_result_free(result);
    }
}

/* Test parameterized query with multiple parameters */
static void test_param_multiple(void)
{
    const char *query = "MATCH (n:Person) WHERE n.age >= $min AND n.age <= $max RETURN n.name ORDER BY n.age";
    const char *params = "{\"min\": 25, \"max\": 32}";

    cypher_result *result = cypher_executor_execute_params(test_executor, query, params);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 2);  /* Bob (25) and Alice (30) */

        if (result->row_count == 2 && result->data) {
            /* With ORDER BY n.age, should be Bob (25) then Alice (30) */
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "Bob");
            CU_ASSERT_STRING_EQUAL(result->data[1][0], "Alice");
        }

        cypher_result_free(result);
    }
}

/* Test parameterized query with no matches */
static void test_param_no_match(void)
{
    const char *query = "MATCH (n:Person) WHERE n.name = $name RETURN n.name";
    const char *params = "{\"name\": \"Nobody\"}";

    cypher_result *result = cypher_executor_execute_params(test_executor, query, params);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 0);  /* No matches */
        cypher_result_free(result);
    }
}

/* Test SQL injection prevention via parameters */
static void test_param_injection_safe(void)
{
    /* This malicious value should be treated as a literal string */
    const char *query = "MATCH (n:Person) WHERE n.name = $name RETURN n.name";
    const char *params = "{\"name\": \"'; DROP TABLE nodes; --\"}";

    cypher_result *result = cypher_executor_execute_params(test_executor, query, params);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 0);  /* Should not match anyone */
        cypher_result_free(result);
    }

    /* Verify data is still intact */
    result = cypher_executor_execute(test_executor, "MATCH (n:Person) RETURN count(n) as cnt");
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 1);
        if (result->row_count == 1 && result->data && result->data[0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "3");  /* All 3 records intact */
        }
        cypher_result_free(result);
    }
}

/* Test parameterized query with boolean parameter */
static void test_param_boolean(void)
{
    /* First create a node with boolean property */
    cypher_result *setup = cypher_executor_execute(test_executor,
        "CREATE (n:Flag {name: 'test', active: true})");
    if (setup) cypher_result_free(setup);

    const char *query = "MATCH (n:Flag) WHERE n.active = $active RETURN n.name";
    const char *params = "{\"active\": true}";

    cypher_result *result = cypher_executor_execute_params(test_executor, query, params);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 1);
        if (result->row_count == 1 && result->data && result->data[0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "test");
        }
        cypher_result_free(result);
    }
}

/* Test parameterized query with null parameter */
static void test_param_null(void)
{
    const char *query = "MATCH (n:Person) WHERE n.name = $name RETURN n.name";
    const char *params = "{\"name\": null}";

    cypher_result *result = cypher_executor_execute_params(test_executor, query, params);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 0);  /* NULL shouldn't match anything */
        cypher_result_free(result);
    }
}

/* Test parameterized query with float parameter */
static void test_param_float(void)
{
    /* Create node with float property */
    cypher_result *setup = cypher_executor_execute(test_executor,
        "CREATE (n:Score {name: 'high', value: 95.5})");
    if (setup) cypher_result_free(setup);

    setup = cypher_executor_execute(test_executor,
        "CREATE (n:Score {name: 'low', value: 45.5})");
    if (setup) cypher_result_free(setup);

    const char *query = "MATCH (n:Score) WHERE n.value > $threshold RETURN n.name";
    const char *params = "{\"threshold\": 50.0}";

    cypher_result *result = cypher_executor_execute_params(test_executor, query, params);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 1);
        if (result->row_count == 1 && result->data && result->data[0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "high");
        }
        cypher_result_free(result);
    }
}

/* Test execute without params (backward compatibility) */
static void test_no_params(void)
{
    const char *query = "MATCH (n:Person) WHERE n.name = 'Alice' RETURN n.name, n.age";

    cypher_result *result = cypher_executor_execute(test_executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 1);

        if (result->row_count == 1 && result->data && result->data[0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "Alice");
        }

        cypher_result_free(result);
    }
}

/* Test execute_params with NULL params (should work like execute) */
static void test_null_params_arg(void)
{
    const char *query = "MATCH (n:Person) WHERE n.name = 'Bob' RETURN n.name";

    cypher_result *result = cypher_executor_execute_params(test_executor, query, NULL);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 1);

        if (result->row_count == 1 && result->data && result->data[0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "Bob");
        }

        cypher_result_free(result);
    }
}

/* Register parameterized query tests */
int register_params_tests(void)
{
    CU_pSuite suite = CU_add_suite("Parameterized Query Tests",
                                    setup_params_suite,
                                    teardown_params_suite);
    if (!suite) {
        return CU_get_error();
    }

    if (!CU_add_test(suite, "String parameter", test_param_string) ||
        !CU_add_test(suite, "Integer parameter", test_param_integer) ||
        !CU_add_test(suite, "Multiple parameters", test_param_multiple) ||
        !CU_add_test(suite, "No match", test_param_no_match) ||
        !CU_add_test(suite, "SQL injection prevention", test_param_injection_safe) ||
        !CU_add_test(suite, "Boolean parameter", test_param_boolean) ||
        !CU_add_test(suite, "Null parameter", test_param_null) ||
        !CU_add_test(suite, "Float parameter", test_param_float) ||
        !CU_add_test(suite, "No params (backward compat)", test_no_params) ||
        !CU_add_test(suite, "NULL params argument", test_null_params_arg)) {
        return CU_get_error();
    }

    return CUE_SUCCESS;
}
