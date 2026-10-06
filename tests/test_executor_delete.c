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
static int setup_executor_delete_suite(void)
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
static int teardown_executor_delete_suite(void)
{
    if (test_db) {
        sqlite3_close(test_db);
        test_db = NULL;
    }
    return 0;
}

/* Helper function to count nodes with specific label */
static int count_nodes_with_label(cypher_executor *executor, const char *label)
{
    char query[256];
    snprintf(query, sizeof(query), "MATCH (n:%s) RETURN COUNT(n) AS count", label);
    
    cypher_result *result = cypher_executor_execute(executor, query);
    if (!result || !result->success || result->row_count == 0) {
        if (result) cypher_result_free(result);
        return -1;
    }
    
    int count = result->data[0][0] ? atoi(result->data[0][0]) : 0;
    cypher_result_free(result);
    return count;
}

/* Helper function to count relationships with specific type */
static int count_relationships_with_type(cypher_executor *executor, const char *type)
{
    char query[256];
    snprintf(query, sizeof(query), "MATCH ()-[r:%s]->() RETURN COUNT(r) AS count", type);
    
    cypher_result *result = cypher_executor_execute(executor, query);
    if (!result || !result->success || result->row_count == 0) {
        if (result) cypher_result_free(result);
        return -1;
    }
    
    int count = result->data[0][0] ? atoi(result->data[0][0]) : 0;
    cypher_result_free(result);
    return count;
}

