---
id: where-clause-and-expression-support
level: initiative
title: "WHERE Clause and Expression Support"
created_at: 2025-07-23T12:28:52.545393+00:00
updated_at: 2025-07-23T12:28:52.545393+00:00
parent: opencypher-implementation-strategy
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/discovery"


exit_criteria_met: false
estimated_complexity: M
---

# WHERE Clause and Expression Support Initiative

## Context

With basic CREATE/MATCH/RETURN operations complete, the next logical step is to add WHERE clause filtering capabilities. This will enable users to write more practical queries by filtering results based on property values and expressions.

**Current State**: 
- MATCH operations work but return all matching nodes/edges
- Property filtering only works inline: `MATCH (n:Person {name: "Alice"})`
- No support for complex conditions or expressions

**Target State**:
Enable WHERE clause filtering with comparison operators and basic expressions:

```cypher
# Basic property comparisons
MATCH (n:Person) WHERE n.age > 21 RETURN n
MATCH (n:Product) WHERE n.price <= 100.0 RETURN n
MATCH (n:Person) WHERE n.name = "Alice" RETURN n

# Boolean logic
MATCH (n:Person) WHERE n.age > 21 AND n.city = "Seattle" RETURN n
MATCH (n:Product) WHERE n.inStock = true OR n.featured = true RETURN n

# NULL checks
MATCH (n:Person) WHERE n.email IS NOT NULL RETURN n

# Pattern-based filtering
MATCH (a:Person)-[r:KNOWS]->(b:Person) WHERE r.since > 2020 RETURN a, b
```

This initiative focuses on implementing the WHERE clause parser support, expression evaluation engine, and integration with the existing MATCH execution pipeline.

## Goals & Non-Goals

**Goals:**
- Parse WHERE clause after MATCH statements
- Support comparison operators: =, <>, <, >, <=, >=
- Support boolean operators: AND, OR, NOT
- Support IS NULL / IS NOT NULL checks
- Evaluate expressions against node and edge properties
- Type-aware comparisons (integer, float, string, boolean)
- Filter results during MATCH execution
- Maintain performance with early filtering
- Add comprehensive test coverage

**Non-Goals:**
- Complex expressions (arithmetic, string manipulation)
- Function calls (e.g., lower(), substring())
- List operations and IN clauses
- Regular expression matching
- EXISTS subqueries
- Pattern predicates in WHERE
- CASE expressions
- Aggregation in WHERE (that requires HAVING)
- Type coercion (strict type matching only)

## Detailed Design

## Grammar Extensions

### 1. WHERE Clause Grammar (BISON)
```yacc
match_statement:
    MATCH pattern_list where_clause_opt RETURN return_list
    ;

where_clause_opt:
    /* empty */
    | WHERE expression
    ;

expression:
    comparison_expression
    | expression AND expression
    | expression OR expression
    | NOT expression
    | LPAREN expression RPAREN
    ;

comparison_expression:
    property_expression EQ property_expression
    | property_expression NEQ property_expression
    | property_expression LT property_expression
    | property_expression GT property_expression
    | property_expression LE property_expression
    | property_expression GE property_expression
    | property_expression IS NULL
    | property_expression IS NOT NULL
    ;

property_expression:
    IDENTIFIER                           // Variable reference
    | IDENTIFIER DOT IDENTIFIER          // Property access
    | literal                           // Direct values
    ;
```

### 2. Lexer Tokens (FLEX)
```flex
"WHERE"     { return WHERE; }
"AND"       { return AND; }
"OR"        { return OR; }
"NOT"       { return NOT; }
"IS"        { return IS; }
"NULL"      { return NULL_TOKEN; }
"<>"        { return NEQ; }
"<="        { return LE; }
">="        { return GE; }
```

### 3. AST Extensions
```c
typedef enum {
    // Existing...
    AST_WHERE_CLAUSE,
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,
    AST_PROPERTY_ACCESS,
    AST_IS_NULL_EXPR
} ast_node_type_t;

typedef enum {
    OP_EQ,    // =
    OP_NEQ,   // <>
    OP_LT,    // <
    OP_GT,    // >
    OP_LE,    // <=
    OP_GE,    // >=
    OP_AND,
    OP_OR,
    OP_NOT
} operator_type_t;

// Binary expression node
struct {
    cypher_ast_node_t *left;
    cypher_ast_node_t *right;
    operator_type_t op;
} binary_expr;

// Property access node
struct {
    char *variable;  // e.g., "n"
    char *property;  // e.g., "age"
} property_access;
```

### 4. Expression Evaluation Engine
```c
typedef struct {
    graphqlite_value_type_t type;
    union {
        int64_t integer;
        double real;
        char *text;
        int boolean;
    } value;
} eval_result_t;

// Evaluate expression in context of current variable bindings
eval_result_t* evaluate_expression(
    cypher_ast_node_t *expr,
    variable_binding_t *bindings,
    sqlite3 *db
);

// Type-aware comparison
int compare_values(eval_result_t *left, eval_result_t *right, operator_type_t op);
```

### 5. Integration with MATCH Execution
```c
// Modified MATCH execution to apply WHERE filtering
static graphqlite_result_t* execute_match_with_where(
    sqlite3 *db, 
    cypher_ast_node_t *match_pattern,
    cypher_ast_node_t *where_clause,
    cypher_ast_node_t *return_stmt
) {
    // 1. Execute pattern matching (existing)
    // 2. For each result row:
    //    - Bind variables to current values
    //    - Evaluate WHERE expression
    //    - Include row only if expression is true
    // 3. Project results (existing)
}
```

