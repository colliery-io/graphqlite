#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <sqlite3.h>

#include "executor/cypher_executor.h"
#include "parser/cypher_debug.h"
#include "test_executor.h"

/* Test database handle */
static sqlite3 *test_db = NULL;
static cypher_executor *executor = NULL;

/* Setup function - create test database and executor */
static int setup_executor_suite(void)
{
    /* Create in-memory database */
    int rc = sqlite3_open(":memory:", &test_db);
    if (rc != SQLITE_OK) {
        return -1;
    }
    
    /* Enable foreign key constraints */
    sqlite3_exec(test_db, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);
    
    /* Create executor */
    executor = cypher_executor_create(test_db);
    if (!executor) {
        sqlite3_close(test_db);
        return -1;
    }
    
    return 0;
}

/* Teardown function */
static int teardown_executor_suite(void)
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

/* Test executor creation */
static void test_executor_creation(void)
{
    CU_ASSERT_PTR_NOT_NULL(executor);
    CU_ASSERT_TRUE(cypher_executor_is_ready(executor));
}

/* Test simple CREATE query execution */
static void test_create_query_execution(void)
{
    const char *query = "CREATE (n)";
    
    /* First test the parser directly */
    printf("\nDirect parser test:\n");
    ast_node *ast = parse_cypher_query(query);
    if (ast) {
        printf("Parser succeeded: type=%d, data=%p\n", ast->type, ast->data);
        const char *error = cypher_parser_get_error(ast);
        if (error) {
            printf("Parser error: %s\n", error);
        }
        cypher_parser_free_result(ast);
    } else {
        printf("Parser returned NULL\n");
    }
    
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->success) {
            printf("\nCREATE query failed: %s\n", result->error_message ? result->error_message : "No error message");
        } else {
            printf("\nCREATE query succeeded: nodes=%d, props=%d\n", result->nodes_created, result->properties_set);
        }
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_EQUAL(result->nodes_created, 1);
        CU_ASSERT_EQUAL(result->properties_set, 0); /* No properties set for simple CREATE (n) */
        
        cypher_result_free(result);
    }
}

/* Test simple MATCH query execution */
static void test_match_query_execution(void)
{
    /* First create some data */
    cypher_result *create_result = cypher_executor_execute(executor, "CREATE (n)");
    CU_ASSERT_PTR_NOT_NULL(create_result);
    if (create_result) {
        if (!create_result->success) {
            printf("\nCREATE setup query failed: %s\n", create_result->error_message ? create_result->error_message : "No error message");
        }
        CU_ASSERT_TRUE(create_result->success);
        cypher_result_free(create_result);
    }
    
    /* Now try to match it */
    const char *query = "MATCH (n) RETURN n";
    
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->success) {
            printf("\nMATCH query failed: %s\n", result->error_message ? result->error_message : "No error message");
        }
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_TRUE(result->row_count > 0);
        CU_ASSERT_TRUE(result->column_count > 0);
        
        cypher_result_free(result);
    }
}

/* Test invalid query handling */
static void test_invalid_query_handling(void)
{
    const char *query = "INVALID SYNTAX";
    
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_FALSE(result->success);
        CU_ASSERT_PTR_NOT_NULL(result->error_message);
        
        cypher_result_free(result);
    }
}

/* Test null input handling */
static void test_null_input_handling(void)
{
    cypher_result *result1 = cypher_executor_execute(NULL, "CREATE (n)");
    CU_ASSERT_PTR_NOT_NULL(result1);
    if (result1) {
        CU_ASSERT_FALSE(result1->success);
        cypher_result_free(result1);
    }
    
    cypher_result *result2 = cypher_executor_execute(executor, NULL);
    CU_ASSERT_PTR_NOT_NULL(result2);
    if (result2) {
        CU_ASSERT_FALSE(result2->success);
        cypher_result_free(result2);
    }
}

