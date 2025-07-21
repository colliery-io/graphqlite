#include "../src/gql/gql_executor.h"
#include "../src/gql/gql_parser.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== Variable Chain Test ===\n");
    
    graphqlite_db_t *db = graphqlite_open(":memory:");
    if (!db) {
        printf("FAIL: Failed to open database\n");
        return 1;
    }
    
    // Create test data for a simple social network
    printf("Creating test data...\n");
    
    // Create nodes: Alice -> Bob -> Charlie -> Diana
    int64_t alice_id = graphqlite_create_node(db);
    int64_t bob_id = graphqlite_create_node(db);
    int64_t charlie_id = graphqlite_create_node(db);
    int64_t diana_id = graphqlite_create_node(db);
    
    // Add labels
    graphqlite_add_node_label(db, alice_id, "Person");
    graphqlite_add_node_label(db, bob_id, "Person");
    graphqlite_add_node_label(db, charlie_id, "Person");
    graphqlite_add_node_label(db, diana_id, "Person");
    
    // Add properties
    property_value_t prop;
    
    // Names and ages
    prop.type = PROP_TEXT;
    prop.value.text_val = "Alice";
    graphqlite_set_property(db, ENTITY_NODE, alice_id, "name", &prop);
    prop.type = PROP_INT;
    prop.value.int_val = 25;
    graphqlite_set_property(db, ENTITY_NODE, alice_id, "age", &prop);
    
    prop.type = PROP_TEXT;
    prop.value.text_val = "Bob";
    graphqlite_set_property(db, ENTITY_NODE, bob_id, "name", &prop);
    prop.type = PROP_INT;
    prop.value.int_val = 30;
    graphqlite_set_property(db, ENTITY_NODE, bob_id, "age", &prop);
    
    prop.type = PROP_TEXT;
    prop.value.text_val = "Charlie";
    graphqlite_set_property(db, ENTITY_NODE, charlie_id, "name", &prop);
    prop.type = PROP_INT;
    prop.value.int_val = 35;
    graphqlite_set_property(db, ENTITY_NODE, charlie_id, "age", &prop);
    
    prop.type = PROP_TEXT;
    prop.value.text_val = "Diana";
    graphqlite_set_property(db, ENTITY_NODE, diana_id, "name", &prop);
    prop.type = PROP_INT;
    prop.value.int_val = 28;
    graphqlite_set_property(db, ENTITY_NODE, diana_id, "age", &prop);
    
    // Create edges: Alice -> Bob -> Charlie, Alice -> Charlie, Bob -> Diana
    int64_t edge1 = graphqlite_create_edge(db, alice_id, bob_id, "KNOWS");
    int64_t edge2 = graphqlite_create_edge(db, bob_id, charlie_id, "KNOWS");
    int64_t edge3 = graphqlite_create_edge(db, alice_id, charlie_id, "KNOWS");
    int64_t edge4 = graphqlite_create_edge(db, bob_id, diana_id, "KNOWS");
    
    printf("Created 4 nodes and 4 edges\n");
    
    // Test 1: Variable consistency in simple pattern
    printf("\nTest 1: Variable consistency (same node bound twice)...\n");
    const char *query1 = "MATCH (p:Person) WHERE p.age > 20 AND p.age < 40 RETURN p.name, p.age";
    gql_result_t *result1 = gql_execute_query(query1, db);
    
    if (result1 && result1->status == 0 && result1->row_count == 4) {
        printf("PASS: Variable consistency worked (found %zu people)\n", result1->row_count);
    } else {
        printf("FAIL: Variable consistency failed (expected 4, got %zu)\n", 
               result1 ? result1->row_count : 0);
        if (result1 && result1->error_message) {
            printf("      Error: %s\n", result1->error_message);
        }
    }
    if (result1) gql_result_destroy(result1);
    
    // Test 2: Chained variable comparisons
    printf("\nTest 2: Chained comparisons (a.age < b.age)...\n");
    const char *query2 = "MATCH (a:Person)-[r:KNOWS]->(b:Person) WHERE a.age < b.age RETURN a.name, b.name";
    gql_result_t *result2 = gql_execute_query(query2, db);
    
    if (result2 && result2->status == 0) {
        printf("PASS: Chained comparisons worked (found %zu age-ascending pairs)\n", result2->row_count);
    } else {
        printf("FAIL: Chained comparisons failed\n");
        if (result2 && result2->error_message) {
            printf("      Error: %s\n", result2->error_message);
        }
    }
    if (result2) gql_result_destroy(result2);
    
    // Test 3: Multiple variable types in single query
    printf("\nTest 3: Multiple variable types (node, edge, node)...\n");
    const char *query3 = "MATCH (start:Person)-[rel:KNOWS]->(end:Person) RETURN start.name, end.name";
    gql_result_t *result3 = gql_execute_query(query3, db);
    
    if (result3 && result3->status == 0 && result3->row_count == 4) {
        printf("PASS: Multiple variable types worked (found %zu relationships)\n", result3->row_count);
    } else {
        printf("FAIL: Multiple variable types failed (expected 4, got %zu)\n", 
               result3 ? result3->row_count : 0);
        if (result3 && result3->error_message) {
            printf("      Error: %s\n", result3->error_message);
        }
    }
    if (result3) gql_result_destroy(result3);
    
    // Test 4: Complex WHERE with multiple variable references
    printf("\nTest 4: Complex WHERE with multiple variables...\n");
    const char *query4 = "MATCH (young:Person)-[r]->(old:Person) WHERE young.age < 30 AND old.age > 30 RETURN young.name, old.name";
    gql_result_t *result4 = gql_execute_query(query4, db);
    
    if (result4 && result4->status == 0) {
        printf("PASS: Complex WHERE worked (found %zu young->old pairs)\n", result4->row_count);
    } else {
        printf("FAIL: Complex WHERE failed\n");
        if (result4 && result4->error_message) {
            printf("      Error: %s\n", result4->error_message);
        }
    }
    if (result4) gql_result_destroy(result4);
    
    // Test 5: Variable name collision handling
    printf("\nTest 5: Variable uniqueness within pattern...\n");
    const char *query5 = "MATCH (alice:Person) WHERE alice.name = \"Alice\" RETURN alice.name, alice.age";
    gql_result_t *result5 = gql_execute_query(query5, db);
    
    if (result5 && result5->status == 0 && result5->row_count == 1) {
        printf("PASS: Variable uniqueness worked (found Alice)\n");
    } else {
        printf("FAIL: Variable uniqueness failed (expected 1, got %zu)\n", 
               result5 ? result5->row_count : 0);
        if (result5 && result5->error_message) {
            printf("      Error: %s\n", result5->error_message);
        }
    }
    if (result5) gql_result_destroy(result5);
    
    graphqlite_close(db);
    printf("\n=== Variable Chain Test Complete ===\n");
    return 0;
}