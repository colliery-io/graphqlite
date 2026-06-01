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

### 2026-05-30: Sub-step 1 (Temporal8) deep analysis

Durations are stored as JSON `{"_iso8601","months","days","seconds","nanosecondsOfSecond"}`.
Reusable runtime helpers in `udf_helpers.c`: `is_duration_value()`, `dur_field_ll()`,
`emit_duration_json(ctx, months, days, total_ns)` (keeps months/days independent, only
splits total_ns→seconds+nanos), `apply_duration_to_temporal()`, `format_iso_duration()`.
ADD/SUB route through `gql_dyn_addsub_func` (`_gql_dyn_add`/`_gql_dyn_sub`), which already
handles dur+dur and dur+temporal.

Concrete Temporal8 bugs found:
- **[7] multiply/divide by number → returns `0`.** `BINARY_OP_MUL`/`DIV` in
  transform_expr_ops.c emit a bare ` * ` / ` / `; SQLite coerces the duration JSON text
  to 0. FIX: route MUL/DIV through new `_gql_dyn_mul`/`_gql_dyn_div` UDFs (mirror the
  ADD/SUB dispatch) that detect a duration operand and scale months/days/total_ns by the
  number (Cypher rounding for non-integer factors TBD — check expected values per example).
- **[2]-[5] duration ± temporal: wrong values** (e.g. local time off by hours:
  exp `22:29:27.5` vs act `17:14:54.5`). `apply_duration_to_temporal` mis-applies some
  components — investigate which (days/seconds vs hours). NOT just formatting.