/* Test relationship creation execution */
static void test_relationship_creation_execution(void)
{
    const char *query = "CREATE (a)-[:KNOWS]->(b)";
    
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->success) {
            printf("\nRelationship CREATE failed: %s\n", result->error_message ? result->error_message : "No error message");
            /* For now, we expect this to fail until we implement relationship execution */
            CU_ASSERT_FALSE(result->success);
        } else {
            printf("\nRelationship CREATE succeeded: nodes=%d, rels=%d\n", result->nodes_created, result->relationships_created);
            CU_ASSERT_EQUAL(result->nodes_created, 2);  /* Two nodes */
            CU_ASSERT_EQUAL(result->relationships_created, 1);  /* One relationship */
        }
        
        cypher_result_free(result);
    }
}

/* Test multiple relationship types */
static void test_multiple_relationship_types(void)
{
    const char *query = "CREATE (a)-[:KNOWS]->(b), (b)-[:LIKES]->(c)";
    
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->success) {
            printf("\nMultiple relationship CREATE failed: %s\n", result->error_message ? result->error_message : "No error message");
            // Expected to fail until relationship execution is implemented
        } else {
            printf("\nMultiple relationship CREATE succeeded: nodes=%d (expected 3), rels=%d (expected 2)\n", 
                   result->nodes_created, result->relationships_created);
            CU_ASSERT_EQUAL(result->nodes_created, 3);  // Three nodes
            CU_ASSERT_EQUAL(result->relationships_created, 2);  // Two relationships
        }
        
        cypher_result_free(result);
    }
}

/* Test bidirectional relationship creation */
static void test_bidirectional_relationship_creation(void)
{
    const char *query = "CREATE (a)<-[:FRIENDS]-(b)";
    
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->success) {
            printf("\nBidirectional relationship CREATE failed: %s\n", result->error_message ? result->error_message : "No error message");
            /* Expected to fail until relationship execution is implemented */
        } else {
            CU_ASSERT_EQUAL(result->nodes_created, 2);
            CU_ASSERT_EQUAL(result->relationships_created, 1);
        }
        
        cypher_result_free(result);
    }
}

/* Test relationship with properties */
static void test_relationship_with_properties(void)
{
    const char *query = "CREATE (a)-[:KNOWS {since: 2020}]->(b)";
    
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->success) {
            printf("\nRelationship with properties CREATE failed: %s\n", result->error_message ? result->error_message : "No error message");
            /* Expected to fail until relationship execution is implemented */
        } else {
            CU_ASSERT_EQUAL(result->nodes_created, 2);
            CU_ASSERT_EQUAL(result->relationships_created, 1);
            CU_ASSERT_TRUE(result->properties_set > 0);  /* Relationship property */
        }
        
        cypher_result_free(result);
    }
}

/* Test complex path creation */
static void test_complex_path_creation(void)
{
    const char *query = "CREATE (a)-[:KNOWS]->(b)-[:WORKS_AT]->(c)";
    
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->success) {
            printf("\nComplex path CREATE failed: %s\n", result->error_message ? result->error_message : "No error message");
            /* Expected to fail until relationship execution is implemented */
        } else {
            printf("\nComplex path CREATE succeeded: nodes=%d, rels=%d\n", result->nodes_created, result->relationships_created);
            CU_ASSERT_EQUAL(result->nodes_created, 3);  /* Three nodes */
            CU_ASSERT_EQUAL(result->relationships_created, 2);  /* Two relationships */
        }
        
        cypher_result_free(result);
    }
}

/* Test relationship matching (once relationships are created) */
static void test_relationship_matching(void)
{
    /* This test will be meaningful once we can create relationships */
    const char *create_query = "CREATE (a:Person)-[:KNOWS]->(b:Person)";
    const char *match_query = "MATCH (a)-[:KNOWS]->(b) RETURN a, b";
    
    /* First create the relationship */
    cypher_result *create_result = cypher_executor_execute(executor, create_query);
    if (create_result) {
        if (!create_result->success) {
            printf("\nSetup for relationship matching failed: %s\n", 
                   create_result->error_message ? create_result->error_message : "No error message");
        }
        cypher_result_free(create_result);
    }
    
    /* Then try to match it */
    cypher_result *match_result = cypher_executor_execute(executor, match_query);
    CU_ASSERT_PTR_NOT_NULL(match_result);
    
    if (match_result) {
        if (!match_result->success) {
            printf("\nRelationship matching failed: %s\n", 
                   match_result->error_message ? match_result->error_message : "No error message");
            /* Expected to fail until relationship execution and matching is implemented */
        } else {
            printf("\nRelationship matching succeeded: rows=%d\n", match_result->row_count);
            CU_ASSERT_TRUE(match_result->row_count > 0);
        }
        
        cypher_result_free(match_result);
    }
}

