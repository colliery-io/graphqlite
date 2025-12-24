#ifndef CYPHER_TOKENS_H
#define CYPHER_TOKENS_H

/* Token values for Cypher keywords and operators */
enum CypherTokens {
    /* Special tokens */
    END_OF_INPUT = 0,
    
    /* Keywords - values from 258 onwards to avoid conflicts with single chars */
    ALL = 258,
    ALLSHORTESTPATHS,
    ANALYZE,
    AND,
    ANY,
    AS,
    ASC,
    ASCENDING,
    BY,
    CALL,
    CASE,
    COALESCE,
    CONTAINS,
    COUNT,
    CREATE,
    DELETE,
    DESC,
    DESCENDING,
    DETACH,
    DISTINCT,
    ELSE,
    END_P,
    ENDS,
    EXISTS,
    EXPLAIN,
    CSV,
    FALSE_P,
    FIELDTERMINATOR,
    FOREACH,
    FROM,
    HEADERS,
    IN,
    IS,
    LOAD,
    LIMIT,
    MATCH,
    MERGE,
    NONE,
    NOT,
    NULL_P,
    ON,
    OPERATOR,
    OPTIONAL,
    OR,
    ORDER,
    PATTERN,
    REDUCE,
    REMOVE,
    RETURN,
    SET,
    SHORTESTPATH,
    SINGLE,
    SKIP,
    STARTS,
    THEN,
    TRUE_P,
    UNION,
    UNWIND,
    VERBOSE,
    WHEN,
    WHERE,
    WITH,
    XOR,
    YIELD,
    
    /* Literals and identifiers */
    INTEGER,
    DECIMAL,
    STRING,
    IDENTIFIER,
    PARAMETER,
    BQIDENT,
    
    /* Multi-character operators */
    NOT_EQ,      /* != or <> */
    LT_EQ,       /* <= */
    GT_EQ,       /* >= */
    DOT_DOT,     /* .. */
    TYPECAST,    /* :: */
    PLUS_EQ,     /* += */
    REGEX_MATCH, /* =~ */
    
    /* Generic operator */
    OP
};

#endif /* CYPHER_TOKENS_H */