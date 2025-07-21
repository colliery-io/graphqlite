---
id: storage-layer-tests
level: task
title: "Storage layer tests"
created_at: 2025-07-19T23:03:35.472229+00:00
updated_at: 2025-07-19T23:03:35.472229+00:00
parent: foundation-layer
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
---

# Storage layer tests

## Parent Initiative

[[foundation-layer]]

## Objective

Develop comprehensive test suites specifically for the storage layer components including schema validation, data integrity, performance benchmarks, and edge cases. These tests ensure the typed EAV storage system, property key interning, transaction management, and mode switching all function correctly under various conditions.

Storage layer tests are critical for validating the foundation upon which all higher-level graph operations depend, ensuring data correctness and performance consistency.

## Acceptance Criteria

- [ ] **Schema Validation**: Complete test coverage for database schema creation and migration
- [ ] **CRUD Operations**: Comprehensive testing of all node, edge, and property operations
- [ ] **Data Integrity**: Tests for referential integrity, constraints, and consistency
- [ ] **Performance Validation**: Automated tests ensuring all performance targets are met
- [ ] **Concurrency Testing**: Multi-threaded test scenarios validating thread safety
- [ ] **Error Handling**: Tests for error conditions, recovery, and rollback scenarios
- [ ] **Property Type System**: Validation of type inference, conversion, and storage
- [ ] **Storage Efficiency**: Tests confirming space optimization and key interning benefits

## Implementation Notes

**Storage Layer Test Organization:**

```c
// Test suite structure for storage layer
REGISTER_TEST(schema_management, create_schema);
REGISTER_TEST(schema_management, migration_v1_to_v2);
REGISTER_TEST(schema_management, schema_validation);

REGISTER_TEST(typed_eav_storage, node_property_types);
REGISTER_TEST(typed_eav_storage, edge_property_types);
REGISTER_TEST(typed_eav_storage, property_type_inference);
REGISTER_TEST(typed_eav_storage, property_type_conversion);

REGISTER_TEST(property_key_interning, key_caching);
REGISTER_TEST(property_key_interning, memory_efficiency);
REGISTER_TEST(property_key_interning, concurrent_access);

REGISTER_TEST(crud_operations, node_lifecycle);
REGISTER_TEST(crud_operations, edge_lifecycle);
REGISTER_TEST(crud_operations, property_lifecycle);
REGISTER_TEST(crud_operations, batch_operations);

REGISTER_TEST(transaction_management, acid_compliance);
REGISTER_TEST(transaction_management, nested_transactions);
REGISTER_TEST(transaction_management, rollback_scenarios);

REGISTER_TEST(mode_switching, interactive_to_bulk);
REGISTER_TEST(mode_switching, bulk_to_interactive);
REGISTER_TEST(mode_switching, mode_isolation);

REGISTER_TEST(performance_validation, single_operation_latency);
REGISTER_TEST(performance_validation, bulk_throughput);
REGISTER_TEST(performance_validation, memory_usage);

REGISTER_TEST(concurrency, multi_threaded_operations);
REGISTER_TEST(concurrency, connection_pooling);
REGISTER_TEST(concurrency, lock_contention);

REGISTER_TEST(error_handling, invalid_operations);
REGISTER_TEST(error_handling, resource_exhaustion);
REGISTER_TEST(error_handling, corruption_recovery);
```

**Schema Management Tests:**

