/**
 * @file test_executor_predicates.c
 * @brief Tests for predicate expressions in Cypher executor
 *
 * Tests cover:
 * - EXISTS { pattern } - pattern existence check
 * - EXISTS(n.property) - property existence check
 * - NOT EXISTS
 * - STARTS WITH / ENDS WITH / CONTAINS operators
 * - startsWith() / endsWith() / contains() functions
 * - Label predicates (n:Label)
 * - IS NULL / IS NOT NULL
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <sqlite3.h>
#include "parser/cypher_parser.h"
#include "executor/cypher_executor.h"

static sqlite3 *test_db = NULL;
static cypher_executor *executor = NULL;

/* ============================================================
 * Suite Setup/Teardown
 * ============================================================ */

static int setup_suite(void)
{
    if (sqlite3_open(":memory:", &test_db) != SQLITE_OK) {
        return -1;
    }

    executor = cypher_executor_create(test_db);
    if (!executor) {
        sqlite3_close(test_db);
        return -1;
    }

    /* Create test data */
    const char *setup_queries[] = {
        "CREATE (a:Person {name: 'Alice', age: 30, email: 'alice@example.com'})",
        "CREATE (b:Person {name: 'Bob', age: 25})",
        "CREATE (c:Person {name: 'Charlie', age: 35, city: 'NYC'})",
        "CREATE (d:Developer:Person {name: 'Diana', age: 28, language: 'Python'})",
        "CREATE (e:Company {name: 'TechCorp'})",
        NULL
    };

    for (int i = 0; setup_queries[i] != NULL; i++) {
        cypher_result *result = cypher_executor_execute(executor, setup_queries[i]);
        if (result) cypher_result_free(result);
    }

    /* Create relationships */
    const char *rel_queries[] = {
        "MATCH (a:Person {name: 'Alice'}), (b:Person {name: 'Bob'}) CREATE (a)-[:KNOWS {since: 2020}]->(b)",
        "MATCH (b:Person {name: 'Bob'}), (c:Person {name: 'Charlie'}) CREATE (b)-[:KNOWS {since: 2021}]->(c)",
        "MATCH (a:Person {name: 'Alice'}), (e:Company {name: 'TechCorp'}) CREATE (a)-[:WORKS_AT]->(e)",
        NULL
    };

    for (int i = 0; rel_queries[i] != NULL; i++) {
        cypher_result *result = cypher_executor_execute(executor, rel_queries[i]);
        if (result) cypher_result_free(result);
    }

    return 0;
}

static int teardown_suite(void)
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
 * STARTS WITH / ENDS WITH / CONTAINS Operators
 * ============================================================ */

/* Test STARTS WITH operator */
static void test_starts_with_operator(void)
{
    const char *query = "MATCH (p:Person) WHERE p.name STARTS WITH 'A' RETURN p.name AS name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (!result->success) {
            printf("\nSTARTS WITH operator failed: %s\n", result->error_message);
        } else {
            /* Should find Alice */
            CU_ASSERT_EQUAL(result->row_count, 1);
            if (result->row_count > 0) {
                CU_ASSERT_STRING_EQUAL(result->data[0][0], "Alice");
            }
        }
        cypher_result_free(result);
    }
}

/* Test STARTS WITH with multiple matches */
static void test_starts_with_multiple(void)
{
    const char *query = "MATCH (p:Person) WHERE p.name STARTS WITH 'C' RETURN p.name AS name ORDER BY p.name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success) {
            CU_ASSERT_EQUAL(result->row_count, 1);
            if (result->row_count > 0) {
                CU_ASSERT_STRING_EQUAL(result->data[0][0], "Charlie");
            }
        }
        cypher_result_free(result);
    }
}

/* Test ENDS WITH operator */
static void test_ends_with_operator(void)
{
    const char *query = "MATCH (p:Person) WHERE p.name ENDS WITH 'e' RETURN p.name AS name ORDER BY p.name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (!result->success) {
            printf("\nENDS WITH operator failed: %s\n", result->error_message);
        } else {
            /* Should find Alice, Charlie */
            CU_ASSERT_EQUAL(result->row_count, 2);
        }
        cypher_result_free(result);
    }
}

