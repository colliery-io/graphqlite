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

/* Setup function */
static int setup_executor_functions_suite(void)
{
    int rc = sqlite3_open(":memory:", &test_db);
    if (rc != SQLITE_OK) {
        return -1;
    }

    cypher_schema_manager *schema_mgr = cypher_schema_create_manager(test_db);
    if (!schema_mgr) {
        return -1;
    }

    if (cypher_schema_initialize(schema_mgr) < 0) {
        cypher_schema_free_manager(schema_mgr);
        return -1;
    }

    cypher_schema_free_manager(schema_mgr);

    executor = cypher_executor_create(test_db);
    if (!executor) {
        return -1;
    }

    /* Create test data for entity function tests */
    const char *setup_queries[] = {
        "CREATE (n:Person:Employee {name: \"Alice\", age: 30, city: \"NYC\"})",
        "CREATE (n:Company {name: \"TechCorp\"})",
        NULL
    };

    for (int i = 0; setup_queries[i] != NULL; i++) {
        cypher_result *result = cypher_executor_execute(executor, setup_queries[i]);
        if (result) cypher_result_free(result);
    }

    return 0;
}

/* Teardown function */
static int teardown_executor_functions_suite(void)
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

/* ============================================================
 * String Functions
 * ============================================================ */

static void test_func_toupper(void)
{
    const char *query = "RETURN toUpper('hello') AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "HELLO");
        }
        cypher_result_free(result);
    }
}

static void test_func_tolower(void)
{
    const char *query = "RETURN toLower('HELLO') AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "hello");
        }
        cypher_result_free(result);
    }
}

static void test_func_trim(void)
{
    const char *query = "RETURN trim('  hello  ') AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "hello");
        }
        cypher_result_free(result);
    }
}

static void test_func_ltrim(void)
{
    const char *query = "RETURN lTrim('  hello') AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "hello");
        }
        cypher_result_free(result);
    }
}

static void test_func_rtrim(void)
{
    const char *query = "RETURN rTrim('hello  ') AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "hello");
        }
        cypher_result_free(result);
    }
}

static void test_func_substring(void)
{
    const char *query = "RETURN substring('hello world', 0, 5) AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "hello");
        }
        cypher_result_free(result);
    }
}

static void test_func_replace(void)
{
    const char *query = "RETURN replace('hello', 'l', 'x') AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "hexxo");
        }
        cypher_result_free(result);
    }
}

static void test_func_split(void)
{
    const char *query = "RETURN split('a,b,c', ',') AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_left(void)
{
    const char *query = "RETURN left('hello', 3) AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "hel");
        }
        cypher_result_free(result);
    }
}

static void test_func_right(void)
{
    const char *query = "RETURN right('hello', 3) AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "llo");
        }
        cypher_result_free(result);
    }
}

static void test_func_reverse(void)
{
    const char *query = "RETURN reverse('hello') AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        if (!result->success) {
            printf("\nreverse() failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "olleh");
        }
        cypher_result_free(result);
    }
}

static void test_func_size_string(void)
{
    const char *query = "RETURN size('hello') AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "5");
        }
        cypher_result_free(result);
    }
}

/* ============================================================
 * Math Functions
 * ============================================================ */

static void test_func_abs(void)
{
    const char *query = "RETURN abs(-5) AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        if (!result->success) {
            printf("\nabs() failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            /* abs(-5) should return 5 */
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "5");
        }
        cypher_result_free(result);
    }
}

