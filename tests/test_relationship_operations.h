#ifndef TEST_RELATIONSHIP_OPERATIONS_H
#define TEST_RELATIONSHIP_OPERATIONS_H

// Test suite registration function
int add_relationship_operation_tests(void);

// Setup and teardown functions
int setup_relationship_tests(void);
int teardown_relationship_tests(void);

// Test functions for relationship operations
void test_relationship_match_return_node(void);
void test_relationship_match_return_relationship(void);
void test_bidirectional_relationship_match(void);
void test_relationship_match_with_properties(void);
void test_multiple_relationships(void);
void test_relationship_operations_limitations(void);

#endif // TEST_RELATIONSHIP_OPERATIONS_H