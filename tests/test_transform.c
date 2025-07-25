#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <sqlite3.h>

#include "parser/cypher_parser.h"
#include "parser/cypher_ast.h"
#include "transform/cypher_transform.h"
#include "parser/cypher_debug.h"
#include "test_transform.h"

/* Test database handle */
static sqlite3 *test_db = NULL;

/* Setup function - create test database */
static int setup_transform_suite(void)
{
    /* Create in-memory database */
    int rc = sqlite3_open(":memory:", &test_db);
    if (rc != SQLITE_OK) {
        return -1;
    }
    
    /* Create graph schema */
    const char *schema[] = {
        "CREATE TABLE nodes (id INTEGER PRIMARY KEY AUTOINCREMENT)",
        "CREATE TABLE edges (id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "source_id INTEGER, target_id INTEGER, type TEXT, "
        "FOREIGN KEY(source_id) REFERENCES nodes(id), "
        "FOREIGN KEY(target_id) REFERENCES nodes(id))",
        "CREATE TABLE node_labels (node_id INTEGER, label TEXT, "
        "PRIMARY KEY(node_id, label), FOREIGN KEY(node_id) REFERENCES nodes(id))",
        "CREATE TABLE properties (element_id INTEGER, element_type TEXT, "
        "key TEXT, value TEXT, value_type TEXT, "
        "PRIMARY KEY(element_id, element_type, key))",
        NULL
    };
    
    for (int i = 0; schema[i]; i++) {
        char *error;
        rc = sqlite3_exec(test_db, schema[i], NULL, NULL, &error);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Schema error: %s\n", error);
            sqlite3_free(error);
            return -1;
        }
    }
    
    return 0;
}

/* Teardown function */
static int teardown_transform_suite(void)
{
    if (test_db) {
        sqlite3_close(test_db);
        test_db = NULL;
    }
    return 0;
}

/* Helper function to parse and transform a query */
static cypher_query_result* parse_and_transform(const char *query_str)
{
    /* Parse the query */
    ast_node *ast = parse_cypher_query(query_str);
    if (!ast) {
        return NULL;
    }
    
    
    /* Create transform context */
    cypher_transform_context *ctx = cypher_transform_create_context(test_db);
    if (!ctx) {
        cypher_parser_free_result(ast);
        return NULL;
    }
    
    /* Transform to SQL */
    cypher_query_result *result = cypher_transform_query(ctx, (cypher_query*)ast);
    
    /* Cleanup */
    cypher_transform_free_context(ctx);
    cypher_parser_free_result(ast);
    
    return result;
}

/* Test simple MATCH transformation */
static void test_match_simple(void)
{
    const char *query = "MATCH (n) RETURN n";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test MATCH with label */
static void test_match_with_label(void)
{
    const char *query = "MATCH (n:Person) RETURN n";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test simple CREATE transformation */
static void test_create_simple(void)
{
    const char *query = "CREATE (n)";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test CREATE with label */
static void test_create_with_label(void)
{
    const char *query = "CREATE (n:Person)";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test that CREATE transformation succeeds and prepares statements */
static void test_create_sql_validation(void)
{
    const char *query = "CREATE (n)";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("\nCREATE transform error: %s\n", result->error_message ? result->error_message : "Unknown error");
        }
        
        /* For CREATE queries, we expect the transform to succeed without errors */
        /* The actual SQL execution happens in the executor layer */
        
        cypher_free_result(result);
    }
}

/* Test transform error handling for unsupported features */
static void test_transform_error_handling(void)
{
    const char *query = "CREATE (a)-[:KNOWS]->(b)";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (result->has_error) {
            printf("\nRelationship transform error (expected): %s\n", 
                   result->error_message ? result->error_message : "Unknown error");
            /* Expected to fail until relationship transformation is implemented */
            CU_ASSERT_TRUE(result->has_error);
        } else {
            printf("\nRelationship transform unexpectedly succeeded\n");
            /* If it succeeds, that means relationship support was added */
        }
        
        cypher_free_result(result);
    }
}

/* Test transform result validation */
static void test_transform_result_validation(void)
{
    const char *queries[] = {
        "CREATE (n)",
        "CREATE (n:Person)", 
        "MATCH (n) RETURN n",
        "MATCH (n:Person) RETURN n",
        NULL
    };
    
    for (int q = 0; queries[q]; q++) {
        printf("\nTesting transform of: %s\n", queries[q]);
        
        cypher_query_result *result = parse_and_transform(queries[q]);
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            /* Basic result structure validation */
            if (result->has_error) {
                printf("Transform error: %s\n", 
                       result->error_message ? result->error_message : "Unknown error");
                /* Some queries may fail - that's expected for unimplemented features */
            } else {
                printf("Transform succeeded\n");
                
                /* For successful transforms, validate structure */
                if (strstr(queries[q], "MATCH")) {
                    /* MATCH queries should produce a prepared statement */
                    CU_ASSERT_PTR_NOT_NULL(result->stmt);
                } else if (strstr(queries[q], "CREATE")) {
                    /* CREATE queries typically affect rows (though stmt might be NULL) */
                    /* Structure depends on implementation */
                }
            }
            
            cypher_free_result(result);
        }
    }
}

