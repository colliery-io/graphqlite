---
id: cross-clause-variable-map
level: initiative
title: "Cross-clause variable-map threading in dispatch layer"
short_code: "GQLITE-I-0036"
created_at: 2026-04-18T21:06:06.965126+00:00
updated_at: 2026-04-18T22:43:47.820789+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/completed"


exit_criteria_met: false
estimated_complexity: M
initiative_id: cross-clause-variable-map
---

# Cross-clause variable-map threading in dispatch layer

## Context

Investigation of GitHub issue #61 validated 7 bugs. Fixing two (T-0186, T-0187) surfaced a shared structural defect behind four others (T-0185, T-0188, T-0189, T-0190) and at least one previously-filed related bug (T-0183, UNWIND+write paths). Every affected symptom is the same shape: **variables bound by one clause fail to reach the next clause because the dispatcher layer drops the `variable_map`** between handlers.

Three concrete manifestations:

1. **`CREATE (n) SET n.k = v`** → SET silently no-ops. `handle_create` calls `execute_create_clause` only; SET is never dispatched.
2. **`MATCH (a) MATCH (b) MERGE (a)-[r]->(b) SET r.k = v`** → `Unbound variable in SET: r`. MERGE registers `r` in a var_map scoped to `execute_merge_clause`; that map is freed before the trailing SET runs under a separate dispatch path.
3. **`MATCH (a) MATCH (b) CREATE (a)-[:REL]->(b)`** → phantom target node. `execute_match_create_query` calls `find_match_clause(query)` which returns only the first MATCH; the second MATCH's `b` binding is dropped and CREATE invents a new anonymous node.

Existing precedent that this work generalizes: the UNWIND+CREATE+SET path at `query_dispatch.c:1471` already threads `execute_create_clause_with_varmap` → `execute_set_operations(set, create_vars, …)` correctly. The fix is to apply that same pattern to every write clause.

## Goals & Non-Goals

### Goals
- Eliminate silent data loss from `CREATE+SET`, `MERGE+SET`, and any other "write-then-use-bound-var" combination.
- Make `MATCH ... MATCH ... <write>` patterns fold all MATCH bindings into a single var_map before the write.
- Thread the post-write var_map into any trailing SET / RETURN / second MATCH / MERGE via the existing dispatch table.
- Zero new SET or MATCH semantics — this initiative only moves `variable_map` pointers between handlers.

### Non-Goals
- Does not touch the SET, MERGE, or CREATE implementations themselves (they work correctly when invoked with the right var_map).
- Does not touch transform-layer SQL generation. T-0191 (multi-property MATCH alias reuse) and T-0185 (UNWIND-scope property access) are **independent transform fixes** and are explicitly scoped out.
- Does not redesign the dispatcher architecture. Surgical patches to existing handlers plus two or three new dispatch entries.

## Success Criteria

- [ ] `CREATE (n:L {k:v}) SET n.x = 1` persists both `k` and `x`.
- [ ] `CREATE (n) SET n += {k: v}` applies the map-merge.
- [ ] `MERGE (n) SET n.x = 1` persists `x` whether the MERGE created or matched.
- [ ] `MATCH (a) MATCH (b) MERGE (a)-[r:T]->(b) SET r.k = v` persists `k` on the merged edge (no "Unbound variable" error).
- [ ] `MATCH (a) MATCH (b) CREATE (a)-[:T]->(b)` reuses matched `a` and `b` — no phantom node.
- [ ] `MATCH (a) MATCH (b) CREATE (a)-[:T {k: $p}]->(b)` works end-to-end with parameters.
- [ ] `UNWIND ... CREATE ... SET` path remains green (no regression).
- [ ] Regression test file (or additions to `39_issue_regression_tests.sql`) covers every combination above.
- [ ] Full functional suite passes with no new failures.

## Affected files (all in `src/backend/executor/`)

- `query_dispatch.c` — dispatch table entries + handler bodies (largest surface).
- `executor_merge.c` — add `execute_merge_clause_with_varmap(out)` variant.
- `executor_match.c` — `execute_match_create_query` + siblings: iterate ALL MATCH clauses.
- `executor_create.c` — already exposes `execute_create_clause_with_varmap` (no change expected).
- `executor_set.c` — `execute_set_operations` is the existing entry point; no change expected.

## Proposed Decomposition (7 tasks)

Tasks are ordered to land incrementally — each is a self-contained slice that ships a shippable regression test.

