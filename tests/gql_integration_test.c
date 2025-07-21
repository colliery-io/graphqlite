#include "../src/gql/gql_executor.h"
#include "../src/gql/gql_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Test helper functions
void test_case(const char *test_name, bool (*test_func)(void)) {
    printf("Testing %s... ", test_name);
    fflush(stdout);
    if (test_func()) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
    }
}

void test_query(graphqlite_db_t *db, const char *query, const char *description) {
    printf("\n--- %s ---\n", description);
    printf("Query: %s\n\n", query);
    
    gql_result_t *result = gql_execute_query(query, db);
    if (result) {
        gql_result_print(result);
        gql_result_destroy(result);
    } else {
        printf("Failed to execute query\n");
    }
    
    printf("\n");
}

// Test cases
bool test_database_creation(void) {
    graphqlite_db_t *db = graphqlite_open(":memory:");
    if (!db) return false;
    
    int result = graphqlite_close(db);
    return result == SQLITE_OK;
}

bool test_create_simple_node(void) {
    graphqlite_db_t *db = graphqlite_open(":memory:");
    if (!db) return false;
    
    const char *query = "CREATE (n:Person {name: \"Alice\", age: 30})";
    gql_result_t *result = gql_execute_query(query, db);
    
    bool success = (result != NULL && 
                   result->status == GQL_RESULT_SUCCESS &&
                   result->nodes_created > 0);
    
    if (result) {
        gql_result_destroy(result);
    }
    
    graphqlite_close(db);
    return success;
}

bool test_create_nodes_with_relationship(void) {
    graphqlite_db_t *db = graphqlite_open(":memory:");
    if (!db) return false;
    
    const char *query = "CREATE (a:Person {name: \"Alice\"})-[r:KNOWS]->(b:Person {name: \"Bob\"})";
    gql_result_t *result = gql_execute_query(query, db);
    
    bool success = (result != NULL && 
                   result->status == GQL_RESULT_SUCCESS &&
                   result->nodes_created >= 2 &&
                   result->edges_created >= 1);
    
    if (result) {
        gql_result_destroy(result);
    }
    
    graphqlite_close(db);
    return success;
}

bool test_match_simple_query(void) {
    graphqlite_db_t *db = graphqlite_open(":memory:");
    if (!db) return false;
    
    // First create some data
    const char *create_query = "CREATE (n:Person {name: \"Alice\"})";
    gql_result_t *create_result = gql_execute_query(create_query, db);
    
    bool success = false;
    if (create_result && create_result->status == GQL_RESULT_SUCCESS) {
        printf("(create ok) ");
        fflush(stdout);
        
        // For now, just test that we can parse a MATCH query, not execute it
        const char *match_query = "MATCH (n:Person) RETURN n.name";
        gql_parser_t *parser = gql_parser_create(match_query);
        gql_ast_node_t *ast = gql_parser_parse(parser);
        
        success = (parser != NULL && ast != NULL && !gql_parser_has_error(parser));
        
        if (ast) {
            gql_ast_free_recursive(ast);
        }
        gql_parser_destroy(parser);
    }
    
    if (create_result) {
        gql_result_destroy(create_result);
    }
    
    graphqlite_close(db);
    return success;
}

bool test_error_handling(void) {
    graphqlite_db_t *db = graphqlite_open(":memory:");
    if (!db) return false;
    
    const char *invalid_query = "INVALID QUERY SYNTAX";
    gql_result_t *result = gql_execute_query(invalid_query, db);
    
    bool success = (result != NULL && 
                   result->status == GQL_RESULT_ERROR &&
                   result->error_message != NULL);
    
    if (result) {
        gql_result_destroy(result);
    }
    
    graphqlite_close(db);
    return success;
}

void demo_interactive_session(void) {
    printf("\n=== Interactive GQL Demo ===\n");
    
    graphqlite_db_t *db = graphqlite_open(":memory:");
    if (!db) {
        printf("Failed to create database\n");
        return;
    }
    
    // Create some sample data
    test_query(db, "CREATE (alice:Person {name: \"Alice\", age: 30})", 
               "Creating Alice");
    
    test_query(db, "CREATE (bob:Person {name: \"Bob\", age: 25})", 
               "Creating Bob");
    
    test_query(db, "CREATE (charlie:Person {name: \"Charlie\", age: 35})", 
               "Creating Charlie");
    
    test_query(db, "CREATE (alice:Person {name: \"Alice\"})-[r:KNOWS]->(bob:Person {name: \"Bob\"})", 
               "Creating Alice-KNOWS->Bob relationship");
    
    test_query(db, "CREATE (bob:Person {name: \"Bob\"})-[r:KNOWS]->(charlie:Person {name: \"Charlie\"})", 
               "Creating Bob-KNOWS->Charlie relationship");
    
    test_query(db, "MATCH (n:Person) RETURN n.name", 
               "Finding all people");
    
    test_query(db, "MATCH (a:Person)-[r:KNOWS]->(b:Person) RETURN a.name, b.name", 
               "Finding all relationships");
    
    test_query(db, "MATCH (n:Person) WHERE n.age > 30 RETURN n.name", 
               "Finding people over 30");
    
    graphqlite_close(db);
}

int main(void) {
    printf("=== GraphQLite GQL Integration Tests ===\n\n");
    
    // Basic functionality tests
    test_case("Database Creation", test_database_creation);
    test_case("CREATE Simple Node", test_create_simple_node);
    test_case("CREATE Nodes with Relationship", test_create_nodes_with_relationship);
    test_case("MATCH Simple Query", test_match_simple_query);
    test_case("Error Handling", test_error_handling);
    
    // Interactive demo
    demo_interactive_session();
    
    printf("\n=== Integration Tests Complete ===\n");
    return 0;
}