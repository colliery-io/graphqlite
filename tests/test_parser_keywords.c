#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "parser/cypher_keywords.h"
#include "parser/cypher_tokens.h"
#include "test_parser_keywords.h"

/* Test exact keyword matching */
static void test_exact_keyword_match(void)
{
    /* Test some common keywords */
    CU_ASSERT_EQUAL(cypher_keyword_lookup("match"), MATCH);
    CU_ASSERT_EQUAL(cypher_keyword_lookup("create"), CREATE);
    CU_ASSERT_EQUAL(cypher_keyword_lookup("where"), WHERE);
    CU_ASSERT_EQUAL(cypher_keyword_lookup("return"), RETURN);
    CU_ASSERT_EQUAL(cypher_keyword_lookup("with"), WITH);
    
    /* Test all keywords exist */
    CU_ASSERT_EQUAL(cypher_keyword_lookup("all"), ALL);
    CU_ASSERT_EQUAL(cypher_keyword_lookup("analyze"), ANALYZE);
    CU_ASSERT_EQUAL(cypher_keyword_lookup("and"), AND);
}

/* Test case-insensitive keyword matching */
static void test_case_insensitive_match(void)
{
    /* Upper case */
    CU_ASSERT_EQUAL(cypher_keyword_lookup("MATCH"), MATCH);
    CU_ASSERT_EQUAL(cypher_keyword_lookup("CREATE"), CREATE);
    CU_ASSERT_EQUAL(cypher_keyword_lookup("WHERE"), WHERE);
    
    /* Mixed case */
    CU_ASSERT_EQUAL(cypher_keyword_lookup("Match"), MATCH);
    CU_ASSERT_EQUAL(cypher_keyword_lookup("CrEaTe"), CREATE);
    CU_ASSERT_EQUAL(cypher_keyword_lookup("WhErE"), WHERE);
    
    /* All variations should return same token */
    CU_ASSERT_EQUAL(cypher_keyword_lookup("match"), cypher_keyword_lookup("MATCH"));
    CU_ASSERT_EQUAL(cypher_keyword_lookup("match"), cypher_keyword_lookup("Match"));
}

/* Test non-keywords return -1 */
static void test_non_keywords(void)
{
    CU_ASSERT_EQUAL(cypher_keyword_lookup("hello"), -1);
    CU_ASSERT_EQUAL(cypher_keyword_lookup("node"), -1);
    CU_ASSERT_EQUAL(cypher_keyword_lookup("property"), -1);
    CU_ASSERT_EQUAL(cypher_keyword_lookup("graph"), -1);
    CU_ASSERT_EQUAL(cypher_keyword_lookup(""), -1);
    CU_ASSERT_EQUAL(cypher_keyword_lookup("123"), -1);
}

/* Test keyword full lookup with category info */
static void test_keyword_full_lookup(void)
{
    const CypherKeywordToken *kwt;
    
    /* Test valid keyword */
    kwt = cypher_keyword_lookup_full("match");
    CU_ASSERT_PTR_NOT_NULL(kwt);
    if (kwt) {
        CU_ASSERT_STRING_EQUAL(kwt->keyword, "match");
        CU_ASSERT_EQUAL(kwt->token, MATCH);
        CU_ASSERT_EQUAL(kwt->category, RESERVED_KEYWORD);
    }
    
    /* Test case insensitive */
    kwt = cypher_keyword_lookup_full("CREATE");
    CU_ASSERT_PTR_NOT_NULL(kwt);
    if (kwt) {
        CU_ASSERT_STRING_EQUAL(kwt->keyword, "create");  /* Should return canonical form */
        CU_ASSERT_EQUAL(kwt->token, CREATE);
        CU_ASSERT_EQUAL(kwt->category, RESERVED_KEYWORD);
    }
    
    /* Test non-keyword */
    kwt = cypher_keyword_lookup_full("notakeyword");
    CU_ASSERT_PTR_NULL(kwt);
}

/* Test keyword table is properly sorted */
static void test_keyword_table_sorted(void)
{
    /* Verify table is sorted for binary search to work */
    for (int i = 1; i < CypherKeywordCount; i++) {
        CU_ASSERT(strcmp(CypherKeywordTable[i-1].name, CypherKeywordTable[i].name) < 0);
    }
}

/* Test all keywords have valid tokens */
static void test_all_keywords_valid(void)
{
    for (int i = 0; i < CypherKeywordCount; i++) {
        /* All keywords should have tokens >= 258 (after ASCII range) */
        CU_ASSERT(CypherKeywordTable[i].token >= 258);
        
        /* All keywords should have valid category */
        CU_ASSERT(CypherKeywordTable[i].category >= UNRESERVED_KEYWORD);
        CU_ASSERT(CypherKeywordTable[i].category <= RESERVED_KEYWORD);
        
        /* Keyword name should not be empty */
        CU_ASSERT(strlen(CypherKeywordTable[i].name) > 0);
    }
}

/* Initialize the test suite */
int init_parser_keywords_suite(void)
{
    CU_pSuite suite = CU_add_suite("Parser Keywords", NULL, NULL);
    if (!suite) {
        return CU_get_error();
    }
    
    /* Add tests to suite */
    if (!CU_add_test(suite, "Exact keyword matching", test_exact_keyword_match) ||
        !CU_add_test(suite, "Case-insensitive matching", test_case_insensitive_match) ||
        !CU_add_test(suite, "Non-keywords return -1", test_non_keywords) ||
        !CU_add_test(suite, "Full keyword lookup", test_keyword_full_lookup) ||
        !CU_add_test(suite, "Keyword table is sorted", test_keyword_table_sorted) ||
        !CU_add_test(suite, "All keywords are valid", test_all_keywords_valid))
    {
        return CU_get_error();
    }
    
    return CUE_SUCCESS;
}