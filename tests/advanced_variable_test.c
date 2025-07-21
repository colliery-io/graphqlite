#include "../src/gql/gql_executor.h"
#include "../src/gql/gql_parser.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== Advanced Variable Binding Test ===\n");
    
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
    int64_t diana_id = graphqlite_create_node(db);
    
    // Add labels
    graphqlite_add_node_label(db, alice_id, "Person");
    graphqlite_add_node_label(db, bob_id, "Person");
    graphqlite_add_node_label(db, charlie_id, "Person");
    graphqlite_add_node_label(db, diana_id, "Company");
    
    // Add properties
    property_value_t prop;
    
    // Alice: name="Alice", age=30, department="Engineering"
    prop.type = PROP_TEXT;
    prop.value.text_val = "Alice";
    graphqlite_set_property(db, ENTITY_NODE, alice_id, "name", &prop);
    prop.type = PROP_INT;
    prop.value.int_val = 30;
    graphqlite_set_property(db, ENTITY_NODE, alice_id, "age", &prop);
    prop.type = PROP_TEXT;
    prop.value.text_val = "Engineering";
    graphqlite_set_property(db, ENTITY_NODE, alice_id, "department", &prop);
    
    // Bob: name="Bob", age=25, department="Sales"
    prop.type = PROP_TEXT;
    prop.value.text_val = "Bob";
    graphqlite_set_property(db, ENTITY_NODE, bob_id, "name", &prop);
    prop.type = PROP_INT;
    prop.value.int_val = 25;
    graphqlite_set_property(db, ENTITY_NODE, bob_id, "age", &prop);
    prop.type = PROP_TEXT;
    prop.value.text_val = "Sales";
    graphqlite_set_property(db, ENTITY_NODE, bob_id, "department", &prop);
    
    // Charlie: name="Charlie", age=35, department="Engineering"
    prop.type = PROP_TEXT;
    prop.value.text_val = "Charlie";
    graphqlite_set_property(db, ENTITY_NODE, charlie_id, "name", &prop);
    prop.type = PROP_INT;
    prop.value.int_val = 35;
    graphqlite_set_property(db, ENTITY_NODE, charlie_id, "age", &prop);
    prop.type = PROP_TEXT;
    prop.value.text_val = "Engineering";
    graphqlite_set_property(db, ENTITY_NODE, charlie_id, "department", &prop);
    
    // Diana (Company): name="TechCorp"
    prop.type = PROP_TEXT;
    prop.value.text_val = "TechCorp";
    graphqlite_set_property(db, ENTITY_NODE, diana_id, "name", &prop);
    
    // Create edges
    int64_t edge1 = graphqlite_create_edge(db, alice_id, bob_id, "KNOWS");
    int64_t edge2 = graphqlite_create_edge(db, alice_id, charlie_id, "WORKS_WITH");
    int64_t edge3 = graphqlite_create_edge(db, bob_id, charlie_id, "KNOWS");
    int64_t edge4 = graphqlite_create_edge(db, alice_id, diana_id, "WORKS_FOR");
    int64_t edge5 = graphqlite_create_edge(db, charlie_id, diana_id, "WORKS_FOR");
    
    printf("Created 4 nodes and 5 edges\n");
    
    // Test 1: Property projection with variables
    printf("\nTest 1: Property projection (name, age)...\n");
    const char *query1 = "MATCH (p:Person) WHERE p.age > 25 RETURN p.name, p.age";
    gql_result_t *result1 = gql_execute_query(query1, db);
    
    if (result1 && result1->status == 0 && result1->row_count == 2) {
        printf("PASS: Property projection worked (found %zu people > 25)\n", result1->row_count);
    } else {
        printf("FAIL: Property projection failed (expected 2, got %zu)\n", 
               result1 ? result1->row_count : 0);
        if (result1 && result1->error_message) {
            printf("      Error: %s\n", result1->error_message);
        }
    }
    if (result1) gql_result_destroy(result1);
    
    // Test 2: Complex WHERE with multiple variable properties
    printf("\nTest 2: Complex WHERE (same department)...\n");
    const char *query2 = "MATCH (p1:Person)-[r:WORKS_WITH]->(p2:Person) WHERE p1.department = p2.department RETURN p1.name, p2.name";
    gql_result_t *result2 = gql_execute_query(query2, db);
    
    if (result2 && result2->status == 0) {
        printf("PASS: Complex WHERE worked (found %zu same-department pairs)\n", result2->row_count);
    } else {
        printf("FAIL: Complex WHERE failed\n");
        if (result2 && result2->error_message) {
            printf("      Error: %s\n", result2->error_message);
        }
    }
    if (result2) gql_result_destroy(result2);
    
    // Test 3: Mixed node types in pattern
    printf("\nTest 3: Mixed node types (Person -> Company)...\n");
    const char *query3 = "MATCH (person:Person)-[r:WORKS_FOR]->(company:Company) RETURN person.name, company.name";
    gql_result_t *result3 = gql_execute_query(query3, db);
    
    if (result3 && result3->status == 0 && result3->row_count == 2) {
        printf("PASS: Mixed node types worked (found %zu work relationships)\n", result3->row_count);
    } else {
        printf("FAIL: Mixed node types failed (expected 2, got %zu)\n", 
               result3 ? result3->row_count : 0);
        if (result3 && result3->error_message) {
            printf("      Error: %s\n", result3->error_message);
        }
    }
    if (result3) gql_result_destroy(result3);
    
    // Test 4: Variable reuse across different contexts
    printf("\nTest 4: Variable reuse in complex pattern...\n");
    const char *query4 = "MATCH (p:Person) WHERE p.age > 30 AND p.department = \"Engineering\" RETURN p.name, p.age";
    gql_result_t *result4 = gql_execute_query(query4, db);
    
    if (result4 && result4->status == 0 && result4->row_count == 1) {
        printf("PASS: Variable reuse worked (found %zu engineering seniors)\n", result4->row_count);
    } else {
        printf("FAIL: Variable reuse failed (expected 1, got %zu)\n", 
               result4 ? result4->row_count : 0);
        if (result4 && result4->error_message) {
            printf("      Error: %s\n", result4->error_message);
        }
    }
    if (result4) gql_result_destroy(result4);
    
    // Test 5: Edge variable properties (if supported)
    printf("\nTest 5: Edge existence check...\n");
    const char *query5 = "MATCH (a:Person)-[rel]->(b) WHERE rel IS NOT NULL RETURN a.name, b.name";
    gql_result_t *result5 = gql_execute_query(query5, db);
    
    if (result5 && result5->status == 0) {
        printf("PASS: Edge existence check worked (found %zu relationships)\n", result5->row_count);
    } else {
        printf("FAIL: Edge existence check failed\n");
        if (result5 && result5->error_message) {
            printf("      Error: %s\n", result5->error_message);
        }
    }
    if (result5) gql_result_destroy(result5);
    
    graphqlite_close(db);
    printf("\n=== Advanced Variable Binding Test Complete ===\n");
    return 0;
}