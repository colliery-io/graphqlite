---
id: s1-inventory-append-sql-append
level: task
title: "S1: Inventory append_sql / append_identifier / append_string_literal call sites"
short_code: "GQLITE-T-0255"
created_at: 2026-05-19T14:43:48.666701+00:00
updated_at: 2026-05-19T14:43:48.666701+00:00
parent: GQLITE-I-0039
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0039
---

# S1: Inventory append_sql / append_identifier / append_string_literal call sites

## Parent Initiative

[[GQLITE-I-0039]]

## Objective

Catalog every call site of `append_sql`, `append_identifier`, and `append_string_literal` in the transform layer. Per-file count, format-string shape, arg types, logical position (SELECT/FROM/WHERE/etc.). Output goes to `docs/internal/sql-migration-inventory.md` and becomes the input to S3's capability gap analysis.

## Acceptance Criteria

- [ ] `docs/internal/sql-migration-inventory.md` exists, lists every call site grouped by file.
- [ ] Each call site is annotated with its unique format-string shape and logical clause position.
- [ ] Per-file totals match `grep -c 'append_sql\b' src/backend/transform/*.c`.

## Status Updates

*To be added during implementation*
