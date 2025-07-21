#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include our test modules
#include "test_gql_parser.h"
#include "test_gql_executor.h"
#include "test_gql_matcher.h"
#include "test_integration.h"

int main() {
    printf("=== GraphQLite CUnit Test Suite ===\n");
    
    // Initialize CUnit test registry
    if (CUE_SUCCESS != CU_initialize_registry()) {
        fprintf(stderr, "Failed to initialize CUnit registry: %s\n", CU_get_error_msg());
        return CU_get_error();
    }
    
    // Add test suites
    int suite_failures = 0;
    
    if (add_parser_tests() != 0) {
        fprintf(stderr, "Failed to add parser test suite\n");
        suite_failures++;
    }
    
    if (add_executor_tests() != 0) {
        fprintf(stderr, "Failed to add executor test suite\n");
        suite_failures++;
    }
    
    if (add_matcher_tests() != 0) {
        fprintf(stderr, "Failed to add matcher test suite\n");
        suite_failures++;
    }
    
    if (add_comprehensive_tests() != 0) {
        fprintf(stderr, "Failed to add comprehensive test suite\n");
        suite_failures++;
    }
    
    if (suite_failures > 0) {
        fprintf(stderr, "Failed to add %d test suite(s)\n", suite_failures);
        CU_cleanup_registry();
        return 1;
    }
    
    // Run all tests using basic interface
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    
    // Print summary
    printf("\n=== Test Summary ===\n");
    printf("Suites run: %u\n", CU_get_number_of_suites_run());
    printf("Tests run: %u\n", CU_get_number_of_tests_run());
    printf("Failures: %u\n", CU_get_number_of_failures());
    printf("Errors: %u\n", CU_get_number_of_tests_failed());
    
    int result = CU_get_number_of_failures();
    
    // Cleanup
    CU_cleanup_registry();
    
    if (result == 0) {
        printf("ALL TESTS PASSED!\n");
        return 0;
    } else {
        printf("%d test(s) failed\n", result);
        return 1;
    }
}