/* Test CONTAINS operator */
static void test_contains_operator(void)
{
    const char *query = "MATCH (p:Person) WHERE p.name CONTAINS 'li' RETURN p.name AS name ORDER BY p.name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (!result->success) {
            printf("\nCONTAINS operator failed: %s\n", result->error_message);
        } else {
            /* Should find Alice, Charlie */
            CU_ASSERT_EQUAL(result->row_count, 2);
        }
        cypher_result_free(result);
    }
}

/* Test STARTS WITH with no matches */
static void test_starts_with_no_match(void)
{
    const char *query = "MATCH (p:Person) WHERE p.name STARTS WITH 'Z' RETURN p.name AS name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 0);
        cypher_result_free(result);
    }
}

/* ============================================================
 * startsWith() / endsWith() / contains() Functions
 * ============================================================ */

/* Test startsWith function */
static void test_starts_with_function(void)
{
    const char *query = "RETURN startsWith('Hello World', 'Hello') AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            /* startsWith returns true (1) */
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "1");
        }
        cypher_result_free(result);
    }
}

/* Test startsWith function - negative case */
static void test_starts_with_function_false(void)
{
    const char *query = "RETURN startsWith('Hello World', 'World') AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            /* startsWith returns false (0) */
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "0");
        }
        cypher_result_free(result);
    }
}

/* Test endsWith function */
static void test_ends_with_function(void)
{
    const char *query = "RETURN endsWith('Hello World', 'World') AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "1");
        }
        cypher_result_free(result);
    }
}

/* Test contains function */
static void test_contains_function(void)
{
    const char *query = "RETURN contains('Hello World', 'lo Wo') AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "1");
        }
        cypher_result_free(result);
    }
}

/* Test contains function - negative case */
static void test_contains_function_false(void)
{
    const char *query = "RETURN contains('Hello World', 'xyz') AS result";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success && result->row_count > 0) {
            CU_ASSERT_STRING_EQUAL(result->data[0][0], "0");
        }
        cypher_result_free(result);
    }
}

/* ============================================================
 * IS NULL / IS NOT NULL
 * ============================================================ */

/* Test IS NULL predicate */
static void test_is_null(void)
{
    const char *query = "MATCH (p:Person) WHERE p.email IS NULL RETURN p.name AS name ORDER BY p.name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (!result->success) {
            printf("\nIS NULL failed: %s\n", result->error_message);
        } else {
            /* Should find Bob, Charlie, Diana (those without email) */
            CU_ASSERT_EQUAL(result->row_count, 3);
        }
        cypher_result_free(result);
    }
}

/* Test IS NOT NULL predicate */
static void test_is_not_null(void)
{
    const char *query = "MATCH (p:Person) WHERE p.email IS NOT NULL RETURN p.name AS name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (!result->success) {
            printf("\nIS NOT NULL failed: %s\n", result->error_message);
        } else {
            /* Should find only Alice (has email) */
            CU_ASSERT_EQUAL(result->row_count, 1);
            if (result->row_count > 0) {
                CU_ASSERT_STRING_EQUAL(result->data[0][0], "Alice");
            }
        }
        cypher_result_free(result);
    }
}

/* Test IS NULL with combined conditions */
static void test_is_null_combined(void)
{
    const char *query = "MATCH (p:Person) WHERE p.city IS NULL AND p.age > 26 RETURN p.name AS name ORDER BY p.name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success) {
            /* Alice (30) and Diana (28) have no city and age > 26 */
            CU_ASSERT_EQUAL(result->row_count, 2);
        }
        cypher_result_free(result);
    }
}

/* ============================================================
 * Label Predicates
 * ============================================================ */

