#include "../src/gql/gql_executor.h"
#include "../src/gql/gql_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("=== Alias Support Test ===\n");
    
    graphqlite_db_t *db = graphqlite_open(":memory:");
    if (!db) {
        printf("FAIL: Failed to open database\n");
        return 1;
    }
    
    // Create test data
    printf("Creating test data...\n");
    
    // Create nodes
    int64_t alice_id = graphqlite_create_node(db);
    int64_t bob_id = graphqlite_create_node(db);
    
    // Add labels
    graphqlite_add_node_label(db, alice_id, "Person");
    graphqlite_add_node_label(db, bob_id, "Person");
    
    // Add properties
    property_value_t prop;
    
    // Alice: name="Alice", age=30
    prop.type = PROP_TEXT;
    prop.value.text_val = "Alice";
    graphqlite_set_property(db, ENTITY_NODE, alice_id, "name", &prop);
    prop.type = PROP_INT;
    prop.value.int_val = 30;
    graphqlite_set_property(db, ENTITY_NODE, alice_id, "age", &prop);
    
    // Bob: name="Bob", age=25
    prop.type = PROP_TEXT;
    prop.value.text_val = "Bob";
    graphqlite_set_property(db, ENTITY_NODE, bob_id, "name", &prop);
    prop.type = PROP_INT;
    prop.value.int_val = 25;
    graphqlite_set_property(db, ENTITY_NODE, bob_id, "age", &prop);
    
    // Create edge
    int64_t edge = graphqlite_create_edge(db, alice_id, bob_id, "KNOWS");
    
    printf("Created 2 nodes and 1 edge\n");
    
    // Test 1: Simple property alias
    printf("\nTest 1: Property alias (name AS person_name)...\n");
    const char *query1 = "MATCH (p:Person) WHERE p.name = \"Alice\" RETURN p.name AS person_name, p.age AS person_age";
    gql_result_t *result1 = gql_execute_query(query1, db);
    
    if (result1 && result1->status == 0 && result1->row_count == 1) {
        printf("PASS: Property alias worked\n");
        printf("      Columns: ");
        for (size_t i = 0; i < result1->column_count; i++) {
            printf("'%s' ", result1->column_names[i]);
        }
        printf("\n");
        
        // Check if aliases are correct
        if (result1->column_count >= 2 &&
            strcmp(result1->column_names[0], "person_name") == 0 &&
            strcmp(result1->column_names[1], "person_age") == 0) {
            printf("      ✓ Column aliases are correct\n");
        } else {
            printf("      ✗ Column aliases are incorrect\n");
        }
    } else {
        printf("FAIL: Property alias failed\n");
        if (result1 && result1->error_message) {
            printf("      Error: %s\n", result1->error_message);
        }
    }
    if (result1) gql_result_destroy(result1);
    
    // Test 2: Node alias
    printf("\nTest 2: Node alias (p AS person)...\n");
    const char *query2 = "MATCH (p:Person) WHERE p.age > 25 RETURN p AS person, p.name AS full_name";
    gql_result_t *result2 = gql_execute_query(query2, db);
    
    if (result2 && result2->status == 0) {
        printf("PASS: Node alias worked (found %zu results)\n", result2->row_count);
        printf("      Columns: ");
        for (size_t i = 0; i < result2->column_count; i++) {
            printf("'%s' ", result2->column_names[i]);
        }
        printf("\n");
        
        // Check if aliases are correct
        if (result2->column_count >= 2 &&
            strcmp(result2->column_names[0], "person") == 0 &&
            strcmp(result2->column_names[1], "full_name") == 0) {
            printf("      ✓ Column aliases are correct\n");
        } else {
            printf("      ✗ Column aliases are incorrect\n");
        }
    } else {
        printf("FAIL: Node alias failed\n");
        if (result2 && result2->error_message) {
            printf("      Error: %s\n", result2->error_message);
        }
    }
    if (result2) gql_result_destroy(result2);
    
    // Test 3: Mixed aliases and regular columns
    printf("\nTest 3: Mixed aliases and regular columns...\n");
    const char *query3 = "MATCH (a:Person)-[r]->(b:Person) RETURN a.name AS source, r, b.name AS target";
    gql_result_t *result3 = gql_execute_query(query3, db);
    
    if (result3 && result3->status == 0) {
        printf("PASS: Mixed aliases worked (found %zu relationships)\n", result3->row_count);
        printf("      Columns: ");
        for (size_t i = 0; i < result3->column_count; i++) {
            printf("'%s' ", result3->column_names[i]);
        }
        printf("\n");
        
        // Check if mixed aliases work
        if (result3->column_count >= 3 &&
            strcmp(result3->column_names[0], "source") == 0 &&
            strcmp(result3->column_names[1], "r") == 0 &&
            strcmp(result3->column_names[2], "target") == 0) {
            printf("      ✓ Mixed aliases are correct\n");
        } else {
            printf("      ✗ Mixed aliases are incorrect\n");
        }
    } else {
        printf("FAIL: Mixed aliases failed\n");
        if (result3 && result3->error_message) {
            printf("      Error: %s\n", result3->error_message);
        }
    }
    if (result3) gql_result_destroy(result3);
    
    // Test 4: No aliases (legacy support)
    printf("\nTest 4: No aliases (legacy behavior)...\n");
    const char *query4 = "MATCH (p:Person) RETURN p.name, p.age";
    gql_result_t *result4 = gql_execute_query(query4, db);
    
    if (result4 && result4->status == 0) {
        printf("PASS: Legacy support worked (found %zu results)\n", result4->row_count);
        printf("      Columns: ");
        for (size_t i = 0; i < result4->column_count; i++) {
            printf("'%s' ", result4->column_names[i]);
        }
        printf("\n");
        
        // Check if legacy column names work
        if (result4->column_count >= 2 &&
            strcmp(result4->column_names[0], "p.name") == 0 &&
            strcmp(result4->column_names[1], "p.age") == 0) {
            printf("      ✓ Legacy column names are correct\n");
        } else {
            printf("      ✗ Legacy column names are incorrect\n");
        }
    } else {
        printf("FAIL: Legacy support failed\n");
        if (result4 && result4->error_message) {
            printf("      Error: %s\n", result4->error_message);
        }
    }
    if (result4) gql_result_destroy(result4);
    
    graphqlite_close(db);
    printf("\n=== Alias Support Test Complete ===\n");
    return 0;
}