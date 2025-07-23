#include <CUnit/CUnit.h>
#include <stdio.h>
#include <stdlib.h>
#include "test_api.h"
#include "../src/core/graphqlite.h"

void test_api_basic(void) {
    printf("Testing GraphQLite openCypher API...\n");
    
    // Test database creation
    graphqlite_t *db = graphqlite_open(":memory:", GRAPHQLITE_OPEN_READWRITE | GRAPHQLITE_OPEN_CREATE);
    CU_ASSERT_PTR_NOT_NULL_FATAL(db);
    
    // Test openCypher API
    graphqlite_result_t *result = NULL;
    int status = graphqlite_exec(db, "MATCH (n) RETURN n", &result);
    
    CU_ASSERT_EQUAL(status, GRAPHQLITE_OK);
    
    if (status == GRAPHQLITE_OK) {
        // Check result structure
        int col_count = graphqlite_result_column_count(result);
        CU_ASSERT_EQUAL(col_count, 1);
        
        if (col_count > 0) {
            const char *col_name = graphqlite_result_column_name(result, 0);
            CU_ASSERT_PTR_NOT_NULL(col_name);
            CU_ASSERT_STRING_EQUAL(col_name, "n");
        }
        
        graphqlite_result_free(result);
    }
    
    // Clean up
    int close_status = graphqlite_close(db);
    CU_ASSERT_EQUAL(close_status, GRAPHQLITE_OK);
    
    printf("GraphQLite now uses openCypher exclusively\n");
}

int add_api_tests(void) {
    CU_pSuite suite = CU_add_suite("GraphQLite API Tests", NULL, NULL);
    if (NULL == suite) {
        return CU_get_error();
    }
    
    if (NULL == CU_add_test(suite, "Basic API", test_api_basic)) {
        return CU_get_error();
    }
    
    return 0;
}