/* Test single label predicate in WHERE */
static void test_label_predicate_single(void)
{
    const char *query = "MATCH (n) WHERE n:Person RETURN n.name AS name ORDER BY n.name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (!result->success) {
            printf("\nLabel predicate failed: %s\n", result->error_message);
        } else {
            /* Should find all Persons: Alice, Bob, Charlie, Diana */
            CU_ASSERT_EQUAL(result->row_count, 4);
        }
        cypher_result_free(result);
    }
}

/* Test multiple label predicate */
static void test_label_predicate_multiple(void)
{
    const char *query = "MATCH (n) WHERE n:Developer RETURN n.name AS name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (!result->success) {
            printf("\nMultiple label predicate failed: %s\n", result->error_message);
        } else {
            /* Should find only Diana (Developer) */
            CU_ASSERT_EQUAL(result->row_count, 1);
            if (result->row_count > 0) {
                CU_ASSERT_STRING_EQUAL(result->data[0][0], "Diana");
            }
        }
        cypher_result_free(result);
    }
}

/* Test NOT label predicate */
static void test_label_predicate_not(void)
{
    const char *query = "MATCH (n) WHERE NOT n:Person RETURN n.name AS name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success) {
            /* Should find only TechCorp (Company, not Person) */
            CU_ASSERT_EQUAL(result->row_count, 1);
            if (result->row_count > 0) {
                CU_ASSERT_STRING_EQUAL(result->data[0][0], "TechCorp");
            }
        }
        cypher_result_free(result);
    }
}

/* ============================================================
 * Comparison Predicates
 * ============================================================ */

/* Test equality predicate */
static void test_equality_predicate(void)
{
    const char *query = "MATCH (p:Person) WHERE p.age = 30 RETURN p.name AS name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success) {
            CU_ASSERT_EQUAL(result->row_count, 1);
            if (result->row_count > 0) {
                CU_ASSERT_STRING_EQUAL(result->data[0][0], "Alice");
            }
        }
        cypher_result_free(result);
    }
}

/* Test inequality predicate */
static void test_inequality_predicate(void)
{
    const char *query = "MATCH (p:Person) WHERE p.age <> 30 RETURN p.name AS name ORDER BY p.name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success) {
            /* Bob (25), Charlie (35), Diana (28) */
            CU_ASSERT_EQUAL(result->row_count, 3);
        }
        cypher_result_free(result);
    }
}

/* Test range predicate */
static void test_range_predicate(void)
{
    const char *query = "MATCH (p:Person) WHERE p.age >= 28 AND p.age <= 32 RETURN p.name AS name ORDER BY p.name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success) {
            /* Alice (30), Diana (28) */
            CU_ASSERT_EQUAL(result->row_count, 2);
        }
        cypher_result_free(result);
    }
}

/* ============================================================
 * Boolean Logic Predicates
 * ============================================================ */

/* Test AND predicate */
static void test_and_predicate(void)
{
    const char *query = "MATCH (p:Person) WHERE p.age > 25 AND p.name STARTS WITH 'A' RETURN p.name AS name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success) {
            CU_ASSERT_EQUAL(result->row_count, 1);
            if (result->row_count > 0) {
                CU_ASSERT_STRING_EQUAL(result->data[0][0], "Alice");
            }
        }
        cypher_result_free(result);
    }
}

/* Test OR predicate */
static void test_or_predicate(void)
{
    const char *query = "MATCH (p:Person) WHERE p.age < 26 OR p.age > 34 RETURN p.name AS name ORDER BY p.name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success) {
            /* Bob (25), Charlie (35) */
            CU_ASSERT_EQUAL(result->row_count, 2);
        }
        cypher_result_free(result);
    }
}

/* Test NOT predicate */
static void test_not_predicate(void)
{
    const char *query = "MATCH (p:Person) WHERE NOT p.age = 30 RETURN p.name AS name ORDER BY p.name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success) {
            /* Bob, Charlie, Diana */
            CU_ASSERT_EQUAL(result->row_count, 3);
        }
        cypher_result_free(result);
    }
}

