/*
 * Multi-graph query tests
 * Tests for MATCH ... FROM graph_name queries across attached databases
 */

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

/* Test database handles */
static sqlite3 *main_db = NULL;
static const char *attached_db_path = "/tmp/graphqlite_test_attached.db";

/* Helper to initialize graph schema in a database */
static int init_graph_schema(sqlite3 *db)
{
    cypher_schema_manager *schema_mgr = cypher_schema_create_manager(db);
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

/* Setup function - create main and attached databases */
static int setup_multigraph_suite(void)
{
    int rc;

    /* Remove any existing attached db file */
    remove(attached_db_path);

    /* Create main in-memory database */
    rc = sqlite3_open(":memory:", &main_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to open main database\n");
        return -1;
    }

    /* Initialize schema in main database */
    if (init_graph_schema(main_db) < 0) {
        fprintf(stderr, "Failed to initialize main schema\n");
        return -1;
    }

    /* Create attached database file and initialize its schema */
    sqlite3 *attached_db = NULL;
    rc = sqlite3_open(attached_db_path, &attached_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create attached database\n");
        return -1;
    }

    if (init_graph_schema(attached_db) < 0) {
        fprintf(stderr, "Failed to initialize attached schema\n");
        sqlite3_close(attached_db);
        return -1;
    }
    sqlite3_close(attached_db);

    /* Attach the database to main */
    char attach_sql[512];
    snprintf(attach_sql, sizeof(attach_sql), "ATTACH DATABASE '%s' AS other_graph", attached_db_path);
    rc = sqlite3_exec(main_db, attach_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to attach database: %s\n", sqlite3_errmsg(main_db));
        return -1;
    }

    return 0;
}

/* Teardown function */
static int teardown_multigraph_suite(void)
{
    if (main_db) {
        sqlite3_exec(main_db, "DETACH DATABASE other_graph", NULL, NULL, NULL);
        sqlite3_close(main_db);
        main_db = NULL;
    }
    remove(attached_db_path);
    return 0;
}

/* Test that we can create data in both graphs */
static void test_create_in_both_graphs(void)
{
    cypher_executor *executor = cypher_executor_create(main_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* Create node in main graph */
        const char *main_create = "CREATE (n:Person {name: \"Alice\", location: \"main\"})";
        cypher_result *result = cypher_executor_execute(executor, main_create);
        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("Main CREATE failed: %s\n", result->error_message);
            }
            cypher_result_free(result);
        }

        /* Create node in attached graph via direct SQL (since CREATE doesn't support FROM yet) */
        int rc = sqlite3_exec(main_db,
            "INSERT INTO other_graph.nodes (id) VALUES (1);"
            "INSERT INTO other_graph.node_labels (node_id, label) VALUES (1, 'Person');"
            "INSERT INTO other_graph.property_keys (id, key) VALUES (1, 'name');"
            "INSERT INTO other_graph.property_keys (id, key) VALUES (2, 'location');"
            "INSERT INTO other_graph.node_props_text (node_id, key_id, value) VALUES (1, 1, 'Bob');"
            "INSERT INTO other_graph.node_props_text (node_id, key_id, value) VALUES (1, 2, 'other');",
            NULL, NULL, NULL);
        CU_ASSERT_EQUAL(rc, SQLITE_OK);
        if (rc != SQLITE_OK) {
            printf("Direct SQL insert failed: %s\n", sqlite3_errmsg(main_db));
        }

        cypher_executor_free(executor);
    }
}

