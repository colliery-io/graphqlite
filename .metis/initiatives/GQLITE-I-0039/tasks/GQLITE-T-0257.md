---
id: s3-capability-gap-analysis-append
level: task
title: "S3: Capability gap analysis — append_sql shapes vs sql_builder API"
short_code: "GQLITE-T-0257"
created_at: 2026-05-19T14:44:03.160351+00:00
updated_at: 2026-05-19T14:44:03.160351+00:00
parent: GQLITE-I-0039
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0039
---

# S3: Capability gap analysis — append_sql shapes vs sql_builder API

## Parent Initiative

[[GQLITE-I-0039]]

## Objective

For each unique shape catalogued in S1/S2, name the `sql_builder` API that currently handles it. Flag any shape with no equivalent as "gap → need new API". Drives the API extensions in S4 (gap fills) and Phase 4 (CTE-prepend section).

## Acceptance Criteria

- [ ] Every shape from S1/S2 mapped to either an existing `sql_builder` function or a named gap.
- [ ] Gaps tracked as concrete extension items (function name, signature, return semantics).

## Status Updates

*To be added during implementation*
