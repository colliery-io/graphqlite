#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "executor/agtype.h"

/* Test creating null value */
static void test_agtype_null(void)
{
    agtype_value *val = agtype_value_create_null();
    CU_ASSERT_PTR_NOT_NULL(val);
    if (val) {
        CU_ASSERT_EQUAL(val->type, AGTV_NULL);
        agtype_value_free(val);
    }
}

/* Test creating integer value */
static void test_agtype_integer(void)
{
    agtype_value *val = agtype_value_create_integer(42);
    CU_ASSERT_PTR_NOT_NULL(val);
    if (val) {
        CU_ASSERT_EQUAL(val->type, AGTV_INTEGER);
        CU_ASSERT_EQUAL(val->val.int_value, 42);
        agtype_value_free(val);
    }
}

/* Test creating negative integer value */
static void test_agtype_negative_integer(void)
{
    agtype_value *val = agtype_value_create_integer(-100);
    CU_ASSERT_PTR_NOT_NULL(val);
    if (val) {
        CU_ASSERT_EQUAL(val->type, AGTV_INTEGER);
        CU_ASSERT_EQUAL(val->val.int_value, -100);
        agtype_value_free(val);
    }
}

/* Test creating float value */
static void test_agtype_float(void)
{
    agtype_value *val = agtype_value_create_float(3.14159);
    CU_ASSERT_PTR_NOT_NULL(val);
    if (val) {
        CU_ASSERT_EQUAL(val->type, AGTV_FLOAT);
        CU_ASSERT_DOUBLE_EQUAL(val->val.float_value, 3.14159, 0.00001);
        agtype_value_free(val);
    }
}

/* Test creating negative float value */
static void test_agtype_negative_float(void)
{
    agtype_value *val = agtype_value_create_float(-2.5);
    CU_ASSERT_PTR_NOT_NULL(val);
    if (val) {
        CU_ASSERT_EQUAL(val->type, AGTV_FLOAT);
        CU_ASSERT_DOUBLE_EQUAL(val->val.float_value, -2.5, 0.00001);
        agtype_value_free(val);
    }
}

/* Test creating string value */
static void test_agtype_string(void)
{
    agtype_value *val = agtype_value_create_string("hello");
    CU_ASSERT_PTR_NOT_NULL(val);
    if (val) {
        CU_ASSERT_EQUAL(val->type, AGTV_STRING);
        CU_ASSERT_STRING_EQUAL(val->val.string.val, "hello");
        CU_ASSERT_EQUAL(val->val.string.len, 5);
        agtype_value_free(val);
    }
}

/* Test creating empty string value */
static void test_agtype_empty_string(void)
{
    agtype_value *val = agtype_value_create_string("");
    CU_ASSERT_PTR_NOT_NULL(val);
    if (val) {
        CU_ASSERT_EQUAL(val->type, AGTV_STRING);
        CU_ASSERT_STRING_EQUAL(val->val.string.val, "");
        CU_ASSERT_EQUAL(val->val.string.len, 0);
        agtype_value_free(val);
    }
}

/* Test creating boolean true value */
static void test_agtype_bool_true(void)
{
    agtype_value *val = agtype_value_create_bool(true);
    CU_ASSERT_PTR_NOT_NULL(val);
    if (val) {
        CU_ASSERT_EQUAL(val->type, AGTV_BOOL);
        CU_ASSERT_TRUE(val->val.boolean);
        agtype_value_free(val);
    }
}

/* Test creating boolean false value */
static void test_agtype_bool_false(void)
{
    agtype_value *val = agtype_value_create_bool(false);
    CU_ASSERT_PTR_NOT_NULL(val);
    if (val) {
        CU_ASSERT_EQUAL(val->type, AGTV_BOOL);
        CU_ASSERT_FALSE(val->val.boolean);
        agtype_value_free(val);
    }
}

/* Test creating vertex value */
static void test_agtype_vertex(void)
{
    agtype_value *val = agtype_value_create_vertex(1, "Person");
    CU_ASSERT_PTR_NOT_NULL(val);
    if (val) {
        CU_ASSERT_EQUAL(val->type, AGTV_VERTEX);
        CU_ASSERT_EQUAL(val->val.entity.id, 1);
        CU_ASSERT_STRING_EQUAL(val->val.entity.label, "Person");
        agtype_value_free(val);
    }
}

/* Test creating edge value */
static void test_agtype_edge(void)
{
    agtype_value *val = agtype_value_create_edge(1, "KNOWS", 10, 20);
    CU_ASSERT_PTR_NOT_NULL(val);
    if (val) {
        CU_ASSERT_EQUAL(val->type, AGTV_EDGE);
        CU_ASSERT_EQUAL(val->val.edge.id, 1);
        CU_ASSERT_EQUAL(val->val.edge.start_id, 10);
        CU_ASSERT_EQUAL(val->val.edge.end_id, 20);
        CU_ASSERT_STRING_EQUAL(val->val.edge.label, "KNOWS");
        agtype_value_free(val);
    }
}

