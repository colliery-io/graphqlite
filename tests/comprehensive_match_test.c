#include "../src/gql/gql_executor.h"
#include "../src/gql/gql_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test result tracking
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        printf("PASS: %s\n", message); \
        tests_passed++; \
    } else { \
        printf("FAIL: %s\n", message); \
    } \
} while(0)

// Create comprehensive test data
void setup_test_data(graphqlite_db_t *db) {
    printf("Setting up comprehensive test data...\n");
    
    // Create nodes representing a small organization
    int64_t alice_id = graphqlite_create_node(db);    // Employee
    int64_t bob_id = graphqlite_create_node(db);      // Employee  
    int64_t charlie_id = graphqlite_create_node(db);  // Manager
    int64_t diana_id = graphqlite_create_node(db);    // Company
    int64_t eve_id = graphqlite_create_node(db);      // Project
    int64_t frank_id = graphqlite_create_node(db);    // Employee
    
    // Add labels
    graphqlite_add_node_label(db, alice_id, "Person");
    graphqlite_add_node_label(db, alice_id, "Employee");
    
    graphqlite_add_node_label(db, bob_id, "Person");
    graphqlite_add_node_label(db, bob_id, "Employee");
    
    graphqlite_add_node_label(db, charlie_id, "Person");
    graphqlite_add_node_label(db, charlie_id, "Manager");
    
    graphqlite_add_node_label(db, diana_id, "Company");
    graphqlite_add_node_label(db, eve_id, "Project");
    
    graphqlite_add_node_label(db, frank_id, "Person");
    graphqlite_add_node_label(db, frank_id, "Employee");
    
    // Add properties
    property_value_t prop;
    
    // Alice: name="Alice", age=30, department="Engineering", salary=75000
    prop.type = PROP_TEXT; prop.value.text_val = "Alice";
    graphqlite_set_property(db, ENTITY_NODE, alice_id, "name", &prop);
    prop.type = PROP_INT; prop.value.int_val = 30;
    graphqlite_set_property(db, ENTITY_NODE, alice_id, "age", &prop);
    prop.type = PROP_TEXT; prop.value.text_val = "Engineering";
    graphqlite_set_property(db, ENTITY_NODE, alice_id, "department", &prop);
    prop.type = PROP_INT; prop.value.int_val = 75000;
    graphqlite_set_property(db, ENTITY_NODE, alice_id, "salary", &prop);
    
    // Bob: name="Bob", age=25, department="Sales", salary=60000
    prop.type = PROP_TEXT; prop.value.text_val = "Bob";
    graphqlite_set_property(db, ENTITY_NODE, bob_id, "name", &prop);
    prop.type = PROP_INT; prop.value.int_val = 25;
    graphqlite_set_property(db, ENTITY_NODE, bob_id, "age", &prop);
    prop.type = PROP_TEXT; prop.value.text_val = "Sales";
    graphqlite_set_property(db, ENTITY_NODE, bob_id, "department", &prop);
    prop.type = PROP_INT; prop.value.int_val = 60000;
    graphqlite_set_property(db, ENTITY_NODE, bob_id, "salary", &prop);
    
    // Charlie: name="Charlie", age=40, department="Engineering", salary=120000
    prop.type = PROP_TEXT; prop.value.text_val = "Charlie";
    graphqlite_set_property(db, ENTITY_NODE, charlie_id, "name", &prop);
    prop.type = PROP_INT; prop.value.int_val = 40;
    graphqlite_set_property(db, ENTITY_NODE, charlie_id, "age", &prop);
    prop.type = PROP_TEXT; prop.value.text_val = "Engineering";
    graphqlite_set_property(db, ENTITY_NODE, charlie_id, "department", &prop);
    prop.type = PROP_INT; prop.value.int_val = 120000;
    graphqlite_set_property(db, ENTITY_NODE, charlie_id, "salary", &prop);
    
    // Diana (Company): name="TechCorp", founded=2010
    prop.type = PROP_TEXT; prop.value.text_val = "TechCorp";
    graphqlite_set_property(db, ENTITY_NODE, diana_id, "name", &prop);
    prop.type = PROP_INT; prop.value.int_val = 2010;
    graphqlite_set_property(db, ENTITY_NODE, diana_id, "founded", &prop);
    
    // Eve (Project): name="GraphQLite", status="Active"
    prop.type = PROP_TEXT; prop.value.text_val = "GraphQLite";
    graphqlite_set_property(db, ENTITY_NODE, eve_id, "name", &prop);
    prop.type = PROP_TEXT; prop.value.text_val = "Active";
    graphqlite_set_property(db, ENTITY_NODE, eve_id, "status", &prop);
    
    // Frank: name="Frank", age=35, department="Engineering", salary=85000
    prop.type = PROP_TEXT; prop.value.text_val = "Frank";
    graphqlite_set_property(db, ENTITY_NODE, frank_id, "name", &prop);
    prop.type = PROP_INT; prop.value.int_val = 35;
    graphqlite_set_property(db, ENTITY_NODE, frank_id, "age", &prop);
    prop.type = PROP_TEXT; prop.value.text_val = "Engineering";
    graphqlite_set_property(db, ENTITY_NODE, frank_id, "department", &prop);
    prop.type = PROP_INT; prop.value.int_val = 85000;
    graphqlite_set_property(db, ENTITY_NODE, frank_id, "salary", &prop);
    
    // Create relationships
    graphqlite_create_edge(db, alice_id, bob_id, "KNOWS");           // Alice knows Bob
    graphqlite_create_edge(db, alice_id, charlie_id, "REPORTS_TO");  // Alice reports to Charlie
    graphqlite_create_edge(db, bob_id, charlie_id, "REPORTS_TO");    // Bob reports to Charlie
    graphqlite_create_edge(db, charlie_id, diana_id, "WORKS_FOR");   // Charlie works for TechCorp
    graphqlite_create_edge(db, alice_id, diana_id, "WORKS_FOR");     // Alice works for TechCorp
    graphqlite_create_edge(db, bob_id, diana_id, "WORKS_FOR");       // Bob works for TechCorp
    graphqlite_create_edge(db, frank_id, diana_id, "WORKS_FOR");     // Frank works for TechCorp
    graphqlite_create_edge(db, alice_id, eve_id, "WORKS_ON");        // Alice works on GraphQLite
    graphqlite_create_edge(db, frank_id, eve_id, "WORKS_ON");        // Frank works on GraphQLite
    graphqlite_create_edge(db, charlie_id, eve_id, "MANAGES");       // Charlie manages GraphQLite
    graphqlite_create_edge(db, alice_id, frank_id, "COLLABORATES");  // Alice collaborates with Frank
    
    printf("Created 6 nodes (4 people, 1 company, 1 project) and 11 relationships\n");
}

