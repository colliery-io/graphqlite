#ifndef CYPHER_KEYWORDS_H
#define CYPHER_KEYWORDS_H

#include <stdint.h>

/* Keyword categories */
typedef enum CypherKeywordCategory {
    UNRESERVED_KEYWORD = 0,
    COL_NAME_KEYWORD = 1,
    TYPE_FUNC_NAME_KEYWORD = 2,
    RESERVED_KEYWORD = 3
} CypherKeywordCategory;

/* Keyword entry structure */
typedef struct CypherKeyword {
    const char *name;           /* keyword text */
    int16_t token;             /* token value */
    int16_t category;          /* keyword category */
} CypherKeyword;

/* Keyword lookup result */
typedef struct CypherKeywordToken {
    const char *keyword;        /* pointer to keyword string */
    int token;                 /* token value */
    int category;              /* keyword category */
} CypherKeywordToken;

/* Global keyword table */
extern const CypherKeyword CypherKeywordTable[];
extern const int CypherKeywordCount;

/* Function prototypes */
int cypher_keyword_lookup(const char *str);
const CypherKeywordToken *cypher_keyword_lookup_full(const char *str);

#endif /* CYPHER_KEYWORDS_H */