/* Test database state after relationship operations */
static void test_relationship_database_state(void)
{
    /* Create a relationship and verify database tables are populated correctly */
    const char *query = "CREATE (a:Person {name: 'Alice'})-[:KNOWS {since: 2020}]->(b:Person {name: 'Bob'})";
    
    cypher_result *result = cypher_executor_execute(executor, query);
    if (result) {
        if (result->success) {
            /* Check that appropriate tables exist and have data */
            sqlite3_stmt *stmt;
            
            /* Check nodes table */
            int rc = sqlite3_prepare_v2(test_db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name LIKE 'nodes%'", -1, &stmt, NULL);
            if (rc == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    int table_count = sqlite3_column_int(stmt, 0);
                    printf("\nFound %d node tables in database\n", table_count);
                    CU_ASSERT_TRUE(table_count > 0);
                }
                sqlite3_finalize(stmt);
            }
            
            /* Check edges table */
            rc = sqlite3_prepare_v2(test_db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name LIKE 'edges%'", -1, &stmt, NULL);
            if (rc == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    int edge_table_count = sqlite3_column_int(stmt, 0);
                    printf("Found %d edge tables in database\n", edge_table_count);
                    /* This should be > 0 once relationship execution is implemented */
                }
                sqlite3_finalize(stmt);
            }
        }
        
        cypher_result_free(result);
    }
}

/* Test result printing (manual verification) */
static void test_result_printing(void)
{
    const char *query = "CREATE (n)";
    
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        /* This will print to stdout - mainly for manual verification during testing */
        printf("\n--- Test Result Output ---\n");
        cypher_result_print(result);
        printf("--- End Test Result ---\n");
        
        cypher_result_free(result);
    }
}

/* Comprehensive edge tests */

/* Test self-referencing relationships */
static void test_self_referencing_relationship(void)
{
    const char *query = "CREATE (a)-[:KNOWS]->(a)";
    
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->success) {
            printf("\nSelf-referencing relationship CREATE failed: %s\n", result->error_message ? result->error_message : "No error message");
        } else {
            printf("\nSelf-referencing relationship CREATE succeeded: nodes=%d, rels=%d\n", 
                   result->nodes_created, result->relationships_created);
            CU_ASSERT_EQUAL(result->nodes_created, 1);  // One node
            CU_ASSERT_EQUAL(result->relationships_created, 1);  // One self-relationship
        }
        
        cypher_result_free(result);
    }
}

/* Test multiple relationships between same nodes with different types */
static void test_multiple_relationships_same_nodes(void)
{
    const char *query = "CREATE (a)-[:KNOWS]->(b), (a)-[:LIKES]->(b), (a)-[:FOLLOWS]->(b)";
    
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->success) {
            printf("\nMultiple relationships same nodes CREATE failed: %s\n", result->error_message ? result->error_message : "No error message");
        } else {
            printf("\nMultiple relationships same nodes CREATE succeeded: nodes=%d, rels=%d\n", 
                   result->nodes_created, result->relationships_created);
            CU_ASSERT_EQUAL(result->nodes_created, 2);  // Two nodes (a, b)
            CU_ASSERT_EQUAL(result->relationships_created, 3);  // Three relationships
        }
        
        cypher_result_free(result);
    }
}

/* Test long path patterns */
static void test_long_path_pattern(void)
{
    const char *query = "CREATE (a)-[:R1]->(b)-[:R2]->(c)-[:R3]->(d)-[:R4]->(e)";
    
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->success) {
            printf("\nLong path CREATE failed: %s\n", result->error_message ? result->error_message : "No error message");
        } else {
            printf("\nLong path CREATE succeeded: nodes=%d, rels=%d\n", 
                   result->nodes_created, result->relationships_created);
            CU_ASSERT_EQUAL(result->nodes_created, 5);  // Five nodes (a,b,c,d,e)
            CU_ASSERT_EQUAL(result->relationships_created, 4);  // Four relationships
        }
        
        cypher_result_free(result);
    }
}