/* Test creating path value */
static void test_agtype_path(void)
{
    /* Create a simple path: vertex -> edge -> vertex */
    agtype_value **elements = malloc(3 * sizeof(agtype_value*));
    elements[0] = agtype_value_create_vertex(1, "Person");
    elements[1] = agtype_value_create_edge(1, "KNOWS", 1, 2);
    elements[2] = agtype_value_create_vertex(2, "Person");

    agtype_value *val = agtype_value_create_path(elements, 3);
    CU_ASSERT_PTR_NOT_NULL(val);
    if (val) {
        CU_ASSERT_EQUAL(val->type, AGTV_PATH);
        CU_ASSERT_EQUAL(val->val.array.num_elems, 3);
        agtype_value_free(val);
    }

    agtype_value_free(elements[0]);
    agtype_value_free(elements[1]);
    agtype_value_free(elements[2]);
    free(elements);
}

/* Test creating path with more elements */
static void test_agtype_path_long(void)
{
    /* Create path: v1 -> e1 -> v2 -> e2 -> v3 */
    agtype_value **elements = malloc(5 * sizeof(agtype_value*));
    elements[0] = agtype_value_create_vertex(1, "A");
    elements[1] = agtype_value_create_edge(1, "R1", 1, 2);
    elements[2] = agtype_value_create_vertex(2, "B");
    elements[3] = agtype_value_create_edge(2, "R2", 2, 3);
    elements[4] = agtype_value_create_vertex(3, "C");

    agtype_value *val = agtype_value_create_path(elements, 5);
    CU_ASSERT_PTR_NOT_NULL(val);
    if (val) {
        CU_ASSERT_EQUAL(val->type, AGTV_PATH);
        CU_ASSERT_EQUAL(val->val.array.num_elems, 5);
        agtype_value_free(val);
    }

    for (int i = 0; i < 5; i++) {
        agtype_value_free(elements[i]);
    }
    free(elements);
}

/* Test creating invalid path (wrong structure) */
static void test_agtype_path_invalid(void)
{
    /* Try to create path starting with edge (invalid) */
    agtype_value **elements = malloc(3 * sizeof(agtype_value*));
    elements[0] = agtype_value_create_edge(1, "KNOWS", 1, 2);
    elements[1] = agtype_value_create_vertex(1, "Person");
    elements[2] = agtype_value_create_vertex(2, "Person");

    agtype_value *val = agtype_value_create_path(elements, 3);
    CU_ASSERT_PTR_NULL(val); /* Should fail - starts with edge */

    agtype_value_free(elements[0]);
    agtype_value_free(elements[1]);
    agtype_value_free(elements[2]);
    free(elements);
}

/* Test value to string - null */
static void test_agtype_to_string_null(void)
{
    agtype_value *val = agtype_value_create_null();
    char *str = agtype_value_to_string(val);
    CU_ASSERT_PTR_NOT_NULL(str);
    if (str) {
        CU_ASSERT_STRING_EQUAL(str, "null");
        free(str);
    }
    agtype_value_free(val);
}

/* Test value to string - integer */
static void test_agtype_to_string_integer(void)
{
    agtype_value *val = agtype_value_create_integer(42);
    char *str = agtype_value_to_string(val);
    CU_ASSERT_PTR_NOT_NULL(str);
    if (str) {
        CU_ASSERT_STRING_EQUAL(str, "42");
        free(str);
    }
    agtype_value_free(val);
}

/* Test value to string - float */
static void test_agtype_to_string_float(void)
{
    agtype_value *val = agtype_value_create_float(3.5);
    char *str = agtype_value_to_string(val);
    CU_ASSERT_PTR_NOT_NULL(str);
    /* Float formatting may vary */
    CU_ASSERT_PTR_NOT_NULL(strstr(str, "3.5"));
    if (str) free(str);
    agtype_value_free(val);
}

/* Test value to string - string */
static void test_agtype_to_string_string(void)
{
    agtype_value *val = agtype_value_create_string("hello");
    char *str = agtype_value_to_string(val);
    CU_ASSERT_PTR_NOT_NULL(str);
    if (str) {
        CU_ASSERT(strstr(str, "hello") != NULL);
        free(str);
    }
    agtype_value_free(val);
}

/* Test value to string - boolean true */
static void test_agtype_to_string_bool_true(void)
{
    agtype_value *val = agtype_value_create_bool(true);
    char *str = agtype_value_to_string(val);
    CU_ASSERT_PTR_NOT_NULL(str);
    if (str) {
        CU_ASSERT_STRING_EQUAL(str, "true");
        free(str);
    }
    agtype_value_free(val);
}