static void test_func_sign(void)
{
    const char *query = "RETURN sign(-5) AS neg, sign(0) AS zero, sign(5) AS pos";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_ceil(void)
{
    const char *query = "RETURN ceil(4.3) AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_floor(void)
{
    const char *query = "RETURN floor(4.7) AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_round(void)
{
    const char *query = "RETURN round(4.5) AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_sqrt(void)
{
    const char *query = "RETURN sqrt(16) AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_log(void)
{
    const char *query = "RETURN log(2.718281828) AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_log10(void)
{
    const char *query = "RETURN log10(100) AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_exp(void)
{
    const char *query = "RETURN exp(1) AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_sin(void)
{
    const char *query = "RETURN sin(0) AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_cos(void)
{
    const char *query = "RETURN cos(0) AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_tan(void)
{
    const char *query = "RETURN tan(0) AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_rand(void)
{
    const char *query = "RETURN rand() AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_pi(void)
{
    const char *query = "RETURN pi() AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_e(void)
{
    const char *query = "RETURN e() AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* ============================================================
 * List Functions
 * ============================================================ */

static void test_func_head(void)
{
    const char *query = "RETURN head([1, 2, 3]) AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_tail(void)
{
    const char *query = "RETURN tail([1, 2, 3]) AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_last(void)
{
    const char *query = "RETURN last([1, 2, 3]) AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_range(void)
{
    const char *query = "RETURN range(1, 5) AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_range_step(void)
{
    const char *query = "RETURN range(0, 10, 2) AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_size_list(void)
{
    const char *query = "RETURN size([1, 2, 3, 4, 5]) AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        if (!result->success) {
            printf("\nsize() on list failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            /* size([1,2,3,4,5]) should return 5 */
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "5");
        }
        cypher_result_free(result);
    }
}

/* ============================================================
 * Type Conversion Functions
 * ============================================================ */

static void test_func_tostring(void)
{
    const char *query = "RETURN toString(42) AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "42");
        }
        cypher_result_free(result);
    }
}

static void test_func_tointeger(void)
{
    const char *query = "RETURN toInteger('42') AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "42");
        }
        cypher_result_free(result);
    }
}

static void test_func_tofloat(void)
{
    const char *query = "RETURN toFloat('3.14') AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_toboolean(void)
{
    const char *query = "RETURN toBoolean('true') AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* ============================================================
 * Entity Functions
 * ============================================================ */

static void test_func_id(void)
{
    const char *query = "MATCH (n:Person) RETURN id(n) AS node_id LIMIT 1";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_labels(void)
{
    const char *query = "MATCH (n:Person) RETURN labels(n) AS node_labels LIMIT 1";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_properties(void)
{
    const char *query = "MATCH (n:Person) RETURN properties(n) AS props LIMIT 1";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_keys(void)
{
    const char *query = "MATCH (n:Person) RETURN keys(n) AS prop_keys LIMIT 1";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* ============================================================
 * Utility Functions
 * ============================================================ */

static void test_func_timestamp(void)
{
    const char *query = "RETURN timestamp() AS ts";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

static void test_func_randomuuid(void)
{
    const char *query = "RETURN randomUUID() AS uuid";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/**
 * Regression test for GQLITE-T-0086: List function results preserve column aliases
 * Previously returned raw array [0,1,2,3,4,5] without column wrapper
 * Now should return [{"result": [0,1,2,3,4,5]}] with proper column name
 */
static void test_list_function_alias_regression(void)
{
    /* Test range() with alias */
    const char *query = "RETURN range(0, 3) AS nums";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 1);
        CU_ASSERT_EQUAL(result->column_count, 1);

        /* Verify column name is preserved */
        if (result->column_names && result->column_names[0]) {
            CU_ASSERT_STRING_EQUAL(result->column_names[0], "nums");
        }

        /* Verify data is the list */
        if (result->data && result->data[0] && result->data[0][0]) {
            /* Should contain [0,1,2,3] as JSON array */
            CU_ASSERT_TRUE(strstr(result->data[0][0], "0") != NULL);
            CU_ASSERT_TRUE(strstr(result->data[0][0], "3") != NULL);
        }

        cypher_result_free(result);
    }
}

/**
 * Regression test for GQLITE-T-0085: Simple CASE syntax
 * Previously only searched CASE worked: CASE WHEN n.status = 'active' THEN 1 ELSE 0 END
 * Now simple CASE also works: CASE n.status WHEN 'active' THEN 1 ELSE 0 END
 */
static void test_simple_case_syntax_regression(void)
{
    /* Create test data */
    const char *create_query = "CREATE (n:CaseTest {name: 'Alice', status: 'active'})";
    cypher_result *create_result = cypher_executor_execute(executor, create_query);
    CU_ASSERT_PTR_NOT_NULL(create_result);
    if (create_result) {
        CU_ASSERT_TRUE(create_result->success);
        cypher_result_free(create_result);
    }

    /* Test simple CASE with status matching 'active' */
    const char *query = "MATCH (n:CaseTest) RETURN CASE n.status WHEN 'active' THEN 1 WHEN 'inactive' THEN 0 ELSE -1 END AS is_active";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 1);
        CU_ASSERT_EQUAL(result->column_count, 1);

        /* Verify column name is correct */
        if (result->column_names && result->column_names[0]) {
            CU_ASSERT_STRING_EQUAL(result->column_names[0], "is_active");
        }

        /* Verify CASE evaluated correctly - should be 1 for 'active' */
        if (result->data && result->data[0] && result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "1");
        }

        cypher_result_free(result);
    }

    /* Clean up test data */
    const char *delete_query = "MATCH (n:CaseTest) DELETE n";
    cypher_result *delete_result = cypher_executor_execute(executor, delete_query);
    if (delete_result) {
        cypher_result_free(delete_result);
    }
}

/**
 * Regression test for GQLITE-T-0089: keys() function returns empty array
 * Previously keys(n) returned [] due to broken EXISTS with UNION ALL in SQL generation
 * Now returns proper array of property key names like ["name", "age"]
 */
static void test_keys_function_regression(void)
{
    /* Create test data with multiple properties */
    const char *create_query = "CREATE (n:KeysTest {name: 'Bob', age: 25, active: true})";
    cypher_result *create_result = cypher_executor_execute(executor, create_query);
    CU_ASSERT_PTR_NOT_NULL(create_result);
    if (create_result) {
        CU_ASSERT_TRUE(create_result->success);
        cypher_result_free(create_result);
    }

    /* Test keys() returns property names */
    const char *query = "MATCH (n:KeysTest) RETURN keys(n) AS prop_keys";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 1);
        CU_ASSERT_EQUAL(result->column_count, 1);

        /* Verify keys array contains our property names */
        if (result->data && result->data[0] && result->data[0][0]) {
            /* Should contain "name", "age", and "active" */
            CU_ASSERT_TRUE(strstr(result->data[0][0], "name") != NULL);
            CU_ASSERT_TRUE(strstr(result->data[0][0], "age") != NULL);
            CU_ASSERT_TRUE(strstr(result->data[0][0], "active") != NULL);
        }

        cypher_result_free(result);
    }

    /* Clean up test data */
    const char *delete_query = "MATCH (n:KeysTest) DELETE n";
    cypher_result *delete_result = cypher_executor_execute(executor, delete_query);
    if (delete_result) {
        cypher_result_free(delete_result);
    }
}

/**
 * Regression test for GQLITE-T-0084: 'end' keyword as identifier
 * Previously 'end' was reserved (for CASE...END) and couldn't be used as variable name.
 * Now 'end' can be used as node/relationship variable and in property access.
 */
static void test_end_as_identifier_regression(void)
{
    /* Create test data with relationships */
    const char *create_query = "CREATE (a:EndTest {name: 'Alice'})-[:KNOWS]->(b:EndTest {name: 'Bob'})";
    cypher_result *create_result = cypher_executor_execute(executor, create_query);
    CU_ASSERT_PTR_NOT_NULL(create_result);
    if (create_result) {
        CU_ASSERT_TRUE(create_result->success);
        cypher_result_free(create_result);
    }

    /* Test using 'end' as node variable with property access */
    const char *query = "MATCH (start:EndTest)-[:KNOWS]->(end) RETURN end.name AS end_name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 1);

        /* Verify we got Bob's name */
        if (result->data && result->data[0] && result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "Bob");
        }

        cypher_result_free(result);
    }

    /* Clean up test data */
    const char *delete_query = "MATCH (n:EndTest) DETACH DELETE n";
    cypher_result *delete_result = cypher_executor_execute(executor, delete_query);
    if (delete_result) {
        cypher_result_free(delete_result);
    }
}

/* ===== New function tests added for 0.3.8+ ===== */

/* isEmpty() */
static void test_func_isempty_string(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN isEmpty('') AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0 && result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "1");
        }
        cypher_result_free(result);
    }
}

static void test_func_isempty_nonempty(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN isEmpty('hello') AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0 && result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "0");
        }
        cypher_result_free(result);
    }
}

/* btrim() */
static void test_func_btrim(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN btrim('  hello  ') AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0 && result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "hello");
        }
        cypher_result_free(result);
    }
}

/* toIntegerOrNull() */
static void test_func_tointegerornull_valid(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN toIntegerOrNull('42') AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0 && result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "42");
        }
        cypher_result_free(result);
    }
}

