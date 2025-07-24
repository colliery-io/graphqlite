#include <stdio.h>
#include <stdlib.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "test_parser_keywords.h"
#include "test_scanner.h"
#include "test_parser.h"
#include "test_transform.h"

int main(void)
{
    /* Initialize CUnit */
    if (CU_initialize_registry() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to initialize CUnit registry\n");
        return CU_get_error();
    }
    
    /* Add test suites */
    if (init_parser_keywords_suite() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add parser keywords suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    if (init_scanner_suite() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add scanner suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    if (init_parser_suite() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add parser suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    if (init_transform_suite() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add transform suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    /* Run tests */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    
    /* Get results */
    unsigned int num_failures = CU_get_number_of_failures();
    
    /* Cleanup */
    CU_cleanup_registry();
    
    return num_failures > 0 ? 1 : 0;
}