/* Test basic MATCH FROM attached graph */
static void test_match_from_attached_graph(void)
{
    cypher_executor *executor = cypher_executor_create(main_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* First verify main graph has Alice */
        const char *main_query = "MATCH (n:Person) RETURN n.name, n.location";
        cypher_result *main_result = cypher_executor_execute(executor, main_query);
        CU_ASSERT_PTR_NOT_NULL(main_result);
        if (main_result) {
            CU_ASSERT_TRUE(main_result->success);
            printf("Main graph query: success=%d, rows=%d\n", main_result->success, main_result->row_count);
            if (main_result->success && main_result->row_count > 0) {
                printf("Main graph: name=%s, location=%s\n",
                       main_result->data[0][0] ? main_result->data[0][0] : "NULL",
                       main_result->data[0][1] ? main_result->data[0][1] : "NULL");
                CU_ASSERT_STRING_EQUAL(main_result->data[0][0], "Alice");
                CU_ASSERT_STRING_EQUAL(main_result->data[0][1], "main");
            }
            cypher_result_free(main_result);
        }

        /* Now query the attached graph using FROM clause */
        const char *attached_query = "MATCH (n:Person) FROM other_graph RETURN n.name, n.location";
        cypher_result *attached_result = cypher_executor_execute(executor, attached_query);
        CU_ASSERT_PTR_NOT_NULL(attached_result);
        if (attached_result) {
            if (!attached_result->success) {
                printf("Attached graph query failed: %s\n", attached_result->error_message);
            }
            CU_ASSERT_TRUE(attached_result->success);
            printf("Attached graph query: success=%d, rows=%d\n", attached_result->success, attached_result->row_count);
            if (attached_result->success && attached_result->row_count > 0) {
                printf("Attached graph: name=%s, location=%s\n",
                       attached_result->data[0][0] ? attached_result->data[0][0] : "NULL",
                       attached_result->data[0][1] ? attached_result->data[0][1] : "NULL");
                CU_ASSERT_STRING_EQUAL(attached_result->data[0][0], "Bob");
                CU_ASSERT_STRING_EQUAL(attached_result->data[0][1], "other");
            }
            cypher_result_free(attached_result);
        }

        cypher_executor_free(executor);
    }
}

/* Test MATCH FROM with WHERE clause */
static void test_match_from_with_where(void)
{
    cypher_executor *executor = cypher_executor_create(main_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* Add another node to attached graph for filtering */
        int rc = sqlite3_exec(main_db,
            "INSERT INTO other_graph.nodes (id) VALUES (2);"
            "INSERT INTO other_graph.node_labels (node_id, label) VALUES (2, 'Person');"
            "INSERT INTO other_graph.node_props_text (node_id, key_id, value) VALUES (2, 1, 'Charlie');"
            "INSERT INTO other_graph.node_props_text (node_id, key_id, value) VALUES (2, 2, 'other');",
            NULL, NULL, NULL);
        CU_ASSERT_EQUAL(rc, SQLITE_OK);

        /* Query with WHERE clause */
        const char *query = "MATCH (n:Person) FROM other_graph WHERE n.name = 'Bob' RETURN n.name";
        cypher_result *result = cypher_executor_execute(executor, query);
        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            if (!result->success) {
                printf("MATCH FROM WHERE failed: %s\n", result->error_message);
            }
            CU_ASSERT_TRUE(result->success);
            if (result->success) {
                printf("MATCH FROM WHERE: rows=%d\n", result->row_count);
                CU_ASSERT_EQUAL(result->row_count, 1);
                if (result->row_count > 0) {
                    CU_ASSERT_STRING_EQUAL(result->data[0][0], "Bob");
                }
            }
            cypher_result_free(result);
        }

        cypher_executor_free(executor);
    }
}

/* Test that labels() function works with FROM */
static void test_labels_from_attached_graph(void)
{
    cypher_executor *executor = cypher_executor_create(main_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        const char *query = "MATCH (n:Person) FROM other_graph RETURN n.name, labels(n)";
        cypher_result *result = cypher_executor_execute(executor, query);
        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            if (!result->success) {
                printf("labels() FROM failed: %s\n", result->error_message);
            }
            CU_ASSERT_TRUE(result->success);
            if (result->success && result->row_count > 0) {
                printf("labels() result: name=%s, labels=%s\n",
                       result->data[0][0] ? result->data[0][0] : "NULL",
                       result->data[0][1] ? result->data[0][1] : "NULL");
            }
            cypher_result_free(result);
        }

        cypher_executor_free(executor);
    }
}

/* Initialize the multi-graph test suite */
int init_executor_multigraph_suite(void)
{
    CU_pSuite suite = CU_add_suite("Executor Multi-Graph", setup_multigraph_suite, teardown_multigraph_suite);
    if (!suite) {
        return CU_get_error();
    }

    /* Add tests */
    if (!CU_add_test(suite, "Create in both graphs", test_create_in_both_graphs) ||
        !CU_add_test(suite, "MATCH FROM attached graph", test_match_from_attached_graph) ||
        !CU_add_test(suite, "MATCH FROM with WHERE", test_match_from_with_where) ||
        !CU_add_test(suite, "labels() FROM attached graph", test_labels_from_attached_graph)) {
        return CU_get_error();
    }

    return CUE_SUCCESS;
}