static void test_func_tointegerornull_invalid(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN toIntegerOrNull('hello') AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_PTR_NULL(result->data[0][0]);
        }
        cypher_result_free(result);
    }
}

/* toFloatOrNull() */
static void test_func_tofloatornull_valid(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN toFloatOrNull('3.14') AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0 && result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "3.14");
        }
        cypher_result_free(result);
    }
}

static void test_func_tofloatornull_invalid(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN toFloatOrNull('nope') AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_PTR_NULL(result->data[0][0]);
        }
        cypher_result_free(result);
    }
}

/* toBooleanOrNull() */
static void test_func_tobooleanornull_valid(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN toBooleanOrNull('true') AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0 && result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "1");
        }
        cypher_result_free(result);
    }
}

static void test_func_tobooleanornull_invalid(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN toBooleanOrNull('maybe') AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_PTR_NULL(result->data[0][0]);
        }
        cypher_result_free(result);
    }
}

/* toStringOrNull() */
static void test_func_tostringornull(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN toStringOrNull(42) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0 && result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "42");
        }
        cypher_result_free(result);
    }
}

/* elementId() */
static void test_func_elementid(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "MATCH (n:Person {name: 'Alice'}) RETURN elementId(n) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_TRUE(result->row_count > 0);
        if (result->row_count > 0 && result->data[0][0]) {
            /* elementId returns same as id() - an integer */
            CU_ASSERT_TRUE(atoi(result->data[0][0]) > 0);
        }
        cypher_result_free(result);
    }
}