```c
REGISTER_TEST(schema_management, create_schema) {
    SETUP_TEST_DB();
    
    // Verify all tables exist
    const char *expected_tables[] = {
        "nodes", "edges", "property_keys",
        "node_props_int", "node_props_text", "node_props_real", "node_props_bool",
        "edge_props_int", "edge_props_text", "edge_props_real", "edge_props_bool",
        "node_labels", NULL
    };
    
    for (int i = 0; expected_tables[i]; i++) {
        sqlite3_stmt *stmt;
        char sql[256];
        snprintf(sql, sizeof(sql), 
                "SELECT name FROM sqlite_master WHERE type='table' AND name='%s'",
                expected_tables[i]);
        
        int rc = sqlite3_prepare_v2(db->sqlite_db, sql, -1, &stmt, NULL);
        ASSERT_EQ(SQLITE_OK, rc);
        
        rc = sqlite3_step(stmt);
        ASSERT_EQ(SQLITE_ROW, rc);  // Table should exist
        
        sqlite3_finalize(stmt);
    }
    
    // Verify indexes exist
    const char *expected_indexes[] = {
        "idx_edges_source", "idx_edges_target", "idx_edges_type",
        "idx_node_props_int_key_value", "idx_node_props_text_key_value",
        "idx_edge_props_int_key_value", "idx_edge_props_text_key_value",
        "idx_node_labels_label", "idx_property_keys_key", NULL
    };
    
    for (int i = 0; expected_indexes[i]; i++) {
        sqlite3_stmt *stmt;
        char sql[256];
        snprintf(sql, sizeof(sql),
                "SELECT name FROM sqlite_master WHERE type='index' AND name='%s'",
                expected_indexes[i]);
        
        int rc = sqlite3_prepare_v2(db->sqlite_db, sql, -1, &stmt, NULL);
        ASSERT_EQ(SQLITE_OK, rc);
        
        rc = sqlite3_step(stmt);
        ASSERT_EQ(SQLITE_ROW, rc);  // Index should exist
        
        sqlite3_finalize(stmt);
    }
    
    TEARDOWN_TEST_DB();
}

REGISTER_TEST(schema_management, schema_validation) {
    SETUP_TEST_DB();
    
    // Test foreign key constraints
    int64_t node1_id = graphqlite_create_node(db);
    int64_t node2_id = graphqlite_create_node(db);
    
    // Valid edge creation should succeed
    int64_t edge_id = graphqlite_create_edge(db, node1_id, node2_id, "CONNECTS");
    ASSERT_TRUE(edge_id > 0);
    
    // Invalid edge creation should fail (non-existent node)
    int64_t invalid_edge = graphqlite_create_edge(db, 99999, node2_id, "INVALID");
    ASSERT_EQ(-1, invalid_edge);
    
    TEARDOWN_TEST_DB();
}
```

**Typed EAV Storage Tests:**

