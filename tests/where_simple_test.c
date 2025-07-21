#include "../src/gql/gql_executor.h"
#include "../src/gql/gql_parser.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    // Open database
    graphqlite_db_t *db = graphqlite_open(":memory:");
    if (!db) {
        printf("Failed to open database\n");
        return 1;
    }
    
    // Create test data
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
    
    // Test 1: Equality filter (name = 'Alice')
    const char *query1 = "MATCH (p:Person) WHERE p.name = \"Alice\" RETURN p";
    gql_result_t *result1 = gql_execute_query(query1, db);
    
    printf("Test 1 - Equality (name = 'Alice'): ");
    if (result1 && result1->status == 0 && result1->row_count == 1) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
    }
    if (result1) gql_result_destroy(result1);
    
    // Test 2: Greater than filter (age > 30)
    const char *query2 = "MATCH (p:Person) WHERE p.age > 30 RETURN p";
    gql_result_t *result2 = gql_execute_query(query2, db);
    
    printf("Test 2 - Greater than (age > 30): ");
    if (result2 && result2->status == 0 && result2->row_count == 1) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
    }
    if (result2) gql_result_destroy(result2);
    
    // Test 3: AND operator (age >= 30 AND city = 'Seattle')
    const char *query3 = "MATCH (p:Person) WHERE p.age >= 30 AND p.city = \"Seattle\" RETURN p";
    gql_result_t *result3 = gql_execute_query(query3, db);
    
    printf("Test 3 - AND operator (age >= 30 AND city = 'Seattle'): ");
    if (result3 && result3->status == 0 && result3->row_count == 2) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
    }
    if (result3) gql_result_destroy(result3);
    
    // Test 4: OR operator (name = 'Alice' OR name = 'Bob')
    const char *query4 = "MATCH (p:Person) WHERE p.name = \"Alice\" OR p.name = \"Bob\" RETURN p";
    gql_result_t *result4 = gql_execute_query(query4, db);
    
    printf("Test 4 - OR operator (name = 'Alice' OR name = 'Bob'): ");
    if (result4 && result4->status == 0 && result4->row_count == 2) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
    }
    if (result4) gql_result_destroy(result4);
    
    graphqlite_close(db);
    return 0;
}