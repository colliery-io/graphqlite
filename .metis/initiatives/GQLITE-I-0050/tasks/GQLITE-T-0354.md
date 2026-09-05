---
id: 115-python-consolidate-the-two
level: task
title: "#115: python consolidate the two sanitize_rel_type implementations"
short_code: "GQLITE-T-0354"
created_at: 2026-09-05T13:18:47.456522+00:00
updated_at: 2026-09-05T13:18:47.456522+00:00
parent: GQLITE-I-0050
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0050
---

# #115: python consolidate the two sanitize_rel_type implementations

## Parent Initiative

[[GQLITE-I-0050]]

## Objective **[REQUIRED]**

Close GitHub #115: `BulkMixin._sanitize_rel_type` diverges from `utils.sanitize_rel_type` (reserved words pass through; empty → `REL` vs `REL_`), so bulk and Cypher paths store different types for the same input.

## Validation

Validated 2026-09-05 against a fresh `angreal build extension` build via the Python binding; behaviour reproduces exactly as filed. `("MATCH","","1abc","a-b")` → utils: `REL_MATCH, REL_, REL_1abc, a_b`; bulk: `MATCH, REL, REL_1abc, a_b`. Rust bulk already calls `crate::sanitize_rel_type`.

## Acceptance Criteria **[REQUIRED]**

- [ ] `BulkMixin._sanitize_rel_type` deleted; `bulk.py` imports `sanitize_rel_type` from `..utils`.
- [ ] `test_bulk_rel_type_matches_cypher_path`: for each input, type stored by `insert_edges_bulk` equals type stored by `upsert_edge`.

## Status Updates **[REQUIRED]**

- 2026-09-05: created, validated.
- 2026-09-05: `BulkMixin._sanitize_rel_type` deleted; bulk imports `sanitize_rel_type` from utils. `test_bulk_rel_type_matches_cypher_path` (4 inputs) + no-private-sanitizer test, green.
