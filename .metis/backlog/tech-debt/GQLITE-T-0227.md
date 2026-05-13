---
id: tck-harness-decode-cypher-json
level: task
title: "[TCK harness] Decode cypher() JSON result payload into structured rows"
short_code: "GQLITE-T-0227"
created_at: 2026-05-13T17:03:59.271837+00:00
updated_at: 2026-05-13T17:03:59.271837+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/backlog"
  - "#tech-debt"


exit_criteria_met: false
initiative_id: NULL
---

# Harness must decode `cypher()` JSON payload into structured rows

## Source
Filed during [[GQLITE-T-0211]] triage of the [[GQLITE-I-0037]] baseline run. See `docs/tck/baseline-2026-05-13.md`.

## Classification
- Type: tech-debt
- Priority: P1
- Affected TCK scenarios: 0

## Description

The TCK harness's `ExtensionBackend` receives the raw TEXT payload from `SELECT cypher(?)` and stores it as a single-cell row. When the TCK expects a multi-row result (`MATCH (n) RETURN n` over 3 nodes), the comparator compares 1 actual row against 3 expected rows and reports `row count: expected 3 got 1` — a false failure.

Fix: implement the same JSON decoding the Python binding does (see `bindings/python/src/graphqlite/connection.py` lines 162-209). The payload is JSON-encoded; parse into a list of `{column: value, ...}` dicts; shape `QueryResult.rows` accordingly so the comparator can match cell-by-cell.

This unblocks accurate triage of T-0222 (expected error not raised) and T-0223 (expected empty got rows), and likely shifts hundreds of current `fail` scenarios into `pass` or into more specific failure modes.

## Parent
Backlog item filed under initiative [[GQLITE-I-0037]] (openCypher TCK Conformance Audit).
