---
id: temporal-cluster
level: task
title: "Temporal cluster: duration arithmetic, parsing, selection, DST"
short_code: "GQLITE-T-0341"
created_at: 2026-05-30T14:17:28.737710+00:00
updated_at: 2026-05-30T14:17:28.737710+00:00
parent: GQLITE-I-0049
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"

exit_criteria_met: false
initiative_id: tck-deep-item-conformance-temporal
---

# Temporal cluster: duration arithmetic, parsing, selection, DST

## Parent Initiative
[[GQLITE-I-0049]]

## Objective

Close the Temporal TCK cluster (~42 failing examples across Temporal1/2/3/5/7/8/10)
with zero regressions. Temporal code lives in `src/backend/transform/transform_func_temporal.c`
and the temporal UDFs/helpers in `src/backend/runtime/udf_helpers.c` (duration JSON
builder, ISO parsing, epoch-ns conversions). Durations are stored as the JSON object
`{"_iso8601": "...", "months": M, "days": D, "seconds": S, "nanosecondsOfSecond": N}`.

## Sub-steps (decomposition, smallest-yield-first ordering TBD after analysis)

1. **Duration arithmetic — Temporal8 (~12)**: add/subtract durations; add/subtract a
   duration to/from date/time/localtime/datetime; multiply/divide a duration by a number.
   Likely the biggest single win; concentrated in duration +/-/*//.
2. **Duration between + DST + large — Temporal10 (~10)**: `duration.between` variants,
   durations across daylight-saving boundaries, very large durations (seconds overflow).
3. **Date/time selection — Temporal3 (~9)**: `date({...})` / `time({...})` /
   `datetime({...})` selecting/combining component fields (truncation/selection forms).
4. **Parsing edge cases — Temporal2 (~6)**: parse date/time/duration from string,
   named time zones (`[Europe/Stockholm]`), fractional-second edge cases.
5. **Construction / accessors / comparison — Temporal1/5/7 (~5)**: construct duration
   (example 3), time-offset construction, datetime accessors, time & duration equality.

## Method

Per sub-step: enumerate failing examples + read each scenario's query and expected
output; reproduce via `sqlite3 :memory:` (NOT ad-hoc chains — single queries through
the harness for verification); find the common root cause; fix; rigorous full pass-set
diff after each (zero regressions); unit 944/944; functional clean. Record findings here.

## Acceptance Criteria
- [ ] Temporal8 duration arithmetic examples pass
- [ ] Temporal10 duration-between / DST / large examples pass
- [ ] Temporal3 selection examples pass
- [ ] Temporal2 parsing examples pass
- [ ] Temporal1/5/7 construction/accessor/comparison examples pass
- [ ] Zero TCK regressions across the whole cluster; unit 944/944; functional clean

## Status Updates

- 2026-05-30: Task created with sub-step breakdown from the failure inventory.
  Baseline main = 3728 pass. Starting analysis with sub-step 1 (Temporal8 duration
  arithmetic) — biggest concentrated win.
