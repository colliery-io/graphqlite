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

/* Setup function - create test database with sample data */
static int setup_executor_expressions_suite(void)
{
    /* Create in-memory database for testing */
    int rc = sqlite3_open(":memory:", &test_db);
    if (rc != SQLITE_OK) {
        return -1;
    }

    /* Initialize schema */
    cypher_schema_manager *schema_mgr = cypher_schema_create_manager(test_db);
    if (!schema_mgr) {
        return -1;
    }

    if (cypher_schema_initialize(schema_mgr) < 0) {
        cypher_schema_free_manager(schema_mgr);
        return -1;
    }

    cypher_schema_free_manager(schema_mgr);

    /* Create executor */
    executor = cypher_executor_create(test_db);
    if (!executor) {
        return -1;
    }

    /* Create test data */
    const char *setup_queries[] = {
        "CREATE (a:Person {name: \"Alice\", age: 30, score: 85})",
        "CREATE (b:Person {name: \"Bob\", age: 25, score: NULL})",
        "CREATE (c:Person {name: \"Charlie\", age: 35, score: 92})",
        "CREATE (d:Person {name: \"Diana\", age: 28})",
        "CREATE (e:Item {name: \"Widget\", price: 10, quantity: 5})",
        "CREATE (f:Item {name: \"Gadget\", price: 25, quantity: 3})",
        NULL
    };

    for (int i = 0; setup_queries[i] != NULL; i++) {
        cypher_result *result = cypher_executor_execute(executor, setup_queries[i]);
        if (!result || !result->success) {
            if (result) {
                printf("Setup query failed: %s\n", result->error_message);
                cypher_result_free(result);
            }
            return -1;
        }
        cypher_result_free(result);
    }

    return 0;
}

/* Teardown function */
static int teardown_executor_expressions_suite(void)
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
 * CASE Expression Tests
 * ============================================================ */

/* Test simple CASE expression */
static void test_case_simple(void)
{
    const char *query = "RETURN CASE WHEN 1 = 1 THEN 'yes' ELSE 'no' END AS result";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nSimple CASE failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "yes");
        }
        cypher_result_free(result);
    }
}

/* Test CASE with multiple WHEN clauses */
static void test_case_multiple_when(void)
{
    const char *query =
        "MATCH (n:Person) "
        "RETURN n.name, "
        "CASE "
        "  WHEN n.age < 26 THEN 'young' "
        "  WHEN n.age < 31 THEN 'mid' "
        "  ELSE 'senior' "
        "END AS category "
        "ORDER BY n.name";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nMultiple WHEN CASE failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test CASE with ELSE clause */
static void test_case_with_else(void)
{
    const char *query =
        "RETURN CASE WHEN 1 = 2 THEN 'a' WHEN 2 = 3 THEN 'b' ELSE 'default' END AS result";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nCASE with ELSE failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "default");
        }
        cypher_result_free(result);
    }
}

/* Test CASE without ELSE (should return NULL) */
static void test_case_without_else(void)
{
    const char *query = "RETURN CASE WHEN 1 = 2 THEN 'found' END AS result";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nCASE without ELSE failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        /* Result should be NULL when no match and no ELSE */
        cypher_result_free(result);
    }
}

/* Test nested CASE expressions */
static void test_case_nested(void)
{
    const char *query =
        "RETURN CASE "
        "  WHEN 1 = 1 THEN CASE WHEN 2 = 2 THEN 'nested-yes' ELSE 'nested-no' END "
        "  ELSE 'outer-no' "
        "END AS result";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nNested CASE failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "nested-yes");
        }
        cypher_result_free(result);
    }
}

/* ============================================================
 * COALESCE Tests
 * ============================================================ */

/* Test COALESCE returns first non-null */
static void test_coalesce_first_non_null(void)
{
    const char *query = "RETURN coalesce(NULL, NULL, 'first', 'second') AS result";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nCOALESCE first non-null failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "first");
        }
        cypher_result_free(result);
    }
}

/* Test COALESCE with all NULL values */
static void test_coalesce_all_null(void)
{
    const char *query = "RETURN coalesce(NULL, NULL) AS result";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nCOALESCE all null failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        /* Result should be NULL */
        cypher_result_free(result);
    }
}