1. **Expose `execute_merge_clause_with_varmap`** — refactor `execute_merge_clause`/`execute_merge_clause_with_vars` to optionally emit the internal var_map through an out-parameter (mirrors `execute_create_clause_with_varmap` signature). S.
2. **Wire `handle_create` to run trailing SET** — detect `CLAUSE_CREATE | CLAUSE_SET` in the dispatch table, replace handler with one that calls `execute_create_clause_with_varmap` then `execute_set_operations`. Add regression tests for literal SET, `SET +=`, `SET $p`. S. *Resolves T-0188 for the CREATE case.*
3. **Wire `handle_merge` to run trailing SET** — same shape as #2 using the variant from #1. Regression tests for `MERGE ... SET n.x`, `MERGE ... SET n +=`. S. *Resolves T-0188 for MERGE.*
4. **Wire `handle_match_merge` to run trailing SET** — dispatch table already allows `MATCH+MERGE+SET`; handler needs to thread the MERGE var_map into `execute_set_operations`. Regression tests for `MATCH (a) MERGE (a)-[r]->(b) SET r.x`. M. *Resolves T-0189.*
5. **Multi-MATCH binding aggregation** — teach `execute_match_create_query` (and MATCH+MERGE, MATCH+SET, MATCH+REMOVE, MATCH+DELETE) to iterate every `AST_NODE_MATCH` clause in `query->clauses`, run each transform in sequence, and UNION the resulting variable bindings into a single var_map before the write. Regression tests for the full `MATCH (a) MATCH (b) CREATE (a)-[:T]->(b)` shape. M–L. *Resolves T-0190.*
6. **Smoke test across combinations** — matrix row aligned with GQLITE-I-0035 (semantic coverage matrix): every write shape × trailing-clause shape. Goal is to catch any combination still dropped by dispatch. S.
7. **Close out the blocked bugs** — re-run T-0188/T-0189/T-0190 repros, confirm green, transition those tickets to completed; link this initiative in their status updates. Update the `11_issue_repro_expected_failures.sql` → `39_issue_regression_tests.sql` promotion list. XS.

## Dependencies & sequencing

- **Blocked bugs cleared at end:** T-0188 (task 2+3), T-0189 (task 4), T-0190 (task 5).
- **Independent from this initiative:** T-0185 (UNWIND+MATCH transform), T-0191 (multi-property MATCH alias), GQLITE-T-0192 (diagnostics API). These can proceed in parallel.
- **Related:** GQLITE-I-0035 (semantic coverage matrix) provides the regression-test scaffolding; filling the matrix cells exercised here is a natural pairing.

## Risks

- **Dispatch table explosion.** Adding `CREATE+SET`, `MERGE+SET`, `MATCH+CREATE+SET` etc. combinatorically grows the table. Mitigate by folding SET detection into the existing `handle_*` handlers (check `has_post_set` via `find_set_clause`) rather than adding N new entries.
- **Var_map ownership.** The existing code frees var_maps eagerly on handler exit; introducing pass-through ownership needs careful free-path audit. Valgrind or ASAN run on the functional suite after each task.
- **Multi-MATCH binding collisions.** If two MATCH clauses bind the same variable name, the spec says the second rebinds. Need to choose: last-wins, or reject ambiguous rebind at transform time. Recommend last-wins to match Neo4j behavior; document the choice.
- **Interaction with WITH.** `WITH` already resets/re-projects scope. The UNWIND+WITH path at `query_dispatch.c:1028` has precedent. Out of scope to unify but should not regress.

## Estimated size

- **Complexity:** M (medium)
- **Calendar:** ~1 week of focused work for one engineer, including tests.
- **Risk level:** Low — existing handlers and helpers do all the semantic work; this is a plumbing exercise with strong precedent in the UNWIND+CREATE+SET path.

## Open Questions

- Should `handle_create` / `handle_merge` detect trailing SET internally (simpler; one dispatch entry) or should new `CREATE+SET` / `MERGE+SET` dispatch entries be added (more explicit)? Recommend internal detection — fewer table entries, lower maintenance.
- Does `RETURN` after `MERGE` / `CREATE` need the same treatment? Empirically `CREATE ... RETURN` works today via `handle_create_return` — confirm during task 2.
- Confirm whether `FOREACH` and `CALL {}` paths also have silent var_map drops. The `CALL` subquery scope rewrite (GQLITE-T-0173 → GQLITE-T-0179) landed recently; spot-check as part of task 6.

## Status

- 2026-04-18: Initiative created from Ralph loop investigation of issue #61 bugs. Five of the seven sub-bugs traced to this single defect.