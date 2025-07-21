#include "../src/gql/gql_executor.h"
#include "../src/gql/gql_parser.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== RETURN Clause Projection Test ===\n");
    
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
    int64_t charlie_id = graphqlite_create_node(db);
    
    // Add labels
    graphqlite_add_node_label(db, alice_id, "Person");
    graphqlite_add_node_label(db, bob_id, "Person");
    graphqlite_add_node_label(db, charlie_id, "Person");
    
    // Add properties
    property_value_t prop;
    
    // Alice: name="Alice", age=30, salary=75000
    prop.type = PROP_TEXT;
    prop.value.text_val = "Alice";
    graphqlite_set_property(db, ENTITY_NODE, alice_id, "name", &prop);
    prop.type = PROP_INT;
    prop.value.int_val = 30;
    graphqlite_set_property(db, ENTITY_NODE, alice_id, "age", &prop);
    prop.type = PROP_INT;
    prop.value.int_val = 75000;
    graphqlite_set_property(db, ENTITY_NODE, alice_id, "salary", &prop);
    
    // Bob: name="Bob", age=25, salary=60000
    prop.type = PROP_TEXT;
    prop.value.text_val = "Bob";
    graphqlite_set_property(db, ENTITY_NODE, bob_id, "name", &prop);
    prop.type = PROP_INT;
    prop.value.int_val = 25;
    graphqlite_set_property(db, ENTITY_NODE, bob_id, "age", &prop);
    prop.type = PROP_INT;
    prop.value.int_val = 60000;
    graphqlite_set_property(db, ENTITY_NODE, bob_id, "salary", &prop);
    
    // Charlie: name="Charlie", age=35, salary=90000
    prop.type = PROP_TEXT;
    prop.value.text_val = "Charlie";
    graphqlite_set_property(db, ENTITY_NODE, charlie_id, "name", &prop);
    prop.type = PROP_INT;
    prop.value.int_val = 35;
    graphqlite_set_property(db, ENTITY_NODE, charlie_id, "age", &prop);
    prop.type = PROP_INT;
    prop.value.int_val = 90000;
    graphqlite_set_property(db, ENTITY_NODE, charlie_id, "salary", &prop);
    
    // Create edges
    int64_t edge1 = graphqlite_create_edge(db, alice_id, bob_id, "KNOWS");
    int64_t edge2 = graphqlite_create_edge(db, alice_id, charlie_id, "WORKS_WITH");
    
    printf("Created 3 nodes and 2 edges\n");
    
    // Test 1: Basic property projection
    printf("\nTest 1: Basic property projection (name, age)...\n");
    const char *query1 = "MATCH (p:Person) RETURN p.name, p.age";
    gql_result_t *result1 = gql_execute_query(query1, db);
    
    if (result1 && result1->status == 0 && result1->row_count == 3) {
        printf("PASS: Basic property projection worked (found %zu people)\n", result1->row_count);
        printf("      Columns: ");
        for (size_t i = 0; i < result1->column_count; i++) {
            printf("'%s' ", result1->column_names[i]);
        }
        printf("\n");
    } else {
        printf("FAIL: Basic property projection failed\n");
        if (result1 && result1->error_message) {
            printf("      Error: %s\n", result1->error_message);
        }
    }
    if (result1) gql_result_destroy(result1);
    
    // Test 2: Mixed variable and property projection
    printf("\nTest 2: Mixed projection (node + properties)...\n");
    const char *query2 = "MATCH (p:Person) WHERE p.age > 25 RETURN p, p.name, p.salary";
    gql_result_t *result2 = gql_execute_query(query2, db);
    
    if (result2 && result2->status == 0) {
        printf("PASS: Mixed projection worked (found %zu results)\n", result2->row_count);
        printf("      Columns: ");
        for (size_t i = 0; i < result2->column_count; i++) {
            printf("'%s' ", result2->column_names[i]);
        }
        printf("\n");
    } else {
        printf("FAIL: Mixed projection failed\n");
        if (result2 && result2->error_message) {
            printf("      Error: %s\n", result2->error_message);
        }
    }
    if (result2) gql_result_destroy(result2);
    
    // Test 3: Edge relationship projection
    printf("\nTest 3: Relationship projection (a, r, b)...\n");
    const char *query3 = "MATCH (a:Person)-[r]->(b:Person) RETURN a.name, r, b.name";
    gql_result_t *result3 = gql_execute_query(query3, db);
    
    if (result3 && result3->status == 0) {
        printf("PASS: Relationship projection worked (found %zu relationships)\n", result3->row_count);
        printf("      Columns: ");
        for (size_t i = 0; i < result3->column_count; i++) {
            printf("'%s' ", result3->column_names[i]);
        }
        printf("\n");
    } else {
        printf("FAIL: Relationship projection failed\n");
        if (result3 && result3->error_message) {
            printf("      Error: %s\n", result3->error_message);
        }
    }
    if (result3) gql_result_destroy(result3);
    
    // Test 4: DISTINCT projection (if implemented)
    printf("\nTest 4: Column naming consistency...\n");
    const char *query4 = "MATCH (older:Person)-[r]->(younger:Person) WHERE older.age > younger.age RETURN older.name, younger.name";
    gql_result_t *result4 = gql_execute_query(query4, db);
    
    if (result4 && result4->status == 0) {
        printf("PASS: Column naming worked (found %zu results)\n", result4->row_count);
        printf("      Columns: ");
        for (size_t i = 0; i < result4->column_count; i++) {
            printf("'%s' ", result4->column_names[i]);
        }
        printf("\n");
    } else {
        printf("FAIL: Column naming failed\n");
        if (result4 && result4->error_message) {
            printf("      Error: %s\n", result4->error_message);
        }
    }
    if (result4) gql_result_destroy(result4);
    
    // Test 5: Single column projection
    printf("\nTest 5: Single column projection...\n");
    const char *query5 = "MATCH (p:Person) WHERE p.name = \"Alice\" RETURN p.salary";
    gql_result_t *result5 = gql_execute_query(query5, db);
    
    if (result5 && result5->status == 0 && result5->row_count == 1) {
        printf("PASS: Single column projection worked\n");
        printf("      Column: '%s'\n", result5->column_names[0]);
    } else {
        printf("FAIL: Single column projection failed\n");
        if (result5 && result5->error_message) {
            printf("      Error: %s\n", result5->error_message);
        }
    }
    if (result5) gql_result_destroy(result5);
    
    graphqlite_close(db);
    printf("\n=== RETURN Projection Test Complete ===\n");
    return 0;
}