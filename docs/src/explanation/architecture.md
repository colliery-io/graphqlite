# Architecture

This document explains how GraphQLite is structured and how queries flow through the system.

## High-Level Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     SQLite Extension                        │
├─────────────────────────────────────────────────────────────┤
│  cypher() function                                          │
│      │                                                      │
│      ▼                                                      │
│  ┌─────────┐    ┌───────────┐    ┌──────────┐              │
│  │ Parser  │───▶│ Transform │───▶│ Executor │              │
│  └─────────┘    └───────────┘    └──────────┘              │
│      │              │                 │                     │
│      ▼              ▼                 ▼                     │
│  Cypher AST     SQL Query        Results                    │
└─────────────────────────────────────────────────────────────┘
```

## Components

### Parser

The parser converts Cypher query text into an Abstract Syntax Tree (AST).

**Implementation**: Flex (lexer) + Bison (parser)

- `src/backend/parser/cypher_scanner.l` - Tokenizer
- `src/backend/parser/cypher_gram.y` - Grammar
- `src/backend/parser/cypher_ast.c` - AST construction

### Transformer

The transformer converts the Cypher AST into SQL that can be executed against the graph schema.

**Key files**:
- `src/backend/transform/cypher_transform.c` - Main entry point
- `src/backend/transform/transform_match.c` - MATCH clause handling
- `src/backend/transform/transform_return.c` - RETURN clause handling
- `src/backend/transform/sql_builder.c` - SQL construction utilities

### Executor

The executor runs the generated SQL and handles special cases like graph algorithms.

**Key files**:
- `src/backend/executor/cypher_executor.c` - Main entry point
- `src/backend/executor/query_dispatch.c` - Pattern-based routing
- `src/backend/executor/graph_algorithms.c` - Algorithm implementations

## Query Flow

### 1. Entry Point

The `cypher()` SQL function receives the query:

```c
// In extension.c
static void cypher_func(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const char *query = (const char *)sqlite3_value_text(argv[0]);
    // ...
}
```

### 2. Parsing

The query is tokenized and parsed:

```c
cypher_parser_context *ctx = cypher_parser_create(query);
ast_node *ast = cypher_parse(ctx);
```

### 3. Pattern Dispatch

Instead of a giant if-else chain, queries are matched against patterns:

```c
clause_flags flags = analyze_query_clauses(ast);
const query_pattern *pattern = find_matching_pattern(flags);
return pattern->handler(executor, ast, result, flags);
```

### 4. Transformation

The AST is converted to SQL using the unified SQL builder:

```c
cypher_transform_context *ctx = create_transform_context(db);
transform_query(ctx, ast);
char *sql = sql_builder_to_string(ctx->unified_builder);
```

### 5. Execution

The SQL is executed against SQLite:

```c
sqlite3_stmt *stmt;
sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
while (sqlite3_step(stmt) == SQLITE_ROW) {
    // Process results
}
```

## Design Decisions

### Why SQLite?

- Zero configuration - single file, no server
- Ubiquitous - available everywhere
- Well-tested - decades of production use
- Extensible - clean extension API

### Why Transform to SQL?

Rather than implementing our own storage engine, we transform Cypher to SQL:

- Leverage SQLite's query optimizer
- Benefit from SQLite's transaction handling
- Interop with regular SQL tables
- Simpler implementation

### Why Pattern Dispatch?

Replacing if-else chains with table-driven dispatch:

- Easier to add new query patterns
- Clear priority ordering
- Better testability
- Reduced cyclomatic complexity

## Extension Loading

When the extension loads:

1. Register the `cypher()` function
2. Create schema tables if they don't exist
3. Create indexes for efficient lookups

```c
int sqlite3_graphqlite_init(
    sqlite3 *db,
    char **pzErrMsg,
    const sqlite3_api_routines *pApi
) {
    SQLITE_EXTENSION_INIT2(pApi);
    create_graph_schema(db);
    sqlite3_create_function(db, "cypher", -1, SQLITE_UTF8, db, cypher_func, 0, 0);
    return SQLITE_OK;
}
```