/* nullIf() */
static void test_func_nullif_equal(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN nullIf(1, 1) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_PTR_NULL(result->data[0][0]);
        }
        cypher_result_free(result);
    }
}

static void test_func_nullif_different(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN nullIf(1, 2) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0 && result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "1");
        }
        cypher_result_free(result);
    }
}

/* valueType() */
static void test_func_valuetype(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN valueType(42) AS r1, valueType('hello') AS r2, valueType(3.14) AS r3");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "INTEGER");
            CU_ASSERT_STRING_EQUAL(result->data[0][1], "STRING");
            CU_ASSERT_STRING_EQUAL(result->data[0][2], "FLOAT");
        }
        cypher_result_free(result);
    }
}

/* char_length() / character_length() */
static void test_func_char_length(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN char_length('hello') AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0 && result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "5");
        }
        cypher_result_free(result);
    }
}

/* RETURN * */
static void test_return_star(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "MATCH (n:Person {name: 'Alice'}) RETURN *");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 1);
        CU_ASSERT_TRUE(result->column_count > 0);
        cypher_result_free(result);
    }
}

/* ===== Tranche 2 function tests ===== */

/* Trig functions */
static void test_func_atan2(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN atan2(1, 1) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_TRUE(result->row_count > 0);
        if (result->data[0][0]) {
            double val = atof(result->data[0][0]);
            CU_ASSERT_DOUBLE_EQUAL(val, 0.7854, 0.001);
        }
        cypher_result_free(result);
    }
}

static void test_func_degrees(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN degrees(3.141592653589793) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            double val = atof(result->data[0][0]);
            CU_ASSERT_DOUBLE_EQUAL(val, 180.0, 0.01);
        }
        cypher_result_free(result);
    }
}

static void test_func_radians(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN radians(180) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            double val = atof(result->data[0][0]);
            CU_ASSERT_DOUBLE_EQUAL(val, 3.14159, 0.001);
        }
        cypher_result_free(result);
    }
}

static void test_func_cot(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN cot(1.0) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_PTR_NOT_NULL(result->data[0][0]);
        cypher_result_free(result);
    }
}

static void test_func_haversin(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN haversin(1.0) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            double val = atof(result->data[0][0]);
            CU_ASSERT_DOUBLE_EQUAL(val, 0.2298, 0.001);
        }
        cypher_result_free(result);
    }
}

static void test_func_sinh(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN sinh(1.0) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            double val = atof(result->data[0][0]);
            CU_ASSERT_DOUBLE_EQUAL(val, 1.1752, 0.001);
        }
        cypher_result_free(result);
    }
}

static void test_func_cosh(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN cosh(1.0) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_PTR_NOT_NULL(result->data[0][0]);
        cypher_result_free(result);
    }
}

static void test_func_tanh(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN tanh(0.5) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_PTR_NOT_NULL(result->data[0][0]);
        cypher_result_free(result);
    }
}

static void test_func_isnan(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN isNaN(42) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "0");
        }
        cypher_result_free(result);
    }
}

/* Statistical aggregates */
static void test_func_stdev(void)
{
    /* Create test data */
    const char *queries[] = {
        "CREATE (n:StDev {val: 10})", "CREATE (n:StDev {val: 20})",
        "CREATE (n:StDev {val: 30})", "CREATE (n:StDev {val: 40})",
        "CREATE (n:StDev {val: 50})", NULL
    };
    for (int i = 0; queries[i]; i++) {
        cypher_result *r = cypher_executor_execute(executor, queries[i]);
        if (r) cypher_result_free(r);
    }

    cypher_result *result = cypher_executor_execute(executor,
        "MATCH (n:StDev) RETURN stDev(n.val) AS sd");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            double val = atof(result->data[0][0]);
            CU_ASSERT_DOUBLE_EQUAL(val, 15.811, 0.01);
        }
        cypher_result_free(result);
    }

    /* Cleanup */
    cypher_result *del = cypher_executor_execute(executor, "MATCH (n:StDev) DETACH DELETE n");
    if (del) cypher_result_free(del);
}