int main(void) {
    printf("=== Comprehensive MATCH Test Suite ===\n");
    
    graphqlite_db_t *db = graphqlite_open(":memory:");
    if (!db) {
        printf("FATAL: Failed to open database\n");
        return 1;
    }
    
    setup_test_data(db);
    
    printf("\n--- Node Pattern Matching Tests ---\n");
    
    // Test 1: Simple node matching by label
    {
        const char *query = "MATCH (p:Person) RETURN p.name";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0 && result->row_count == 4, 
                   "Simple node matching by label (Person)");
        if (result) gql_result_destroy(result);
    }
    
    // Test 2: Node matching by multiple labels using GQL syntax
    {
        const char *query = "MATCH (e:Person & Employee) RETURN e.name";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0 && result->row_count == 3, 
                   "Node matching by multiple labels (Person & Employee)");
        if (result) gql_result_destroy(result);
    }
    
    // Test 3: Node matching without labels
    {
        const char *query = "MATCH (n) RETURN n";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0 && result->row_count == 6, 
                   "Node matching without labels (all nodes)");
        if (result) gql_result_destroy(result);
    }
    
    printf("\n--- Edge Pattern Matching Tests ---\n");
    
    // Test 4: Simple edge matching
    {
        const char *query = "MATCH (a)-[r:KNOWS]->(b) RETURN a.name, b.name";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0 && result->row_count == 1, 
                   "Simple edge matching (KNOWS relationship)");
        if (result) gql_result_destroy(result);
    }
    
    // Test 5: Edge matching without type
    {
        const char *query = "MATCH (a)-[r]->(b) RETURN a.name, b.name";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0 && result->row_count == 11, 
                   "Edge matching without type (all relationships)");
        if (result) gql_result_destroy(result);
    }
    
    // Test 6: Complex edge pattern with labels
    {
        const char *query = "MATCH (emp:Employee)-[r:REPORTS_TO]->(mgr:Manager) RETURN emp.name, mgr.name";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0 && result->row_count == 2, 
                   "Complex edge pattern with labels (Employee -> Manager)");
        if (result) gql_result_destroy(result);
    }
    
    printf("\n--- WHERE Clause Integration Tests ---\n");
    
    // Test 7: Property equality
    {
        const char *query = "MATCH (p:Person) WHERE p.department = \"Engineering\" RETURN p.name";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0 && result->row_count == 3, 
                   "WHERE with property equality (Engineering department)");
        if (result) gql_result_destroy(result);
    }
    
    // Test 8: Numeric comparison
    {
        const char *query = "MATCH (p:Person) WHERE p.age > 30 RETURN p.name";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0 && result->row_count == 2, 
                   "WHERE with numeric comparison (age > 30)");
        if (result) gql_result_destroy(result);
    }
    
    // Test 9: AND operator
    {
        const char *query = "MATCH (p:Person) WHERE p.age > 25 AND p.department = \"Engineering\" RETURN p.name";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0 && result->row_count == 3, 
                   "WHERE with AND operator (age > 25 AND department = Engineering)");
        if (result) gql_result_destroy(result);
    }
    
    // Test 10: OR operator
    {
        const char *query = "MATCH (p:Person) WHERE p.name = \"Alice\" OR p.name = \"Bob\" RETURN p.name";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0 && result->row_count == 2, 
                   "WHERE with OR operator (name = Alice OR name = Bob)");
        if (result) gql_result_destroy(result);
    }
    
    // Test 11: String operations
    {
        const char *query = "MATCH (p:Person) WHERE p.name STARTS WITH \"A\" RETURN p.name";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0 && result->row_count == 1, 
                   "WHERE with string operations (name STARTS WITH A)");
        if (result) gql_result_destroy(result);
    }
    
    printf("\n--- Variable Binding Tests ---\n");
    
    // Test 12: Variable reuse in WHERE
    {
        const char *query = "MATCH (a:Person)-[r]->(b:Person) WHERE a.age > b.age RETURN a.name, b.name";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0 && result->row_count >= 1, 
                   "Variable reuse in WHERE (comparing ages)");
        if (result) gql_result_destroy(result);
    }
    
    // Test 13: Complex variable relationships
    {
        const char *query = "MATCH (emp:Employee)-[r:REPORTS_TO]->(mgr) WHERE emp.salary < mgr.salary RETURN emp.name, mgr.name";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0 && result->row_count == 2, 
                   "Complex variable relationships (employee salary < manager salary)");
        if (result) gql_result_destroy(result);
    }
    
    printf("\n--- RETURN Clause Tests ---\n");
    
    // Test 14: Property projection
    {
        const char *query = "MATCH (p:Person) RETURN p.name, p.age, p.department";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0 && result->row_count == 4 && result->column_count == 3, 
                   "Property projection (name, age, department)");
        if (result) gql_result_destroy(result);
    }
    
    // Test 15: Mixed projection
    {
        const char *query = "MATCH (a:Person)-[r]->(b) RETURN a, r, b.name";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0 && result->column_count == 3, 
                   "Mixed projection (node, edge, property)");
        if (result) gql_result_destroy(result);
    }
    
    // Test 16: Alias usage
    {
        const char *query = "MATCH (p:Person) RETURN p.name AS person_name, p.age AS years";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0 && result->column_count == 2 &&
                   strcmp(result->column_names[0], "person_name") == 0 &&
                   strcmp(result->column_names[1], "years") == 0, 
                   "Alias usage (AS keyword)");
        if (result) gql_result_destroy(result);
    }
    
    printf("\n--- Complex Integration Tests ---\n");
    
    // Test 17: Multi-hop relationships
    {
        const char *query = "MATCH (emp:Employee)-[:REPORTS_TO]->(mgr:Manager)-[:WORKS_FOR]->(company:Company) RETURN emp.name, company.name";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0 && result->row_count == 2, 
                   "Multi-hop relationships (Employee -> Manager -> Company)");
        if (result) gql_result_destroy(result);
    }
    
    // Test 18: Complex WHERE with relationships
    {
        const char *query = "MATCH (a:Person)-[r:WORKS_ON]->(p:Project) WHERE p.status = \"Active\" AND a.department = \"Engineering\" RETURN a.name, p.name";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0 && result->row_count == 2, 
                   "Complex WHERE with relationships (Active projects in Engineering)");
        if (result) gql_result_destroy(result);
    }
    
    // Test 19: Triangular relationships
    {
        const char *query = "MATCH (a:Person)-[:COLLABORATES]->(b:Person), (a)-[:WORKS_ON]->(p:Project), (b)-[:WORKS_ON]->(p) RETURN a.name, b.name, p.name";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0, 
                   "Triangular relationships (people collaborating on same project)");
        if (result) gql_result_destroy(result);
    }
    
    // Test 20: Salary analysis query
    {
        const char *query = "MATCH (high:Person)-[:WORKS_FOR]->(company:Company), (low:Person)-[:WORKS_FOR]->(company) WHERE high.salary > 80000 AND low.salary < 70000 RETURN high.name AS high_earner, low.name AS low_earner, company.name AS company";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0, 
                   "Salary analysis query (high vs low earners at same company)");
        if (result) gql_result_destroy(result);
    }
    
    printf("\n--- Edge Cases and Error Handling ---\n");
    
    // Test 21: Non-existent label
    {
        const char *query = "MATCH (x:NonExistent) RETURN x";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0 && result->row_count == 0, 
                   "Non-existent label (should return empty result)");
        if (result) gql_result_destroy(result);
    }
    
    // Test 22: Non-existent relationship type
    {
        const char *query = "MATCH (a)-[r:NON_EXISTENT]->(b) RETURN a, b";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0 && result->row_count == 0, 
                   "Non-existent relationship type (should return empty result)");
        if (result) gql_result_destroy(result);
    }
    
    // Test 23: Invalid property access
    {
        const char *query = "MATCH (p:Person) WHERE p.nonexistent = \"test\" RETURN p";
        gql_result_t *result = gql_execute_query(query, db);
        TEST_ASSERT(result && result->status == 0 && result->row_count == 0, 
                   "Invalid property access (should return empty result)");
        if (result) gql_result_destroy(result);
    }
    
    graphqlite_close(db);
    
    printf("\n=== Test Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("Success rate: %.1f%%\n", tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
    
    if (tests_passed == tests_run) {
        printf("🎉 ALL TESTS PASSED!\n");
        return 0;
    } else {
        printf("❌ Some tests failed\n");
        return 1;
    }
}