#include "../src/gql/gql_executor.h"
#include "../src/gql/gql_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

void timeout_handler(int sig) {
    (void)sig;
    printf("\nTIMEOUT: Test hung for more than 5 seconds\n");
    _exit(1);
}

int main(void) {
    printf("=== Simple MATCH Test ===\n");
    
    // Set up timeout to prevent infinite hang
    signal(SIGALRM, timeout_handler);
    alarm(5); // 5 second timeout
    
    graphqlite_db_t *db = graphqlite_open(":memory:");
    if (!db) {
        printf("Failed to open database\n");
        return 1;
    }
    
    // Test 1: Create some nodes
    printf("Creating nodes...\n");
    const char *create_query = "CREATE (alice:Person {name: \"Alice\"})";
    gql_result_t *create_result = gql_execute_query(create_query, db);
    
    if (create_result) {
        printf("CREATE result: status=%d, nodes_created=%llu\n", 
               create_result->status, (unsigned long long)create_result->nodes_created);
        gql_result_destroy(create_result);
    }
    
    // Test 2: Try a simple MATCH query
    printf("Testing simple MATCH...\n");
    const char *match_query = "MATCH (n) RETURN n";
    printf("DEBUG: About to call gql_execute_query\n");
    gql_result_t *match_result = gql_execute_query(match_query, db);
    printf("DEBUG: gql_execute_query returned\n");
    
    if (match_result) {
        printf("DEBUG: match_result is not NULL\n");
        printf("MATCH result: status=%d, rows=%zu\n", 
               match_result->status, match_result->row_count);
        
        if (match_result->status == GQL_RESULT_ERROR) {
            printf("Error: %s\n", match_result->error_message ? match_result->error_message : "Unknown");
        } else {
            printf("DEBUG: About to call gql_result_print\n");
            gql_result_print(match_result);
            printf("DEBUG: gql_result_print returned\n");
        }
        
        printf("DEBUG: About to call gql_result_destroy\n");
        gql_result_destroy(match_result);
        printf("DEBUG: gql_result_destroy returned\n");
    } else {
        printf("DEBUG: match_result is NULL\n");
    }
    
    graphqlite_close(db);
    printf("=== Test Complete ===\n");
    return 0;
}