static void test_func_stdevp(void)
{
    const char *queries[] = {
        "CREATE (n:StDevP {val: 10})", "CREATE (n:StDevP {val: 20})",
        "CREATE (n:StDevP {val: 30})", "CREATE (n:StDevP {val: 40})",
        "CREATE (n:StDevP {val: 50})", NULL
    };
    for (int i = 0; queries[i]; i++) {
        cypher_result *r = cypher_executor_execute(executor, queries[i]);
        if (r) cypher_result_free(r);
    }

    cypher_result *result = cypher_executor_execute(executor,
        "MATCH (n:StDevP) RETURN stDevP(n.val) AS sd");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            double val = atof(result->data[0][0]);
            CU_ASSERT_DOUBLE_EQUAL(val, 14.142, 0.01);
        }
        cypher_result_free(result);
    }

    cypher_result *del = cypher_executor_execute(executor, "MATCH (n:StDevP) DETACH DELETE n");
    if (del) cypher_result_free(del);
}

/* List slicing */
static void test_list_slice_range(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN [1,2,3,4,5][1..3] AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "[2,3]");
        }
        cypher_result_free(result);
    }
}

static void test_list_slice_from(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN [1,2,3,4,5][2..] AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "[3,4,5]");
        }
        cypher_result_free(result);
    }
}

static void test_list_slice_to(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN [1,2,3,4,5][..2] AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "[1,2]");
        }
        cypher_result_free(result);
    }
}

/* RETURN * with relationship */
static void test_return_star_with_rel(void)
{
    cypher_result *setup = cypher_executor_execute(executor,
        "CREATE (a:RStar {id: 'rs1'})-[:RTEST]->(b:RStar {id: 'rs2'})");
    if (setup) cypher_result_free(setup);

    cypher_result *result = cypher_executor_execute(executor,
        "MATCH (a:RStar)-[r]->(b:RStar) RETURN *");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 1);
        CU_ASSERT_TRUE(result->column_count >= 3); /* a, r, b */
        cypher_result_free(result);
    }

    cypher_result *del = cypher_executor_execute(executor, "MATCH (n:RStar) DETACH DELETE n");
    if (del) cypher_result_free(del);
}

/* ===== Tranche 3: Temporal + Spatial tests ===== */

/* Temporal construction */
static void test_func_date_map(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN date({year: 2024, month: 3, day: 15}) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "2024-03-15");
        }
        cypher_result_free(result);
    }
}

static void test_func_date_string(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN date('2024-06-01') AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "2024-06-01");
        }
        cypher_result_free(result);
    }
}

static void test_func_time_map(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN time({hour: 14, minute: 30, second: 0}) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "14:30:00");
        }
        cypher_result_free(result);
    }
}

static void test_func_datetime_map(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN datetime({year: 2024, month: 6, day: 15, hour: 10, minute: 30}) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "2024-06-15T10:30:00");
        }
        cypher_result_free(result);
    }
}

static void test_func_duration_map(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN duration({days: 5, hours: 3}) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_PTR_NOT_NULL(result->data[0][0]);
        if (result->data[0][0]) {
            /* Should contain days:5 somewhere in JSON */
            CU_ASSERT_PTR_NOT_NULL(strstr(result->data[0][0], "\"days\":5"));
        }
        cypher_result_free(result);
    }
}

static void test_func_datetime_from_epoch(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN datetimeFromEpoch(0) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "1970-01-01 00:00:00");
        }
        cypher_result_free(result);
    }
}

static void test_func_duration_in_days(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN durationInDays('2024-01-01', '2024-03-15') AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "74");
        }
        cypher_result_free(result);
    }
}

static void test_func_duration_in_seconds(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN durationInSeconds('2024-01-01 00:00:00', '2024-01-01 01:30:00') AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "5400");
        }
        cypher_result_free(result);
    }
}

/* Duration arithmetic */
static void test_func_date_add(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN dateAdd('2024-01-15', {days: 30}) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "2024-02-14 00:00:00");
        }
        cypher_result_free(result);
    }
}

static void test_func_date_sub(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN dateSub('2024-06-15', {months: 3}) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "2024-03-15 00:00:00");
        }
        cypher_result_free(result);
    }
}

/* Spatial */
static void test_func_point_cartesian(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN point({x: 3, y: 4}) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_PTR_NOT_NULL(strstr(result->data[0][0], "\"srid\":7203"));
            CU_ASSERT_PTR_NOT_NULL(strstr(result->data[0][0], "\"x\":3"));
        }
        cypher_result_free(result);
    }
}

static void test_func_point_geographic(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN point({latitude: 40.7128, longitude: -74.006}) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_PTR_NOT_NULL(strstr(result->data[0][0], "\"srid\":4326"));
        }
        cypher_result_free(result);
    }
}

static void test_func_distance_euclidean(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN distance(point({x: 0, y: 0}), point({x: 3, y: 4})) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            double val = atof(result->data[0][0]);
            CU_ASSERT_DOUBLE_EQUAL(val, 5.0, 0.001);
        }
        cypher_result_free(result);
    }
}