/* Test COALESCE with property access */
static void test_coalesce_with_property(void)
{
    const char *query =
        "MATCH (n:Person) "
        "RETURN n.name, coalesce(n.score, 0) AS score "
        "ORDER BY n.name";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nCOALESCE with property failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* ============================================================
 * List Literal Tests
 * ============================================================ */

/* Test basic list literal */
static void test_list_literal(void)
{
    const char *query = "RETURN [1, 2, 3] AS numbers";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nList literal failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test empty list literal */
static void test_list_literal_empty(void)
{
    const char *query = "RETURN [] AS empty_list";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nEmpty list literal failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test mixed type list */
static void test_list_literal_mixed(void)
{
    const char *query = "RETURN [1, 'two', 3.0, true] AS mixed";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nMixed list literal failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test nested list */
static void test_list_literal_nested(void)
{
    const char *query = "RETURN [[1, 2], [3, 4]] AS nested";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nNested list literal failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* ============================================================
 * List Comprehension Tests
 * ============================================================ */

/* Test basic list comprehension */
static void test_list_comprehension_basic(void)
{
    const char *query = "RETURN [x IN [1, 2, 3] | x * 2] AS doubled";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nBasic list comprehension failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test list comprehension with WHERE filter */
static void test_list_comprehension_with_where(void)
{
    const char *query = "RETURN [x IN [1, 2, 3, 4, 5] WHERE x > 2 | x] AS filtered";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nList comprehension with WHERE failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test list comprehension with transform */
static void test_list_comprehension_with_transform(void)
{
    const char *query = "RETURN [x IN ['a', 'b', 'c'] | toUpper(x)] AS uppercased";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nList comprehension with transform failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* ============================================================
 * Map Literal Tests
 * ============================================================ */

/* Test basic map literal */
static void test_map_literal(void)
{
    const char *query = "RETURN {name: 'John', age: 30} AS person";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nMap literal failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test empty map literal */
static void test_map_literal_empty(void)
{
    const char *query = "RETURN {} AS empty_map";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nEmpty map literal failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test nested map literal */
static void test_map_literal_nested(void)
{
    const char *query = "RETURN {person: {name: 'John', address: {city: 'NYC'}}} AS nested";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nNested map literal failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* ============================================================
 * REDUCE Expression Tests
 * ============================================================ */

/* Test REDUCE for sum */
static void test_reduce_sum(void)
{
    const char *query = "RETURN reduce(total = 0, x IN [1, 2, 3, 4] | total + x) AS sum";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nREDUCE sum failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        /* sum should be 10 */
        cypher_result_free(result);
    }
}

/* Test REDUCE for string concatenation */
static void test_reduce_string_concat(void)
{
    const char *query = "RETURN reduce(s = '', x IN ['a', 'b', 'c'] | s + x) AS concat";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nREDUCE string concat failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        /* concat should be 'abc' */
        cypher_result_free(result);
    }
}

/* ============================================================
 * IN Operator Tests
 * ============================================================ */

/* Test IN operator with list - match found */
static void test_in_operator_list(void)
{
    const char *query = "RETURN 2 IN [1, 2, 3] AS found";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nIN operator list failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test IN operator - no match */
static void test_in_operator_no_match(void)
{
    const char *query = "RETURN 5 IN [1, 2, 3] AS found";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nIN operator no match failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test IN operator with strings */
static void test_in_operator_strings(void)
{
    const char *query = "RETURN 'b' IN ['a', 'b', 'c'] AS found";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nIN operator strings failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test IN operator in WHERE clause */
static void test_in_operator_where(void)
{
    const char *query =
        "MATCH (n:Person) "
        "WHERE n.name IN ['Alice', 'Bob'] "
        "RETURN n.name ORDER BY n.name";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nIN operator WHERE failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        if (result->success) {
            CU_ASSERT_EQUAL(result->row_count, 2);
        }
        cypher_result_free(result);
    }
}

/* ============================================================
 * NULL Handling Tests
 * ============================================================ */

/* Test NULL equals NULL (should be NULL, not true) */
static void test_null_equals_null(void)
{
    const char *query = "RETURN NULL = NULL AS result";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nNULL = NULL failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        /* In SQL/Cypher, NULL = NULL is NULL (not true) */
        cypher_result_free(result);
    }
}

/* Test NULL in arithmetic */
static void test_null_arithmetic(void)
{
    const char *query = "RETURN 1 + NULL AS result";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nNULL arithmetic failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        /* Result should be NULL */
        cypher_result_free(result);
    }
}

/* Test IS NULL check */
static void test_is_null_check(void)
{
    const char *query = "RETURN NULL IS NULL AS result";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nIS NULL check failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test IS NOT NULL check */
static void test_is_not_null_check(void)
{
    const char *query = "RETURN 5 IS NOT NULL AS result";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nIS NOT NULL check failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test property IS NULL in WHERE */
static void test_null_property_where(void)
{
    const char *query =
        "MATCH (n:Person) "
        "WHERE n.score IS NULL "
        "RETURN n.name";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nProperty IS NULL WHERE failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test property IS NOT NULL in WHERE */
static void test_not_null_property_where(void)
{
    const char *query =
        "MATCH (n:Person) "
        "WHERE n.score IS NOT NULL "
        "RETURN n.name ORDER BY n.name";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nProperty IS NOT NULL WHERE failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        /* Alice and Charlie have scores */
        cypher_result_free(result);
    }
}

/* ============================================================
 * Arithmetic Expression Tests
 * ============================================================ */

/* Test basic arithmetic operators */
static void test_arithmetic_basic(void)
{
    const char *query = "RETURN 10 + 5 AS add, 10 - 5 AS sub, 10 * 5 AS mul, 10 / 5 AS div";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nBasic arithmetic failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test modulo operator */
static void test_arithmetic_modulo(void)
{
    const char *query = "RETURN 10 % 3 AS result";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nModulo operator failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        /* Result should be 1 */
        cypher_result_free(result);
    }
}

/* Test unary minus */
static void test_arithmetic_unary_minus(void)
{
    const char *query = "RETURN -5 AS result";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nUnary minus failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test operator precedence */
static void test_arithmetic_precedence(void)
{
    const char *query = "RETURN 2 + 3 * 4 AS result"; /* Should be 14, not 20 */

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nOperator precedence failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test parentheses in expressions */
static void test_arithmetic_parentheses(void)
{
    const char *query = "RETURN (2 + 3) * 4 AS result"; /* Should be 20 */

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nParentheses failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* ============================================================
 * Comparison Expression Tests
 * ============================================================ */

/* Test comparison operators */
static void test_comparison_operators(void)
{
    const char *query =
        "RETURN 5 > 3 AS gt, 5 >= 5 AS gte, 3 < 5 AS lt, 3 <= 3 AS lte, 5 = 5 AS eq, 5 <> 3 AS neq";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nComparison operators failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test string comparison */
static void test_comparison_strings(void)
{
    const char *query = "RETURN 'abc' < 'abd' AS result";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nString comparison failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* ============================================================
 * Boolean Expression Tests
 * ============================================================ */

/* Test AND operator */
static void test_boolean_and(void)
{
    const char *query = "RETURN true AND true AS tt, true AND false AS tf, false AND false AS ff";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nBoolean AND failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test OR operator */
static void test_boolean_or(void)
{
    const char *query = "RETURN true OR false AS tf, false OR false AS ff, true OR true AS tt";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nBoolean OR failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test NOT operator */
static void test_boolean_not(void)
{
    const char *query = "RETURN NOT true AS nt, NOT false AS nf";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nBoolean NOT failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test XOR operator */
static void test_boolean_xor(void)
{
    const char *query = "RETURN true XOR false AS tf, true XOR true AS tt, false XOR false AS ff";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nBoolean XOR failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* ============================================================
 * String Operator Tests
 * ============================================================ */

/* Test STARTS WITH operator */
static void test_starts_with(void)
{
    const char *query = "RETURN 'hello world' STARTS WITH 'hello' AS result";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nSTARTS WITH failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test ENDS WITH operator */
static void test_ends_with(void)
{
    const char *query = "RETURN 'hello world' ENDS WITH 'world' AS result";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nENDS WITH failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test CONTAINS operator */
static void test_contains_operator(void)
{
    const char *query = "RETURN 'hello world' CONTAINS 'lo wo' AS result";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nCONTAINS failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        cypher_result_free(result);
    }
}

/* Test string concatenation - In Cypher, + concatenates strings */
static void test_string_concat(void)
{
    const char *query = "RETURN 'hello' + ' ' + 'world' AS result";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nString concatenation failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "hello world");
        }
        cypher_result_free(result);
    }
}

/* Initialize the expressions test suite */
int init_executor_expressions_suite(void)
{
    CU_pSuite suite = CU_add_suite("Executor Expressions",
                                    setup_executor_expressions_suite,
                                    teardown_executor_expressions_suite);
    if (!suite) {
        return CU_get_error();
    }

    /* CASE expression tests */
    if (!CU_add_test(suite, "CASE simple", test_case_simple) ||
        !CU_add_test(suite, "CASE multiple WHEN", test_case_multiple_when) ||
        !CU_add_test(suite, "CASE with ELSE", test_case_with_else) ||
        !CU_add_test(suite, "CASE without ELSE", test_case_without_else) ||
        !CU_add_test(suite, "CASE nested", test_case_nested) ||

        /* COALESCE tests */
        !CU_add_test(suite, "COALESCE first non-null", test_coalesce_first_non_null) ||
        !CU_add_test(suite, "COALESCE all null", test_coalesce_all_null) ||
        !CU_add_test(suite, "COALESCE with property", test_coalesce_with_property) ||

        /* List literal tests */
        !CU_add_test(suite, "List literal", test_list_literal) ||
        !CU_add_test(suite, "List literal empty", test_list_literal_empty) ||
        !CU_add_test(suite, "List literal mixed", test_list_literal_mixed) ||
        !CU_add_test(suite, "List literal nested", test_list_literal_nested) ||

        /* List comprehension tests */
        !CU_add_test(suite, "List comprehension basic", test_list_comprehension_basic) ||
        !CU_add_test(suite, "List comprehension with WHERE", test_list_comprehension_with_where) ||
        !CU_add_test(suite, "List comprehension with transform", test_list_comprehension_with_transform) ||

        /* Map literal tests */
        !CU_add_test(suite, "Map literal", test_map_literal) ||
        !CU_add_test(suite, "Map literal empty", test_map_literal_empty) ||
        !CU_add_test(suite, "Map literal nested", test_map_literal_nested) ||

        /* REDUCE tests */
        !CU_add_test(suite, "REDUCE sum", test_reduce_sum) ||
        !CU_add_test(suite, "REDUCE string concat", test_reduce_string_concat) ||

        /* IN operator tests */
        !CU_add_test(suite, "IN operator list", test_in_operator_list) ||
        !CU_add_test(suite, "IN operator no match", test_in_operator_no_match) ||
        !CU_add_test(suite, "IN operator strings", test_in_operator_strings) ||
        !CU_add_test(suite, "IN operator WHERE", test_in_operator_where) ||

        /* NULL handling tests */
        !CU_add_test(suite, "NULL equals NULL", test_null_equals_null) ||
        !CU_add_test(suite, "NULL arithmetic", test_null_arithmetic) ||
        !CU_add_test(suite, "IS NULL check", test_is_null_check) ||
        !CU_add_test(suite, "IS NOT NULL check", test_is_not_null_check) ||
        !CU_add_test(suite, "Property IS NULL WHERE", test_null_property_where) ||
        !CU_add_test(suite, "Property IS NOT NULL WHERE", test_not_null_property_where) ||

        /* Arithmetic tests */
        !CU_add_test(suite, "Arithmetic basic", test_arithmetic_basic) ||
        !CU_add_test(suite, "Arithmetic modulo", test_arithmetic_modulo) ||
        !CU_add_test(suite, "Arithmetic unary minus", test_arithmetic_unary_minus) ||
        !CU_add_test(suite, "Arithmetic precedence", test_arithmetic_precedence) ||
        !CU_add_test(suite, "Arithmetic parentheses", test_arithmetic_parentheses) ||

        /* Comparison tests */
        !CU_add_test(suite, "Comparison operators", test_comparison_operators) ||
        !CU_add_test(suite, "Comparison strings", test_comparison_strings) ||

        /* Boolean tests */
        !CU_add_test(suite, "Boolean AND", test_boolean_and) ||
        !CU_add_test(suite, "Boolean OR", test_boolean_or) ||
        !CU_add_test(suite, "Boolean NOT", test_boolean_not) ||
        !CU_add_test(suite, "Boolean XOR", test_boolean_xor) ||

        /* String operator tests */
        !CU_add_test(suite, "STARTS WITH", test_starts_with) ||
        !CU_add_test(suite, "ENDS WITH", test_ends_with) ||
        !CU_add_test(suite, "CONTAINS operator", test_contains_operator) ||
        !CU_add_test(suite, "String concatenation", test_string_concat))
    {
        return CU_get_error();
    }

    return CUE_SUCCESS;
}
