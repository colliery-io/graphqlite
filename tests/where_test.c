#include "../src/gql/gql_executor.h"
#include "../src/gql/gql_parser.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== WHERE Clause Filtering Test ===\n");
    
    graphqlite_db_t *db = graphqlite_open(":memory:");
    if (!db) {
        printf("❌ Failed to open database\n");
        return 1;
    }
    
    // Create test data with properties
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
    
    // Alice: name="Alice", age=30, city="Seattle"
    prop.type = PROP_TEXT;
    prop.value.text_val = "Alice";
    graphqlite_set_property(db, ENTITY_NODE, alice_id, "name", &prop);
    
    prop.type = PROP_INT;
    prop.value.int_val = 30;
    graphqlite_set_property(db, ENTITY_NODE, alice_id, "age", &prop);
    
    prop.type = PROP_TEXT;
    prop.value.text_val = "Seattle";
    graphqlite_set_property(db, ENTITY_NODE, alice_id, "city", &prop);
    
    // Bob: name="Bob", age=25, city="Portland"
    prop.type = PROP_TEXT;
    prop.value.text_val = "Bob";
    graphqlite_set_property(db, ENTITY_NODE, bob_id, "name", &prop);
    
    prop.type = PROP_INT;
    prop.value.int_val = 25;
    graphqlite_set_property(db, ENTITY_NODE, bob_id, "age", &prop);
    
    prop.type = PROP_TEXT;
    prop.value.text_val = "Portland";
    graphqlite_set_property(db, ENTITY_NODE, bob_id, "city", &prop);
    
    // Charlie: name="Charlie", age=35, city="Seattle"
    prop.type = PROP_TEXT;
    prop.value.text_val = "Charlie";
    graphqlite_set_property(db, ENTITY_NODE, charlie_id, "name", &prop);
    
    prop.type = PROP_INT;
    prop.value.int_val = 35;
    graphqlite_set_property(db, ENTITY_NODE, charlie_id, "age", &prop);
    
    prop.type = PROP_TEXT;
    prop.value.text_val = "Seattle";
    graphqlite_set_property(db, ENTITY_NODE, charlie_id, "city", &prop);
    
    printf("✅ Created 3 people with properties\n");
    
    // Test 1: Simple equality
    printf("\nTest 1: Equality filter (name = 'Alice')...\n");
    const char *query1 = "MATCH (p:Person) WHERE p.name = \"Alice\" RETURN p";
    gql_result_t *result1 = gql_execute_query(query1, db);
    
    if (result1 && result1->status == 0) {
        printf("✅ Query executed successfully\n");
        printf("✅ Found %zu person(s)\n", result1->row_count);
        if (result1->row_count == 1) {
            printf("✅ Correct: Found only Alice\n");
        } else {
            printf("❌ Expected 1 result, got %zu\n", result1->row_count);
        }
    } else {
        printf("❌ Query failed: %s\n", 
               result1 && result1->error_message ? result1->error_message : "Unknown error");
    }
    if (result1) gql_result_destroy(result1);
    
    // Test 2: Greater than
    printf("\nTest 2: Greater than filter (age > 30)...\n");
    const char *query2 = "MATCH (p:Person) WHERE p.age > 30 RETURN p";
    gql_result_t *result2 = gql_execute_query(query2, db);
    
    if (result2 && result2->status == 0) {
        printf("✅ Query executed successfully\n");
        printf("✅ Found %zu person(s)\n", result2->row_count);
        if (result2->row_count == 1) {
            printf("✅ Correct: Found only Charlie (age 35)\n");
        } else {
            printf("❌ Expected 1 result, got %zu\n", result2->row_count);
        }
    } else {
        printf("❌ Query failed: %s\n", 
               result2 && result2->error_message ? result2->error_message : "Unknown error");
    }
    if (result2) gql_result_destroy(result2);
    
    // Test 3: AND operator
    printf("\nTest 3: AND operator (age >= 30 AND city = 'Seattle')...\n");
    const char *query3 = "MATCH (p:Person) WHERE p.age >= 30 AND p.city = \"Seattle\" RETURN p";
    gql_result_t *result3 = gql_execute_query(query3, db);
    
    if (result3 && result3->status == 0) {
        printf("✅ Query executed successfully\n");
        printf("✅ Found %zu person(s)\n", result3->row_count);
        if (result3->row_count == 2) {
            printf("✅ Correct: Found Alice and Charlie\n");
        } else {
            printf("❌ Expected 2 results, got %zu\n", result3->row_count);
        }
    } else {
        printf("❌ Query failed: %s\n", 
               result3 && result3->error_message ? result3->error_message : "Unknown error");
    }
    if (result3) gql_result_destroy(result3);
    
    // Test 4: OR operator
    printf("\nTest 4: OR operator (name = 'Alice' OR name = 'Bob')...\n");
    const char *query4 = "MATCH (p:Person) WHERE p.name = \"Alice\" OR p.name = \"Bob\" RETURN p";
    gql_result_t *result4 = gql_execute_query(query4, db);
    
    if (result4 && result4->status == 0) {
        printf("✅ Query executed successfully\n");
        printf("✅ Found %zu person(s)\n", result4->row_count);
        if (result4->row_count == 2) {
            printf("✅ Correct: Found Alice and Bob\n");
        } else {
            printf("❌ Expected 2 results, got %zu\n", result4->row_count);
        }
    } else {
        printf("❌ Query failed: %s\n", 
               result4 && result4->error_message ? result4->error_message : "Unknown error");
    }
    if (result4) gql_result_destroy(result4);
    
    // Test 5: String operations
    printf("\nTest 5: STARTS WITH operator (name STARTS WITH 'C')...\n");
    const char *query5 = "MATCH (p:Person) WHERE p.name STARTS WITH \"C\" RETURN p";
    gql_result_t *result5 = gql_execute_query(query5, db);
    
    if (result5 && result5->status == 0) {
        printf("✅ Query executed successfully\n");
        printf("✅ Found %zu person(s)\n", result5->row_count);
        if (result5->row_count == 1) {
            printf("✅ Correct: Found only Charlie\n");
        } else {
            printf("❌ Expected 1 result, got %zu\n", result5->row_count);
        }
    } else {
        printf("❌ Query failed: %s\n", 
               result5 && result5->error_message ? result5->error_message : "Unknown error");
    }
    if (result5) gql_result_destroy(result5);
    
    // Test 6: NOT operator
    printf("\nTest 6: NOT operator (NOT p.age < 30)...\n");
    const char *query6 = "MATCH (p:Person) WHERE NOT p.age < 30 RETURN p";
    gql_result_t *result6 = gql_execute_query(query6, db);
    
    if (result6 && result6->status == 0) {
        printf("✅ Query executed successfully\n");
        printf("✅ Found %zu person(s)\n", result6->row_count);
        if (result6->row_count == 2) {
            printf("✅ Correct: Found Alice (30) and Charlie (35)\n");
        } else {
            printf("❌ Expected 2 results, got %zu\n", result6->row_count);
        }
    } else {
        printf("❌ Query failed: %s\n", 
               result6 && result6->error_message ? result6->error_message : "Unknown error");
    }
    if (result6) gql_result_destroy(result6);
    
    // Test 7: Complex edge pattern with WHERE
    printf("\nTest 7: Edge pattern with WHERE...\n");
    
    // Create some edges
    int64_t edge1 = graphqlite_create_edge(db, alice_id, bob_id, "KNOWS");
    int64_t edge2 = graphqlite_create_edge(db, alice_id, charlie_id, "KNOWS");
    int64_t edge3 = graphqlite_create_edge(db, bob_id, charlie_id, "WORKS_WITH");
    
    const char *query7 = "MATCH (a:Person)-[r:KNOWS]->(b:Person) WHERE a.age > 25 RETURN a, b";
    gql_result_t *result7 = gql_execute_query(query7, db);
    
    if (result7 && result7->status == 0) {
        printf("✅ Query executed successfully\n");
        printf("✅ Found %zu relationship(s)\n", result7->row_count);
        if (result7->row_count == 2) {
            printf("✅ Correct: Found Alice's relationships (age 30 > 25)\n");
        } else {
            printf("❌ Expected 2 results, got %zu\n", result7->row_count);
        }
    } else {
        printf("❌ Query failed: %s\n", 
               result7 && result7->error_message ? result7->error_message : "Unknown error");
    }
    if (result7) gql_result_destroy(result7);
    
    graphqlite_close(db);
    printf("\n=== WHERE Test Complete ===\n");
    return 0;
}