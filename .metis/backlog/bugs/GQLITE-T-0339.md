---
id: match-create-first-row-only
level: task
title: "MATCH+CREATE only processes the first matched row"
short_code: "GQLITE-T-0339"
created_at: 2026-05-27T00:00:00.000000+00:00
updated_at: 2026-05-27T00:00:00.000000+00:00
archived: false
tags:
  - "#task"
  - "#phase/backlog"
exit_criteria_met: false
backlog_category: bug
initiative_id: NULL
---

# MATCH+CREATE only processes the first matched row

## Objective

Fix the write-path so `MATCH (x) ... CREATE (...)` executes the CREATE **once
per matched row**, not just for the first match — and so a subsequent CREATE
clause that references variables created by an earlier CREATE clause fires per
row too.

## Backlog Item Details

### Type
- [x] Bug - Production issue that needs fixing

### Priority
- [x] P1 - High (blocks correct write semantics and several TCK setups)

### Impact Assessment
- **Affected scenarios**: Any `MATCH … CREATE …` over a multi-row match. Blocks
  the setup of Match5 [25]/[26]/[28]/[29] and Match4 [4] (so they read 0 rows),
  and likely more across the suite.
- **Reproduction** (3 `:D` nodes in the graph):
  1. `MATCH (d:D) CREATE (e:E {name: d.name})` → `nodes_created = 1` (expect 3).
  2. `MATCH (d:D) CREATE (d)-[:LIKES]->(x:X)` → `nc=1 rc=1` (expect 3, 3).
  3. `MATCH (d:D) CREATE (g:G {name:d.name}) CREATE (d)-[:HAS]->(g)`
     → `nc=1 rc=0` (expect 3, 3).
- **Expected vs actual**: CREATE should iterate every MATCH row; instead only
  the first row's CREATE runs. The second CREATE clause (relationship between a
  matched node and a first-clause-created node) creates **0** relationships.

### Root cause (suspected)

`execute_match_create_query` / `handle_match_create` appears to bind a single
match row (or take the first) and run CREATE once, rather than looping over all
match rows. The two-CREATE-clause rel=0 case suggests the second clause doesn't
see the per-row binding of the first clause's new variables.

## Acceptance Criteria

- [ ] `MATCH (d:D) CREATE (e:E)` creates one node per matched `d`.
- [ ] `MATCH (d:D) CREATE (d)-[:R]->(x)` creates one rel + node per matched `d`.
- [ ] Multi-CREATE-clause (`CREATE (g) CREATE (d)-[:R]->(g)`) fires per row,
  with the second clause seeing the first clause's new bindings.
- [ ] Zero TCK regressions; unit + functional clean.
- [ ] Re-run Match5 [25]/[26]/[28]/[29] and Match4 [4] (should unblock; verify).

## Implementation Notes

Discovered during I-0047 P2 ([[GQLITE-T-0335]]) — the Match5 multi-segment
varlen failures turned out to be this setup bug, not path matching (varlen
chains match correctly). This is a write-path concern, intentionally kept out
of I-0047 (Pattern & Path Matching Completeness). Candidate home: a CREATE/
write-semantics initiative, or promote standalone.

## Status Updates

### 2026-05-27 — Filed from I-0047 P2 investigation

Characterized via execution harness (see [[GQLITE-T-0335]] status notes).
