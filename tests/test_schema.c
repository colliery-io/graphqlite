#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <sqlite3.h>

#include "executor/cypher_schema.h"
#include "parser/cypher_debug.h"
#include "test_schema.h"

/* Test database handle */
static sqlite3 *test_db = NULL;
static cypher_schema_manager *schema_mgr = NULL;

/* Setup function - create test database */
static int setup_schema_suite(void)
{
    /* Create in-memory database */
    int rc = sqlite3_open(":memory:", &test_db);
    if (rc != SQLITE_OK) {
        return -1;
    }
    
    /* Enable foreign key constraints */
    sqlite3_exec(test_db, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);
    
    /* Create schema manager */
    schema_mgr = cypher_schema_create_manager(test_db);
    if (!schema_mgr) {
        sqlite3_close(test_db);
        return -1;
    }
    
    return 0;
}

/* Teardown function */
static int teardown_schema_suite(void)
{
    if (schema_mgr) {
        cypher_schema_free_manager(schema_mgr);
        schema_mgr = NULL;
    }
    
    if (test_db) {
        sqlite3_close(test_db);
        test_db = NULL;
    }
    return 0;
}

/* Helper function to check if table exists */
static bool table_exists(const char *table_name)
{
    const char *sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?";
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_prepare_v2(test_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, table_name, -1, SQLITE_STATIC);
    
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    
    return exists;
}

/* Helper function to check if index exists */
static bool index_exists(const char *index_name)
{
    const char *sql = "SELECT name FROM sqlite_master WHERE type='index' AND name=?";
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_prepare_v2(test_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, index_name, -1, SQLITE_STATIC);
    
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    
    return exists;
}

/* Test schema manager creation */
static void test_schema_manager_creation(void)
{
    CU_ASSERT_PTR_NOT_NULL(schema_mgr);
    CU_ASSERT_FALSE(cypher_schema_is_initialized(schema_mgr));
}

/* Test schema initialization */
static void test_schema_initialization(void)
{
    int result = cypher_schema_initialize(schema_mgr);
    CU_ASSERT_EQUAL(result, 0);
    CU_ASSERT_TRUE(cypher_schema_is_initialized(schema_mgr));
}

/* Test table creation */
static void test_table_creation(void)
{
    /* Core tables */
    CU_ASSERT_TRUE(table_exists("nodes"));
    CU_ASSERT_TRUE(table_exists("edges"));
    CU_ASSERT_TRUE(table_exists("property_keys"));
    CU_ASSERT_TRUE(table_exists("node_labels"));
    
    /* Node property tables */
    CU_ASSERT_TRUE(table_exists("node_props_int"));
    CU_ASSERT_TRUE(table_exists("node_props_text"));
    CU_ASSERT_TRUE(table_exists("node_props_real"));
    CU_ASSERT_TRUE(table_exists("node_props_bool"));
    
    /* Edge property tables */
    CU_ASSERT_TRUE(table_exists("edge_props_int"));
    CU_ASSERT_TRUE(table_exists("edge_props_text"));
    CU_ASSERT_TRUE(table_exists("edge_props_real"));
    CU_ASSERT_TRUE(table_exists("edge_props_bool"));
}

/* Test index creation */
static void test_index_creation(void)
{
    /* Core indexes */
    CU_ASSERT_TRUE(index_exists("idx_edges_source"));
    CU_ASSERT_TRUE(index_exists("idx_edges_target"));
    CU_ASSERT_TRUE(index_exists("idx_edges_type"));
    CU_ASSERT_TRUE(index_exists("idx_node_labels_label"));
    CU_ASSERT_TRUE(index_exists("idx_property_keys_key"));
    
    /* Property indexes */
    CU_ASSERT_TRUE(index_exists("idx_node_props_int_key_value"));
    CU_ASSERT_TRUE(index_exists("idx_node_props_text_key_value"));
    CU_ASSERT_TRUE(index_exists("idx_node_props_real_key_value"));
    CU_ASSERT_TRUE(index_exists("idx_node_props_bool_key_value"));
    
    CU_ASSERT_TRUE(index_exists("idx_edge_props_int_key_value"));
    CU_ASSERT_TRUE(index_exists("idx_edge_props_text_key_value"));
    CU_ASSERT_TRUE(index_exists("idx_edge_props_real_key_value"));
    CU_ASSERT_TRUE(index_exists("idx_edge_props_bool_key_value"));
}

