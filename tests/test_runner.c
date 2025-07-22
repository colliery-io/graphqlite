#include <CUnit/Basic.h>
#include <CUnit/CUnit.h>
#include <stdio.h>
#include <stdlib.h>

// Test suite headers
#include "test_parser.h"
#include "test_database.h"


int main(void) {
    // Initialize CUnit test registry
    if (CUE_SUCCESS != CU_initialize_registry()) {
        return CU_get_error();
    }

    // Add test suites
    if (!add_parser_tests()) {
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    if (!add_database_tests()) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    printf("GraphQLite OpenCypher Test Suite\\n");
    printf("==================================\\n");

    // Run all tests
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();

    // Get results
    int failures = CU_get_number_of_failures();
    
    // Clean up
    CU_cleanup_registry();
    
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}