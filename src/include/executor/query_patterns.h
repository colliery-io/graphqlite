/*
 * query_patterns.h
 *    Table-driven query pattern dispatch for Cypher execution
 *
 * OVERVIEW
 * --------
 * Replaces the 500+ line if-else chain in cypher_executor.c with a declarative
 * pattern registry. Queries are matched by analyzing which clauses are present
 * and finding the highest-priority pattern that matches.
 *
 * SUPPORTED PATTERNS (in priority order)
 * --------------------------------------
 * Priority 100: UNWIND+CREATE, WITH+MATCH+RETURN, MATCH+CREATE+RETURN
 * Priority 90:  MATCH+SET, MATCH+DELETE, MATCH+REMOVE, MATCH+MERGE, MATCH+CREATE
 * Priority 80:  OPTIONAL_MATCH+RETURN, MULTI_MATCH+RETURN
 * Priority 70:  MATCH+RETURN (simple)
 * Priority 60:  UNWIND+RETURN
 * Priority 50:  CREATE, MERGE, SET, FOREACH
 * Priority 40:  MATCH (no RETURN)
 * Priority 10:  RETURN (standalone, including graph algorithms)
 * Priority 0:   GENERIC (fallback for any query)
 *
 * ADDING NEW PATTERNS
 * -------------------
 * 1. Add entry to the patterns[] array in query_dispatch.c
 * 2. Set required/forbidden clause flags
 * 3. Choose priority (higher = matched first)
 * 4. Implement handler function or use handle_generic_transform
 * 5. Add tests to test_query_dispatch.c
 *
 * PATTERN MATCHING RULES
 * ----------------------
 * - All 'required' clauses must be present
 * - No 'forbidden' clauses may be present
 * - Higher priority patterns are checked first
 * - First matching pattern wins
 * - GENERIC pattern (priority 0) catches anything not matched
 *
 * DEBUG OUTPUT
 * ------------
 * With GRAPHQLITE_DEBUG defined, pattern matching logs:
 *   "Query clauses: MATCH|RETURN"
 *   "Matched pattern: MATCH+RETURN (priority 70)"
 *
 * Use EXPLAIN prefix to see pattern info without executing:
 *   EXPLAIN MATCH (n) RETURN n
 */

#ifndef QUERY_PATTERNS_H
#define QUERY_PATTERNS_H

#include "parser/cypher_ast.h"
#include "executor/cypher_executor.h"

/*
 * Clause presence flags - bitmask for query analysis
 */
typedef enum {
    CLAUSE_NONE        = 0,
    CLAUSE_MATCH       = 1 << 0,
    CLAUSE_OPTIONAL    = 1 << 1,   /* Has OPTIONAL MATCH */
    CLAUSE_MULTI_MATCH = 1 << 2,   /* Has multiple MATCH clauses */
    CLAUSE_RETURN      = 1 << 3,
    CLAUSE_CREATE      = 1 << 4,
    CLAUSE_MERGE       = 1 << 5,
    CLAUSE_SET         = 1 << 6,
    CLAUSE_DELETE      = 1 << 7,
    CLAUSE_REMOVE      = 1 << 8,
    CLAUSE_WITH        = 1 << 9,
    CLAUSE_UNWIND      = 1 << 10,
    CLAUSE_FOREACH     = 1 << 11,
    CLAUSE_UNION       = 1 << 12,
    CLAUSE_CALL        = 1 << 13,
    CLAUSE_LOAD_CSV    = 1 << 14,
    CLAUSE_EXPLAIN     = 1 << 15,
} clause_flags;

/*
 * Pattern handler function signature
 *
 * executor: The executor instance
 * query: The parsed query
 * result: Output result structure
 * flags: The clause flags that matched this pattern
 *
 * Returns 0 on success, -1 on error (error set in result)
 */
typedef int (*pattern_handler)(
    cypher_executor *executor,
    cypher_query *query,
    cypher_result *result,
    clause_flags flags
);

/*
 * Query pattern definition
 *
 * Patterns are matched in priority order (highest first).
 * A pattern matches if:
 *   1. All required clauses are present
 *   2. No forbidden clauses are present
 */
typedef struct {
    const char *name;           /* Pattern name for debugging */
    clause_flags required;      /* Must have all these clauses */
    clause_flags forbidden;     /* Must NOT have any of these */
    pattern_handler handler;    /* Function to execute this pattern */
    int priority;               /* Higher = checked first (0-100) */
} query_pattern;

/*
 * Analyze a query to determine which clauses are present.
 * Returns a bitmask of clause_flags.
 */
clause_flags analyze_query_clauses(cypher_query *query);

/*
 * Find the best matching pattern for the given clause flags.
 * Returns NULL if no pattern matches.
 */
const query_pattern *find_matching_pattern(clause_flags present);

/*
 * Get the pattern registry (for testing/debugging).
 * Returns pointer to static pattern array, terminated by NULL handler.
 */
const query_pattern *get_pattern_registry(void);

/*
 * Convert clause flags to a human-readable string.
 * Returns a static buffer - not thread-safe, for debugging only.
 */
const char *clause_flags_to_string(clause_flags flags);

/*
 * Main dispatch function - replaces the if-else chain.
 * Analyzes query, finds matching pattern, and executes handler.
 */
int dispatch_query_pattern(
    cypher_executor *executor,
    cypher_query *query,
    cypher_result *result
);

#endif /* QUERY_PATTERNS_H */
