#include "../src/gql/gql_executor.h"
#include "../src/gql/gql_parser.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== GraphQLite MATCH Test ===\n");
    
    graphqlite_db_t *db = graphqlite_open(":memory:");
    if (!db) {
        printf("Failed to open database\n");
        return 1;
    }
    
    // Test 1: Create some nodes
    printf("Creating test nodes...\n");
    const char *create_query = "CREATE (alice:Person {name: \"Alice\"})";
    gql_result_t *create_result = gql_execute_query(create_query, db);
    
    if (create_result) {
        printf("CREATE result: status=%d, nodes_created=%llu\n", 
               create_result->status, (unsigned long long)create_result->nodes_created);
        gql_result_destroy(create_result);
    }
    
    // Test 2: MATCH query
    printf("\nTesting MATCH (n) RETURN n...\n");
    const char *match_query = "MATCH (n) RETURN n";
    gql_result_t *match_result = gql_execute_query(match_query, db);
    
    if (match_result) {
        printf("MATCH result: status=%d, rows=%zu\n", 
               match_result->status, match_result->row_count);
        
        if (match_result->status == GQL_RESULT_ERROR) {
            printf("Error: %s\n", match_result->error_message ? match_result->error_message : "Unknown");
        } else {
            gql_result_print(match_result);
        }
        
        gql_result_destroy(match_result);
    }
    
    // Test 3: Create more nodes and test again
    printf("\nCreating additional nodes...\n");
    const char *create_query2 = "CREATE (bob:Person {name: \"Bob\"})";
    gql_result_t *create_result2 = gql_execute_query(create_query2, db);
    
    if (create_result2) {
        printf("CREATE result: status=%d, nodes_created=%llu\n", 
               create_result2->status, (unsigned long long)create_result2->nodes_created);
        gql_result_destroy(create_result2);
    }
    
    // Test 4: MATCH all nodes again
    printf("\nTesting MATCH (n) RETURN n (with multiple nodes)...\n");
    gql_result_t *match_result2 = gql_execute_query(match_query, db);
    
    if (match_result2) {
        printf("MATCH result: status=%d, rows=%zu\n", 
               match_result2->status, match_result2->row_count);
        
        if (match_result2->status == GQL_RESULT_ERROR) {
            printf("Error: %s\n", match_result2->error_message ? match_result2->error_message : "Unknown");
        } else {
            gql_result_print(match_result2);
        }
        
        gql_result_destroy(match_result2);
    }
    
    graphqlite_close(db);
    printf("\n=== All Tests Complete ===\n");
    return 0;
}