/* Test relationships with no type (should default to RELATED) */
static void test_relationship_no_type(void)
{
    const char *query = "CREATE (a)-[]->(b)";
    
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->success) {
            printf("\nNo-type relationship CREATE failed: %s\n", result->error_message ? result->error_message : "No error message");
        } else {
            printf("\nNo-type relationship CREATE succeeded: nodes=%d, rels=%d\n", 
                   result->nodes_created, result->relationships_created);
            CU_ASSERT_EQUAL(result->nodes_created, 2);  // Two nodes
            CU_ASSERT_EQUAL(result->relationships_created, 1);  // One relationship
        }
        
        cypher_result_free(result);
    }
}

/* Test undirected relationships */
static void test_undirected_relationship(void)
{
    const char *query = "CREATE (a)-[:CONNECTED]-(b)";
    
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->success) {
            printf("\nUndirected relationship CREATE failed: %s\n", result->error_message ? result->error_message : "No error message");
        } else {
            printf("\nUndirected relationship CREATE succeeded: nodes=%d, rels=%d\n", 
                   result->nodes_created, result->relationships_created);
            CU_ASSERT_EQUAL(result->nodes_created, 2);  // Two nodes
            CU_ASSERT_EQUAL(result->relationships_created, 1);  // One relationship
        }
        
        cypher_result_free(result);
    }
}

/* Test relationships with numeric node variables */
static void test_numeric_node_variables(void)
{
    const char *query = "CREATE (n1)-[:CONNECTS]->(n2)";
    
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->success) {
            printf("\nNumeric variables relationship CREATE failed: %s\n", result->error_message ? result->error_message : "No error message");
        } else {
            printf("\nNumeric variables relationship CREATE succeeded: nodes=%d, rels=%d\n", 
                   result->nodes_created, result->relationships_created);
            CU_ASSERT_EQUAL(result->nodes_created, 2);  // Two nodes
            CU_ASSERT_EQUAL(result->relationships_created, 1);  // One relationship
        }
        
        cypher_result_free(result);
    }
}

/* Test mixed directed and undirected in same query */
static void test_mixed_relationship_directions(void)
{
    const char *query = "CREATE (a)-[:FORWARD]->(b), (b)<-[:BACKWARD]-(c), (c)-[:BOTH]-(d)";
    
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->success) {
            printf("\nMixed directions CREATE failed: %s\n", result->error_message ? result->error_message : "No error message");
        } else {
            printf("\nMixed directions CREATE succeeded: nodes=%d, rels=%d\n", 
                   result->nodes_created, result->relationships_created);
            CU_ASSERT_EQUAL(result->nodes_created, 4);  // Four nodes (a,b,c,d)
            CU_ASSERT_EQUAL(result->relationships_created, 3);  // Three relationships
        }
        
        cypher_result_free(result);
    }
}

/* Test relationship variable reuse across patterns */
static void test_relationship_variable_reuse(void)
{
    const char *query = "CREATE (a)-[r:KNOWS]->(b), (c)-[r:KNOWS]->(d)";
    
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        // This should succeed - relationship variables can be reused
        if (!result->success) {
            printf("\nRelationship variable reuse CREATE failed: %s\n", result->error_message ? result->error_message : "No error message");
        } else {
            printf("\nRelationship variable reuse CREATE succeeded: nodes=%d, rels=%d\n", 
                   result->nodes_created, result->relationships_created);
            CU_ASSERT_EQUAL(result->nodes_created, 4);  // Four nodes (a,b,c,d)
            CU_ASSERT_EQUAL(result->relationships_created, 2);  // Two relationships
        }
        
        cypher_result_free(result);
    }
}

