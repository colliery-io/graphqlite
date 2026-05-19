---
id: m3-move-return-projection-helpers
level: task
title: "M3: Move RETURN-projection helpers → executor_result_project.c"
short_code: "GQLITE-T-0276"
created_at: 2026-05-19T14:46:43.530726+00:00
updated_at: 2026-05-19T14:46:43.530726+00:00
parent: GQLITE-I-0040
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0040
---

# M3: Move RETURN-projection helpers → executor_result_project.c

## Parent Initiative

[[GQLITE-I-0040]]

## Objective

Helpers scattered around lines 753–2656 of `query_dispatch.c`: `project_return_row_from_var_map`, `set_return_column_names`, `compute_aggregate_for_varmap`, `fetch_node_property_int`, `get_agg_function_name`, `has_aggregates`, `synthesize_delete_return`. Carve out into `executor_result_project.c`. Also unblocks I-0041 C12 (MATCH+SET+RETURN refactor) which needs a clean home for the projection code.

## Acceptance Criteria

- [ ] New file `executor_result_project.c` exists with all listed helpers.
- [ ] `query_dispatch.c` is ~400 LOC smaller.
- [ ] Symbols are exposed via a new header for cross-module use.
- [ ] TCK delta zero.

## Status Updates

*To be added during implementation*
