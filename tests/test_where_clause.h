#ifndef TEST_WHERE_CLAUSE_H
#define TEST_WHERE_CLAUSE_H

// Test suite registration function
int add_where_clause_tests(void);

// Setup and teardown functions
int setup_where_tests(void);
int teardown_where_tests(void);

// WHERE Clause Comparison Operator Tests
void test_where_equality_integer(void);
void test_where_equality_string(void);
void test_where_equality_boolean(void);
void test_where_greater_than(void);
void test_where_less_than(void);
void test_where_greater_equal(void);
void test_where_less_equal(void);
void test_where_not_equal(void);

// WHERE Clause with Different Property Types
void test_where_with_float_properties(void);
void test_where_with_mixed_property_types(void);

// WHERE Clause Edge Cases and Error Handling
void test_where_with_nonexistent_property(void);
void test_where_type_mismatch(void);
void test_where_boundary_values(void);

// WHERE Clause Data Validation Tests
void test_where_data_integrity(void);
void test_where_comprehensive_age_ranges(void);

#endif // TEST_WHERE_CLAUSE_H