/* Test value to string - boolean false */
static void test_agtype_to_string_bool_false(void)
{
    agtype_value *val = agtype_value_create_bool(false);
    char *str = agtype_value_to_string(val);
    CU_ASSERT_PTR_NOT_NULL(str);
    if (str) {
        CU_ASSERT_STRING_EQUAL(str, "false");
        free(str);
    }
    agtype_value_free(val);
}

/* Test value to string - vertex */
static void test_agtype_to_string_vertex(void)
{
    agtype_value *val = agtype_value_create_vertex(1, "Person");
    char *str = agtype_value_to_string(val);
    CU_ASSERT_PTR_NOT_NULL(str);
    if (str) {
        CU_ASSERT(strstr(str, "vertex") != NULL || strstr(str, "Person") != NULL);
        free(str);
    }
    agtype_value_free(val);
}

/* Test value to string - edge */
static void test_agtype_to_string_edge(void)
{
    agtype_value *val = agtype_value_create_edge(1, "KNOWS", 10, 20);
    char *str = agtype_value_to_string(val);
    CU_ASSERT_PTR_NOT_NULL(str);
    if (str) {
        CU_ASSERT(strstr(str, "edge") != NULL || strstr(str, "KNOWS") != NULL);
        free(str);
    }
    agtype_value_free(val);
}

/* Test NULL handling */
static void test_agtype_null_handling(void)
{
    /* These should handle NULL gracefully */
    agtype_value_free(NULL);  /* Should not crash */

    char *str = agtype_value_to_string(NULL);
    /* Implementation may return NULL or a string - just verify it doesn't crash */
    if (str) {
        free(str);
    }
}

/* Test large integer */
static void test_agtype_large_integer(void)
{
    agtype_value *val = agtype_value_create_integer(9223372036854775807LL); /* Max int64 */
    CU_ASSERT_PTR_NOT_NULL(val);
    if (val) {
        CU_ASSERT_EQUAL(val->type, AGTV_INTEGER);
        CU_ASSERT_EQUAL(val->val.int_value, 9223372036854775807LL);
        agtype_value_free(val);
    }
}

/* Test string with special characters */
static void test_agtype_string_special(void)
{
    agtype_value *val = agtype_value_create_string("hello\nworld\ttab\"quote");
    CU_ASSERT_PTR_NOT_NULL(val);
    if (val) {
        CU_ASSERT_EQUAL(val->type, AGTV_STRING);
        agtype_value_free(val);
    }
}

/* Register test suite */
int register_agtype_tests(void)
{
    CU_pSuite suite = CU_add_suite("AGType", NULL, NULL);
    if (!suite) return -1;

    if (!CU_add_test(suite, "Create null", test_agtype_null)) return -1;
    if (!CU_add_test(suite, "Create integer", test_agtype_integer)) return -1;
    if (!CU_add_test(suite, "Create negative integer", test_agtype_negative_integer)) return -1;
    if (!CU_add_test(suite, "Create float", test_agtype_float)) return -1;
    if (!CU_add_test(suite, "Create negative float", test_agtype_negative_float)) return -1;
    if (!CU_add_test(suite, "Create string", test_agtype_string)) return -1;
    if (!CU_add_test(suite, "Create empty string", test_agtype_empty_string)) return -1;
    if (!CU_add_test(suite, "Create bool true", test_agtype_bool_true)) return -1;
    if (!CU_add_test(suite, "Create bool false", test_agtype_bool_false)) return -1;
    if (!CU_add_test(suite, "Create vertex", test_agtype_vertex)) return -1;
    if (!CU_add_test(suite, "Create edge", test_agtype_edge)) return -1;
    if (!CU_add_test(suite, "Create path", test_agtype_path)) return -1;
    if (!CU_add_test(suite, "Create long path", test_agtype_path_long)) return -1;
    if (!CU_add_test(suite, "Create invalid path", test_agtype_path_invalid)) return -1;
    if (!CU_add_test(suite, "To string - null", test_agtype_to_string_null)) return -1;
    if (!CU_add_test(suite, "To string - integer", test_agtype_to_string_integer)) return -1;
    if (!CU_add_test(suite, "To string - float", test_agtype_to_string_float)) return -1;
    if (!CU_add_test(suite, "To string - string", test_agtype_to_string_string)) return -1;
    if (!CU_add_test(suite, "To string - bool true", test_agtype_to_string_bool_true)) return -1;
    if (!CU_add_test(suite, "To string - bool false", test_agtype_to_string_bool_false)) return -1;
    if (!CU_add_test(suite, "To string - vertex", test_agtype_to_string_vertex)) return -1;
    if (!CU_add_test(suite, "To string - edge", test_agtype_to_string_edge)) return -1;
    if (!CU_add_test(suite, "NULL handling", test_agtype_null_handling)) return -1;
    if (!CU_add_test(suite, "Large integer", test_agtype_large_integer)) return -1;
    if (!CU_add_test(suite, "String with special chars", test_agtype_string_special)) return -1;

    return 0;
}
