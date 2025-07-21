#include "graphqlite_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

void test_database_lifecycle() {
    printf("Testing database lifecycle...\n");
    
    // Test database opening
    graphqlite_db_t *db = graphqlite_open(":memory:");
    assert(db != NULL);
    
    // Test database closing
    int rc = graphqlite_close(db);
    assert(rc == SQLITE_OK);
    
    printf("✓ Database lifecycle test passed\n");
}

void test_node_operations() {
    printf("Testing node operations...\n");
    
    graphqlite_db_t *db = graphqlite_open(":memory:");
    assert(db != NULL);
    
    // Test node creation
    int64_t node_id = graphqlite_create_node(db);
    assert(node_id > 0);
    
    // Test node existence
    bool exists = graphqlite_node_exists(db, node_id);
    assert(exists == true);
    
    // Test node deletion
    int rc = graphqlite_delete_node(db, node_id);
    assert(rc == SQLITE_OK);
    
    // Verify node no longer exists
    exists = graphqlite_node_exists(db, node_id);
    assert(exists == false);
    
    graphqlite_close(db);
    printf("✓ Node operations test passed\n");
}

void test_edge_operations() {
    printf("Testing edge operations...\n");
    
    graphqlite_db_t *db = graphqlite_open(":memory:");
    assert(db != NULL);
    
    // Create two nodes
    int64_t node1 = graphqlite_create_node(db);
    int64_t node2 = graphqlite_create_node(db);
    assert(node1 > 0 && node2 > 0);
    
    // Create edge
    int64_t edge_id = graphqlite_create_edge(db, node1, node2, "CONNECTS");
    assert(edge_id > 0);
    
    // Test edge existence
    bool exists = graphqlite_edge_exists(db, edge_id);
    assert(exists == true);
    
    // Test edge deletion
    int rc = graphqlite_delete_edge(db, edge_id);
    assert(rc == SQLITE_OK);
    
    // Verify edge no longer exists
    exists = graphqlite_edge_exists(db, edge_id);
    assert(exists == false);
    
    graphqlite_close(db);
    printf("✓ Edge operations test passed\n");
}

void test_property_operations() {
    printf("Testing property operations...\n");
    
    graphqlite_db_t *db = graphqlite_open(":memory:");
    assert(db != NULL);
    
    // Create a node
    int64_t node_id = graphqlite_create_node(db);
    assert(node_id > 0);
    
    // Test integer property
    property_value_t int_value = {.type = PROP_INT, .value = {.int_val = 42}};
    int rc = graphqlite_set_property(db, ENTITY_NODE, node_id, "age", &int_value);
    assert(rc == SQLITE_OK);
    
    // Retrieve integer property
    property_value_t retrieved_value;
    rc = graphqlite_get_property(db, ENTITY_NODE, node_id, "age", &retrieved_value);
    assert(rc == SQLITE_OK);
    assert(retrieved_value.type == PROP_INT);
    assert(retrieved_value.value.int_val == 42);
    
    // Test text property
    property_value_t text_value = {.type = PROP_TEXT, .value = {.text_val = "John Doe"}};
    rc = graphqlite_set_property(db, ENTITY_NODE, node_id, "name", &text_value);
    assert(rc == SQLITE_OK);
    
    // Retrieve text property
    rc = graphqlite_get_property(db, ENTITY_NODE, node_id, "name", &retrieved_value);
    assert(rc == SQLITE_OK);
    assert(retrieved_value.type == PROP_TEXT);
    assert(strcmp(retrieved_value.value.text_val, "John Doe") == 0);
    
    // Clean up text value
    free_property_value(&retrieved_value);
    
    graphqlite_close(db);
    printf("✓ Property operations test passed\n");
}

void test_property_key_interning() {
    printf("Testing property key interning...\n");
    
    graphqlite_db_t *db = graphqlite_open(":memory:");
    assert(db != NULL);
    
    // Test key interning
    int key_id1 = intern_property_key(db->key_cache, "test_key");
    int key_id2 = intern_property_key(db->key_cache, "test_key");
    
    // Should return same ID for same key
    assert(key_id1 == key_id2);
    assert(key_id1 > 0);
    
    // Test different key
    int key_id3 = intern_property_key(db->key_cache, "different_key");
    assert(key_id3 != key_id1);
    assert(key_id3 > 0);
    
    graphqlite_close(db);
    printf("✓ Property key interning test passed\n");
}

void test_transaction_management() {
    printf("Testing transaction management...\n");
    
    graphqlite_db_t *db = graphqlite_open(":memory:");
    assert(db != NULL);
    
    // Test transaction state
    assert(graphqlite_in_transaction(db) == false);
    
    // Begin transaction
    int rc = graphqlite_begin_transaction(db);
    assert(rc == SQLITE_OK);
    assert(graphqlite_in_transaction(db) == true);
    
    // Create node within transaction
    int64_t node_id = graphqlite_create_node(db);
    assert(node_id > 0);
    
    // Rollback transaction
    rc = graphqlite_rollback_transaction(db);
    assert(rc == SQLITE_OK);
    assert(graphqlite_in_transaction(db) == false);
    
    // Node should not exist after rollback
    bool exists = graphqlite_node_exists(db, node_id);
    assert(exists == false);
    
    graphqlite_close(db);
    printf("✓ Transaction management test passed\n");
}

void test_mode_switching() {
    printf("Testing mode switching...\n");
    
    graphqlite_db_t *db = graphqlite_open(":memory:");
    assert(db != NULL);
    
    // Should start in interactive mode
    assert(graphqlite_get_current_mode(db) == GRAPHQLITE_MODE_INTERACTIVE);
    
    // Switch to bulk import mode
    int rc = graphqlite_switch_to_bulk_import_mode(db);
    assert(rc == SQLITE_OK);
    assert(graphqlite_get_current_mode(db) == GRAPHQLITE_MODE_BULK_IMPORT);
    
    // Switch back to interactive mode
    rc = graphqlite_switch_to_interactive_mode(db);
    assert(rc == SQLITE_OK);
    assert(graphqlite_get_current_mode(db) == GRAPHQLITE_MODE_INTERACTIVE);
    
    graphqlite_close(db);
    printf("✓ Mode switching test passed\n");
}

void test_batch_operations() {
    printf("Testing batch operations...\n");
    
    graphqlite_db_t *db = graphqlite_open(":memory:");
    assert(db != NULL);
    
    // Test batch node creation
    size_t node_count = 100;
    int64_t *node_ids = malloc(node_count * sizeof(int64_t));
    
    int rc = graphqlite_create_nodes_batch(db, node_count, node_ids);
    assert(rc == SQLITE_OK);
    
    // Verify all nodes exist
    for (size_t i = 0; i < node_count; i++) {
        assert(node_ids[i] > 0);
        assert(graphqlite_node_exists(db, node_ids[i]) == true);
    }
    
    free(node_ids);
    graphqlite_close(db);
    printf("✓ Batch operations test passed\n");
}

int main() {
    printf("Running GraphQLite Core Tests\n");
    printf("=============================\n\n");
    
    test_database_lifecycle();
    test_node_operations();
    test_edge_operations();
    test_property_operations();
    test_property_key_interning();
    test_transaction_management();
    test_mode_switching();
    test_batch_operations();
    
    printf("\n✓ All tests passed!\n");
    return 0;
}