static void test_func_distance_haversine(void)
{
    /* NYC to London ≈ 5,570 km */
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN distance(point({latitude: 40.7128, longitude: -74.006}), "
        "point({latitude: 51.5074, longitude: -0.1278})) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            double val = atof(result->data[0][0]);
            CU_ASSERT_TRUE(val > 5500000 && val < 5600000); /* ~5570 km in meters */
        }
        cypher_result_free(result);
    }
}

static void test_func_within_bbox_inside(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN pointWithinBBox(point({x: 5, y: 5}), point({x: 0, y: 0}), point({x: 10, y: 10})) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "1");
        }
        cypher_result_free(result);
    }
}

static void test_func_within_bbox_outside(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN pointWithinBBox(point({x: 15, y: 5}), point({x: 0, y: 0}), point({x: 10, y: 10})) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "0");
        }
        cypher_result_free(result);
    }
}

/* Additional temporal coverage */
static void test_func_localtime(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN localtime() AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_PTR_NOT_NULL(result->data[0][0]);
        cypher_result_free(result);
    }
}

static void test_func_datetime_from_epoch_millis(void)
{
    /* 86400000 ms = exactly 1 day after epoch */
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN datetimeFromEpochMillis(86400000) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "1970-01-02 00:00:00");
        }
        cypher_result_free(result);
    }
}

static void test_func_duration_in_months(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN durationInMonths('2024-01-01', '2024-07-01') AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            int val = atoi(result->data[0][0]);
            CU_ASSERT_TRUE(val >= 5 && val <= 6); /* ~6 months, approximate */
        }
        cypher_result_free(result);
    }
}

static void test_func_duration_between(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN durationBetween('2024-01-01', '2024-01-15') AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_PTR_NOT_NULL(strstr(result->data[0][0], "\"days\":14"));
        }
        cypher_result_free(result);
    }
}

static void test_func_date_truncate(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN dateTruncate('month', '2024-03-15') AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "2024-03-01");
        }
        cypher_result_free(result);
    }
}

static void test_func_date_add_mixed(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN dateAdd('2024-01-15', {years: 1, months: 2, days: 10}) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "2025-03-25 00:00:00");
        }
        cypher_result_free(result);
    }
}

static void test_func_date_add_with_duration(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN dateAdd('2024-01-01', duration({days: 100})) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "2024-04-10 00:00:00");
        }
        cypher_result_free(result);
    }
}

/* Additional spatial coverage */
static void test_func_point_3d(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN point({x: 1, y: 2, z: 3}) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_PTR_NOT_NULL(strstr(result->data[0][0], "\"z\":3"));
        }
        cypher_result_free(result);
    }
}

static void test_func_point_geo_with_height(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN point({latitude: 40.71, longitude: -74.00, height: 100}) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_PTR_NOT_NULL(strstr(result->data[0][0], "\"height\":100"));
        }
        cypher_result_free(result);
    }
}

static void test_func_within_bbox_geographic(void)
{
    /* Point in NYC, bbox covers NYC area */
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN pointWithinBBox("
        "point({latitude: 40.71, longitude: -74.00}), "
        "point({latitude: 40.0, longitude: -75.0}), "
        "point({latitude: 41.0, longitude: -73.0})) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "1");
        }
        cypher_result_free(result);
    }
}

static void test_func_distance_zero(void)
{
    cypher_result *result = cypher_executor_execute(executor,
        "RETURN distance(point({x: 5, y: 5}), point({x: 5, y: 5})) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            double val = atof(result->data[0][0]);
            CU_ASSERT_DOUBLE_EQUAL(val, 0.0, 0.001);
        }
        cypher_result_free(result);
    }
}

/* Missing coverage tests */
static void test_func_coth(void)
{
    cypher_result *result = cypher_executor_execute(executor, "RETURN coth(1.0) AS result");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->data[0][0]) {
            double val = atof(result->data[0][0]);
            CU_ASSERT_TRUE(val > 1.31 && val < 1.32); /* coth(1) ≈ 1.3130 */
        }
        cypher_result_free(result);
    }
}

static void test_func_percentile_error(void)
{
    /* percentileCont/Disc currently return error — verify graceful handling */
    cypher_result *result = cypher_executor_execute(executor,
        "MATCH (n:Person) RETURN percentileDisc(n.age, 0.5) AS median");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        /* Should fail gracefully with error, not crash */
        CU_ASSERT_FALSE(result->success);
        cypher_result_free(result);
    }
}

