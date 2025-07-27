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

/* Setup function - create test database */
static int setup_executor_relationships_suite(void)
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
    return 0;
}

/* Teardown function */
static int teardown_executor_relationships_suite(void)
{
    if (test_db) {
        sqlite3_close(test_db);
        test_db = NULL;
    }
    return 0;
}

/* Test relationship creation execution */
static void test_relationship_creation_execution(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        const char *query = "CREATE (a:Person {name: \"Alice\"})-[r:KNOWS]->(b:Person {name: \"Bob\"})";
        cypher_result *result = cypher_executor_execute(executor, query);
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("Relationship creation error: %s\n", result->error_message);
            } else {
                printf("Relationship CREATE succeeded: nodes=%d, rels=%d\n", 
                       result->nodes_created, result->relationships_created);
                CU_ASSERT_TRUE(result->nodes_created >= 2);
                CU_ASSERT_TRUE(result->relationships_created >= 1);
            }
            cypher_result_free(result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test multiple relationship types */
static void test_multiple_relationship_types(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        const char *query = "CREATE (a:Person {name: \"Alice\"})-[r1:KNOWS]->(b:Person {name: \"Bob\"}), (a)-[r2:WORKS_WITH]->(c:Person {name: \"Charlie\"})";
        cypher_result *result = cypher_executor_execute(executor, query);
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("Multiple relationship types error: %s\n", result->error_message);
            } else {
                printf("Multiple relationship CREATE succeeded: nodes=%d (expected 3), rels=%d (expected 2)\n", 
                       result->nodes_created, result->relationships_created);
                CU_ASSERT_TRUE(result->nodes_created >= 3);
                CU_ASSERT_TRUE(result->relationships_created >= 2);
            }
            cypher_result_free(result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test bidirectional relationship creation */
static void test_bidirectional_relationship_creation(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        const char *query = "CREATE (a:Person {name: \"Alice\"})-[r1:FRIENDS]->(b:Person {name: \"Bob\"})<-[r2:LIKES]-(a)";
        cypher_result *result = cypher_executor_execute(executor, query);
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("Bidirectional relationship error: %s\n", result->error_message);
            } else {
                printf("Bidirectional relationship CREATE succeeded\n");
            }
            cypher_result_free(result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test relationship with properties */
static void test_relationship_with_properties(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        const char *query = "CREATE (a:Person {name: \"Alice\"})-[r:KNOWS {since: 2020, strength: 8.5}]->(b:Person {name: \"Bob\"})";
        cypher_result *result = cypher_executor_execute(executor, query);
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("Relationship with properties error: %s\n", result->error_message);
            } else {
                printf("Relationship with properties CREATE succeeded\n");
            }
            cypher_result_free(result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test complex path creation */
static void test_complex_path_creation(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        const char *query = "CREATE (a:Person {name: \"Alice\"})-[r1:KNOWS]->(b:Person {name: \"Bob\"})-[r2:WORKS_AT]->(c:Company {name: \"TechCorp\"})";
        cypher_result *result = cypher_executor_execute(executor, query);
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("Complex path creation error: %s\n", result->error_message);
            } else {
                printf("Complex path CREATE succeeded: nodes=%d, rels=%d\n", 
                       result->nodes_created, result->relationships_created);
                CU_ASSERT_TRUE(result->nodes_created >= 3);
                CU_ASSERT_TRUE(result->relationships_created >= 2);
            }
            cypher_result_free(result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test relationship matching (once relationships are created) */
static void test_relationship_matching(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* First create some relationships */
        const char *create_query = "CREATE (a:Person {name: \"Alice\"})-[r1:KNOWS]->(b:Person {name: \"Bob\"}), "
                                   "(b)-[r2:WORKS_AT]->(c:Company {name: \"TechCorp\"}), "
                                   "(c)-[r3:LOCATED_IN]->(d:City {name: \"San Francisco\"}), "
                                   "(a)-[r4:LIVES_IN]->(d), "
                                   "(a)-[r5:FRIENDS]->(e:Person {name: \"Charlie\"})";
        
        cypher_result *create_result = cypher_executor_execute(executor, create_query);
        CU_ASSERT_PTR_NOT_NULL(create_result);
        
        if (create_result) {
            CU_ASSERT_TRUE(create_result->success);
            cypher_result_free(create_result);
        }
        
        /* Now test relationship matching */
        const char *match_query = "MATCH (a:Person)-[r:KNOWS]->(b:Person) RETURN a, r, b";
        cypher_result *match_result = cypher_executor_execute(executor, match_query);
        CU_ASSERT_PTR_NOT_NULL(match_result);
        
        if (match_result) {
            CU_ASSERT_TRUE(match_result->success);
            if (!match_result->success) {
                printf("Relationship matching error: %s\n", match_result->error_message);
            } else {
                printf("Relationship matching succeeded: rows=%d\n", match_result->row_count);
            }
            cypher_result_free(match_result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test database state after relationship operations */
static void test_relationship_database_state(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create complex relationship pattern */
        const char *create_query = "CREATE (a:Person {name: \"Alice\"})-[r1:MANAGES]->(b:Person {name: \"Bob\"}), "
                                   "(b)-[r2:DEVELOPS]->(c:Project {name: \"WebApp\"})";
        
        cypher_result *create_result = cypher_executor_execute(executor, create_query);
        CU_ASSERT_PTR_NOT_NULL(create_result);
        
        if (create_result) {
            CU_ASSERT_TRUE(create_result->success);
            cypher_result_free(create_result);
        }
        
        /* Check database state */
        const char *tables_sql = "SELECT name FROM sqlite_master WHERE type='table' AND name LIKE '%node%' OR name LIKE '%edge%'";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(test_db, tables_sql, -1, &stmt, NULL);
        CU_ASSERT_EQUAL(rc, SQLITE_OK);
        
        if (rc == SQLITE_OK) {
            int node_tables = 0, edge_tables = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *table_name = (const char*)sqlite3_column_text(stmt, 0);
                if (strstr(table_name, "node")) {
                    node_tables++;
                } else if (strstr(table_name, "edge")) {
                    edge_tables++;
                }
            }
            printf("Found %d node tables in database\n", node_tables);
            printf("Found %d edge tables in database\n", edge_tables);
            CU_ASSERT_TRUE(node_tables > 0);
            CU_ASSERT_TRUE(edge_tables > 0);
            sqlite3_finalize(stmt);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test self-referencing relationships */
static void test_self_referencing_relationship(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        const char *query = "CREATE (a:Person {name: \"Alice\"})-[r:FOLLOWS]->(a)";
        cypher_result *result = cypher_executor_execute(executor, query);
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("Self-referencing relationship error: %s\n", result->error_message);
            } else {
                printf("Self-referencing relationship CREATE succeeded: nodes=%d, rels=%d\n", 
                       result->nodes_created, result->relationships_created);
                CU_ASSERT_TRUE(result->nodes_created >= 1);
                CU_ASSERT_TRUE(result->relationships_created >= 1);
            }
            cypher_result_free(result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test multiple relationships between same nodes with different types */
static void test_multiple_relationships_same_nodes(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        const char *query = "CREATE (a:Person {name: \"Alice\"})-[r1:KNOWS]->(b:Person {name: \"Bob\"}), "
                           "(a)-[r2:WORKS_WITH]->(b), (a)-[r3:FRIENDS]->(b)";
        cypher_result *result = cypher_executor_execute(executor, query);
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("Multiple relationships same nodes error: %s\n", result->error_message);
            } else {
                printf("Multiple relationships same nodes CREATE succeeded: nodes=%d, rels=%d\n", 
                       result->nodes_created, result->relationships_created);
                CU_ASSERT_TRUE(result->nodes_created >= 2);
                CU_ASSERT_TRUE(result->relationships_created >= 3);
            }
            cypher_result_free(result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test long path patterns */
static void test_long_path_pattern(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        const char *query = "CREATE (a:Person {name: \"A\"})-[r1:CONNECTED]->(b:Person {name: \"B\"})-[r2:CONNECTED]->(c:Person {name: \"C\"})-[r3:CONNECTED]->(d:Person {name: \"D\"})-[r4:CONNECTED]->(e:Person {name: \"E\"})";
        cypher_result *result = cypher_executor_execute(executor, query);
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("Long path pattern error: %s\n", result->error_message);
            } else {
                printf("Long path CREATE succeeded: nodes=%d, rels=%d\n", 
                       result->nodes_created, result->relationships_created);
                CU_ASSERT_TRUE(result->nodes_created >= 5);
                CU_ASSERT_TRUE(result->relationships_created >= 4);
            }
            cypher_result_free(result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test relationships with no type (should default to RELATED) */
static void test_relationship_no_type(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        const char *query = "CREATE (a:Person {name: \"Alice\"})-[r]->(b:Person {name: \"Bob\"})";
        cypher_result *result = cypher_executor_execute(executor, query);
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("No-type relationship error: %s\n", result->error_message);
            } else {
                printf("No-type relationship CREATE succeeded: nodes=%d, rels=%d\n", 
                       result->nodes_created, result->relationships_created);
                CU_ASSERT_TRUE(result->nodes_created >= 2);
                CU_ASSERT_TRUE(result->relationships_created >= 1);
            }
            cypher_result_free(result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test undirected relationships */
static void test_undirected_relationship(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        const char *query = "CREATE (a:Person {name: \"Alice\"})-[r:CONNECTED]-(b:Person {name: \"Bob\"})";
        cypher_result *result = cypher_executor_execute(executor, query);
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("Undirected relationship error: %s\n", result->error_message);
            } else {
                printf("Undirected relationship CREATE succeeded: nodes=%d, rels=%d\n", 
                       result->nodes_created, result->relationships_created);
                CU_ASSERT_TRUE(result->nodes_created >= 2);
                CU_ASSERT_TRUE(result->relationships_created >= 1);
            }
            cypher_result_free(result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test relationship variable reuse across patterns */
static void test_relationship_variable_reuse(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        const char *query = "CREATE (a:Person {name: \"Alice\"})-[r1:KNOWS]->(b:Person {name: \"Bob\"}), "
                           "(c:Person {name: \"Charlie\"})-[r2:KNOWS]->(d:Person {name: \"David\"})";
        cypher_result *result = cypher_executor_execute(executor, query);
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("Relationship variable reuse error: %s\n", result->error_message);
            } else {
                printf("Relationship variable reuse CREATE succeeded: nodes=%d, rels=%d\n", 
                       result->nodes_created, result->relationships_created);
                CU_ASSERT_TRUE(result->nodes_created >= 4);
                CU_ASSERT_TRUE(result->relationships_created >= 2);
            }
            cypher_result_free(result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Initialize the relationships executor test suite */
int init_executor_relationships_suite(void)
{
    CU_pSuite suite = CU_add_suite("Executor Relationships", setup_executor_relationships_suite, teardown_executor_relationships_suite);
    if (!suite) {
        return CU_get_error();
    }
    
    /* Add tests */
    if (!CU_add_test(suite, "Relationship creation execution", test_relationship_creation_execution) ||
        !CU_add_test(suite, "Multiple relationship types", test_multiple_relationship_types) ||
        !CU_add_test(suite, "Bidirectional relationship creation", test_bidirectional_relationship_creation) ||
        !CU_add_test(suite, "Relationship with properties", test_relationship_with_properties) ||
        !CU_add_test(suite, "Complex path creation", test_complex_path_creation) ||
        !CU_add_test(suite, "Relationship matching", test_relationship_matching) ||
        !CU_add_test(suite, "Relationship database state", test_relationship_database_state) ||
        !CU_add_test(suite, "Self-referencing relationship", test_self_referencing_relationship) ||
        !CU_add_test(suite, "Multiple relationships same nodes", test_multiple_relationships_same_nodes) ||
        !CU_add_test(suite, "Long path pattern", test_long_path_pattern) ||
        !CU_add_test(suite, "Relationship no type", test_relationship_no_type) ||
        !CU_add_test(suite, "Undirected relationship", test_undirected_relationship) ||
        !CU_add_test(suite, "Relationship variable reuse", test_relationship_variable_reuse)) {
        return CU_get_error();
    }
    
    return CUE_SUCCESS;
}