/* Test property type inference */
static void test_property_type_inference(void)
{
    CU_ASSERT_EQUAL(cypher_schema_infer_property_type("123"), PROP_TYPE_INTEGER);
    CU_ASSERT_EQUAL(cypher_schema_infer_property_type("123.45"), PROP_TYPE_REAL);
    CU_ASSERT_EQUAL(cypher_schema_infer_property_type("true"), PROP_TYPE_BOOLEAN);
    CU_ASSERT_EQUAL(cypher_schema_infer_property_type("false"), PROP_TYPE_BOOLEAN);
    CU_ASSERT_EQUAL(cypher_schema_infer_property_type("hello"), PROP_TYPE_TEXT);
    CU_ASSERT_EQUAL(cypher_schema_infer_property_type(""), PROP_TYPE_TEXT);
    CU_ASSERT_EQUAL(cypher_schema_infer_property_type(NULL), PROP_TYPE_TEXT);
}

/* Test property type names */
static void test_property_type_names(void)
{
    CU_ASSERT_STRING_EQUAL(cypher_schema_property_type_name(PROP_TYPE_INTEGER), "INTEGER");
    CU_ASSERT_STRING_EQUAL(cypher_schema_property_type_name(PROP_TYPE_TEXT), "TEXT");
    CU_ASSERT_STRING_EQUAL(cypher_schema_property_type_name(PROP_TYPE_REAL), "REAL");
    CU_ASSERT_STRING_EQUAL(cypher_schema_property_type_name(PROP_TYPE_BOOLEAN), "BOOLEAN");
}

/* Test basic node operations */
static void test_basic_node_operations(void)
{
    /* Initialize schema first */
    int result = cypher_schema_initialize(schema_mgr);
    CU_ASSERT_EQUAL(result, 0);
    
    /* Test node creation */
    int node_id1 = cypher_schema_create_node(schema_mgr);
    CU_ASSERT_TRUE(node_id1 > 0);
    
    int node_id2 = cypher_schema_create_node(schema_mgr);
    CU_ASSERT_TRUE(node_id2 > 0);
    CU_ASSERT_NOT_EQUAL(node_id1, node_id2);
    
    /* Test label addition */
    result = cypher_schema_add_node_label(schema_mgr, node_id1, "Person");
    CU_ASSERT_EQUAL(result, 0);
    
    result = cypher_schema_add_node_label(schema_mgr, node_id1, "Employee");
    CU_ASSERT_EQUAL(result, 0);
    
    result = cypher_schema_add_node_label(schema_mgr, node_id2, "Company");
    CU_ASSERT_EQUAL(result, 0);
    
    /* Test adding duplicate label (should succeed with INSERT OR IGNORE) */
    result = cypher_schema_add_node_label(schema_mgr, node_id1, "Person");
    CU_ASSERT_EQUAL(result, 0);
}

/* Test property key cache creation */
static void test_property_key_cache_creation(void)
{
    property_key_cache *cache = create_property_key_cache(test_db, 256);
    CU_ASSERT_PTR_NOT_NULL(cache);
    
    if (cache) {
        long hits, misses, insertions;
        property_key_cache_stats(cache, &hits, &misses, &insertions);
        
        CU_ASSERT_EQUAL(hits, 0);
        CU_ASSERT_EQUAL(misses, 0);
        CU_ASSERT_EQUAL(insertions, 0);
        
        free_property_key_cache(cache);
    }
}

/* Test property key operations */
static void test_property_key_operations(void)
{
    /* Initialize schema first */
    int result = cypher_schema_initialize(schema_mgr);
    CU_ASSERT_EQUAL(result, 0);
    
    /* Test ensure property key (creates new) */
    int name_key_id = cypher_schema_ensure_property_key(schema_mgr, "name");
    CU_ASSERT_TRUE(name_key_id > 0);
    
    int age_key_id = cypher_schema_ensure_property_key(schema_mgr, "age");
    CU_ASSERT_TRUE(age_key_id > 0);
    CU_ASSERT_NOT_EQUAL(name_key_id, age_key_id);
    
    /* Test get property key (retrieves existing) */
    int name_key_id2 = cypher_schema_get_property_key_id(schema_mgr, "name");
    CU_ASSERT_EQUAL(name_key_id, name_key_id2);
    
    /* Test ensure property key (retrieves existing) */
    int name_key_id3 = cypher_schema_ensure_property_key(schema_mgr, "name");
    CU_ASSERT_EQUAL(name_key_id, name_key_id3);
    
    /* Test non-existent key */
    int missing_key_id = cypher_schema_get_property_key_id(schema_mgr, "nonexistent");
    CU_ASSERT_EQUAL(missing_key_id, -1);
    
    /* Test cache statistics */
    long hits, misses, insertions;
    property_key_cache_stats(schema_mgr->key_cache, &hits, &misses, &insertions);
    
    CU_ASSERT_TRUE(hits > 0);   /* Should have cache hits from repeated lookups */
    CU_ASSERT_TRUE(misses > 0); /* Should have cache misses from initial lookups */
    CU_ASSERT_EQUAL(insertions, 2); /* Should have inserted "name" and "age" */
    
    /* Test key name lookup */
    const char *name_key_name = cypher_schema_get_property_key_name(schema_mgr, name_key_id);
    if (name_key_name) {
        CU_ASSERT_STRING_EQUAL(name_key_name, "name");
    }
}

