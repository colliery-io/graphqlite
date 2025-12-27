/*
 * test_query_dispatch.c
 *    Unit tests for query pattern dispatch infrastructure
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "executor/query_patterns.h"

/*
 * Test analyze_query_clauses with NULL query
 */
static void test_analyze_null_query(void)
{
    clause_flags flags = analyze_query_clauses(NULL);
    CU_ASSERT_EQUAL(flags, CLAUSE_NONE);
}

/*
 * Test clause_flags_to_string
 */
static void test_flags_to_string_none(void)
{
    const char *str = clause_flags_to_string(CLAUSE_NONE);
    CU_ASSERT_PTR_NOT_NULL(str);
    CU_ASSERT_STRING_EQUAL(str, "(none)");
}

static void test_flags_to_string_single(void)
{
    const char *str = clause_flags_to_string(CLAUSE_MATCH);
    CU_ASSERT_PTR_NOT_NULL(str);
    CU_ASSERT_STRING_EQUAL(str, "MATCH");
}

static void test_flags_to_string_multiple(void)
{
    const char *str = clause_flags_to_string(CLAUSE_MATCH | CLAUSE_RETURN);
    CU_ASSERT_PTR_NOT_NULL(str);
    CU_ASSERT(strstr(str, "MATCH") != NULL);
    CU_ASSERT(strstr(str, "RETURN") != NULL);
}

static void test_flags_to_string_all_flags(void)
{
    clause_flags all = CLAUSE_MATCH | CLAUSE_OPTIONAL | CLAUSE_MULTI_MATCH |
                       CLAUSE_RETURN | CLAUSE_CREATE | CLAUSE_MERGE |
                       CLAUSE_SET | CLAUSE_DELETE | CLAUSE_REMOVE |
                       CLAUSE_WITH | CLAUSE_UNWIND | CLAUSE_FOREACH;
    const char *str = clause_flags_to_string(all);
    CU_ASSERT_PTR_NOT_NULL(str);
    CU_ASSERT(strstr(str, "MATCH") != NULL);
    CU_ASSERT(strstr(str, "RETURN") != NULL);
    CU_ASSERT(strstr(str, "CREATE") != NULL);
}

/*
 * Test find_matching_pattern
 */
static void test_find_pattern_match_return(void)
{
    /* MATCH+RETURN should match the specific pattern, not GENERIC */
    const query_pattern *p = find_matching_pattern(CLAUSE_MATCH | CLAUSE_RETURN);
    CU_ASSERT_PTR_NOT_NULL(p);
    if (p) {
        CU_ASSERT_STRING_EQUAL(p->name, "MATCH+RETURN");
    }
}

static void test_find_pattern_empty_flags(void)
{
    /* Empty flags should match generic pattern */
    const query_pattern *p = find_matching_pattern(CLAUSE_NONE);
    CU_ASSERT_PTR_NOT_NULL(p);
    if (p) {
        CU_ASSERT_STRING_EQUAL(p->name, "GENERIC");
    }
}

/*
 * Test get_pattern_registry
 */
static void test_get_registry(void)
{
    const query_pattern *registry = get_pattern_registry();
    CU_ASSERT_PTR_NOT_NULL(registry);

    /* First pattern should be highest priority */
    CU_ASSERT_PTR_NOT_NULL(registry[0].handler);
    CU_ASSERT_EQUAL(registry[0].priority, 100);

    /* Find GENERIC pattern (sentinel is NULL handler) */
    int found_generic = 0;
    for (int i = 0; registry[i].handler != NULL; i++) {
        if (strcmp(registry[i].name, "GENERIC") == 0) {
            found_generic = 1;
            CU_ASSERT_EQUAL(registry[i].priority, 0);
            break;
        }
    }
    CU_ASSERT_TRUE(found_generic);
}

/*
 * Test pattern matching with specific patterns
 */
static void test_pattern_match_set(void)
{
    const query_pattern *p = find_matching_pattern(CLAUSE_MATCH | CLAUSE_SET);
    CU_ASSERT_PTR_NOT_NULL(p);
    if (p) {
        CU_ASSERT_STRING_EQUAL(p->name, "MATCH+SET");
        CU_ASSERT_EQUAL(p->priority, 90);
    }
}

static void test_pattern_match_delete(void)
{
    const query_pattern *p = find_matching_pattern(CLAUSE_MATCH | CLAUSE_DELETE);
    CU_ASSERT_PTR_NOT_NULL(p);
    if (p) {
        CU_ASSERT_STRING_EQUAL(p->name, "MATCH+DELETE");
        CU_ASSERT_EQUAL(p->priority, 90);
    }
}

static void test_pattern_create_only(void)
{
    const query_pattern *p = find_matching_pattern(CLAUSE_CREATE);
    CU_ASSERT_PTR_NOT_NULL(p);
    if (p) {
        CU_ASSERT_STRING_EQUAL(p->name, "CREATE");
        CU_ASSERT_EQUAL(p->priority, 50);
    }
}

static void test_pattern_unwind_create(void)
{
    const query_pattern *p = find_matching_pattern(CLAUSE_UNWIND | CLAUSE_CREATE);
    CU_ASSERT_PTR_NOT_NULL(p);
    if (p) {
        CU_ASSERT_STRING_EQUAL(p->name, "UNWIND+CREATE");
        CU_ASSERT_EQUAL(p->priority, 100);
    }
}