/* Test delete_edge_by_id helper function */
static void test_delete_edge_by_id(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create test relationships */
        const char *create_query = "CREATE (a:DelEdgeTest {name: \"A\"})-[r1:TEST_REL {prop: \"value1\"}]->(b:DelEdgeTest {name: \"B\"}), "
                                   "(b)-[r2:TEST_REL {prop: \"value2\"}]->(c:DelEdgeTest {name: \"C\"})";
        cypher_result *create_result = cypher_executor_execute(executor, create_query);
        CU_ASSERT_PTR_NOT_NULL(create_result);
        
        if (create_result) {
            CU_ASSERT_TRUE(create_result->success);
            if (!create_result->success) {
                printf("Create for edge delete test failed: %s\n", create_result->error_message);
            } else {
                printf("Created test relationships for edge deletion\n");
            }
            cypher_result_free(create_result);
        }
        
        /* Get edge ID to delete */
        const char *get_edge_query = "MATCH ()-[r:TEST_REL]->() WHERE r.prop = \"value1\" RETURN id(r) LIMIT 1";
        cypher_result *edge_result = cypher_executor_execute(executor, get_edge_query);
        
        if (edge_result && edge_result->success && edge_result->row_count > 0) {
            const char *edge_id_str = edge_result->data[0][0];
            if (edge_id_str) {
                int edge_id = atoi(edge_id_str);
                printf("Found edge to delete with ID: %d\n", edge_id);
                
                /* Test the delete_edge_by_id function - commented out since it's static */
                /* int delete_result = delete_edge_by_id(executor, edge_id); */
                printf("Found edge with ID %d for deletion test\n", edge_id);
                /* TODO: Test through DELETE clause once implemented */
            }
            cypher_result_free(edge_result);
        } else {
            printf("Could not get edge ID for deletion test\n");
            if (edge_result) cypher_result_free(edge_result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test delete_node_by_id helper function */
static void test_delete_node_by_id(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create test nodes */
        const char *create_query = "CREATE (a:DelNodeTest {name: \"ToDelete\", value: 123}), "
                                   "(b:DelNodeTest {name: \"KeepThis\", value: 456})";
        cypher_result *create_result = cypher_executor_execute(executor, create_query);
        CU_ASSERT_PTR_NOT_NULL(create_result);
        
        if (create_result) {
            CU_ASSERT_TRUE(create_result->success);
            if (!create_result->success) {
                printf("Create for node delete test failed: %s\n", create_result->error_message);
            } else {
                printf("Created test nodes for node deletion\n");
            }
            cypher_result_free(create_result);
        }
        
        /* Get node ID to delete */
        const char *get_node_query = "MATCH (n:DelNodeTest) WHERE n.name = \"ToDelete\" RETURN id(n) LIMIT 1";
        cypher_result *node_result = cypher_executor_execute(executor, get_node_query);
        
        if (node_result && node_result->success && node_result->row_count > 0) {
            const char *node_id_str = node_result->data[0][0];
            if (node_id_str) {
                int node_id = atoi(node_id_str);
                printf("Found node to delete with ID: %d\n", node_id);
                
                /* Test the delete_node_by_id function - commented out since it's static */
                /* int delete_result = delete_node_by_id(executor, node_id); */
                printf("Found node with ID %d for deletion test\n", node_id);
                /* TODO: Test through DELETE clause once implemented */
            }
            cypher_result_free(node_result);
        } else {
            printf("Could not get node ID for deletion test\n");
            if (node_result) cypher_result_free(node_result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test DELETE clause execution */
static void test_delete_clause_execution(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create test data */
        const char *create_query = "CREATE (a:DeleteTest {name: \"Alice\"}), (b:DeleteTest {name: \"Bob\"})";
        cypher_result *create_result = cypher_executor_execute(executor, create_query);
        CU_ASSERT_PTR_NOT_NULL(create_result);
        
        if (create_result) {
            CU_ASSERT_TRUE(create_result->success);
            cypher_result_free(create_result);
        }
        
        /* Test DELETE clause */
        const char *delete_query = "MATCH (n:DeleteTest) WHERE n.name = \"Alice\" DELETE n";
        cypher_result *delete_result = cypher_executor_execute(executor, delete_query);
        CU_ASSERT_PTR_NOT_NULL(delete_result);
        
        if (delete_result) {
            if (!delete_result->success) {
                printf("DELETE clause execution error: %s\n", delete_result->error_message);
                /* DELETE may not be fully implemented yet */
            } else {
                printf("DELETE clause executed successfully: nodes_deleted=%d\n", 
                       delete_result->nodes_deleted);
                CU_ASSERT_TRUE(delete_result->nodes_deleted > 0);
            }
            cypher_result_free(delete_result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test DELETE with relationships */
static void test_delete_with_relationships(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create test data with relationships */
        const char *create_query = "CREATE (a:DeleteRelTest {name: \"A\"})-[r:CONNECTED]->(b:DeleteRelTest {name: \"B\"})";
        cypher_result *create_result = cypher_executor_execute(executor, create_query);
        CU_ASSERT_PTR_NOT_NULL(create_result);
        
        if (create_result) {
            CU_ASSERT_TRUE(create_result->success);
            cypher_result_free(create_result);
        }
        
        /* Test DELETE relationship */
        const char *delete_query = "MATCH ()-[r:CONNECTED]->() DELETE r";
        cypher_result *delete_result = cypher_executor_execute(executor, delete_query);
        CU_ASSERT_PTR_NOT_NULL(delete_result);
        
        if (delete_result) {
            if (!delete_result->success) {
                printf("DELETE relationship error: %s\n", delete_result->error_message);
                /* DELETE may not be fully implemented yet */
            } else {
                printf("DELETE relationship executed successfully: rels_deleted=%d\n", 
                       delete_result->relationships_deleted);
            }
            cypher_result_free(delete_result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test DELETE multiple items */
static void test_delete_multiple_items(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create test data */
        const char *create_query = "CREATE (a:DeleteMultiTest {name: \"A\"})-[r:REL]->(b:DeleteMultiTest {name: \"B\"})";
        cypher_result *create_result = cypher_executor_execute(executor, create_query);
        CU_ASSERT_PTR_NOT_NULL(create_result);
        
        if (create_result) {
            CU_ASSERT_TRUE(create_result->success);
            cypher_result_free(create_result);
        }
        
        /* Test DELETE multiple items */
        const char *delete_query = "MATCH (a:DeleteMultiTest)-[r:REL]->(b:DeleteMultiTest) DELETE a, r, b";
        cypher_result *delete_result = cypher_executor_execute(executor, delete_query);
        CU_ASSERT_PTR_NOT_NULL(delete_result);
        
        if (delete_result) {
            /* GQLITE-T-0254: the statement is verified as a whole, so the
             * relationship r being deleted in the same clause makes `a` and
             * `b` deletable regardless of item order. */
            if (!delete_result->success) {
                printf("DELETE multiple items error: %s\n", delete_result->error_message);
            }
            CU_ASSERT_TRUE(delete_result->success);
            if (delete_result->success) {
                CU_ASSERT_EQUAL(delete_result->nodes_deleted, 2);
                CU_ASSERT_EQUAL(delete_result->relationships_deleted, 1);
            }
            cypher_result_free(delete_result);
        }
        CU_ASSERT_EQUAL(count_nodes_with_label(executor, "DeleteMultiTest"), 0);
        
        cypher_executor_free(executor);
    }
}

/* Test DELETE with WHERE clause */
static void test_delete_with_where(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create test data */
        const char *create_query = "CREATE (a:DeleteWhereTest {age: 25}), (b:DeleteWhereTest {age: 35}), (c:DeleteWhereTest {age: 45})";
        cypher_result *create_result = cypher_executor_execute(executor, create_query);
        CU_ASSERT_PTR_NOT_NULL(create_result);
        
        if (create_result) {
            CU_ASSERT_TRUE(create_result->success);
            cypher_result_free(create_result);
        }
        
        /* Test DELETE with WHERE */
        const char *delete_query = "MATCH (n:DeleteWhereTest) WHERE n.age > 30 DELETE n";
        cypher_result *delete_result = cypher_executor_execute(executor, delete_query);
        CU_ASSERT_PTR_NOT_NULL(delete_result);
        
        if (delete_result) {
            if (!delete_result->success) {
                printf("DELETE with WHERE error: %s\n", delete_result->error_message);
                /* DELETE may not be fully implemented yet */
            } else {
                printf("DELETE with WHERE executed successfully\n");
            }
            cypher_result_free(delete_result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test DETACH DELETE (delete node and its relationships) */
static void test_detach_delete(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* Create test data with relationships - use separate statements */
        const char *create_query1 = "CREATE (a:DetachTest {name: \"Central\"})-[r1:OUT]->(b:DetachTest {name: \"B\"})";
        cypher_result *create_result1 = cypher_executor_execute(executor, create_query1);
        CU_ASSERT_PTR_NOT_NULL(create_result1);

        if (create_result1) {
            if (!create_result1->success) {
                printf("DETACH DELETE setup error: %s\n", create_result1->error_message);
            }
            cypher_result_free(create_result1);
        }

        /* Create incoming relationship */
        const char *create_query2 = "MATCH (a:DetachTest {name: \"Central\"}) CREATE (c:DetachTest {name: \"C\"})-[r2:IN]->(a)";
        cypher_result *create_result2 = cypher_executor_execute(executor, create_query2);
        CU_ASSERT_PTR_NOT_NULL(create_result2);

        if (create_result2) {
            if (!create_result2->success) {
                printf("DETACH DELETE setup error: %s\n", create_result2->error_message);
            }
            cypher_result_free(create_result2);
        }

        /* Test DETACH DELETE */
        const char *delete_query = "MATCH (n:DetachTest) WHERE n.name = \"Central\" DETACH DELETE n";
        cypher_result *delete_result = cypher_executor_execute(executor, delete_query);
        CU_ASSERT_PTR_NOT_NULL(delete_result);

        if (delete_result) {
            if (!delete_result->success) {
                printf("DETACH DELETE error: %s\n", delete_result->error_message);
                /* DETACH DELETE may not be fully implemented yet */
            } else {
                printf("DETACH DELETE executed successfully\n");
            }
            cypher_result_free(delete_result);
        }

        cypher_executor_free(executor);
    }
}

/* Test DELETE error conditions */
static void test_delete_error_conditions(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Test DELETE without MATCH */
        const char *delete_query1 = "DELETE n";
        cypher_result *delete_result1 = cypher_executor_execute(executor, delete_query1);
        CU_ASSERT_PTR_NOT_NULL(delete_result1);
        
        if (delete_result1) {
            CU_ASSERT_FALSE(delete_result1->success);
            printf("DELETE without MATCH correctly failed: %s\n", 
                   delete_result1->error_message ? delete_result1->error_message : "Parse error");
            cypher_result_free(delete_result1);
        }
        
        /* Test DELETE undefined variable */
        const char *delete_query2 = "MATCH (a) DELETE b";
        cypher_result *delete_result2 = cypher_executor_execute(executor, delete_query2);
        CU_ASSERT_PTR_NOT_NULL(delete_result2);
        
        if (delete_result2) {
            if (!delete_result2->success) {
                printf("DELETE undefined variable correctly failed: %s\n", 
                       delete_result2->error_message ? delete_result2->error_message : "Unknown error");
            }
            cypher_result_free(delete_result2);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test DELETE with anonymous entities */
static void test_delete_anonymous_entities(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create test data */
        const char *create_query = "CREATE (a:DeleteAnonTest {name: \"A\"})-[r:REL]->(b:DeleteAnonTest {name: \"B\"})";
        cypher_result *create_result = cypher_executor_execute(executor, create_query);
        CU_ASSERT_PTR_NOT_NULL(create_result);
        
        if (create_result) {
            CU_ASSERT_TRUE(create_result->success);
            cypher_result_free(create_result);
        }
        
        /* Test DELETE with anonymous pattern */
        const char *delete_query = "MATCH (a:DeleteAnonTest)-[]->(b:DeleteAnonTest) DELETE a";
        cypher_result *delete_result = cypher_executor_execute(executor, delete_query);
        CU_ASSERT_PTR_NOT_NULL(delete_result);
        
        if (delete_result) {
            /* GQLITE-T-0254: `a` still has the REL relationship and the
             * statement has no DETACH, so it must fail with
             * ConstraintVerificationFailed: DeleteConnectedNode (this test
             * used to accept either outcome). */
            CU_ASSERT_FALSE(delete_result->success);
            CU_ASSERT_PTR_NOT_NULL(delete_result->error_message);
            if (delete_result->error_message) {
                CU_ASSERT_PTR_NOT_NULL(strstr(delete_result->error_message,
                    "ConstraintVerificationFailed: DeleteConnectedNode"));
            }
            cypher_result_free(delete_result);
        }
        CU_ASSERT_EQUAL(count_nodes_with_label(executor, "DeleteAnonTest"), 2);
        
        cypher_executor_free(executor);
    }
}

/* GQLITE-T-0253: reading a property or the labels of an entity deleted in
 * the same statement raises EntityNotFound: DeletedEntityAccess and leaves
 * the graph unchanged. RETURN n / id(n) / type(r) / count(*) stay legal, and
 * a null binding (OPTIONAL MATCH miss) is not a deleted entity. */
static void run_and_expect_deleted_access_error(cypher_executor *executor, const char *query)
{
    cypher_result *res = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(res);
    if (!res) return;
    CU_ASSERT_FALSE(res->success);
    CU_ASSERT_PTR_NOT_NULL(res->error_message);
    if (res->error_message) {
        if (!strstr(res->error_message, "EntityNotFound: DeletedEntityAccess")) {
            printf("\nT-0253 unexpected error for %s: %s\n", query, res->error_message);
        }
        CU_ASSERT_PTR_NOT_NULL(strstr(res->error_message, "EntityNotFound: DeletedEntityAccess"));
    }
    cypher_result_free(res);
}

static void test_delete_then_access_deleted_entity(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    if (!executor) return;

    cypher_result *setup = cypher_executor_execute(executor,
        "CREATE (:Del253 {num: 0})-[:T253 {num: 7}]->(:Del253 {num: 1})");
    CU_ASSERT_PTR_NOT_NULL(setup);
    if (setup) { CU_ASSERT_TRUE(setup->success); cypher_result_free(setup); }

    /* Property of a deleted node, labels of a deleted node, property inside a
     * larger expression, property of a deleted relationship. */
    run_and_expect_deleted_access_error(executor,
        "MATCH ()-[r:T253]->() DELETE r RETURN r.num");
    run_and_expect_deleted_access_error(executor,
        "MATCH (n:Del253) DETACH DELETE n RETURN n.num");
    run_and_expect_deleted_access_error(executor,
        "MATCH (n:Del253) DETACH DELETE n RETURN labels(n)");
    run_and_expect_deleted_access_error(executor,
        "MATCH (n:Del253) DETACH DELETE n RETURN n.num + 1 AS x");
    run_and_expect_deleted_access_error(executor,
        "MATCH (n:Del253) DETACH DELETE n RETURN properties(n) AS p");

    /* The failed statements must not have deleted anything. */
    CU_ASSERT_EQUAL(count_nodes_with_label(executor, "Del253"), 2);
    CU_ASSERT_EQUAL(count_relationships_with_type(executor, "T253"), 1);

    /* A null binding is not a deleted entity: null propagates, no error. */
    cypher_result *opt = cypher_executor_execute(executor,
        "OPTIONAL MATCH (a:Del253DoesNotExist) DELETE a RETURN a.num");
    CU_ASSERT_PTR_NOT_NULL(opt);
    if (opt) {
        CU_ASSERT_TRUE(opt->success);
        if (opt->success) {
            CU_ASSERT_EQUAL(opt->row_count, 1);
            if (opt->row_count == 1) CU_ASSERT_PTR_NULL(opt->data[0][0]);
        }
        cypher_result_free(opt);
    }

    /* Identity-only projections of a deleted entity stay legal. */
    cypher_result *type_res = cypher_executor_execute(executor,
        "MATCH ()-[r:T253]->() DELETE r RETURN type(r)");
    CU_ASSERT_PTR_NOT_NULL(type_res);
    if (type_res) {
        CU_ASSERT_TRUE(type_res->success);
        if (type_res->success && type_res->row_count == 1 && type_res->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(type_res->data[0][0], "T253");
        }
        cypher_result_free(type_res);
    }
    CU_ASSERT_EQUAL(count_relationships_with_type(executor, "T253"), 0);

    cypher_result *cnt = cypher_executor_execute(executor,
        "MATCH (n:Del253) DELETE n RETURN count(*) AS c");
    CU_ASSERT_PTR_NOT_NULL(cnt);
    if (cnt) {
        CU_ASSERT_TRUE(cnt->success);
        if (cnt->success && cnt->row_count == 1 && cnt->data[0][0]) {
            CU_ASSERT_STRING_EQUAL(cnt->data[0][0], "2");
        }
        cypher_result_free(cnt);
    }
    CU_ASSERT_EQUAL(count_nodes_with_label(executor, "Del253"), 0);

    cypher_executor_free(executor);
}

/* GQLITE-T-0254: DELETE without DETACH on a node with incident
 * relationships raises ConstraintVerificationFailed: DeleteConnectedNode and
 * leaves the graph unchanged (no partial application across rows); DETACH
 * DELETE and deleting the relationship in the same clause keep working. */
static void test_delete_connected_node_constraint(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    if (!executor) return;

    /* Two free nodes and one connected node, all matched by one DELETE. The
     * free ones must NOT be deleted when the connected one fails. */
    cypher_result *setup = cypher_executor_execute(executor,
        "CREATE (:Free254 {k: 1}), (:Free254 {k: 2}), (x:Conn254)-[:R254]->(:Other254), (x)-[:R254]->(:Other254)");
    CU_ASSERT_PTR_NOT_NULL(setup);
    if (setup) { CU_ASSERT_TRUE(setup->success); cypher_result_free(setup); }

    cypher_result *res = cypher_executor_execute(executor,
        "MATCH (n) WHERE n:Free254 OR n:Conn254 DELETE n");
    CU_ASSERT_PTR_NOT_NULL(res);
    if (res) {
        CU_ASSERT_FALSE(res->success);
        CU_ASSERT_PTR_NOT_NULL(res->error_message);
        if (res->error_message) {
            if (!strstr(res->error_message, "ConstraintVerificationFailed: DeleteConnectedNode")) {
                printf("\nT-0254 unexpected error: %s\n", res->error_message);
            }
            CU_ASSERT_PTR_NOT_NULL(strstr(res->error_message,
                "ConstraintVerificationFailed: DeleteConnectedNode"));
        }
        cypher_result_free(res);
    }
    /* Graph unchanged: both free nodes, the connected node and its edges remain. */
    CU_ASSERT_EQUAL(count_nodes_with_label(executor, "Free254"), 2);
    CU_ASSERT_EQUAL(count_nodes_with_label(executor, "Conn254"), 1);
    CU_ASSERT_EQUAL(count_relationships_with_type(executor, "R254"), 2);

    /* Deleting the node together with its relationships in one clause is
     * fine, whatever the item order. */
    cypher_result *with_rels = cypher_executor_execute(executor,
        "MATCH (x:Conn254)-[r:R254]->() DELETE x, r");
    CU_ASSERT_PTR_NOT_NULL(with_rels);
    if (with_rels) {
        if (!with_rels->success) {
            printf("\nT-0254 DELETE x, r error: %s\n", with_rels->error_message);
        }
        CU_ASSERT_TRUE(with_rels->success);
        if (with_rels->success) {
            CU_ASSERT_EQUAL(with_rels->nodes_deleted, 1);
            CU_ASSERT_EQUAL(with_rels->relationships_deleted, 2);
        }
        cypher_result_free(with_rels);
    }
    CU_ASSERT_EQUAL(count_nodes_with_label(executor, "Conn254"), 0);
    CU_ASSERT_EQUAL(count_relationships_with_type(executor, "R254"), 0);

    /* DETACH DELETE still cascades. */
    cypher_result *setup2 = cypher_executor_execute(executor,
        "CREATE (y:Conn254b)-[:R254b]->(:Other254), (y)-[:R254b]->(:Other254)");
    CU_ASSERT_PTR_NOT_NULL(setup2);
    if (setup2) { CU_ASSERT_TRUE(setup2->success); cypher_result_free(setup2); }
    cypher_result *detach = cypher_executor_execute(executor,
        "MATCH (y:Conn254b) DETACH DELETE y");
    CU_ASSERT_PTR_NOT_NULL(detach);
    if (detach) {
        CU_ASSERT_TRUE(detach->success);
        if (detach->success) {
            CU_ASSERT_EQUAL(detach->nodes_deleted, 1);
            CU_ASSERT_EQUAL(detach->relationships_deleted, 2);
        }
        cypher_result_free(detach);
    }
    CU_ASSERT_EQUAL(count_nodes_with_label(executor, "Conn254b"), 0);
    CU_ASSERT_EQUAL(count_relationships_with_type(executor, "R254b"), 0);

    cypher_executor_free(executor);
}

/* Initialize the DELETE executor test suite */
int init_executor_delete_suite(void)
{
    CU_pSuite suite = CU_add_suite("Executor DELETE", setup_executor_delete_suite, teardown_executor_delete_suite);
    if (!suite) {
        return CU_get_error();
    }
    
    /* Add tests */
    if (!CU_add_test(suite, "Delete edge by ID", test_delete_edge_by_id) ||
        !CU_add_test(suite, "Delete node by ID", test_delete_node_by_id) ||
        !CU_add_test(suite, "DELETE clause execution", test_delete_clause_execution) ||
        !CU_add_test(suite, "DELETE with relationships", test_delete_with_relationships) ||
        !CU_add_test(suite, "DELETE multiple items", test_delete_multiple_items) ||
        !CU_add_test(suite, "DELETE with WHERE", test_delete_with_where) ||
        !CU_add_test(suite, "DETACH DELETE", test_detach_delete) ||
        !CU_add_test(suite, "DELETE error conditions", test_delete_error_conditions) ||
        !CU_add_test(suite, "DELETE anonymous entities", test_delete_anonymous_entities) ||
        !CU_add_test(suite, "T-0253: access of deleted entity raises EntityNotFound", test_delete_then_access_deleted_entity) ||
        !CU_add_test(suite, "T-0254: DELETE of a connected node raises ConstraintVerificationFailed", test_delete_connected_node_constraint)) {
        return CU_get_error();
    }
    
    return CUE_SUCCESS;
}