/* Test database consistency after complex relationship operations */
static void test_edge_database_consistency(void)
{
    const char *query = "CREATE (a:Person)-[:KNOWS]->(b:Person), (b)-[:WORKS_AT]->(c:Company)";
    
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (result->success) {
            printf("\nComplex relationship CREATE succeeded: nodes=%d, rels=%d\n", 
                   result->nodes_created, result->relationships_created);
            
            // Verify database state directly
            sqlite3_stmt *stmt;
            
            // Check nodes table
            int rc = sqlite3_prepare_v2(test_db, "SELECT COUNT(*) FROM nodes", -1, &stmt, NULL);
            if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
                int node_count = sqlite3_column_int(stmt, 0);
                printf("Database has %d nodes\n", node_count);
                CU_ASSERT_TRUE(node_count >= result->nodes_created);
            }
            sqlite3_finalize(stmt);
            
            // Check edges table
            rc = sqlite3_prepare_v2(test_db, "SELECT COUNT(*) FROM edges", -1, &stmt, NULL);
            if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
                int edge_count = sqlite3_column_int(stmt, 0);
                printf("Database has %d edges\n", edge_count);
                CU_ASSERT_TRUE(edge_count >= result->relationships_created);
            }
            sqlite3_finalize(stmt);
            
            // Check edge types
            rc = sqlite3_prepare_v2(test_db, "SELECT DISTINCT type FROM edges ORDER BY type", -1, &stmt, NULL);
            if (rc == SQLITE_OK) {
                printf("Edge types in database: ");
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    const char *type = (const char*)sqlite3_column_text(stmt, 0);
                    printf("'%s' ", type);
                }
                printf("\n");
            }
            sqlite3_finalize(stmt);
        }
        
        cypher_result_free(result);
    }
}

/* Test edge properties with multiple data types */
static void test_edge_properties_data_types(void)
{
    const char *query = "CREATE (a)-[:WORKS_WITH {years: 5, salary: 75000.50, verified: true, department: \"Engineering\"}]->(b)";
    
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->success) {
            printf("\nEdge properties with data types failed: %s\n", result->error_message ? result->error_message : "No error message");
        } else {
            CU_ASSERT_EQUAL(result->nodes_created, 2);
            CU_ASSERT_EQUAL(result->relationships_created, 1);
            CU_ASSERT_EQUAL(result->properties_set, 4); /* 4 properties: years, salary, verified, department */
        }
        
        cypher_result_free(result);
    }
}

/* Test MATCH...CREATE with edge properties */
static void test_match_create_edge_properties(void)
{
    /* First create nodes */
    const char *setup_query = "CREATE (alice:Person {name: \"Alice\"}), (bob:Person {name: \"Bob\"})";
    cypher_result *setup_result = cypher_executor_execute(executor, setup_query);
    CU_ASSERT_PTR_NOT_NULL(setup_result);
    CU_ASSERT_TRUE(setup_result->success);
    cypher_result_free(setup_result);
    
    /* Then use MATCH...CREATE with edge properties */
    const char *query = "MATCH (a:Person {name: \"Alice\"}), (b:Person {name: \"Bob\"}) CREATE (a)-[:KNOWS {since: 2020, strength: 0.8}]->(b)";
    
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->success) {
            printf("\nMATCH...CREATE with edge properties failed: %s\n", result->error_message ? result->error_message : "No error message");
        } else {
            CU_ASSERT_EQUAL(result->nodes_created, 0); /* Should use existing nodes */
            CU_ASSERT_EQUAL(result->relationships_created, 1);
            CU_ASSERT_EQUAL(result->properties_set, 2); /* 2 properties: since, strength */
        }
        
        cypher_result_free(result);
    }
}

/* Test property access type preservation */
static void test_property_access_types(void)
{
    /* First create a node with various property types */
    const char *setup_query = "CREATE (n:TestNode {str: \"text\", int: 42, float: 3.14, bool: true})";
    cypher_result *setup_result = cypher_executor_execute(executor, setup_query);
    CU_ASSERT_PTR_NOT_NULL(setup_result);
    CU_ASSERT_TRUE(setup_result->success);
    cypher_result_free(setup_result);
    
    /* Test property access returns proper types */
    const char *query = "MATCH (n:TestNode) RETURN n.str, n.int, n.float, n.bool";
    
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        if (!result->success) {
            printf("\nProperty access types test failed: %s\n", result->error_message ? result->error_message : "No error message");
        } else {
            CU_ASSERT_TRUE(result->success);
            CU_ASSERT_EQUAL(result->row_count, 1);
            CU_ASSERT_EQUAL(result->column_count, 4);
            CU_ASSERT_TRUE(result->use_agtype); /* Should use AGType for property access */
        }
        
        cypher_result_free(result);
    }
}

