---
id: m5-move-temporal-udfs-from
level: task
title: "M5: Move temporal UDFs from extension.c → runtime/udf_temporal.c"
short_code: "GQLITE-T-0278"
created_at: 2026-05-19T14:46:58.998124+00:00
updated_at: 2026-05-19T14:46:58.998124+00:00
parent: GQLITE-I-0040
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0040
---

# M5: Move temporal UDFs from extension.c → runtime/udf_temporal.c

## Parent Initiative

[[GQLITE-I-0040]]

## Objective

Move date/time/datetime/duration UDFs: `_gql_normalize_date`, `_gql_normalize_time`, `_gql_normalize_datetime`, `_gql_duration_compose`, `_gql_duration_parse_iso`, `_gql_temporal_field`, `_gql_tz_offset_for`, `_gql_extract_tz`, `_gql_strip_tz`, `_gql_duration_from_total_ns`, `_gql_temporal_diff_ns`, `_gql_duration_calendar`, `_gql_duration_in_days/_months/_seconds`, `_gql_date_compose`, `_gql_time_compose`.

## Acceptance Criteria

- [ ] `src/backend/runtime/udf_temporal.c` exists.
- [ ] Build green, TCK delta zero.

## Status Updates

*To be added during implementation*
