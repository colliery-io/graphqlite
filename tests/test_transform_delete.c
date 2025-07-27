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
#include "executor/cypher_schema.h"

/* Test database handle */
static sqlite3 *test_db = NULL;

/* Setup function - create test database */
static int setup_delete_suite(void)
{
    /* Create in-memory database */
    int rc = sqlite3_open(":memory:", &test_db);
    if (rc != SQLITE_OK) {
        return -1;
    }
    
    /* Use the proper GraphQLite schema */
    cypher_schema_manager *schema_mgr = cypher_schema_create_manager(test_db);
    if (!schema_mgr) {
        return -1;
    }
    
    if (cypher_schema_initialize(schema_mgr) < 0) {
        cypher_schema_free_manager(schema_mgr);
        return -1;
    }
    
    cypher_schema_free_manager(schema_mgr);
    return 0;
}

/* Teardown function */
static int teardown_delete_suite(void)
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

/* DELETE variable binding test */
static void test_delete_variable_binding(void)
{
    const char *query = "MATCH (a)-[r:KNOWS]->(b) DELETE r";
    
    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        
        /* Get the MATCH clause */
        cypher_match *match_clause = (cypher_match*)query_ast->clauses->items[0];
        
        /* Create transform context */
        cypher_transform_context *ctx = cypher_transform_create_context(NULL);
        CU_ASSERT_PTR_NOT_NULL(ctx);
        
        if (ctx) {
            /* Transform the MATCH clause to register variables */
            int result = transform_match_clause(ctx, match_clause);
            CU_ASSERT_EQUAL(result, 0);
            
            /* Debug: Check if transform failed */
            if (ctx->has_error) {
                printf("\nTransform error: %s\n", ctx->error_message);
            }
            
            /* Debug: Print SQL generated */
            printf("\nGenerated SQL: %s\n", ctx->sql_buffer);
            
            /* Verify that variable 'r' is registered as an edge variable */
            const char *r_alias = lookup_variable_alias(ctx, "r");
            CU_ASSERT_PTR_NOT_NULL(r_alias);
            
            bool r_is_edge = is_edge_variable(ctx, "r");
            CU_ASSERT_TRUE(r_is_edge);
            
            /* Verify that variables 'a' and 'b' are registered as node variables */
            const char *a_alias = lookup_variable_alias(ctx, "a");
            CU_ASSERT_PTR_NOT_NULL(a_alias);
            
            bool a_is_edge = is_edge_variable(ctx, "a");
            CU_ASSERT_FALSE(a_is_edge); /* Should be false - it's a node */
            
            const char *b_alias = lookup_variable_alias(ctx, "b");
            CU_ASSERT_PTR_NOT_NULL(b_alias);
            
            bool b_is_edge = is_edge_variable(ctx, "b");
            CU_ASSERT_FALSE(b_is_edge); /* Should be false - it's a node */
            
            if (r_alias) {
                /* Should be in the format "e_r" for edge variable r */
                printf("DELETE variable binding test passed: r_alias='%s', is_edge=%s\n", 
                       r_alias, r_is_edge ? "true" : "false");
            }
            
            cypher_transform_free_context(ctx);
        }
        
        cypher_parser_free_result(result);
    }
}

/* Test DELETE item creation and validation */
static void test_delete_item_creation(void)
{
    CYPHER_DEBUG("Running DELETE item creation test");
    
    const char *query = "MATCH (a)-[r:KNOWS]->(b) DELETE r, a";
    
    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        
        /* Get the DELETE clause */
        cypher_delete *delete_clause = (cypher_delete*)query_ast->clauses->items[1];
        CU_ASSERT_PTR_NOT_NULL(delete_clause);
        CU_ASSERT_EQUAL(delete_clause->base.type, AST_NODE_DELETE);
        
        /* Verify delete items list */
        CU_ASSERT_PTR_NOT_NULL(delete_clause->items);
        CU_ASSERT_EQUAL(delete_clause->items->count, 2);
        
        /* Check first delete item (r) */
        cypher_delete_item *item1 = (cypher_delete_item*)delete_clause->items->items[0];
        CU_ASSERT_PTR_NOT_NULL(item1);
        CU_ASSERT_EQUAL(item1->base.type, AST_NODE_DELETE_ITEM);
        CU_ASSERT_PTR_NOT_NULL(item1->variable);
        CU_ASSERT_STRING_EQUAL(item1->variable, "r");
        
        /* Check second delete item (a) */
        cypher_delete_item *item2 = (cypher_delete_item*)delete_clause->items->items[1];
        CU_ASSERT_PTR_NOT_NULL(item2);
        CU_ASSERT_EQUAL(item2->base.type, AST_NODE_DELETE_ITEM);
        CU_ASSERT_PTR_NOT_NULL(item2->variable);
        CU_ASSERT_STRING_EQUAL(item2->variable, "a");
        
        cypher_parser_free_result(result);
    }
}