## Alternatives Considered

**1. SQL-Based Filtering**
- **Approach**: Generate SQL WHERE clauses instead of post-filtering
- **Pros**: Better performance, leverage SQLite optimizer
- **Cons**: Complex SQL generation, harder to support all OpenCypher semantics
- **Decision**: Start with post-filtering, optimize to SQL generation later

**2. Inline Property Filtering Only**
- **Approach**: Extend existing `{property: value}` syntax instead of WHERE
- **Pros**: Simpler implementation, no expression evaluation needed
- **Cons**: Limited expressiveness, not OpenCypher compliant
- **Decision**: Implement proper WHERE clause for standard compliance

**3. Full Expression Language**
- **Approach**: Implement arithmetic, functions, etc. from the start
- **Pros**: More complete OpenCypher support
- **Cons**: Significant complexity increase, longer development time
- **Decision**: Start with comparisons and boolean logic, extend later

**4. Compiled Expression Trees**
- **Approach**: Compile expressions to bytecode for faster evaluation
- **Pros**: Better performance for complex expressions
- **Cons**: Additional complexity, premature optimization
- **Decision**: Use simple interpreter pattern initially

**5. Lazy Evaluation**
- **Approach**: Stream results and filter one at a time
- **Pros**: Lower memory usage for large result sets
- **Cons**: More complex implementation
- **Decision**: Implement eager evaluation first, optimize if needed

## Implementation Plan

## Phase 1: Parser Support (Week 1)
**Tasks:**
- Add WHERE keyword and operator tokens to lexer
- Extend BISON grammar with where_clause rules
- Create AST node types for expressions
- Implement AST construction functions
- Add expression memory management

**Exit Criteria:** Parser successfully generates AST for WHERE clauses

## Phase 2: Expression Evaluation (Week 1-2)
**Tasks:**
- Build expression evaluation engine
- Implement property value lookups
- Create type-aware comparison functions
- Handle NULL value semantics
- Add variable binding context

**Exit Criteria:** Expressions can be evaluated with test data

## Phase 3: MATCH Integration (Week 2)
**Tasks:**
- Modify MATCH execution to accept WHERE clause
- Implement row-by-row filtering
- Optimize filtering to happen during SQL query when possible
- Handle edge property filtering in relationship patterns
- Ensure proper memory cleanup

**Exit Criteria:** WHERE filtering works in MATCH queries

## Phase 4: Testing & Edge Cases (Week 2-3)
**Tasks:**
- Unit tests for expression evaluation
- Integration tests for WHERE with MATCH
- Type mismatch error handling
- NULL comparison edge cases
- Performance testing with large result sets

**Exit Criteria:** All tests passing, good performance

## Testing Strategy

## Unit Tests

**Parser Tests:**
- WHERE clause with single comparison
- Compound conditions with AND/OR
- Nested expressions with parentheses
- IS NULL / IS NOT NULL parsing
- Syntax error handling

**Expression Evaluation Tests:**
- Integer comparisons (all operators)
- Float comparisons with precision
- String comparisons (lexicographic)
- Boolean comparisons
- NULL handling in comparisons
- Type mismatch scenarios

**Integration Tests:**
- WHERE with node properties
- WHERE with edge properties
- Complex boolean expressions
- Performance with large datasets

## SQL Test Cases
```sql
-- Test 1: Basic integer comparison
CREATE (n:Person {name: "Alice", age: 25})
CREATE (n:Person {name: "Bob", age: 30})
CREATE (n:Person {name: "Charlie", age: 20})
MATCH (n:Person) WHERE n.age > 22 RETURN n
-- Expected: Alice and Bob

-- Test 2: String equality
MATCH (n:Person) WHERE n.name = "Bob" RETURN n
-- Expected: Only Bob

-- Test 3: Boolean AND
CREATE (p:Product {name: "Widget", price: 50, inStock: true})
CREATE (p:Product {name: "Gadget", price: 150, inStock: true})
CREATE (p:Product {name: "Tool", price: 75, inStock: false})
MATCH (p:Product) WHERE p.price < 100 AND p.inStock = true RETURN p
-- Expected: Only Widget

-- Test 4: NULL checks
CREATE (n:Person {name: "David"})
CREATE (n:Person {name: "Eve", email: "eve@example.com"})
MATCH (n:Person) WHERE n.email IS NULL RETURN n
-- Expected: Alice, Bob, Charlie, David (not Eve)

-- Test 5: Edge property filtering
CREATE (a:Person {name: "Frank"})-[:KNOWS {since: 2019}]->(b:Person {name: "Grace"})
CREATE (c:Person {name: "Henry"})-[:KNOWS {since: 2022}]->(d:Person {name: "Iris"})
MATCH (a:Person)-[r:KNOWS]->(b:Person) WHERE r.since > 2020 RETURN a, b
-- Expected: Henry and Iris
```

## Performance Benchmarks
- Filter 1K nodes: < 10ms
- Filter 10K nodes: < 50ms
- Complex expressions: < 2x simple comparison time
- Memory usage: O(1) for filtering (no extra storage)
