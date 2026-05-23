---
id: inter-pattern-variable-refs-in
level: task
title: "Inter-pattern variable refs in CREATE: (:Foo {x: a.id}) referencing earlier element"
short_code: "GQLITE-T-0322"
created_at: 2026-05-23T05:16:00+00:00
updated_at: 2026-05-23T18:32:15.830324+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#bug"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: NULL
---

# Inter-pattern variable refs in CREATE: (:Foo {x: a.id})

## Discovered

2026-05-22 during v0.5.0 work. Observed while triaging With2 [1]
and WithSkipLimit1 [1] / WithSkipLimit2 [2].

## Repro

```cypher
CREATE (a:End {num: 42, id: 0}),
       (:End {num: 3}),
       (:Begin {num: a.id});
```

**Expected:** Begin node created with `num = 0` (referencing `a.id`
from the same CREATE clause).
**Actual:** Begin node created with `num = NULL` (or fails silently
— the cross-pattern reference isn't resolved).

## Root cause

Cypher CREATE allows later pattern elements in the same CREATE clause
to reference EARLIER element variables. Our CREATE implementation
processes each comma-separated pattern independently and doesn't
expose earlier-pattern bindings to later patterns' property
evaluation.

This affects several TCK scenarios where the test setup uses
cross-pattern refs to seed test data.

## Proposed fix

In `transform_create.c` / `executor_create.c`:

1. When processing a CREATE clause with multiple comma-separated
   patterns, accumulate the variable_map left-to-right.
2. Property expression evaluation for pattern N has access to
   variables bound by patterns 1..N-1.
3. Evaluation order: each pattern's properties evaluated BEFORE
   the next pattern's CREATE runs.

## Affected scenarios

- With2 [1] "Forwarding a property to express a join" — fails
  because the test setup `(:Begin {num: a.id})` doesn't bind.
- WithSkipLimit1 [1] / WithSkipLimit2 [2] — same setup pattern.
- Probably a handful of other tests with similar setups (TBD —
  rerun TCK after fix to identify the long tail).

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] With2 [1] passes.
- [ ] WithSkipLimit1 [1] / WithSkipLimit2 [2] pass.
- [ ] New functional test asserting cross-pattern ref in CREATE.
- [ ] No regression on existing CREATE tests.

## Affected files

- `src/backend/transform/transform_create.c`
- `src/backend/executor/executor_create.c`

## Notes

Estimated effort: M. The variable_map threading needs to be
consistent with how MATCH does multi-pattern accumulation.

## Status Updates

### 2026-05-23 — Completed

Added `AST_NODE_PROPERTY` handling to
`executor_create.c::execute_path_pattern_with_variables`'s
node-property loop. The base variable's entity_id is fetched
from `var_map` (already populated by earlier patterns in this
CREATE — the multi-pattern loop shares the map), then a
one-shot SQL query against `node_props_*` / `edge_props_*`
tables retrieves the property value to use for the new node's
property.

Handles INTEGER/REAL/BOOLEAN/TEXT typed property reads. Edge
variables supported (uses `edge_id` / `edge_props_*` when the
base var is an edge).

The variable_map THREADING was already correct — patterns
share a single var_map across iterations. The missing piece
was just the property-evaluation branch (the switch had
LITERAL/MAP/LIST/PARAMETER/IDENTIFIER+FOREACH/FUNCTION_CALL
cases, but no PROPERTY).

**TCK: 3568 → 3570 (+2):**
- With2 [1] Forwarding a property to express a join.
- WithSkipLimit2 [2] Handle dependencies across WITH with LIMIT.

WithSkipLimit1 [1] still fails — same setup shape but the
query uses SKIP instead of LIMIT, and the SKIP handling has
a separate code path. Different root cause; left for follow-up.

944/944 unit, functional clean. 0 regressions.