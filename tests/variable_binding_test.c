#include "../src/gql/gql_executor.h"
#include "../src/gql/gql_parser.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== Variable Binding Test ===\n");
    
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
    
    // Charlie: name="Charlie", age=35
    prop.type = PROP_TEXT;
    prop.value.text_val = "Charlie";
    graphqlite_set_property(db, ENTITY_NODE, charlie_id, "name", &prop);
    prop.type = PROP_INT;
    prop.value.int_val = 35;
    graphqlite_set_property(db, ENTITY_NODE, charlie_id, "age", &prop);
    
    // Create edges
    int64_t edge1 = graphqlite_create_edge(db, alice_id, bob_id, "KNOWS");
    int64_t edge2 = graphqlite_create_edge(db, alice_id, charlie_id, "WORKS_WITH");
    int64_t edge3 = graphqlite_create_edge(db, bob_id, charlie_id, "KNOWS");
    
    printf("Created 3 nodes and 3 edges\n");
    
    // Test 1: Simple node variable binding
    printf("\nTest 1: Simple node variable binding (p)...\n");
    const char *query1 = "MATCH (p:Person) WHERE p.name = \"Alice\" RETURN p";
    gql_result_t *result1 = gql_execute_query(query1, db);
    
    if (result1 && result1->status == 0 && result1->row_count == 1) {
        printf("PASS: Found Alice using variable 'p'\n");
    } else {
        printf("FAIL: Variable binding for node failed\n");
    }
    if (result1) gql_result_destroy(result1);
    
    // Test 2: Edge pattern with all variables bound
    printf("\nTest 2: Edge pattern variables (a, r, b)...\n");
    const char *query2 = "MATCH (a:Person)-[r:KNOWS]->(b:Person) RETURN a, r, b";
    gql_result_t *result2 = gql_execute_query(query2, db);
    
    if (result2 && result2->status == 0 && result2->row_count >= 1) {
        printf("PASS: Edge pattern with variables worked (found %zu matches)\n", result2->row_count);
    } else {
        printf("FAIL: Edge pattern variable binding failed\n");
    }
    if (result2) gql_result_destroy(result2);
    
    // Test 3: Variable reuse in WHERE clause
    printf("\nTest 3: Variable reuse (a in pattern and WHERE)...\n");
    const char *query3 = "MATCH (a:Person)-[r]->(b:Person) WHERE a.age > 25 RETURN a, b";
    gql_result_t *result3 = gql_execute_query(query3, db);
    
    if (result3 && result3->status == 0) {
        printf("PASS: Variable reuse in WHERE clause worked (found %zu matches)\n", result3->row_count);
    } else {
        printf("FAIL: Variable reuse in WHERE clause failed\n");
    }
    if (result3) gql_result_destroy(result3);
    
    // Test 4: Multiple node variables with property access
    printf("\nTest 4: Multiple variables with property access...\n");
    const char *query4 = "MATCH (older:Person)-[r]->(younger:Person) WHERE older.age > younger.age RETURN older, younger";
    gql_result_t *result4 = gql_execute_query(query4, db);
    
    if (result4 && result4->status == 0) {
        printf("PASS: Multiple variable property access worked (found %zu matches)\n", result4->row_count);
    } else {
        printf("FAIL: Multiple variable property access failed\n");
    }
    if (result4) gql_result_destroy(result4);
    
    // Test 5: Edge variable in WHERE clause
    printf("\nTest 5: Edge variable in WHERE clause...\n");
    const char *query5 = "MATCH (a:Person)-[r]->(b:Person) WHERE r IS NOT NULL RETURN a, r, b";
    gql_result_t *result5 = gql_execute_query(query5, db);
    
    if (result5 && result5->status == 0) {
        printf("PASS: Edge variable in WHERE clause worked (found %zu matches)\n", result5->row_count);
    } else {
        printf("FAIL: Edge variable in WHERE clause failed\n");
    }
    if (result5) gql_result_destroy(result5);
    
    // Test 6: Variable scoping - same variable name in different patterns
    printf("\nTest 6: Variable scoping with reused names...\n");
    const char *query6 = "MATCH (p:Person) WHERE p.name = \"Alice\" OR p.name = \"Bob\" RETURN p";
    gql_result_t *result6 = gql_execute_query(query6, db);
    
    if (result6 && result6->status == 0 && result6->row_count == 2) {
        printf("PASS: Variable scoping worked correctly (found %zu matches)\n", result6->row_count);
    } else {
        printf("FAIL: Variable scoping failed (expected 2, got %zu)\n", 
               result6 ? result6->row_count : 0);
    }
    if (result6) gql_result_destroy(result6);
    
    graphqlite_close(db);
    printf("\n=== Variable Binding Test Complete ===\n");
    return 0;
}