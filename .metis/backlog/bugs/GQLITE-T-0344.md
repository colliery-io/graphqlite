---
id: dst-named-zone-string-parse-lmt
level: task
title: "DST: datetime string parse doesn't resolve named-zone historical/LMT offset"
short_code: "GQLITE-T-0344"
created_at: 2026-06-01T00:00:00.000000+00:00
updated_at: 2026-06-01T00:00:00.000000+00:00
parent: 
blocked_by: []
archived: false
tags:
  - "#task"
  - "#phase/backlog"
  - "#bug"
exit_criteria_met: false
initiative_id: NULL
---

# DST: datetime string parse doesn't resolve named-zone historical/LMT offset

## Type
- [x] Bug

## Priority
- [ ] P3 - Low (needs an embedded IANA tz database; largest/most data-heavy)

## Objective
Parsing a datetime string that carries a named zone must insert the resolved
numeric offset (including pre-standardization Local Mean Time) before the
`[Region]` suffix. Closes **Temporal2 [6]** (2 examples) and is the umbrella
"embed IANA tzdata" work the other two DST bugs depend on.

## Impact Assessment
- **Affected:** `gql_normalize_datetime_func` in `src/backend/runtime/udf_helpers.c`.
- **Reproduction / Expected vs Actual:**
  - `datetime('2015-07-21T21:40:32.142[Europe/London]')`
    expected `2015-07-21T21:40:32.142+01:00[Europe/London]`,
    actual `2015-07-21T21:40:32.142[Europe/London]` (no resolved offset inserted).
  - `datetime('1818-07-21T21:40:32.142[Europe/Stockholm]')`
    expected `1818-07-21T21:40:32.142+00:53:28[Europe/Stockholm]` — **Local Mean
    Time** (Stockholm's longitude-based offset before standardized zones in 1879).

## Root cause
1. The datetime string normalizer keeps the `[Region]` bracket but never computes
   and inserts the offset for the value's date.
2. `+00:53:28` cannot be produced by ANY rule — it is historical LMT data. This is
   the hard floor: full Temporal DST conformance requires embedding the real IANA
   tz database (transition instants + historical/LMT offset records). A rule-based
   approximation (see [[GQLITE-T-0343]]) cannot reach these cases.

## Suggested approach
- Embed a compact subset of IANA tzdata for the zones the TCK exercises
  (Europe/Stockholm, Europe/London, Europe/Paris, Europe/Berlin, America/New_York,
  America/Los_Angeles, Pacific/Honolulu, America/Anchorage, Asia/Tokyo,
  Asia/Shanghai, Australia/Sydney, Pacific/Auckland, Australia/Eucla, ...),
  including historical transitions and pre-standard LMT, OR vendor a tz library.
- Replace `named_tz_offset` + the transition logic with lookups against it; this
  then unblocks [[GQLITE-T-0342]] (across-transition elapsed) and
  [[GQLITE-T-0343]] (offset-by-date).

## Acceptance Criteria
- [ ] Temporal2 [6] examples pass (incl. the 1818 LMT case)
- [ ] Rigorous full pass-set diff zero regressions; unit 944/944; functional clean

## Notes
This is the foundational DST item; [[GQLITE-T-0342]] and [[GQLITE-T-0343]] likely
collapse into it once real tzdata is available. Context: [[GQLITE-T-0341]],
[[GQLITE-I-0049]].
