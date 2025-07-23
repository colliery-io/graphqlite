#ifndef TEST_MINIMAL_EXECUTOR_H
#define TEST_MINIMAL_EXECUTOR_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Add minimal executor test suite to CUnit registry
 * @return 0 on success, -1 on failure
 */
int add_minimal_executor_tests(void);

#ifdef __cplusplus
}
#endif

#endif // TEST_MINIMAL_EXECUTOR_H