```c
REGISTER_TEST(typed_eav_storage, property_type_storage) {
    SETUP_TEST_DB();
    
    int64_t node_id = graphqlite_create_node(db);
    ASSERT_TRUE(node_id > 0);
    
    // Test all property types
    property_value_t int_prop = {.type = PROP_INT, .value = {.int_val = 42}};
    property_value_t text_prop = {.type = PROP_TEXT, .value = {.text_val = "test string"}};
    property_value_t real_prop = {.type = PROP_REAL, .value = {.real_val = 3.14159}};
    property_value_t bool_prop = {.type = PROP_BOOL, .value = {.bool_val = 1}};
    
    // Set properties
    ASSERT_EQ(SQLITE_OK, 
             graphqlite_set_property(db, ENTITY_NODE, node_id, "int_prop", &int_prop));
    ASSERT_EQ(SQLITE_OK,
             graphqlite_set_property(db, ENTITY_NODE, node_id, "text_prop", &text_prop));
    ASSERT_EQ(SQLITE_OK,
             graphqlite_set_property(db, ENTITY_NODE, node_id, "real_prop", &real_prop));
    ASSERT_EQ(SQLITE_OK,
             graphqlite_set_property(db, ENTITY_NODE, node_id, "bool_prop", &bool_prop));
    
    // Verify properties are stored in correct typed tables
    sqlite3_stmt *stmt;
    
    // Check int property in int table
    int rc = sqlite3_prepare_v2(db->sqlite_db,
        "SELECT COUNT(*) FROM node_props_int WHERE node_id = ? AND value = ?",
        -1, &stmt, NULL);
    ASSERT_EQ(SQLITE_OK, rc);
    sqlite3_bind_int64(stmt, 1, node_id);
    sqlite3_bind_int64(stmt, 2, 42);
    rc = sqlite3_step(stmt);
    ASSERT_EQ(SQLITE_ROW, rc);
    ASSERT_EQ(1, sqlite3_column_int(stmt, 0));
    sqlite3_finalize(stmt);
    
    // Check text property in text table
    rc = sqlite3_prepare_v2(db->sqlite_db,
        "SELECT COUNT(*) FROM node_props_text WHERE node_id = ? AND value = ?",
        -1, &stmt, NULL);
    ASSERT_EQ(SQLITE_OK, rc);
    sqlite3_bind_int64(stmt, 1, node_id);
    sqlite3_bind_text(stmt, 2, "test string", -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    ASSERT_EQ(SQLITE_ROW, rc);
    ASSERT_EQ(1, sqlite3_column_int(stmt, 0));
    sqlite3_finalize(stmt);
    
    TEARDOWN_TEST_DB();
}

REGISTER_TEST(typed_eav_storage, property_type_inference) {
    SETUP_TEST_DB();
    
    // Test automatic type inference
    struct {
        const char *input;
        property_type_t expected_type;
    } test_cases[] = {
        {"42", PROP_INT},
        {"-123", PROP_INT},
        {"3.14159", PROP_REAL},
        {"true", PROP_BOOL},
        {"false", PROP_BOOL},
        {"hello world", PROP_TEXT},
        {"123abc", PROP_TEXT},  // Mixed should be text
        {NULL, PROP_NULL}
    };
    
    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        property_type_t inferred = infer_property_type(test_cases[i].input);
        ASSERT_EQ(test_cases[i].expected_type, inferred);
    }
    
    TEARDOWN_TEST_DB();
}
```

**Property Key Interning Tests:**

```c
REGISTER_TEST(property_key_interning, memory_efficiency) {
    SETUP_TEST_DB();
    
    const char *keys[] = {
        "name", "age", "email", "phone", "address",
        "name", "age", "email", "phone", "address"  // Duplicates
    };
    
    size_t initial_memory = get_current_memory_usage();
    
    // Intern the same keys multiple times
    for (int i = 0; i < 10; i++) {
        for (size_t j = 0; j < sizeof(keys) / sizeof(keys[0]); j++) {
            int key_id = intern_property_key(db->key_cache, keys[j]);
            ASSERT_TRUE(key_id > 0);
        }
    }
    
    size_t final_memory = get_current_memory_usage();
    
    // Memory growth should be minimal due to interning
    size_t memory_growth = final_memory - initial_memory;
    size_t expected_max_growth = 1024;  // Should be very small
    
    ASSERT_TRUE(memory_growth < expected_max_growth);
    
    // Verify only unique keys are stored
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->sqlite_db, "SELECT COUNT(*) FROM property_keys",
                               -1, &stmt, NULL);
    ASSERT_EQ(SQLITE_OK, rc);
    rc = sqlite3_step(stmt);
    ASSERT_EQ(SQLITE_ROW, rc);
    
    int unique_key_count = sqlite3_column_int(stmt, 0);
    ASSERT_EQ(5, unique_key_count);  // Only 5 unique keys should be stored
    
    sqlite3_finalize(stmt);
    
    TEARDOWN_TEST_DB();
}

REGISTER_TEST(property_key_interning, concurrent_access) {
    SETUP_TEST_DB();
    
    const int thread_count = 4;
    const int operations_per_thread = 1000;
    
    typedef struct {
        graphqlite_db_t *db;
        const char **keys;
        size_t key_count;
        int thread_id;
    } thread_data_t;
    
    const char *test_keys[] = {
        "key1", "key2", "key3", "key4", "key5"
    };
    
    thread_data_t thread_data[thread_count];
    pthread_t threads[thread_count];
    
    for (int i = 0; i < thread_count; i++) {
        thread_data[i].db = db;
        thread_data[i].keys = test_keys;
        thread_data[i].key_count = sizeof(test_keys) / sizeof(test_keys[0]);
        thread_data[i].thread_id = i;
        
        pthread_create(&threads[i], NULL, intern_keys_worker, &thread_data[i]);
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Verify data integrity after concurrent access
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->sqlite_db, "SELECT COUNT(*) FROM property_keys",
                               -1, &stmt, NULL);
    ASSERT_EQ(SQLITE_OK, rc);
    rc = sqlite3_step(stmt);
    ASSERT_EQ(SQLITE_ROW, rc);
    
    int final_key_count = sqlite3_column_int(stmt, 0);
    ASSERT_EQ(5, final_key_count);  // Still only 5 unique keys
    
    sqlite3_finalize(stmt);
    
    TEARDOWN_TEST_DB();
}

void* intern_keys_worker(void *arg) {
    thread_data_t *data = (thread_data_t*)arg;
    
    for (int i = 0; i < 1000; i++) {
        for (size_t j = 0; j < data->key_count; j++) {
            int key_id = intern_property_key(data->db->key_cache, data->keys[j]);
            assert(key_id > 0);
        }
    }
    
    return NULL;
}
```