/* Test nested boolean logic */
static void test_nested_boolean_logic(void)
{
    const char *query = "MATCH (p:Person) WHERE (p.age > 27 AND p.age < 32) OR p.name = 'Charlie' RETURN p.name AS name ORDER BY p.name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success) {
            /* Alice (30), Charlie (35), Diana (28) */
            CU_ASSERT_EQUAL(result->row_count, 3);
        }
        cypher_result_free(result);
    }
}

/* ============================================================
 * IN Predicate
 * ============================================================ */

/* Test IN list predicate */
static void test_in_list_predicate(void)
{
    const char *query = "MATCH (p:Person) WHERE p.name IN ['Alice', 'Bob'] RETURN p.name AS name ORDER BY p.name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success) {
            CU_ASSERT_EQUAL(result->row_count, 2);
        }
        cypher_result_free(result);
    }
}

/* Test IN list predicate - no match */
static void test_in_list_no_match(void)
{
    const char *query = "MATCH (p:Person) WHERE p.name IN ['Unknown', 'Nobody'] RETURN p.name AS name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 0);
        cypher_result_free(result);
    }
}

/* Test IN with numbers */
static void test_in_numbers_predicate(void)
{
    const char *query = "MATCH (p:Person) WHERE p.age IN [25, 30, 40] RETURN p.name AS name ORDER BY p.name";
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (result->success) {
            /* Alice (30), Bob (25) */
            CU_ASSERT_EQUAL(result->row_count, 2);
        }
        cypher_result_free(result);
    }
}

/* ============================================================
 * Suite Registration
 * ============================================================ */

int init_executor_predicates_suite(void)
{
    CU_pSuite suite = CU_add_suite("Executor Predicates", setup_suite, teardown_suite);
    if (!suite) {
        return CU_get_error();
    }

    if (!CU_add_test(suite, "STARTS WITH operator", test_starts_with_operator) ||
        !CU_add_test(suite, "STARTS WITH multiple matches", test_starts_with_multiple) ||
        !CU_add_test(suite, "ENDS WITH operator", test_ends_with_operator) ||
        !CU_add_test(suite, "CONTAINS operator", test_contains_operator) ||
        !CU_add_test(suite, "STARTS WITH no match", test_starts_with_no_match) ||
        !CU_add_test(suite, "startsWith() function", test_starts_with_function) ||
        !CU_add_test(suite, "startsWith() false", test_starts_with_function_false) ||
        !CU_add_test(suite, "endsWith() function", test_ends_with_function) ||
        !CU_add_test(suite, "contains() function", test_contains_function) ||
        !CU_add_test(suite, "contains() false", test_contains_function_false) ||
        !CU_add_test(suite, "IS NULL", test_is_null) ||
        !CU_add_test(suite, "IS NOT NULL", test_is_not_null) ||
        !CU_add_test(suite, "IS NULL combined", test_is_null_combined) ||
        !CU_add_test(suite, "Label predicate single", test_label_predicate_single) ||
        !CU_add_test(suite, "Label predicate multiple", test_label_predicate_multiple) ||
        !CU_add_test(suite, "Label predicate NOT", test_label_predicate_not) ||
        !CU_add_test(suite, "Equality predicate", test_equality_predicate) ||
        !CU_add_test(suite, "Inequality predicate", test_inequality_predicate) ||
        !CU_add_test(suite, "Range predicate", test_range_predicate) ||
        !CU_add_test(suite, "AND predicate", test_and_predicate) ||
        !CU_add_test(suite, "OR predicate", test_or_predicate) ||
        !CU_add_test(suite, "NOT predicate", test_not_predicate) ||
        !CU_add_test(suite, "Nested boolean logic", test_nested_boolean_logic) ||
        !CU_add_test(suite, "IN list predicate", test_in_list_predicate) ||
        !CU_add_test(suite, "IN list no match", test_in_list_no_match) ||
        !CU_add_test(suite, "IN numbers predicate", test_in_numbers_predicate)) {
        return CU_get_error();
    }

    return CUE_SUCCESS;
}
