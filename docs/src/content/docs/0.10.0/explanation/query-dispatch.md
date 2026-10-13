---
title: Query Dispatch
slug: 0.10.0/explanation/query-dispatch
---

When GraphQLite receives a Cypher query, the executor does not immediately start generating SQL. First it determines what kind of query it is — a MATCH+RETURN? A CREATE? A MATCH+SET? — and routes it to a handler that knows how to process that specific combination of clauses. This routing mechanism is the query dispatch system.

## The Pattern Registry

The dispatch system maintains a static table of `query_pattern` entries. Each entry describes:

* A **name** (used in EXPLAIN output and debug logs).
* A bitmask of **required** clauses that must all be present.
* A bitmask of **forbidden** clauses that must all be absent.
* A **handler** function pointer.
* A **priority** integer used to break ties when multiple patterns match.

The full pattern table, ordered from highest to lowest priority:

| Priority | Pattern Name | Required | Forbidden |
|---|---|---|---|
| 100 | `UNWIND+CREATE` | UNWIND, CREATE | RETURN, MATCH |
| 100 | `WITH+MATCH+RETURN` | WITH, MATCH, RETURN | — |
| 100 | `MATCH+CREATE+RETURN` | MATCH, CREATE, RETURN | — |
| 90 | `MATCH+SET` | MATCH, SET | — |
| 90 | `MATCH+DELETE` | MATCH, DELETE | — |
| 90 | `MATCH+REMOVE` | MATCH, REMOVE | — |
| 90 | `MATCH+MERGE` | MATCH, MERGE | — |
| 90 | `MATCH+CREATE` | MATCH, CREATE | RETURN |
| 80 | `OPTIONAL_MATCH+RETURN` | MATCH, OPTIONAL, RETURN | CREATE, SET, DELETE, MERGE |
| 80 | `MULTI_MATCH+RETURN` | MATCH, MULTI_MATCH, RETURN | CREATE, SET, DELETE, MERGE |
| 70 | `MATCH+RETURN` | MATCH, RETURN | OPTIONAL, MULTI_MATCH, CREATE, SET, DELETE, MERGE |
| 60 | `UNWIND+RETURN` | UNWIND, RETURN | CREATE |
| 50 | `CREATE` | CREATE | MATCH, UNWIND |
| 50 | `MERGE` | MERGE | MATCH |
| 50 | `SET` | SET | MATCH |
| 50 | `FOREACH` | FOREACH | — |
| 40 | `MATCH` | MATCH | RETURN, CREATE, SET, DELETE, MERGE, REMOVE |
| 10 | `RETURN` | RETURN | MATCH, UNWIND, WITH |
| 0 | `GENERIC` | — | — |

The `GENERIC` pattern at priority 0 is the catch-all: it has no required or forbidden clauses, so it matches any query. It uses the full transform pipeline, which handles complex multi-clause queries including WITH chains.

## How Pattern Matching Works

The dispatch function `dispatch_query_pattern()` follows three steps:

**Step 1: Analyse.** `analyze_query_clauses()` walks the clause list of the parsed query and sets bits in a `clause_flags` integer. Each clause type maps to one bit:

```
CLAUSE_MATCH       CLAUSE_RETURN      CLAUSE_CREATE
CLAUSE_MERGE       CLAUSE_SET         CLAUSE_DELETE
CLAUSE_REMOVE      CLAUSE_WITH        CLAUSE_UNWIND
CLAUSE_FOREACH     CLAUSE_LOAD_CSV    CLAUSE_EXPLAIN
CLAUSE_OPTIONAL    CLAUSE_MULTI_MATCH CLAUSE_UNION
CLAUSE_CALL
```

`CLAUSE_OPTIONAL` is set when any MATCH clause has `optional = true`. `CLAUSE_MULTI_MATCH` is set when more than one MATCH clause is present.

**Step 2: Find best match.** `find_matching_pattern()` iterates the pattern table and evaluates each entry against the flags:

```c
// Required clauses must all be present
if ((present & p->required) != p->required) continue;

// Forbidden clauses must all be absent
if (present & p->forbidden) continue;

// Higher priority wins
if (!best || p->priority > best->priority) best = p;
```

**Step 3: Dispatch.** The winning pattern's handler is called with the executor, query AST, result, and flags.

## Priority Ordering Rationale

The priorities reflect increasing specificity:

**Priority 100** entries are the most specific multi-clause combinations. A `WITH+MATCH+RETURN` query is unambiguous — it must use the generic transform pipeline which understands how WITH propagates variables into subsequent MATCH clauses. Putting it at the top prevents it from being matched by `MATCH+RETURN` (priority 70), which uses a simpler, more direct execution path that does not handle WITH.

**Priority 90** covers the common write patterns: MATCH followed by SET, DELETE, REMOVE, MERGE, or CREATE. These all require first finding graph elements (via MATCH) and then modifying them. The forbidden clauses on `MATCH+CREATE` (priority 90) exclude RETURN, because `MATCH+CREATE+RETURN` has its own handler at priority 100 with different result-formatting behaviour.