**Performance Validation Tests:**

```c
REGISTER_TEST(performance_validation, single_operation_latency) {
    SETUP_TEST_DB();
    
    // Test node creation latency
    ASSERT_PERFORMANCE({
        int64_t node_id = graphqlite_create_node(db);
        ASSERT_TRUE(node_id > 0);
    }, 5.0);  // Must complete in under 5ms
    
    // Test property setting latency
    int64_t node_id = graphqlite_create_node(db);
    property_value_t prop = {.type = PROP_INT, .value = {.int_val = 42}};
    
    ASSERT_PERFORMANCE({
        int rc = graphqlite_set_property(db, ENTITY_NODE, node_id, "test_prop", &prop);
        ASSERT_EQ(SQLITE_OK, rc);
    }, 5.0);
    
    // Test edge creation latency
    int64_t node2_id = graphqlite_create_node(db);
    
    ASSERT_PERFORMANCE({
        int64_t edge_id = graphqlite_create_edge(db, node_id, node2_id, "CONNECTS");
        ASSERT_TRUE(edge_id > 0);
    }, 5.0);
    
    TEARDOWN_TEST_DB();
}

REGISTER_TEST(performance_validation, bulk_throughput) {
    SETUP_TEST_DB();
    
    // Switch to interactive mode for throughput test
    graphqlite_switch_to_interactive_mode(db);
    
    typedef struct {
        graphqlite_db_t *db;
    } batch_data_t;
    
    batch_data_t batch_data = {.db = db};
    
    throughput_result_t result = measure_throughput(
        [](void *data, size_t count) {
            batch_data_t *bd = (batch_data_t*)data;
            for (size_t i = 0; i < count; i++) {
                int64_t node_id = graphqlite_create_node(bd->db);
                assert(node_id > 0);
            }
        },
        &batch_data,
        10000,  // Target: 10,000 nodes/second
        2000.0  // Test for 2 seconds
    );
    
    ASSERT_TRUE(result.target_met);
    printf("    Achieved: %.0f nodes/second\n", result.operations_per_second);
    
    // Test bulk import mode throughput
    graphqlite_switch_to_bulk_import_mode(db);
    
    throughput_result_t bulk_result = measure_throughput(
        [](void *data, size_t count) {
            batch_data_t *bd = (batch_data_t*)data;
            int64_t *node_ids = malloc(count * sizeof(int64_t));
            int rc = graphqlite_bulk_create_nodes(bd->db, count, NULL, NULL, NULL, node_ids);
            assert(rc == SQLITE_OK);
            free(node_ids);
        },
        &batch_data,
        50000,  // Target: 50,000 nodes/second in bulk mode
        2000.0  // Test for 2 seconds
    );
    
    ASSERT_TRUE(bulk_result.target_met);
    printf("    Bulk mode achieved: %.0f nodes/second\n", bulk_result.operations_per_second);
    
    TEARDOWN_TEST_DB();
}
```

