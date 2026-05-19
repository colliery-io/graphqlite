---
id: module-decomposition-split-god
level: initiative
title: "Module Decomposition — split god files along natural seams"
short_code: "GQLITE-I-0040"
created_at: 2026-05-19T14:17:50.941661+00:00
updated_at: 2026-05-19T14:17:50.941661+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/discovery"


exit_criteria_met: false
estimated_complexity: M
initiative_id: module-decomposition-split-god
---

# Module Decomposition — split god files along natural seams

## Context

Three source files have grown well past their intended abstraction
boundary, identified as Risk 1 in the architectural review of 2026-05-19:

| File | LOC | Intended role | Actual role |
|---|---|---|---|
| `src/backend/executor/query_dispatch.c` | 3,829 | Thin dispatcher to specialized executors | Dispatcher + 800-line `handle_call_subquery` + 383-line `handle_merge_with_pipeline` + 400 lines of RETURN projection helpers |
| `src/extension.c` | 3,404 | SQLite extension entry + `cypher()` UDF | Entry + ~30 helper UDF implementations (`_gql_bool`, `_gql_normalize_date`, `_gql_in`, `_gql_dyn_add`, regex, `cypher_validate`, etc.) |
| `src/backend/transform/transform_func_list.c` | 1,979 | Cypher list functions | Junk drawer: list + temporal + JSON + geometry/point + duration + type-conversion |

All three are on the hot path for most Cypher queries, so every feature
touches them. They are a friction tax on every PR. The splits are mechanical
— pure code moves, no behavior change. Each split unlocks easier review and
reduces merge conflicts.

This initiative also resolves Risk 5 (helper UDFs registered from the wrong
layer). The transform/executor layers currently call
`graphqlite_register_helper_udfs` defensively because the UDF
implementations live in `extension.c`. Moving them into
`src/backend/runtime/` fixes the inverted dependency.

## Goals

- **G1**: No source file in `src/` exceeds ~1,500 LOC, except generated code
  under `src/generated/`.
- **G2**: Each split is one PR — clean code move, headers updated, build
  green, unit + functional + TCK clean, zero scenario delta.
- **G3**: New file names follow existing conventions: `executor_*` for
  execution, `udf_*` for helper UDFs, `transform_func_*` for Cypher function
  categories.
- **G4**: `extension.c` no longer contains helper UDF implementations.
  Transform/executor layers depend on `src/backend/runtime/` instead of the
  extension entry point.

## Non-Goals

- Behavior change of any kind. Every move verified by zero TCK delta.
- API rename or signature change for moved functions.
- The `append_sql` → `sql_builder` migration (GQLITE-I-0039 — runs
  separately, but ordering matters; see Operational Notes).
- Header normalization beyond what each split forces (GQLITE-I-0041 covers
  the broader cleanup).
- Splitting any file under 1,500 LOC. Don't go looking for additional
  victims.

## Detailed Design

### Split 1: `query_dispatch.c` (3,829 LOC → ~1,500)

`query_dispatch.c` started as a declarative pattern registry
(`query_pattern[]` table, ~200 LOC) plus thin wrapper handlers. Two large
handlers and a body of result-projection helpers grew inside it. Carve
them out:

- `executor_call_subquery.c` — `handle_call_subquery` and its helpers
  (~800 LOC, currently at lines 3029–3829).
- `executor_merge_pipeline.c` — `handle_merge_with_pipeline` and its
  helpers (~400 LOC, currently at lines 1250–1633).
- `executor_result_project.c` — `project_return_row_from_var_map`,
  `set_return_column_names`, `compute_aggregate_for_varmap`,
  `fetch_node_property_int`, `get_agg_function_name`, `has_aggregates`,
  `synthesize_delete_return` (~400 LOC, currently scattered around
  lines 753–2656).

Remaining in `query_dispatch.c`: pattern registry table,
`analyze_query_clauses`, `find_matching_pattern`, `dispatch_query_pattern`,
clause-finder helpers, simple wrapper handlers.

### Split 2: `extension.c` (3,404 LOC → ~700)

`extension.c` should be only: SQLite init, `cypher()` UDF, result
formatting, the small `graphqlite_test` / `gql_load_graph` /
`gql_unload_graph` / `gql_reload_graph` / `gql_graph_loaded` functions.
Helper UDF implementations move to a new `src/backend/runtime/` directory.

- `src/backend/runtime/udf_core.c` — `_gql_bool`, `_gql_bool_str`,
  `_gql_to_bool_strict`, `_gql_to_int_strict`, `_gql_to_float_strict`,
  `_gql_to_string_strict`, `_gql_in`, `_gql_eq`, `_gql_subscript`,
  `_gql_order_key`, `_gql_extract_ns`, `_gql_dyn_add`, `_gql_dyn_sub`.
- `src/backend/runtime/udf_temporal.c` — `_gql_normalize_date`,
  `_gql_normalize_time`, `_gql_normalize_datetime`, `_gql_duration_compose`,
  `_gql_duration_parse_iso`, `_gql_temporal_field`, `_gql_tz_offset_for`,
  `_gql_extract_tz`, `_gql_strip_tz`, `_gql_duration_from_total_ns`,
  `_gql_temporal_diff_ns`, `_gql_duration_calendar`,
  `_gql_duration_in_days` / `_months` / `_seconds`, `_gql_date_compose`,
  `_gql_time_compose`.