**Priority 80** handles OPTIONAL MATCH and multi-MATCH queries. These require the generic transform pipeline for correct LEFT JOIN generation. They are kept separate from the simpler `MATCH+RETURN` (priority 70) because the naive MATCH+RETURN handler assumes a single, non-optional MATCH clause.

**Priority 70** is the hot path for the most common read query: `MATCH ... RETURN`. Its forbidden clause list is broad — it excludes OPTIONAL, MULTI_MATCH, and all write operations — ensuring only genuinely simple single-MATCH queries reach this handler.

**Priority 50** covers standalone write operations. A bare `CREATE (n:Person)` or `MERGE` without a preceding MATCH is simpler than the MATCH+write variants; the handler can skip the join generation step.

**Priority 10** covers standalone RETURN, which is how graph algorithms are invoked: `RETURN pageRank()`. The forbidden list excludes MATCH and WITH to prevent this from matching a `MATCH ... RETURN` accidentally.

**Priority 0** is the GENERIC fallback. Any query that reaches this point is handled by the full transform pipeline, which is the most capable but least optimised path.

## The GENERIC Fallback

The GENERIC handler creates a transform context and passes the entire query AST to `cypher_transform_generate_sql()`. The transform layer processes clauses sequentially — MATCH generates JOINs, WITH generates a subquery boundary, RETURN generates the SELECT list. This handles:

* WITH chains (`MATCH ... WITH ... MATCH ... RETURN`)
* OPTIONAL MATCH
* Multiple MATCH clauses
* UNWIND with complex logic
* Any combination not covered by a specific handler

The specific handlers at higher priorities exist as optimisations over GENERIC — they take shortcuts that are only valid for their specific clause combinations. If you add a new clause type or combination that GENERIC handles incorrectly, you add a new specific pattern rather than modifying GENERIC.

## Multi-Clause Queries and WITH Chains

A query like:

```cypher
MATCH (a:Person)
WITH a, count(*) AS c
WHERE c > 2
MATCH (a)-[:KNOWS]->(b)
RETURN a.name, b.name
```

contains clauses: MATCH, WITH, MATCH, RETURN. `analyze_query_clauses()` sets `CLAUSE_MATCH | CLAUSE_WITH | CLAUSE_RETURN | CLAUSE_MULTI_MATCH`. The pattern `WITH+MATCH+RETURN` (priority 100) matches because it requires MATCH, WITH, and RETURN with no forbidden clauses.

The GENERIC transform pipeline processes this by generating a subquery for the first MATCH+WITH block, then joining the second MATCH into that subquery's output. The WITH clause acts as a boundary: variables named in the WITH are projected out and remain available to subsequent clauses; variables not named are out of scope.

## UNION Queries

UNION queries are handled outside the pattern dispatch system. When `cypher_executor_execute_ast()` receives an `AST_NODE_UNION` node, it passes it directly to the transform layer, bypassing `dispatch_query_pattern()` entirely. The transform layer handles UNION by generating `SELECT ... UNION ALL SELECT ...` (or `UNION` for UNION DISTINCT) SQL.

## Algorithm Detection

The `RETURN`-only pattern (priority 10) handles standalone RETURN clauses. When the handler processes the RETURN items and finds a function call whose name is a known graph algorithm — `pageRank`, `dijkstra`, `betweenness`, `louvain`, etc. — it dispatches to the graph algorithm subsystem rather than generating SQL.

Detection happens by name in the RETURN handler. If the function name is registered as a graph algorithm, the executor checks whether a CSR graph is loaded (`executor->cached_graph != NULL`) and calls the appropriate algorithm function. If no graph is loaded, an error is returned indicating that `gql_load_graph()` must be called first.

This means graph algorithm calls look syntactically identical to scalar function calls:

```cypher
RETURN pageRank()
RETURN dijkstra('alice', 'bob')
RETURN betweennessCentrality()
```

They are distinguished from SQL functions only inside the executor.

## Adding New Patterns

To add a new execution path for a clause combination not currently covered:

1. **Identify the clause combination.** Determine which clauses must be present and which must be absent. Be careful not to create ambiguity with existing patterns.

2. **Write the handler.** Handler functions have the signature:
   ```c
   static int handle_my_pattern(
       cypher_executor *executor,
       cypher_query *query,
       cypher_result *result,
       clause_flags flags
   );
   ```
   The handler is responsible for setting `result->success` or calling `set_result_error()` on failure. It returns 0 on success and -1 on error.

3. **Add the pattern to the registry** in `query_dispatch.c`, choosing a priority that correctly orders it relative to existing patterns. The registry is scanned sequentially, with the highest-priority match winning, so placement within the array matters only for readability — the priority field determines the winner.

4. **Forward-declare the handler** in the declarations block at the top of `query_dispatch.c`.

5. **Test.** Use `EXPLAIN` to verify that the new pattern is selected for your target queries: `EXPLAIN MATCH (n) ... RETURN n` returns `Pattern: <matched_name>` in its output.

The `EXPLAIN` output also shows the clause flags string (e.g., `MATCH|RETURN|MULTI_MATCH`), which makes it easy to debug pattern selection issues.
