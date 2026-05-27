---
id: temporal-duration-type-completeness
level: initiative
title: "Temporal & Duration Type Completeness"
short_code: "GQLITE-I-0046"
created_at: 2026-05-27T02:25:05.547608+00:00
updated_at: 2026-05-27T02:25:05.547608+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/discovery"


exit_criteria_met: false
estimated_complexity: L
initiative_id: temporal-duration-type-completeness
---

# Temporal & Duration Type Completeness

## Context **[REQUIRED]**

This is the **C2** work that [[GQLITE-I-0044]] (TCK Conformance Push II)
explicitly scoped out as its own initiative. During the I-0044 push the
*contained* temporal failures were closed (Temporal5 accessors, Temporal6
ISO duration round-trip, offsetSeconds, time tz-strip — see I-0044 status
2026-05-26). What remains is the genuine **Duration type-system + temporal
arithmetic** implementation, which needs the exact openCypher/Neo4j semantics
in hand, not example reverse-engineering.

Durations are stored as a normalized JSON object
`{_iso8601, months, days, seconds, nanosecondsOfSecond}` (the `_iso8601`
key marks a value as a duration). The contained accessor/serialization work
is done; the arithmetic and fractional-composition cascade is not.

**~30 scenarios remain**, in `expressions/temporal/Temporal{2,3,8,10}.feature`.

## Goals & Non-Goals **[REQUIRED]**

**Goals:**
- **G1**: Correct Duration arithmetic — duration ± duration, duration ×/÷
  number, with the openCypher carry rules (no sub-day→day carry; fractional
  remainders cascade months→days→seconds using avg-month conventions).
- **G2**: `duration.between` / `inMonths` / `inDays` / `inSeconds` and
  DST-aware temporal differences.
- **G3**: Fractional-component composition (`duration({years: 12.5, ...})`).
- **G4**: Calendar week/quarter date construction.
- **G5**: Named-timezone resolution at `time({timezone:'Name'})` construction.
- **G6**: Temporal string-parsing edge cases (Temporal2).
- **G7**: Each change zero-regression (full TCK diff + unit + functional),
  per the [[GQLITE-I-0044]] verification recipe.

**Non-Goals:**
- **NOT** a temporal storage-format redesign — the normalized JSON stays.
- **NOT** a full IANA tzdata embed — expand `named_tz_offset` only as needed.
- **NOT** temporal features outside the failing TCK set.

## Detailed Design **[REQUIRED]**

Code locus: `src/backend/runtime/udf_helpers.c` (the `_gql_duration_*` /
`_gql_*_compose` / `_gql_normalize_*` / `_gql_temporal_field` UDFs) and
`src/backend/transform/transform_func_temporal.c` (construction emission).

### Failing clusters & root causes (from I-0044 investigation)

**D1 — Duration arithmetic (Temporal8 [6]/[7], Temporal10).** `duration ×/÷
number` returns 0 (the duration JSON is treated as numeric 0); `duration ±
duration` mis-carries. `*`/`/` are emitted as native SQL in
`transform_expr_ops.c::transform_binary_operation` (~line 518) — they need a
`_gql_dyn_mul`/`_gql_dyn_div` like the existing `_gql_dyn_add`/`_gql_dyn_sub`
(`gql_dyn_addsub_func`). Multiply is per-component (m×n, d×n, totalNs×n);
**divide carries** months→days→seconds (the example `÷2` of
P12Y5M14DT16H13M10.0…01S → P6Y2M22DT13H21M8S could not be reconciled with a
naive per-component divide — needs the exact Neo4j algorithm, likely 30-day
avg-month carry + day→second carry).

**D2 — Fractional composition (Temporal8 [2]-[5] example 3).** `duration({years:
12.5, months: 5.5, days: 14.5, ...})` must cascade fractional years→months→
days→seconds. `gql_duration_compose_func` partially does this but the result
diverges from expected on the fractional path; the avg-month/day conventions
need pinning to spec.

**D3 — `duration.between` + DST (Temporal10).** between two temporals in
months/days/seconds; DST-day handling. `gql_duration_calendar_func` /
`gql_duration_in_*` exist but are wrong on these.

**D4 — Calendar week/quarter dates (Temporal3 [1], 5 examples).** `date({year,
week, dayOfWeek})` and quarter selection compute the wrong date (ISO-week math).

**D5 — Named-tz at `time()` construction (Temporal3 [3] 17/19).**
`time({timezone:'Europe/Stockholm'})` stores `[Name]` with no offset; a `time`
must resolve to the numeric offset. Expose `named_tz_offset` as a UDF and call
it from the time() construction in transform_func_temporal.c (~line 282-296);
`_gql_time_compose` then inherits the offset. (`_gql_normalize_time` already
strips the bracket as of I-0044.)

**D6 — Temporal parsing (Temporal2 [3]/[5]/[6]/[7]).** time/datetime/named-tz/
duration string-parse edge cases.

### Reference

The canonical algorithm is Neo4j's `DurationValue` / `TemporalValue`
(openCypher TCK is generated against it). Acquire the spec/source semantics
for D1-D3 before implementing — these are not reverse-engineerable from the
examples (attempted in I-0044, division did not reconcile).

## Alternatives Considered **[REQUIRED]**

- **Store durations as total nanoseconds + total months** (two longs) instead
  of the JSON object. Rejected for now — the JSON form already round-trips
  (I-0044 Temporal6) and accessors work; a representation change is a larger,
  riskier refactor that doesn't itself close scenarios.
- **Embed full IANA tzdata.** Rejected — overkill; the `named_tz_offset` table
  covers the TCK zones. Expand per scenario.

## Implementation Plan **[REQUIRED]**

Decompose at pickup (human-driven, per Metis policy). Suggested tasks:
1. **T: Duration ×/÷ number** — `_gql_dyn_mul`/`_gql_dyn_div` + transform
   routing. (Temporal8 [7].) Start here; multiply is the simplest.
2. **T: Duration ± duration carry** + fractional composition cascade.
   (Temporal8 [2]-[6].) Needs the Neo4j carry algorithm.
3. **T: duration.between + DST.** (Temporal10.)
4. **T: Calendar week/quarter date construction.** (Temporal3 [1].)
5. **T: Named-tz resolution at time() construction.** (Temporal3 [3] 17/19.)
6. **T: Temporal string-parse edge cases.** (Temporal2.)

## Acceptance Criteria

- [ ] Temporal2/3/8/10 failing scenarios move to pass (target: the ~30 open).
- [ ] Zero TCK regressions vs the initiative-start baseline; unit 944/944 and
  functional clean on every PR.
- [ ] Each landed task notes its TCK delta in this initiative's status log.

## Status Updates

### 2026-05-27 — Created

Filed from the I-0044 temporal investigation (session reached 94.9%). Discovery
phase. Contained temporal wins already landed under I-0044; this initiative
owns the deep Duration type-system + arithmetic remainder. Awaiting human
review before decomposing into tasks.