**Transaction and Concurrency Tests:**

```c
REGISTER_TEST(transaction_management, acid_compliance) {
    SETUP_TEST_DB();
    
    // Test atomicity
    int rc = graphqlite_begin_transaction(db);
    ASSERT_EQ(SQLITE_OK, rc);
    
    int64_t node1 = graphqlite_create_node(db);
    int64_t node2 = graphqlite_create_node(db);
    ASSERT_TRUE(node1 > 0 && node2 > 0);
    
    // Rollback should remove both nodes
    rc = graphqlite_rollback_transaction(db);
    ASSERT_EQ(SQLITE_OK, rc);
    
    ASSERT_FALSE(graphqlite_node_exists(db, node1));
    ASSERT_FALSE(graphqlite_node_exists(db, node2));
    
    // Test consistency
    rc = graphqlite_begin_transaction(db);
    ASSERT_EQ(SQLITE_OK, rc);
    
    int64_t node_a = graphqlite_create_node(db);
    int64_t node_b = graphqlite_create_node(db);
    int64_t edge_id = graphqlite_create_edge(db, node_a, node_b, "CONNECTS");
    
    rc = graphqlite_commit_transaction(db);
    ASSERT_EQ(SQLITE_OK, rc);
    
    // All entities should exist after commit
    ASSERT_TRUE(graphqlite_node_exists(db, node_a));
    ASSERT_TRUE(graphqlite_node_exists(db, node_b));
    ASSERT_TRUE(graphqlite_edge_exists(db, edge_id));
    
    TEARDOWN_TEST_DB();
}

REGISTER_TEST(concurrency, multi_threaded_operations) {
    SETUP_TEST_DB();
    
    const int thread_count = 8;
    const int operations_per_thread = 100;
    
    typedef struct {
        graphqlite_db_t *db;
        int thread_id;
        int64_t *created_nodes;
        size_t node_count;
    } thread_test_data_t;
    
    thread_test_data_t thread_data[thread_count];
    pthread_t threads[thread_count];
    
    for (int i = 0; i < thread_count; i++) {
        thread_data[i].db = db;
        thread_data[i].thread_id = i;
        thread_data[i].created_nodes = malloc(operations_per_thread * sizeof(int64_t));
        thread_data[i].node_count = operations_per_thread;
        
        pthread_create(&threads[i], NULL, concurrent_node_worker, &thread_data[i]);
    }
    
    // Wait for all threads
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Verify all nodes were created correctly
    for (int i = 0; i < thread_count; i++) {
        for (size_t j = 0; j < thread_data[i].node_count; j++) {
            ASSERT_TRUE(graphqlite_node_exists(db, thread_data[i].created_nodes[j]));
        }
        free(thread_data[i].created_nodes);
    }
    
    TEARDOWN_TEST_DB();
}

void* concurrent_node_worker(void *arg) {
    thread_test_data_t *data = (thread_test_data_t*)arg;
    
    for (size_t i = 0; i < data->node_count; i++) {
        int64_t node_id = graphqlite_create_node(data->db);
        assert(node_id > 0);
        data->created_nodes[i] = node_id;
        
        // Add some properties
        property_value_t prop = {
            .type = PROP_INT, 
            .value = {.int_val = data->thread_id * 1000 + i}
        };
        
        int rc = graphqlite_set_property(data->db, ENTITY_NODE, node_id, "thread_data", &prop);
        assert(rc == SQLITE_OK);
    }
    
    return NULL;
}
```

**Test Execution and Reporting:**
- Storage layer tests run as part of the main test suite
- Performance tests have separate targets for regression detection
- Memory tests use Valgrind integration for leak detection
- Concurrency tests validate thread safety under realistic load
- All tests use isolated database instances for reproducibility

## Status Updates

*To be added during implementation*