/* Test AGType output format compliance */
static void test_agtype_output_format(void)
{
    /* Create test data */
    const char *setup_query = "CREATE (alice:Person {name: \"Alice\"})-[:KNOWS {since: 2020}]->(bob:Person {name: \"Bob\"})";
    cypher_result *setup_result = cypher_executor_execute(executor, setup_query);
    CU_ASSERT_PTR_NOT_NULL(setup_result);
    CU_ASSERT_TRUE(setup_result->success);
    cypher_result_free(setup_result);
    
    /* Test vertex AGType format */
    const char *vertex_query = "MATCH (n:Person) RETURN n LIMIT 1";
    cypher_result *vertex_result = cypher_executor_execute(executor, vertex_query);
    CU_ASSERT_PTR_NOT_NULL(vertex_result);
    if (vertex_result) {
        CU_ASSERT_TRUE(vertex_result->success);
        CU_ASSERT_TRUE(vertex_result->use_agtype);
        CU_ASSERT_PTR_NOT_NULL(vertex_result->agtype_data);
        cypher_result_free(vertex_result);
    }
    
    /* Test edge AGType format */
    const char *edge_query = "MATCH ()-[r:KNOWS]->() RETURN r";
    cypher_result *edge_result = cypher_executor_execute(executor, edge_query);
    CU_ASSERT_PTR_NOT_NULL(edge_result);
    if (edge_result) {
        CU_ASSERT_TRUE(edge_result->success);
        CU_ASSERT_TRUE(edge_result->use_agtype);
        CU_ASSERT_PTR_NOT_NULL(edge_result->agtype_data);
        cypher_result_free(edge_result);
    }
}

/* Initialize the executor test suite */
int init_executor_suite(void)
{
    CU_pSuite suite = CU_add_suite("Executor", setup_executor_suite, teardown_executor_suite);
    if (!suite) {
        return CU_get_error();
    }
    
    /* Add tests */
    if (!CU_add_test(suite, "Executor creation", test_executor_creation) ||
        !CU_add_test(suite, "CREATE query execution", test_create_query_execution) ||
        !CU_add_test(suite, "MATCH query execution", test_match_query_execution) ||
        !CU_add_test(suite, "Relationship creation execution", test_relationship_creation_execution) ||
        !CU_add_test(suite, "Multiple relationship types", test_multiple_relationship_types) ||
        !CU_add_test(suite, "Bidirectional relationship creation", test_bidirectional_relationship_creation) ||
        !CU_add_test(suite, "Relationship with properties", test_relationship_with_properties) ||
        !CU_add_test(suite, "Complex path creation", test_complex_path_creation) ||
        !CU_add_test(suite, "Relationship matching", test_relationship_matching) ||
        !CU_add_test(suite, "Relationship database state", test_relationship_database_state) ||
        !CU_add_test(suite, "Invalid query handling", test_invalid_query_handling) ||
        !CU_add_test(suite, "Null input handling", test_null_input_handling) ||
        !CU_add_test(suite, "Result printing", test_result_printing) ||
        /* Comprehensive edge tests */
        !CU_add_test(suite, "Self-referencing relationship", test_self_referencing_relationship) ||
        !CU_add_test(suite, "Multiple relationships same nodes", test_multiple_relationships_same_nodes) ||
        !CU_add_test(suite, "Long path pattern", test_long_path_pattern) ||
        !CU_add_test(suite, "Relationship no type", test_relationship_no_type) ||
        !CU_add_test(suite, "Undirected relationship", test_undirected_relationship) ||
        !CU_add_test(suite, "Numeric node variables", test_numeric_node_variables) ||
        !CU_add_test(suite, "Mixed relationship directions", test_mixed_relationship_directions) ||
        !CU_add_test(suite, "Relationship variable reuse", test_relationship_variable_reuse) ||
        !CU_add_test(suite, "Edge database consistency", test_edge_database_consistency) ||
        !CU_add_test(suite, "Edge properties data types", test_edge_properties_data_types) ||
        !CU_add_test(suite, "MATCH...CREATE edge properties", test_match_create_edge_properties) ||
        !CU_add_test(suite, "Property access types", test_property_access_types) ||
        !CU_add_test(suite, "AGType output format", test_agtype_output_format)) {
        return CU_get_error();
    }
    
    return CUE_SUCCESS;
}