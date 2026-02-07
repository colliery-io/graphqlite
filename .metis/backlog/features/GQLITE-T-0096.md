---
id: cypher-dialect-parity-for-common
level: task
title: "Cypher dialect parity for common queries"
short_code: "GQLITE-T-0096"
created_at: 2026-02-07T02:09:56.178299+00:00
updated_at: 2026-02-07T16:08:59.284721+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#feature"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# Cypher dialect parity for common queries

**GitHub Issue**: [#13](https://github.com/colliery-io/graphqlite/issues/13)

## Objective

Expand the Cypher parser to accept common Memgraph/Neo4j-style syntax, eliminating the need for client-side string rewrites for bracket access, backtick identifiers, nested dot access, and trailing semicolons. (IN list literals already work — verified with 47+ passing tests.)

## Backlog Item Details

### Type
- [x] Feature - New functionality or enhancement

### Priority
- [ ] P1 - High (important for user experience)

### Business Justification
- **User Value**: Users can write natural Cypher queries without client-side rewriting hacks
- **Business Value**: Reduces friction for adoption by Neo4j/Memgraph users; removes a class of client bugs
- **Effort Estimate**: M - Parser/grammar changes (phases 1-2 trivial, phases 3-4 medium — shared transform work). IN lists already done.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] Trailing semicolons are accepted without error
- [ ] Backtick identifiers work in property access: `n.\`special-key\``
- [ ] Nested dot access parses and executes: `p.metadata.name`
- [ ] Bracket property access parses and executes: `p['status']['phase']`
- [x] ~~List literals in expressions work: `IN ['Failed','Unknown']`~~ (already implemented — 47+ tests passing in `23_in_operator.sql`)
- [ ] Existing accepted syntax continues to work (no regressions)

## Implementation Plan

### Approach

Two commits:

1. **Commit A — Trailing semicolons** (grammar only, trivial)
2. **Commit B — Backtick + nested dot + bracket chaining** (grammar refactor + transform layer, batched)

---

### Commit A: Trailing Semicolons

**File:** `src/backend/parser/cypher_gram.y` — `stmt` rule (line 159)

Add two productions:

```
| union_query ';'
    { $$ = $1; context->result = $1; }
| EXPLAIN union_query ';'
    { if ($2->type == AST_NODE_QUERY) { ((cypher_query*)$2)->explain = true; }
      $$ = $2; context->result = $2; }
```

Scanner already tokenizes `;` as `CYPHER_TOKEN_CHAR` (line 243 of `cypher_scanner.l`).

**Test:** New `tests/functional/32_dialect_parity.sql` — Section 1 with `RETURN 1;`, `MATCH (n) RETURN n;`, `EXPLAIN MATCH (n) RETURN n;`.

Build, test, commit.

---

### Commit B: Backtick + Nested Dot + Bracket Chaining

#### Grammar changes (`src/backend/parser/cypher_gram.y`)

**B1. Add 3 productions to `expr` rule** (after line 987):

```
| expr '.' IDENTIFIER
    { $$ = (ast_node*)make_property($1, $3, @3.first_line); free($3); }
| expr '.' BQIDENT
    { $$ = (ast_node*)make_property($1, $3, @3.first_line); free($3); }
| expr '[' expr ']'
    { $$ = (ast_node*)make_subscript($1, $3, @2.first_line); }
```

Enables: `n.\`special-key\``, `p.metadata.name`, `p['status']['phase']`.

**B2. Remove redundant rules from `primary_expr`:**

- `IDENTIFIER '.' IDENTIFIER` (line 1002)
- `END_P '.' IDENTIFIER` (line 1009) — replaced by adding END_P to `identifier` rule
- `IDENTIFIER '[' expr ']'` (line 1023)
- `'(' expr ')' '[' expr ']'` (line 1030)
- `list_literal '[' expr ']'` (line 1035)

All subsumed by the new `expr` productions.

**B3. Add `END_P` to `identifier` rule:**

```
| END_P  { $$ = make_identifier(strdup("end"), @1.first_line); }
```

**B4. Add BQIDENT variants to `remove_item`** (line 577):

```
| IDENTIFIER '.' BQIDENT  { ... same as IDENTIFIER '.' IDENTIFIER ... }
| BQIDENT '.' IDENTIFIER  { ... }
| BQIDENT '.' BQIDENT     { ... }
```

(SET uses `expr '=' expr` so gets backtick support for free.)

**B5. Update `%expect` / `%expect-rr`** (lines 35-36):

Run `bison` and adjust conflict counts.

#### Transform changes

**B6. `transform_property_access()` in `transform_expr_ops.c`** (line 314):

Replace hard rejection at line 319 with recursive handling:

```c
if (prop->expr->type == AST_NODE_PROPERTY) {
    /* Nested: p.metadata.name → json_extract(p.metadata_sql, '$.name') */
    append_sql(ctx, "json_extract(");
    if (transform_property_access(ctx, (cypher_property*)prop->expr) < 0) return -1;
    append_sql(ctx, ", '$."); append_sql(ctx, prop->property_name); append_sql(ctx, "')");
    return 0;
}
if (prop->expr->type == AST_NODE_SUBSCRIPT) {
    /* list[0].name → json_extract(list_subscript_sql, '$.name') */
    append_sql(ctx, "json_extract(");
    if (transform_expression(ctx, prop->expr) < 0) return -1;
    append_sql(ctx, ", '$."); append_sql(ctx, prop->property_name); append_sql(ctx, "')");
    return 0;
}
/* Existing AST_NODE_IDENTIFIER check remains below */
```

