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
        !CU_add_test(suite, "'end' as identifier regression", test_end_as_identifier_regression))
    {
        return CU_get_error();
    }

    return CUE_SUCCESS;
}