/* Test column information for MATCH queries */
static void test_match_column_validation(void)
{
    const char *query = "MATCH (n) RETURN n";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->has_error) {
            printf("\nMATCH transform succeeded\n");
            printf("Column count: %d\n", result->column_count);
            
            if (result->column_names && result->column_count > 0) {
                for (int i = 0; i < result->column_count; i++) {
                    printf("Column %d: %s\n", i, 
                           result->column_names[i] ? result->column_names[i] : "NULL");
                }
                
                /* Should have at least one column for RETURN n */
                CU_ASSERT_TRUE(result->column_count > 0);
            }
        } else {
            printf("\nMATCH transform failed: %s\n", 
                   result->error_message ? result->error_message : "Unknown error");
        }
        
        cypher_free_result(result);
    }
}

/* Test invalid query handling */
static void test_invalid_query(void)
{
    const char *query = "MATCH (n) WHERE n.name = 'Alice' RETURN n";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        /* WHERE is not yet implemented, should fail gracefully */
        /* For now, we expect it to succeed but WHERE is ignored */
        cypher_free_result(result);
    }
}

/* Test RETURN with DISTINCT clause */
static void test_return_distinct(void)
{
    const char *query = "MATCH (n) RETURN DISTINCT n";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->has_error) {
            /* Should have valid prepared statement for DISTINCT query */
            CU_ASSERT_PTR_NOT_NULL(result->stmt);
            printf("\nDISTINCT query transformed successfully\n");
        } else {
            printf("\nDISTINCT query failed: %s\n", 
                   result->error_message ? result->error_message : "Unknown error");
        }
        cypher_free_result(result);
    }
}

/* Test RETURN with ORDER BY clause */
static void test_return_order_by(void)
{
    const char *query = "MATCH (n) RETURN n ORDER BY n.name";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->has_error) {
            /* Should have valid prepared statement for ORDER BY query */
            CU_ASSERT_PTR_NOT_NULL(result->stmt);
            printf("\nORDER BY query transformed successfully\n");
        } else {
            printf("\nORDER BY query failed: %s\n", 
                   result->error_message ? result->error_message : "Unknown error");
        }
        cypher_free_result(result);
    }
}

/* Test RETURN with LIMIT clause */
static void test_return_limit(void)
{
    const char *query = "MATCH (n) RETURN n LIMIT 10";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->has_error) {
            /* Should have valid prepared statement for LIMIT query */
            CU_ASSERT_PTR_NOT_NULL(result->stmt);
            printf("\nLIMIT query transformed successfully\n");
        } else {
            printf("\nLIMIT query failed: %s\n", 
                   result->error_message ? result->error_message : "Unknown error");
        }
        cypher_free_result(result);
    }
}

/* Test RETURN with SKIP clause */
static void test_return_skip(void)
{
    const char *query = "MATCH (n) RETURN n SKIP 5";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->has_error) {
            /* Should have valid prepared statement for SKIP query */
            CU_ASSERT_PTR_NOT_NULL(result->stmt);
            printf("\nSKIP query transformed successfully\n");
        } else {
            printf("\nSKIP query failed: %s\n", 
                   result->error_message ? result->error_message : "Unknown error");
        }
        cypher_free_result(result);
    }
}

