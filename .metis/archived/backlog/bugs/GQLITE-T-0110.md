---
id: match-set-return-ignores-return
level: task
title: "MATCH+SET+RETURN ignores RETURN clause"
short_code: "GQLITE-T-0110"
created_at: 2026-02-08T02:19:38.244430+00:00
updated_at: 2026-03-17T13:57:14.708135+00:00
parent: 
blocked_by: []
archived: true

tags:
  - "#task"
  - "#bug"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: NULL
---

# MATCH+SET+RETURN ignores RETURN clause

## Objective

When a Cypher query combines MATCH+SET+RETURN (e.g., `MATCH (n:Person) SET n.score = 100 RETURN n.score`), the RETURN clause is silently ignored. The SET executes correctly but no data is returned. In standard Cypher (Neo4j), this pattern is valid and returns the updated values.

## Backlog Item Details

### Type
- [x] Bug - Production issue that needs fixing

### Priority
- [x] P2 - Medium (nice to have)

### Impact Assessment
- **Affected Users**: Any user combining SET and RETURN in a single Cypher query
- **Reproduction Steps**: 
  1. `SELECT cypher('CREATE (n:Test {name: "Alice"})');`
  2. `SELECT cypher('MATCH (n:Test) SET n.score = 100 RETURN n.score');`
  3. Observe: result is NULL instead of `[{"n.score": 100}]`
- **Expected vs Actual**: Expected the updated `n.score` value to be returned. Actual: no data returned (NULL result). The SET itself succeeds — a subsequent `MATCH (n:Test) RETURN n.score` correctly returns 100.
- **Workaround**: Split into two queries — one for SET, one for RETURN.

## Root Cause

The `handle_match_set` dispatch handler in `query_dispatch.c:625` (priority 90, `forbidden = CLAUSE_NONE`) catches any query containing MATCH+SET, regardless of other clauses present. It calls `execute_match_set_query()` in `executor_set.c:31`, which:

1. Transforms the MATCH clause to SQL
2. Executes it to get matched node/edge IDs
3. Applies SET operations for each matched row
4. Returns — **never inspecting or processing the RETURN clause**

The same issue likely applies to MATCH+DELETE+RETURN and MATCH+REMOVE+RETURN, since those handlers follow the same pattern.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `MATCH (n:Label) SET n.prop = value RETURN n.prop` returns the updated property value
- [ ] `MATCH (n:Label) SET n += {map} RETURN n.prop` returns updated values after merge
- [ ] `MATCH (n:Label) SET n = {map} RETURN n.prop` returns updated values after replace
- [ ] Works with both parameterized and non-parameterized queries
- [ ] Existing SET tests continue to pass
- [ ] Add functional tests for SET+RETURN patterns

## Implementation Plan

### Pattern to Follow

`handle_match_create_return` at `query_dispatch.c:700` already solves this for CREATE. It calls `execute_match_create_query()` to do the mutation, then `execute_match_return_query()` to process RETURN. The underlying `execute_match_return_query()` function (declared in `executor_internal.h:98`, defined in `executor_match.c:57`) handles transform, params, aliases, and agtype — all reusable.

### Step 1: Modify 4 handler functions in `query_dispatch.c`

Each of the four mutation handlers gets a RETURN check after the mutation succeeds. Remove `(void)flags;` since we now use `flags`. Same pattern for all four:

**`handle_match_set`** (line 625):
```c
static int handle_match_set(..., clause_flags flags) {
    cypher_match *match = find_match_clause(query);
    cypher_set *set = find_set_clause(query);
    CYPHER_DEBUG("Executing MATCH+SET via pattern dispatch");
    int rc = execute_match_set_query(executor, match, set, result);
    if (rc >= 0) {
        result->success = true;
        if (flags & CLAUSE_RETURN) {
            cypher_return *ret = find_return_clause(query);
            if (ret) {
                rc = execute_match_return_query(executor, match, ret, result);
            }
        }
    }
    return rc;
}
```

**`handle_match_remove`** (line 655): Same — add RETURN check after `execute_match_remove_query`.

**`handle_match_merge`** (line 670): Same — add RETURN check after `execute_match_merge_query`.

**`handle_match_delete`** (line 640): Same — add RETURN check after `execute_match_delete_query`. Note: deleted entities won't be found by the re-query, so DELETE+RETURN returns empty results. Acceptable for now.

### Step 2: Add functional tests — `tests/functional/35_mutation_return.sql`

New test file covering mutation+RETURN combinations:
- `MATCH (n) SET n.prop = value RETURN n.prop` — single property
- `MATCH (n) SET n += {map} RETURN n.prop` — bulk merge
- `MATCH (n) SET n = {map} RETURN n.prop` — bulk replace
- SET+RETURN with parameterized query
- `MATCH (n) REMOVE n.prop RETURN n.prop` — returns NULL
- `MATCH (n) SET n:NewLabel RETURN labels(n)` — label + RETURN

### Step 3: Add binding tests

- **Rust** (`integration.rs`): Restore `test_bulk_set_with_builder_params` to use combined SET+RETURN. Add 2-3 more SET+RETURN tests.
- **Python** (`test_graph.py`): Add 2-3 SET+RETURN tests.

### Files to Modify

| File | Change |
|------|--------|
| `src/backend/executor/query_dispatch.c` | Modify 4 `handle_match_*` functions (~6 lines each) |
| `tests/functional/35_mutation_return.sql` | New file — functional tests |
| `bindings/rust/tests/integration.rs` | Add/update SET+RETURN tests |
| `bindings/python/tests/test_graph.py` | Add SET+RETURN tests |

No header changes needed — `execute_match_return_query` is already declared in `executor_internal.h`.

### Verification

1. `angreal build extension`
2. `angreal test unit` — 849 existing tests pass
3. `angreal test functional` — existing + new `35_mutation_return.sql` pass
4. `angreal test python` — existing + new tests pass
5. `angreal test rust` — existing + new tests pass

### Risk Considerations
- RETURN re-queries post-mutation, so it sees updated values (correct for SET/REMOVE/MERGE)
- DELETE+RETURN won't find deleted entities — acceptable, can be improved later
- No dispatch table changes — mutation handlers still match at priority 90, just do more work when RETURN is present

## Status Updates

### Completed
- Modified 4 handlers in `query_dispatch.c`: `handle_match_set`, `handle_match_delete`, `handle_match_remove`, `handle_match_merge` — each now checks `flags & CLAUSE_RETURN` after mutation and calls `execute_match_return_query()` if present
- Added `tests/functional/35_mutation_return.sql` — 11 tests covering SET+RETURN (single prop, multiple, bulk merge, bulk replace, aliases), parameterized SET+RETURN, REMOVE+RETURN, edge SET+RETURN, multi-row SET+RETURN
- Added 4 Python tests: `test_set_return_single_property`, `test_set_return_bulk_merge`, `test_set_return_with_params`, `test_remove_return`
- Added/updated 4 Rust tests: restored `test_bulk_set_with_builder_params` to combined SET+RETURN, added `test_set_single_property_return`, `test_set_bulk_replace_return`, `test_remove_property_return`
- All tests pass: 849 unit, all functional, 219 Python (6 skipped), 165 Rust integration