- **[6] duration ± duration: wrong values AND wraps as object.** exp `P25Y4M43DT50H...`
  vs act `{_iso8601:'P25Y4M44DT20H...'}`. Two issues: (a) value differs by ~6h (computation
  or the test's input durations carry hours we mis-split between days/seconds), (b) the
  result renders as the full duration object, but the expected is the ISO string — the
  renderer already extracts `_iso8601` for duration values (udf_helpers.c:905), so check
  why the arithmetic result loses that path (likely the column type/rendering path).

NEXT (next work session): implement `_gql_dyn_mul`/`_gql_dyn_div` for durations ([7],
clearest win), then chase the value-computation discrepancy in apply_duration_to_temporal
([2]-[5]) and dur+dur ([6]). Verify each with rigorous pass-set diff.

### 2026-05-30: Sub-step 1 (Temporal8) COMPLETE — Temporal8 27/27

Two commits (branch i0049-temporal): duration mul/div (+3), fractional construction
+ date arithmetic (+11). Branch 3728 -> 3742. Key learnings (durations are NOT
normalized across components; 1 month = 30.436875 days, 1 day = 86400 s; only
date+duration rolls whole-day time into the date). Remaining Temporal:
Temporal10 (10, duration.between + DST), Temporal3 (9, date selection),
Temporal2 (6, parsing), Temporal1 [13]/Temporal5 [6]/Temporal7 [3] (1 each).
NEXT: Temporal10 duration.between.

### 2026-05-30: Sub-step 2 (Temporal10) partial — inMonths/inDays tz fix (+2)

duration.inMonths/inDays now compare time-of-day in UTC when both sides have tz
(Temporal10 [3] ex19, [4] ex17). Branch 3742 -> 3744. Remaining Temporal10 (8):
DST-aware durations [8] (5 — needs real DST transition handling, hard) and large
durations [9]/[10] (2 — int overflow in the calendar/seconds path). Deferring DST
+ overflow; next consider Temporal3 (date selection, 9) and Temporal2 (parsing, 6).

### 2026-05-30: Session checkpoint — branch i0049-temporal at 3744 (+16 vs main 3728)

Commits: duration mul/div (+3), fractional construction+date arith (+11),
inMonths/inDays tz (+2). Temporal8 fully closed (27/27).

Remaining Temporal (next sub-steps):
- Temporal3 [1] date selection (5): `date({date: other, quarter: N})` must
  preserve monthOfQuarter + day (we reset to quarter-start month + day 1, giving
  1984-07-01 vs expected 1984-08-11). Plus week/ordinalDay base-selection forms.
- Temporal3 [3] time() with named tz (2): `time(...)` must DROP the `[Region]`
  suffix, keeping only the offset (datetime keeps it; time drops it).
- Temporal3 [10] (2): datetime named-tz DST offset (+01 vs +02).
- Temporal2 parsing (6), Temporal1 [13], Temporal5 [6], Temporal7 [3].
- Temporal10 DST [8] (5) + large/overflow [9]/[10] (2) — hardest, deferred.

### 2026-05-31: checkpoint — branch i0049-temporal at 3749 (+21 vs main 3728)

Closed this session: Temporal8 (27/27), Temporal10 inMonths/inDays, Temporal3
quarter selection + time() region-drop, Temporal1 [12], Temporal7 [6].
7 commits, all rigorous-diff zero-regression, unit 944/944.

Remaining Temporal (precise, with root cause for next session):
- **Temporal2 [3]/[5]** (2): tz offset `-00:00`/`+00:00` must render as `Z`.
  Formatting fix in the offset emitters.
- **Temporal1 [13]** (3 examples): tz offset with seconds — `+02:05:00` -> `+02:05`
  (drop `:00`), `+02:05:59` -> keep `:59`. Offset parse must retain seconds and
  the formatter must emit `:SS` only when non-zero. (Currently offsets stored as
  minutes; need second precision.)
- **Temporal2 [7]** (2): parse duration FROM string `P22DT19H51M49.5S` — fractional
  / multi-field ISO duration parse broken (gives P22DT12H / PT0S).
- **Temporal3 [1] ex8/15** (2): date selection returns None — investigate which
  selection form (likely week/weekYear combo) crashes.
- **Temporal5 [6]** (1): datetime accessors (one accessor value off).
- **Temporal7 [3]** (1): compare times — boolean vector inverted (tz/offset compare).
- **DST cluster (hard, deferred): Temporal3 [10] (2), Temporal2 [6] (2),
  Temporal10 [8] (6)** — need real DST-transition handling at named-zone dates
  (named_tz_offset is a month approximation; these test exact transition days).
- **Overflow (deferred): Temporal10 [9]/[10] (2)** — int64 overflow in calendar/
  seconds path for billion-year / huge-second durations.

### 2026-05-31: Temporal cluster nearly closed — branch 3758 (+30 vs main 3728)

All non-DST Temporal now passes. Remaining = DST only (12):
- Temporal10 [8] (8): duration.inSeconds across the 2017-10-29 fall-back —
  needs across-transition elapsed-time (24 wall hrs = 25 real hrs).
- Temporal3 [10] (2) / Temporal2 [6] (2): named-zone offset on a DST-transition
  date (e.g. 1984-03-28 -> +02).
GOTCHA: named_tz_offset's month approximation (Apr–Sep summer) is LOAD-BEARING.
A clean last-Sunday-rule rewrite REGRESSED -29 (many passing tests depend on the
coarse rule). DST must be done empirically/per-date against the TCK's exact
expectations, NOT a simple rule swap. Recommend PR the +30 branch; tackle DST
as a separate focused task.

### 2026-06-01: DST cluster — ROOT CAUSE = needs an IANA tz database (NOT closeable by rules)

Dug into the 12 remaining DST scenarios empirically. Findings:
- **Why the offset approximation is load-bearing / risky:** `named_tz_offset` is
  called from many paths (construction, parsing, duration.between, accessors,
  rendering). The TCK encodes HISTORICALLY-ACCURATE IANA data, so any rule must
  match history exactly. A modern "last Sunday of March..October" EU rule
  REGRESSED -52 examples because pre-1996 EU DST ended the last Sunday of
  SEPTEMBER (e.g. 1984-10-11 Stockholm = +01:00, not +02:00). The OLD coarse
  Apr–Sep approximation happens to match those pre-1996 Sept endings.
- A regression-free improvement IS possible: historical end-month
  (`(y>=1996)?Oct:Sep`) + threading the real base date into the datetime
  named-tz offset lookup (`_gql_tz_offset_for(tz, _gql_date_compose(... $.date,
  $.datetime))`). Verified 0 regressions, and it fixes individual examples
  (e.g. Temporal3 [10] ex14) — but flips NO full scenario, so net +0. Reverted
  to keep the +30 branch coherent; re-derive from this note when doing IANA work.
- **Hard stop:** full DST is impossible without an embedded IANA tz database.
  Temporal2 [6] expects `1818-07-21 Stockholm = +00:53:28` — Local Mean Time
  before standardization. No rule produces that. Temporal10 [8] additionally
  needs across-transition elapsed time (24 wall-clock hrs = 25 real hrs on the
  fall-back day). Both require the real tzdata.
- RECOMMENDATION: close T-0341 as "Temporal non-DST done (+30)"; open a separate
  task "embed IANA tzdata for full temporal DST conformance" (large, data-heavy)
  for the 12 DST scenarios.

### 2026-06-01: DST gaps filed as bugs
Non-DST Temporal closed via PR #88 (main 3758, +30). The 12 DST scenarios are now
tracked as follow-up bugs:
- [[GQLITE-T-0342]] — duration across a DST transition (Temporal10 [8], 8)
- [[GQLITE-T-0343]] — named-zone offset coarse approximation / historical rules
  (Temporal3 [10], 2); documents the -52-regression trap
- [[GQLITE-T-0344]] — datetime string parse named-zone historical/LMT offset
  (Temporal2 [6], 2); the foundational "embed IANA tzdata" item the others depend on