static void test_pattern_optional_match(void)
{
    const query_pattern *p = find_matching_pattern(CLAUSE_MATCH | CLAUSE_OPTIONAL | CLAUSE_RETURN);
    CU_ASSERT_PTR_NOT_NULL(p);
    if (p) {
        CU_ASSERT_STRING_EQUAL(p->name, "OPTIONAL_MATCH+RETURN");
        CU_ASSERT_EQUAL(p->priority, 80);
    }
}

static void test_pattern_priority_ordering(void)
{
    /* Verify that higher-priority patterns win when multiple could match */

    /* MATCH+SET+RETURN: Both MATCH+SET (90) and MATCH+RETURN (70) match requirements
     * but MATCH+SET has no RETURN in forbidden, so it wins at priority 90 */
    const query_pattern *p = find_matching_pattern(CLAUSE_MATCH | CLAUSE_SET | CLAUSE_RETURN);
    CU_ASSERT_PTR_NOT_NULL(p);
    if (p) {
        CU_ASSERT_EQUAL(p->priority, 90);
    }

    /* WITH+MATCH+RETURN should match the specific pattern at priority 100 */
    p = find_matching_pattern(CLAUSE_WITH | CLAUSE_MATCH | CLAUSE_RETURN);
    CU_ASSERT_PTR_NOT_NULL(p);
    if (p) {
        CU_ASSERT_STRING_EQUAL(p->name, "WITH+MATCH+RETURN");
        CU_ASSERT_EQUAL(p->priority, 100);
    }
}

/*
 * Test clause flag bit values are unique powers of 2
 */
static void test_clause_flags_unique(void)
{
    /* Verify each flag is a unique power of 2 */
    CU_ASSERT_EQUAL(CLAUSE_MATCH, 1 << 0);
    CU_ASSERT_EQUAL(CLAUSE_OPTIONAL, 1 << 1);
    CU_ASSERT_EQUAL(CLAUSE_MULTI_MATCH, 1 << 2);
    CU_ASSERT_EQUAL(CLAUSE_RETURN, 1 << 3);
    CU_ASSERT_EQUAL(CLAUSE_CREATE, 1 << 4);
    CU_ASSERT_EQUAL(CLAUSE_MERGE, 1 << 5);
    CU_ASSERT_EQUAL(CLAUSE_SET, 1 << 6);
    CU_ASSERT_EQUAL(CLAUSE_DELETE, 1 << 7);
    CU_ASSERT_EQUAL(CLAUSE_REMOVE, 1 << 8);
    CU_ASSERT_EQUAL(CLAUSE_WITH, 1 << 9);
    CU_ASSERT_EQUAL(CLAUSE_UNWIND, 1 << 10);
    CU_ASSERT_EQUAL(CLAUSE_FOREACH, 1 << 11);
}

/*
 * Test flag combination operations
 */
static void test_flag_operations(void)
{
    clause_flags a = CLAUSE_MATCH | CLAUSE_RETURN;
    clause_flags b = CLAUSE_CREATE;

    /* OR combines flags */
    clause_flags combined = a | b;
    CU_ASSERT(combined & CLAUSE_MATCH);
    CU_ASSERT(combined & CLAUSE_RETURN);
    CU_ASSERT(combined & CLAUSE_CREATE);
    CU_ASSERT(!(combined & CLAUSE_DELETE));

    /* AND checks presence */
    CU_ASSERT((combined & CLAUSE_MATCH) == CLAUSE_MATCH);
    CU_ASSERT((combined & CLAUSE_DELETE) == 0);
}

/* Register test suite */
int init_query_dispatch_suite(void)
{
    CU_pSuite suite = CU_add_suite("Query Dispatch", NULL, NULL);
    if (!suite) return -1;

    if (!CU_add_test(suite, "analyze: NULL query", test_analyze_null_query)) return -1;
    if (!CU_add_test(suite, "flags_to_string: none", test_flags_to_string_none)) return -1;
    if (!CU_add_test(suite, "flags_to_string: single", test_flags_to_string_single)) return -1;
    if (!CU_add_test(suite, "flags_to_string: multiple", test_flags_to_string_multiple)) return -1;
    if (!CU_add_test(suite, "flags_to_string: all flags", test_flags_to_string_all_flags)) return -1;
    if (!CU_add_test(suite, "find_pattern: MATCH+RETURN", test_find_pattern_match_return)) return -1;
    if (!CU_add_test(suite, "find_pattern: empty flags", test_find_pattern_empty_flags)) return -1;
    if (!CU_add_test(suite, "get_registry", test_get_registry)) return -1;
    if (!CU_add_test(suite, "pattern: MATCH+SET", test_pattern_match_set)) return -1;
    if (!CU_add_test(suite, "pattern: MATCH+DELETE", test_pattern_match_delete)) return -1;
    if (!CU_add_test(suite, "pattern: CREATE only", test_pattern_create_only)) return -1;
    if (!CU_add_test(suite, "pattern: UNWIND+CREATE", test_pattern_unwind_create)) return -1;
    if (!CU_add_test(suite, "pattern: OPTIONAL MATCH", test_pattern_optional_match)) return -1;
    if (!CU_add_test(suite, "pattern: priority ordering", test_pattern_priority_ordering)) return -1;
    if (!CU_add_test(suite, "flags: unique values", test_clause_flags_unique)) return -1;
    if (!CU_add_test(suite, "flags: operations", test_flag_operations)) return -1;

    return 0;
}
