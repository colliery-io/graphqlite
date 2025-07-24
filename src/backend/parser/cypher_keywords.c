#include <string.h>
#include <ctype.h>
#include "parser/cypher_keywords.h"
#include "parser/cypher_tokens.h"

/* Define KEYWORD macro to build the keyword table */
#define KEYWORD(name, token, category) {name, token, category},

/* The keyword table - must be sorted alphabetically for binary search */
const CypherKeyword CypherKeywordTable[] = {
#include "parser/cypher_kwlist.h"
};

#undef KEYWORD

/* Number of keywords */
const int CypherKeywordCount = sizeof(CypherKeywordTable) / sizeof(CypherKeyword);

/* Case-insensitive string comparison for keywords */
static int keyword_cmp(const char *s1, const char *s2)
{
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2)
            return c1 - c2;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

/* Binary search for keyword lookup */
static const CypherKeyword *binary_search_keyword(const char *str)
{
    int low = 0;
    int high = CypherKeywordCount - 1;
    
    while (low <= high) {
        int mid = low + (high - low) / 2;
        int cmp = keyword_cmp(str, CypherKeywordTable[mid].name);
        
        if (cmp == 0)
            return &CypherKeywordTable[mid];
        else if (cmp < 0)
            high = mid - 1;
        else
            low = mid + 1;
    }
    
    return NULL;
}

/* Look up a keyword and return its token, or -1 if not found */
int cypher_keyword_lookup(const char *str)
{
    const CypherKeyword *kw = binary_search_keyword(str);
    return kw ? kw->token : -1;
}

/* Look up a keyword and return full information */
const CypherKeywordToken *cypher_keyword_lookup_full(const char *str)
{
    static CypherKeywordToken result;
    const CypherKeyword *kw = binary_search_keyword(str);
    
    if (kw) {
        result.keyword = kw->name;
        result.token = kw->token;
        result.category = kw->category;
        return &result;
    }
    
    return NULL;
}