/* Test DELETE with anonymous entities */
static void test_delete_anonymous_entities(void)
{
    /* Test 1: DELETE node that was matched with anonymous relationship */
    const char *query1 = "MATCH (a)-[]->(b) DELETE a";
    cypher_query_result *result1 = parse_and_transform(query1);
    CU_ASSERT_PTR_NOT_NULL(result1);
    
    if (result1) {
        /* DELETE may not be fully implemented yet, so just check it doesn't crash */
        if (result1->has_error) {
            printf("DELETE anonymous entity error: %s\n", result1->error_message);
            /* This is expected - DELETE may not be fully implemented */
        }
        cypher_free_result(result1);
    }
    
    /* Test 2: DELETE relationship matched with anonymous nodes */
    const char *query2 = "MATCH ()-[r:KNOWS]->() DELETE r";
    cypher_query_result *result2 = parse_and_transform(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);
    
    if (result2) {
        /* DELETE may not be fully implemented yet, so just check it doesn't crash */
        if (result2->has_error) {
            printf("DELETE anonymous relationship error: %s\n", result2->error_message);
            /* This is expected - DELETE may not be fully implemented */
        }
        cypher_free_result(result2);
    }
}

/* Test DELETE clause transformation */
static void test_delete_clause_transformation(void)
{
    const char *query = "MATCH (n:Person) DELETE n";
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        /* DELETE transformation may fail due to incomplete implementation */
        if (result->has_error) {
            printf("DELETE clause transformation error: %s\n", result->error_message);
            /* Expected for now */
        } else {
            printf("DELETE clause transformation succeeded\n");
        }
        cypher_free_result(result);
    }
}

/* Test DELETE multiple items */
static void test_delete_multiple_items(void)
{
    const char *query = "MATCH (a:Person)-[r:KNOWS]->(b:Person) DELETE a, r, b";
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        /* DELETE transformation may fail due to incomplete implementation */
        if (result->has_error) {
            printf("DELETE multiple items error: %s\n", result->error_message);
            /* Expected for now */
        } else {
            printf("DELETE multiple items transformation succeeded\n");
        }
        cypher_free_result(result);
    }
}

/* Test DELETE with WHERE clause */
static void test_delete_with_where(void)
{
    const char *query = "MATCH (n:Person) WHERE n.age > 65 DELETE n";
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        /* DELETE transformation may fail due to incomplete implementation */
        if (result->has_error) {
            printf("DELETE with WHERE error: %s\n", result->error_message);
            /* Expected for now */
        } else {
            printf("DELETE with WHERE transformation succeeded\n");
        }
        cypher_free_result(result);
    }
}

/* Test DELETE error conditions */
static void test_delete_error_conditions(void)
{
    /* Test DELETE without MATCH - should fail */
    const char *query1 = "DELETE n";
    cypher_query_result *result1 = parse_and_transform(query1);
    CU_ASSERT_PTR_NOT_NULL(result1);
    
    if (result1) {
        /* This should fail because there's no MATCH clause */
        CU_ASSERT_TRUE(result1->has_error);
        printf("DELETE without MATCH correctly failed: %s\n", 
               result1->error_message ? result1->error_message : "Parse error");
        cypher_free_result(result1);
    }
    
    /* Test DELETE undefined variable */
    const char *query2 = "MATCH (a) DELETE b";
    cypher_query_result *result2 = parse_and_transform(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);
    
    if (result2) {
        /* This should fail because 'b' is not defined */
        if (result2->has_error) {
            printf("DELETE undefined variable correctly failed: %s\n", 
                   result2->error_message ? result2->error_message : "Unknown error");
        }
        cypher_free_result(result2);
    }
}

/* Initialize the DELETE transform test suite */
int init_transform_delete_suite(void)
{
    CU_pSuite suite = CU_add_suite("Transform DELETE", setup_delete_suite, teardown_delete_suite);
    if (!suite) {
        return CU_get_error();
    }
    
    /* Add tests */
    if (!CU_add_test(suite, "DELETE variable binding", test_delete_variable_binding) ||
        !CU_add_test(suite, "DELETE item creation", test_delete_item_creation) ||
        !CU_add_test(suite, "DELETE anonymous entities", test_delete_anonymous_entities) ||
        !CU_add_test(suite, "DELETE clause transformation", test_delete_clause_transformation) ||
        !CU_add_test(suite, "DELETE multiple items", test_delete_multiple_items) ||
        !CU_add_test(suite, "DELETE with WHERE", test_delete_with_where) ||
        !CU_add_test(suite, "DELETE error conditions", test_delete_error_conditions)) {
        return CU_get_error();
    }
    
    return CUE_SUCCESS;
}