/* Initialize the functions test suite */
int init_executor_functions_suite(void)
{
    CU_pSuite suite = CU_add_suite("Executor Functions",
                                    setup_executor_functions_suite,
                                    teardown_executor_functions_suite);
    if (!suite) {
        return CU_get_error();
    }

    /* String functions */
    if (!CU_add_test(suite, "toUpper()", test_func_toupper) ||
        !CU_add_test(suite, "toLower()", test_func_tolower) ||
        !CU_add_test(suite, "trim()", test_func_trim) ||
        !CU_add_test(suite, "lTrim()", test_func_ltrim) ||
        !CU_add_test(suite, "rTrim()", test_func_rtrim) ||
        !CU_add_test(suite, "substring()", test_func_substring) ||
        !CU_add_test(suite, "replace()", test_func_replace) ||
        !CU_add_test(suite, "split()", test_func_split) ||
        !CU_add_test(suite, "left()", test_func_left) ||
        !CU_add_test(suite, "right()", test_func_right) ||
        !CU_add_test(suite, "reverse()", test_func_reverse) ||
        !CU_add_test(suite, "size() on string", test_func_size_string) ||

        /* Math functions */
        !CU_add_test(suite, "abs()", test_func_abs) ||
        !CU_add_test(suite, "sign()", test_func_sign) ||
        !CU_add_test(suite, "ceil()", test_func_ceil) ||
        !CU_add_test(suite, "floor()", test_func_floor) ||
        !CU_add_test(suite, "round()", test_func_round) ||
        !CU_add_test(suite, "sqrt()", test_func_sqrt) ||
        !CU_add_test(suite, "log()", test_func_log) ||
        !CU_add_test(suite, "log10()", test_func_log10) ||
        !CU_add_test(suite, "exp()", test_func_exp) ||
        !CU_add_test(suite, "sin()", test_func_sin) ||
        !CU_add_test(suite, "cos()", test_func_cos) ||
        !CU_add_test(suite, "tan()", test_func_tan) ||
        !CU_add_test(suite, "rand()", test_func_rand) ||
        !CU_add_test(suite, "pi()", test_func_pi) ||
        !CU_add_test(suite, "e()", test_func_e) ||

        /* List functions */
        !CU_add_test(suite, "head()", test_func_head) ||
        !CU_add_test(suite, "tail()", test_func_tail) ||
        !CU_add_test(suite, "last()", test_func_last) ||
        !CU_add_test(suite, "range()", test_func_range) ||
        !CU_add_test(suite, "range() with step", test_func_range_step) ||
        !CU_add_test(suite, "size() on list", test_func_size_list) ||

        /* Type conversion functions */
        !CU_add_test(suite, "toString()", test_func_tostring) ||
        !CU_add_test(suite, "toInteger()", test_func_tointeger) ||
        !CU_add_test(suite, "toFloat()", test_func_tofloat) ||
        !CU_add_test(suite, "toBoolean()", test_func_toboolean) ||

        /* Entity functions */
        !CU_add_test(suite, "id()", test_func_id) ||
        !CU_add_test(suite, "labels()", test_func_labels) ||
        !CU_add_test(suite, "properties()", test_func_properties) ||
        !CU_add_test(suite, "keys()", test_func_keys) ||

        /* Utility functions */
        !CU_add_test(suite, "timestamp()", test_func_timestamp) ||
        !CU_add_test(suite, "randomUUID()", test_func_randomuuid) ||

        /* Regression tests */
        !CU_add_test(suite, "List function alias regression", test_list_function_alias_regression) ||
        !CU_add_test(suite, "Simple CASE syntax regression", test_simple_case_syntax_regression) ||
        !CU_add_test(suite, "keys() function returns property names", test_keys_function_regression) ||
        !CU_add_test(suite, "'end' as identifier regression", test_end_as_identifier_regression) ||

        /* New functions (0.3.8+) */
        !CU_add_test(suite, "isEmpty() empty string", test_func_isempty_string) ||
        !CU_add_test(suite, "isEmpty() non-empty", test_func_isempty_nonempty) ||
        !CU_add_test(suite, "btrim()", test_func_btrim) ||
        !CU_add_test(suite, "toIntegerOrNull() valid", test_func_tointegerornull_valid) ||
        !CU_add_test(suite, "toIntegerOrNull() invalid", test_func_tointegerornull_invalid) ||
        !CU_add_test(suite, "toFloatOrNull() valid", test_func_tofloatornull_valid) ||
        !CU_add_test(suite, "toFloatOrNull() invalid", test_func_tofloatornull_invalid) ||
        !CU_add_test(suite, "toBooleanOrNull() valid", test_func_tobooleanornull_valid) ||
        !CU_add_test(suite, "toBooleanOrNull() invalid", test_func_tobooleanornull_invalid) ||
        !CU_add_test(suite, "toStringOrNull()", test_func_tostringornull) ||
        !CU_add_test(suite, "elementId()", test_func_elementid) ||
        !CU_add_test(suite, "nullIf() equal", test_func_nullif_equal) ||
        !CU_add_test(suite, "nullIf() different", test_func_nullif_different) ||
        !CU_add_test(suite, "valueType()", test_func_valuetype) ||
        !CU_add_test(suite, "char_length()", test_func_char_length) ||
        !CU_add_test(suite, "RETURN *", test_return_star) ||

        /* Tranche 2: Trig functions */
        !CU_add_test(suite, "atan2()", test_func_atan2) ||
        !CU_add_test(suite, "degrees()", test_func_degrees) ||
        !CU_add_test(suite, "radians()", test_func_radians) ||
        !CU_add_test(suite, "cot()", test_func_cot) ||
        !CU_add_test(suite, "haversin()", test_func_haversin) ||
        !CU_add_test(suite, "sinh()", test_func_sinh) ||
        !CU_add_test(suite, "cosh()", test_func_cosh) ||
        !CU_add_test(suite, "tanh()", test_func_tanh) ||
        !CU_add_test(suite, "isNaN()", test_func_isnan) ||

        /* Tranche 2: Statistical aggregates */
        !CU_add_test(suite, "stDev()", test_func_stdev) ||
        !CU_add_test(suite, "stDevP()", test_func_stdevp) ||

        /* Tranche 2: List slicing */
        !CU_add_test(suite, "list[1..3] slice", test_list_slice_range) ||
        !CU_add_test(suite, "list[2..] slice from", test_list_slice_from) ||
        !CU_add_test(suite, "list[..2] slice to", test_list_slice_to) ||

        /* Tranche 2: RETURN * with relationship */
        !CU_add_test(suite, "RETURN * with rel", test_return_star_with_rel) ||

        /* Tranche 3: Temporal */
        !CU_add_test(suite, "date({map})", test_func_date_map) ||
        !CU_add_test(suite, "date(string)", test_func_date_string) ||
        !CU_add_test(suite, "time({map})", test_func_time_map) ||
        !CU_add_test(suite, "datetime({map})", test_func_datetime_map) ||
        !CU_add_test(suite, "duration({map})", test_func_duration_map) ||
        !CU_add_test(suite, "datetimeFromEpoch()", test_func_datetime_from_epoch) ||
        !CU_add_test(suite, "durationInDays()", test_func_duration_in_days) ||
        !CU_add_test(suite, "durationInSeconds()", test_func_duration_in_seconds) ||
        !CU_add_test(suite, "dateAdd()", test_func_date_add) ||
        !CU_add_test(suite, "dateSub()", test_func_date_sub) ||

        /* Tranche 3: Spatial */
        !CU_add_test(suite, "point() Cartesian", test_func_point_cartesian) ||
        !CU_add_test(suite, "point() Geographic", test_func_point_geographic) ||
        !CU_add_test(suite, "distance() Euclidean", test_func_distance_euclidean) ||
        !CU_add_test(suite, "distance() Haversine", test_func_distance_haversine) ||
        !CU_add_test(suite, "withinBBox inside", test_func_within_bbox_inside) ||
        !CU_add_test(suite, "withinBBox outside", test_func_within_bbox_outside) ||

        /* Additional temporal coverage */
        !CU_add_test(suite, "localtime()", test_func_localtime) ||
        !CU_add_test(suite, "datetimeFromEpochMillis()", test_func_datetime_from_epoch_millis) ||
        !CU_add_test(suite, "durationInMonths()", test_func_duration_in_months) ||
        !CU_add_test(suite, "durationBetween()", test_func_duration_between) ||
        !CU_add_test(suite, "dateTruncate()", test_func_date_truncate) ||
        !CU_add_test(suite, "dateAdd() mixed", test_func_date_add_mixed) ||
        !CU_add_test(suite, "dateAdd() with duration()", test_func_date_add_with_duration) ||

        /* Additional spatial coverage */
        !CU_add_test(suite, "point() 3D", test_func_point_3d) ||
        !CU_add_test(suite, "point() geographic with height", test_func_point_geo_with_height) ||
        !CU_add_test(suite, "withinBBox geographic", test_func_within_bbox_geographic) ||
        !CU_add_test(suite, "distance() zero", test_func_distance_zero) ||

        /* Missing coverage */
        !CU_add_test(suite, "coth()", test_func_coth) ||
        !CU_add_test(suite, "percentile error handling", test_func_percentile_error))
    {
        return CU_get_error();
    }

    return CUE_SUCCESS;
}