**B7. Subscript string-key normalization in `transform_return.c`** (line 661):

At top of `AST_NODE_SUBSCRIPT` case, normalize `p['key']` → property access:

```c
cypher_subscript *subscript = (cypher_subscript*)expr;
if (subscript->index->type == AST_NODE_LITERAL) {
    cypher_literal *idx_lit = (cypher_literal*)subscript->index;
    if (idx_lit->literal_type == LITERAL_STRING) {
        if (subscript->expr->type == AST_NODE_IDENTIFIER ||
            subscript->expr->type == AST_NODE_PROPERTY) {
            cypher_property temp_prop;
            memset(&temp_prop, 0, sizeof(temp_prop));
            temp_prop.base.type = AST_NODE_PROPERTY;
            temp_prop.expr = subscript->expr;
            temp_prop.property_name = idx_lit->value.string;
            return transform_property_access(ctx, &temp_prop);
        }
    }
}
/* ... existing json_extract logic below unchanged ... */
```

#### Tests

New `tests/functional/32_dialect_parity.sql` sections:
- **Section 2:** Backtick property access (`n.\`special-key\``)
- **Section 3:** Nested dot access (JSON-valued property + `n.metadata.name`)
- **Section 4:** Bracket chaining (`n['status']`, `n['status']['phase']`)
- **Section 5:** Mixed (`n['metadata'].name`, `n.items[0]`)

Also un-skip `tests/functional/09_edge_cases.sql` line 199-202.

---

### Files Modified

| File | Change |
|------|--------|
| `src/backend/parser/cypher_gram.y` | `stmt` + `;`, `expr '.' IDENT/BQIDENT`, `expr '[' expr ']'`, remove redundant `primary_expr` rules, END_P in `identifier`, BQIDENT in `remove_item`, `%expect` update |
| `src/backend/transform/transform_expr_ops.c` | `transform_property_access()` handles nested `AST_NODE_PROPERTY` and `AST_NODE_SUBSCRIPT` base |
| `src/backend/transform/transform_return.c` | String-key-to-property normalization in `AST_NODE_SUBSCRIPT` case |
| `tests/functional/32_dialect_parity.sql` | **New** — tests for all 4 features |
| `tests/functional/09_edge_cases.sql` | Un-skip nested property test (line 199) |

### Out of Scope

- SET/REMOVE of nested properties (`SET n.metadata.name = 'foo'`) — requires JSON path updates, much more complex
- Nested/bracket access is read-only (RETURN, WHERE, WITH contexts)

### Verification

1. `angreal build extension` — bison/flex compile clean
2. `angreal test unit` — CUnit tests pass
3. `angreal test functional` — all SQL tests pass including new `32_dialect_parity.sql`
4. `angreal test all` — no regressions

## Status Updates

### 2026-02-07: Code Review & Triage

**Code areas reviewed:**
- `src/backend/parser/cypher_gram.y` — `stmt` rule (line 159), property access (lines 1002-1015), subscript (lines 1023-1039), BQIDENT usage (25 sites), `%expect 4`/`%expect-rr 3` (line 35-36), `%left '.'` (line 152), `%dprec` on list_literal/list_comprehension (lines 996-997)
- `src/backend/transform/transform_expr_ops.c` — `transform_property_access()` (line 314), hard rejects non-IDENTIFIER base at line 319 with "Complex property access not yet supported"
- `src/backend/transform/transform_return.c` — `AST_NODE_SUBSCRIPT` handler (lines 661-691), generates `json_extract()` with negative-index support

**Feature status after review:**

| Feature | Status | Evidence |
|---------|--------|----------|
| Trailing semicolons | NOT SUPPORTED | `stmt` rule has no `;` production |
| Backtick property access | NOT SUPPORTED | BQIDENT accepted in labels, variables, rel types, map keys — but NOT in property access (`IDENTIFIER '.' IDENTIFIER` only at line 1002) |
| Nested dot access | NOT SUPPORTED | `transform_property_access()` rejects non-IDENTIFIER base at line 319 |
| Bracket property access | PARTIAL | `IDENTIFIER '[' expr ']'` works (line 1023) but no chaining (`expr '[' expr ']'` missing) |
| IN list literals | ALREADY WORKS | 47+ tests passing in `tests/functional/23_in_operator.sql` using `["Alice", "Bob"]` syntax |

**Test harness findings:**
- `tests/functional/09_edge_cases.sql:199-202` — Nested dot access test **explicitly SKIPPED** (`n.level1.level2.level3`)
- `tests/functional/23_in_operator.sql` — IN with list literals extensively tested and **passing** (strings, ints, NOT IN, edge cases)
- No existing tests for trailing semicolons, bracket access chaining, or backtick property access
- No tests are erroneously failing for these features — they're either skipped or not tested

**Revised scope — IN list literals should be removed from acceptance criteria (already done). Remaining work:**
1. Trailing semicolons — grammar-only, trivial
2. Backtick property access — add `BQIDENT` variants to property access rules in `primary_expr`
3. Nested dot access — move property access to `expr` rule + update transform layer
4. Bracket access chaining — move subscript to `expr` rule + transform normalization for string keys