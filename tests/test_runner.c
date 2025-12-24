#include <stdio.h>
#include <stdlib.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "test_parser_keywords.h"
#include "test_scanner.h"
#include "test_parser.h"
#include "test_schema.h"

/* Forward declarations for modular test suites */
int init_transform_create_suite(void);
int init_transform_set_suite(void);
int init_transform_delete_suite(void);
int init_transform_functions_suite(void);
int register_match_tests(void);
int register_return_tests(void);
int register_agtype_tests(void);
int init_executor_basic_suite(void);
int init_executor_relationships_suite(void);
int init_executor_set_suite(void);
int init_executor_delete_suite(void);
int init_executor_varlen_suite(void);
int init_executor_with_suite(void);
int init_executor_unwind_suite(void);
int init_executor_merge_suite(void);
int init_executor_pagerank_suite(void);
int init_executor_label_propagation_suite(void);
int init_output_format_suite(void);

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
    
    if (init_transform_create_suite() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add transform CREATE suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    if (init_transform_set_suite() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add transform SET suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    if (init_transform_delete_suite() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add transform DELETE suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    if (init_transform_functions_suite() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add transform functions suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (register_match_tests() != 0) {
        fprintf(stderr, "Failed to add transform MATCH suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (register_return_tests() != 0) {
        fprintf(stderr, "Failed to add transform RETURN suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (register_agtype_tests() != 0) {
        fprintf(stderr, "Failed to add AGType suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (init_schema_suite() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add schema suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    if (init_executor_basic_suite() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add executor basic suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    if (init_executor_relationships_suite() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add executor relationships suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    if (init_executor_set_suite() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add executor SET suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    if (init_executor_delete_suite() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add executor DELETE suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (init_executor_varlen_suite() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add executor varlen suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (init_executor_with_suite() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add executor WITH suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (init_executor_unwind_suite() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add executor UNWIND suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (init_executor_merge_suite() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add executor MERGE suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (init_executor_pagerank_suite() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add executor PageRank suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (init_executor_label_propagation_suite() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add executor Label Propagation suite\n");
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (init_output_format_suite() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add output format suite\n");
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