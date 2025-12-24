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
#include "parser/cypher_debug.h"

/* Test database handle */
static sqlite3 *test_db = NULL;
static cypher_executor *executor = NULL;

/* Setup function - create test database */
static int setup_executor_unwind_suite(void)
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

    return 0;
}

/* Teardown function */
static int teardown_executor_unwind_suite(void)
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

/* Test basic UNWIND with integer list */
static void test_unwind_integer_list(void)
{
    const char *query = "UNWIND [1, 2, 3] AS x RETURN x";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nUNWIND integer list failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 3);
        cypher_result_free(result);
    }
}

/* Test UNWIND with string list */
static void test_unwind_string_list(void)
{
    const char *query = "UNWIND [\"a\", \"b\", \"c\"] AS s RETURN s";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nUNWIND string list failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 3);
        cypher_result_free(result);
    }
}

/* Test UNWIND with empty list */
static void test_unwind_empty_list(void)
{
    const char *query = "UNWIND [] AS x RETURN x";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nUNWIND empty list failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        /* Empty list should return no rows */
        CU_ASSERT_EQUAL(result->row_count, 0);
        cypher_result_free(result);
    }
}

/* Test UNWIND with single element */
static void test_unwind_single_element(void)
{
    const char *query = "UNWIND [42] AS x RETURN x";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nUNWIND single element failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 1);
        cypher_result_free(result);
    }
}

/* Test UNWIND with mixed types */
static void test_unwind_mixed_types(void)
{
    const char *query = "UNWIND [1, \"two\", 3.0] AS x RETURN x";

    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (!result->success) {
            printf("\nUNWIND mixed types failed: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->row_count, 3);
        cypher_result_free(result);
    }
}

/* Test list literal parsing */
static void test_list_literal_parsing(void)
{
    /* Test that list literals are parsed correctly */
    cypher_parse_result *parse = parse_cypher_query_ext("RETURN [1, 2, 3]");
    CU_ASSERT_PTR_NOT_NULL(parse);

    if (parse) {
        CU_ASSERT_PTR_NULL(parse->error_message);
        CU_ASSERT_PTR_NOT_NULL(parse->ast);
        cypher_parse_result_free(parse);
    }
}

/* Test UNWIND parsing */
static void test_unwind_parsing(void)
{
    /* Test that UNWIND is parsed correctly */
    cypher_parse_result *parse = parse_cypher_query_ext("UNWIND [1, 2, 3] AS x RETURN x");
    CU_ASSERT_PTR_NOT_NULL(parse);

    if (parse) {
        if (parse->error_message) {
            printf("\nUNWIND parsing failed: %s\n", parse->error_message);
        }
        CU_ASSERT_PTR_NULL(parse->error_message);
        CU_ASSERT_PTR_NOT_NULL(parse->ast);
        cypher_parse_result_free(parse);
    }
}

/* Test empty list literal parsing */
static void test_empty_list_parsing(void)
{
    cypher_parse_result *parse = parse_cypher_query_ext("RETURN []");
    CU_ASSERT_PTR_NOT_NULL(parse);

    if (parse) {
        CU_ASSERT_PTR_NULL(parse->error_message);
        CU_ASSERT_PTR_NOT_NULL(parse->ast);
        cypher_parse_result_free(parse);
    }
}

/* Initialize the UNWIND executor test suite */
int init_executor_unwind_suite(void)
{
    CU_pSuite suite = CU_add_suite("Executor UNWIND", setup_executor_unwind_suite, teardown_executor_unwind_suite);
    if (!suite) {
        return CU_get_error();
    }

    /* Add tests */
    if (!CU_add_test(suite, "List literal parsing", test_list_literal_parsing) ||
        !CU_add_test(suite, "Empty list parsing", test_empty_list_parsing) ||
        !CU_add_test(suite, "UNWIND parsing", test_unwind_parsing) ||
        !CU_add_test(suite, "UNWIND integer list", test_unwind_integer_list) ||
        !CU_add_test(suite, "UNWIND string list", test_unwind_string_list) ||
        !CU_add_test(suite, "UNWIND empty list", test_unwind_empty_list) ||
        !CU_add_test(suite, "UNWIND single element", test_unwind_single_element) ||
        !CU_add_test(suite, "UNWIND mixed types", test_unwind_mixed_types)) {
        return CU_get_error();
    }

    return CUE_SUCCESS;
}