/* Test node property operations */
static void test_node_property_operations(void)
{
    /* Initialize schema first */
    int result = cypher_schema_initialize(schema_mgr);
    CU_ASSERT_EQUAL(result, 0);
    
    /* Create a test node */
    int node_id = cypher_schema_create_node(schema_mgr);
    CU_ASSERT_TRUE(node_id > 0);
    
    /* Test setting different property types */
    int int_value = 42;
    result = cypher_schema_set_node_property(schema_mgr, node_id, "age", PROP_TYPE_INTEGER, &int_value);
    CU_ASSERT_EQUAL(result, 0);
    
    const char *text_value = "John Doe";
    result = cypher_schema_set_node_property(schema_mgr, node_id, "name", PROP_TYPE_TEXT, text_value);
    CU_ASSERT_EQUAL(result, 0);
    
    double real_value = 3.14159;
    result = cypher_schema_set_node_property(schema_mgr, node_id, "pi", PROP_TYPE_REAL, &real_value);
    CU_ASSERT_EQUAL(result, 0);
    
    int bool_value = 1;
    result = cypher_schema_set_node_property(schema_mgr, node_id, "active", PROP_TYPE_BOOLEAN, &bool_value);
    CU_ASSERT_EQUAL(result, 0);
    
    /* Test updating existing property */
    int new_age = 43;
    result = cypher_schema_set_node_property(schema_mgr, node_id, "age", PROP_TYPE_INTEGER, &new_age);
    CU_ASSERT_EQUAL(result, 0);
    
    /* Verify properties were stored in correct tables by direct DB query */
    const char *check_sql = "SELECT COUNT(*) FROM node_props_int WHERE node_id = ? AND key_id = (SELECT id FROM property_keys WHERE key = 'age')";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(test_db, check_sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, node_id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int count = sqlite3_column_int(stmt, 0);
        CU_ASSERT_EQUAL(count, 1); /* Should have exactly one age property */
    }
    sqlite3_finalize(stmt);
}

/* Test database integrity constraints */
static void test_database_integrity(void)
{
    /* Test that we can insert into nodes table */
    const char *sql = "INSERT INTO nodes DEFAULT VALUES";
    char *error;
    int rc = sqlite3_exec(test_db, sql, NULL, NULL, &error);
    
    CU_ASSERT_EQUAL(rc, SQLITE_OK);
    if (rc != SQLITE_OK) {
        printf("Insert error: %s\n", error);
        sqlite3_free(error);
    }
    
    /* Test that foreign key constraints work */
    const char *bad_sql = "INSERT INTO node_labels (node_id, label) VALUES (999, 'NonExistent')";
    rc = sqlite3_exec(test_db, bad_sql, NULL, NULL, &error);
    
    /* Should fail due to foreign key constraint */
    CU_ASSERT_NOT_EQUAL(rc, SQLITE_OK);
    if (error) {
        sqlite3_free(error);
    }
}

/* Initialize the schema test suite */
int init_schema_suite(void)
{
    CU_pSuite suite = CU_add_suite("Schema", setup_schema_suite, teardown_schema_suite);
    if (!suite) {
        return CU_get_error();
    }
    
    /* Add tests */
    if (!CU_add_test(suite, "Schema manager creation", test_schema_manager_creation) ||
        !CU_add_test(suite, "Schema initialization", test_schema_initialization) ||
        !CU_add_test(suite, "Table creation", test_table_creation) ||
        !CU_add_test(suite, "Index creation", test_index_creation) ||
        !CU_add_test(suite, "Property type inference", test_property_type_inference) ||
        !CU_add_test(suite, "Property type names", test_property_type_names) ||
        !CU_add_test(suite, "Basic node operations", test_basic_node_operations) ||
        !CU_add_test(suite, "Property key cache creation", test_property_key_cache_creation) ||
        !CU_add_test(suite, "Property key operations", test_property_key_operations) ||
        !CU_add_test(suite, "Node property operations", test_node_property_operations) ||
        !CU_add_test(suite, "Database integrity", test_database_integrity)) {
        return CU_get_error();
    }
    
    return CUE_SUCCESS;
}