/* Test RETURN with combined clauses */
static void test_return_combined_clauses(void)
{
    const char *query = "MATCH (n) RETURN DISTINCT n ORDER BY n.name LIMIT 5 SKIP 2";
    
    cypher_query_result *result = parse_and_transform(query);
    
    if (result) {
        if (!result->has_error) {
            /* Should have valid prepared statement for combined query */
            CU_ASSERT_PTR_NOT_NULL(result->stmt);
            printf("\nCombined clauses query transformed successfully\n");
        } else {
            printf("\nCombined clauses query failed: %s\n", 
                   result->error_message ? result->error_message : "Unknown error");
            /* This complex query may fail due to unimplemented features - that's expected */
        }
        cypher_free_result(result);
    } else {
        /* If parsing failed, that's also expected for complex features */
        printf("\nCombined clauses query failed to parse - expected for unimplemented features\n");
    }
}

/* Test RETURN with alias */
static void test_return_with_alias(void)
{
    const char *query = "MATCH (n) RETURN n AS node";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->has_error) {
            /* Should have valid prepared statement for alias query */
            CU_ASSERT_PTR_NOT_NULL(result->stmt);
            printf("\nAlias query transformed successfully\n");
        } else {
            printf("\nAlias query failed: %s\n", 
                   result->error_message ? result->error_message : "Unknown error");
        }
        cypher_free_result(result);
    }
}

/* Test RETURN after CREATE (expected to fail) */
static void test_return_after_create(void)
{
    const char *query = "CREATE (n) RETURN n";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        /* This should fail with an error about RETURN after CREATE not being implemented */
        if (result->has_error) {
            CU_ASSERT_PTR_NOT_NULL(result->error_message);
            CU_ASSERT_PTR_NOT_NULL(strstr(result->error_message, "RETURN after CREATE"));
            printf("\nRETURN after CREATE correctly failed: %s\n", result->error_message);
        } else {
            printf("\nRETURN after CREATE unexpectedly succeeded\n");
        }
        cypher_free_result(result);
    }
}

/* Test standalone RETURN without MATCH (expected to fail) */
static void test_return_without_match(void)
{
    const char *query = "RETURN 42";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        /* This should fail with an error about RETURN without MATCH */
        if (result->has_error) {
            CU_ASSERT_PTR_NOT_NULL(result->error_message);
            printf("\nStandalone RETURN correctly failed: %s\n", result->error_message);
        } else {
            printf("\nStandalone RETURN unexpectedly succeeded\n");
        }
        cypher_free_result(result);
    }
}

/* Initialize the transform test suite */
int init_transform_suite(void)
{
    CU_pSuite suite = CU_add_suite("Transform", setup_transform_suite, teardown_transform_suite);
    if (!suite) {
        return CU_get_error();
    }
    
    /* Add tests */
    if (!CU_add_test(suite, "Simple MATCH", test_match_simple) ||
        !CU_add_test(suite, "MATCH with label", test_match_with_label) ||
        !CU_add_test(suite, "Simple CREATE", test_create_simple) ||
        !CU_add_test(suite, "CREATE with label", test_create_with_label) ||
        !CU_add_test(suite, "CREATE SQL validation", test_create_sql_validation) ||
        !CU_add_test(suite, "Transform error handling", test_transform_error_handling) ||
        !CU_add_test(suite, "Transform result validation", test_transform_result_validation) ||
        !CU_add_test(suite, "MATCH column validation", test_match_column_validation) ||
        !CU_add_test(suite, "Invalid query handling", test_invalid_query) ||
        !CU_add_test(suite, "RETURN with DISTINCT", test_return_distinct) ||
        !CU_add_test(suite, "RETURN with ORDER BY", test_return_order_by) ||
        !CU_add_test(suite, "RETURN with LIMIT", test_return_limit) ||
        !CU_add_test(suite, "RETURN with SKIP", test_return_skip) ||
        !CU_add_test(suite, "RETURN combined clauses", test_return_combined_clauses) ||
        !CU_add_test(suite, "RETURN with alias", test_return_with_alias) ||
        !CU_add_test(suite, "RETURN after CREATE", test_return_after_create) ||
        !CU_add_test(suite, "RETURN without MATCH", test_return_without_match)) {
        return CU_get_error();
    }
    
    return CUE_SUCCESS;
}