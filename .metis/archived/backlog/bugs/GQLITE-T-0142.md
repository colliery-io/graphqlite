---
id: optional-match-ignores-label
level: task
title: "OPTIONAL MATCH ignores label filter and drops null rows"
short_code: "GQLITE-T-0142"
created_at: 2026-03-28T00:46:58.952686+00:00
updated_at: 2026-03-28T02:15:35.807947+00:00
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

# OPTIONAL MATCH ignores label filter and drops null rows

**GitHub Issue**: #34
**Priority**: P1 - High

## Objective

Fix OPTIONAL MATCH to respect label filters on target patterns and preserve null rows when no match is found.

## Bug Description

Two related symptoms:
1. `OPTIONAL MATCH (a)-->(r:Car)` ignores the `:Car` label filter and returns `:Pet` nodes instead
2. When OPTIONAL MATCH finds no results (e.g., WHERE filters everything), zero rows are returned instead of rows with null columns

## Root Cause

In `src/backend/transform/transform_match.c` (lines 763, 951), label joins are hardcoded to `SQL_JOIN_INNER` regardless of whether the parent node uses `SQL_JOIN_LEFT` for OPTIONAL MATCH. The INNER JOIN on `node_labels` effectively converts the LEFT JOIN back to an INNER JOIN, dropping unmatched rows.

## Reproduction

```cypher
-- Setup
CREATE (a:Person {id: 'alice'})
CREATE (r:Pet {id: 'rex'})
MATCH (a:Person), (r:Pet) CREATE (a)-[:OWNS]->(r)

-- Bug 1: Returns rex instead of NULL
MATCH (a:Person) OPTIONAL MATCH (a)-->(r:Car) RETURN a.id, r.id
-- Actual: alice | rex   Expected: alice | NULL

-- Bug 2: Returns empty instead of row with nulls
MATCH (a:Person) OPTIONAL MATCH (a)-->(r:Pet) WHERE r.name = 'nonexistent' RETURN a.id, r.id
-- Actual: []   Expected: alice | NULL
```

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `OPTIONAL MATCH (a)-->(r:Label)` only matches nodes with the specified label
- [ ] Unmatched OPTIONAL MATCH returns rows with null columns (not empty results)
- [ ] Label joins use LEFT JOIN when inside an OPTIONAL MATCH context
- [ ] Existing MATCH label filtering still works correctly
- [ ] Repro tests pass: `TestIssue34` in `test_issue_repro.py`, tests 34a/34b in `11_issue_repro.sql`

## Affected Files

- `src/backend/transform/transform_match.c` — propagate `optional` flag to label joins

## Status Updates

### 2026-03-27: Label filter fix implemented (partial)

**Fixed:** OPTIONAL MATCH now respects label filters on target nodes. `OPTIONAL MATCH (a)-->(r:Car)` no longer returns `:Pet` nodes — it correctly returns NULL when no `:Car` node exists.

**Change:** `src/backend/transform/transform_match.c` — in `generate_relationship_match()`, for OPTIONAL MATCH with labeled target nodes, fold the label condition into the target node's LEFT JOIN ON clause using `EXISTS (SELECT 1 FROM node_labels ...)` instead of a separate INNER JOIN.

**Not fixed:** WHERE clause after OPTIONAL MATCH still drops rows instead of preserving NULLs. This is a deeper issue: `WHERE r.name = 'nonexistent'` after OPTIONAL MATCH becomes a regular SQL WHERE that converts the LEFT JOIN to an effective INNER JOIN. The correct fix requires moving WHERE conditions that reference optional variables into the LEFT JOIN ON clause. This should be a separate task.

**Test results:**
- 921/921 C unit tests pass
- `TestIssue34::test_label_filter_respected` — PASSES
- `TestIssue34::test_null_rows_preserved_with_where` — still fails (WHERE semantics, separate issue)
- All 43 functional test files pass