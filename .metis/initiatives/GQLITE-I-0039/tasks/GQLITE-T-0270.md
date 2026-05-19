---
id: s16-define-transform-output-sub
level: task
title: "S16: Define transform_output sub-struct; move unified_builder into it"
short_code: "GQLITE-T-0270"
created_at: 2026-05-19T14:45:57.146381+00:00
updated_at: 2026-05-19T14:45:57.146381+00:00
parent: GQLITE-I-0039
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0039
---

# S16: Define transform_output sub-struct; move unified_builder into it

## Parent Initiative

[[GQLITE-I-0039]]

## Objective

Introduce `typedef struct { sql_builder *builder; ... } transform_output;` and replace `cypher_transform_context.unified_builder` with `transform_output *output`. All callers shift from `ctx->unified_builder` to `ctx->output->builder`.

## Acceptance Criteria

- [ ] `transform_output` struct defined and used as the canonical handle.
- [ ] `cypher_transform_context` no longer exposes `unified_builder` as a top-level field.
- [ ] All callers updated.
- [ ] TCK delta zero.

## Status Updates

*To be added during implementation*