- `src/backend/runtime/udf_misc.c` — `regexp_func`, `cypher_validate_func`.
- `src/backend/runtime/udf_register.c` — owns
  `graphqlite_register_helper_udfs`. The transform and executor layers
  depend on this header instead of forward-declaring
  `extern int graphqlite_register_helper_udfs(sqlite3 *db);` from
  `extension.c`.

### Split 3: `transform_func_list.c` (1,979 LOC → ~600)

The junk drawer. Carve along the natural function categories:

- `transform_func_temporal.c` — date, time, datetime, localdatetime,
  duration, all `*_truncate_function`, `duration_between`, `duration_in`,
  `date_add`, `datetime_from_epoch`.
- `transform_func_geo.c` — `point_function`, `point_distance_function`,
  `point_within_bbox_function`.
- `transform_func_json.c` — `json_get_function`, `json_keys_function`,
  `json_type_function`, `isempty_function`.
- `transform_func_typeconv.c` — `tostring_function`,
  `type_conversion_function`, `type_conversion_ornull_function`,
  `valuetype_function`, `nullif_function`.

Remaining in `transform_func_list.c`: actual list operators — `coalesce`,
`list_function` (range, head, tail, etc.), `range_function`,
`collect_function`, `length_function` for lists.

### Header updates

Each new `.c` gets a paired header — `transform_func_temporal.h`,
`udf_register.h`, etc. The existing manifest header `transform_functions.h`
will be deleted in GQLITE-I-0041; don't re-add to it here.

## Alternatives Considered

- **One mega-PR with all three splits.** Rejected — the diff would touch
  headers, Makefile, and ~6,000 LOC of moves. Unreviewable. Sequenced
  per-split commits are safer.
- **Wait until GQLITE-I-0039 (SQL consolidation) completes.** Rejected —
  SQL consolidation touches `transform_func_*.c` heavily; doing it before
  the split means migrating a 1,979-line file. Doing the split first gives
  the SQL initiative smaller, focused files to migrate.
- **Split further (every `transform_func_*.c`, every `executor_*.c`).**
  Rejected — only the three files above are over 1,500 LOC. Splitting
  smaller files is unnecessary churn.
- **Split `transform_return.c` (1,656 LOC) and `transform_match.c`
  (1,492 LOC) too.** Considered, deferred. They're at the edge of the
  threshold and structurally single-purpose. Re-evaluate after Phase 3.

## Implementation Plan

### Phase 1 — `query_dispatch.c` split

- **M1**: Move `handle_call_subquery` + helpers → `executor_call_subquery.c`.
- **M2**: Move `handle_merge_with_pipeline` + helpers →
  `executor_merge_pipeline.c`.
- **M3**: Move RETURN-projection helpers → `executor_result_project.c`.

### Phase 2 — `extension.c` / runtime layer

- **M4**: Create `src/backend/runtime/` directory; move helper UDFs to
  `udf_core.c`. Update Makefile + headers.
- **M5**: Move temporal UDFs to `udf_temporal.c`.
- **M6**: Move regex + `cypher_validate` to `udf_misc.c`.
- **M7**: Move `graphqlite_register_helper_udfs` to `udf_register.c`. Update
  transform/executor to depend on `runtime/udf_register.h` instead of the
  `extern` declaration. Delete the
  `extern int graphqlite_register_helper_udfs(...)` forward-decls from
  `cypher_transform.c` and `cypher_executor.c`.

### Phase 3 — `transform_func_list.c` split

- **M8**: Move temporal functions → `transform_func_temporal.c`.
- **M9**: Move geo functions → `transform_func_geo.c`.
- **M10**: Move JSON helpers → `transform_func_json.c`.
- **M11**: Move type-conversion functions → `transform_func_typeconv.c`.

### Phase 4 — Final pass

- **M12**: Re-measure LOC. Confirm all source files under threshold. Update
  `.metis/code-index.md` (re-run `metis index`).

## Exit Criteria

- [ ] No `src/**/*.c` over 1,500 LOC except `src/generated/`.
- [ ] `src/extension.c` is under 800 LOC.
- [ ] `src/backend/runtime/` directory exists with `udf_core.c`,
      `udf_temporal.c`, `udf_misc.c`, `udf_register.c`.
- [ ] No `extern int graphqlite_register_helper_udfs` declarations outside
      `src/backend/runtime/`.
- [ ] Each split commit shows zero TCK scenario delta.
- [ ] Code index regenerated and committed.
- [ ] All 12 child tasks (M1–M12) closed.

## Operational Notes

- **Ordering vs GQLITE-I-0039 (SQL consolidation):** prefer to land Phase 3
  of *this* initiative (transform_func_list.c split) **before** Phase 3-S12
  of the SQL initiative. Migrating four files of ~500 LOC each is easier
  than migrating one of 1,979 LOC. The other two splits
  (`query_dispatch.c`, `extension.c`) are independent of SQL consolidation.
- **Per-PR verification protocol:** every move PR runs
  `angreal build extension && angreal test unit && angreal test functional && angreal test tck`.
  The TCK output JSON is compared byte-for-byte against the baseline at PR
  start — any scenario movement is grounds for revert.
- Use `git mv` where possible to preserve blame; if blocked by partial
  moves, add an explicit "moved from path/to/file.c:NN" comment at the top
  of new files.
