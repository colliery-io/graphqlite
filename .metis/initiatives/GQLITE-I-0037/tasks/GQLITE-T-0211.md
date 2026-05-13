---
id: tck-06-triage-tck-failures-into
level: task
title: "TCK-06: Triage TCK failures into Metis backlog items"
short_code: "GQLITE-T-0211"
created_at: 2026-05-13T12:51:05.505574+00:00
updated_at: 2026-05-13T17:12:41.118658+00:00
parent: GQLITE-I-0037
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0037
---

# TCK-06: Triage TCK failures into Metis backlog items

## Parent Initiative

[[GQLITE-I-0037]]

## Objective

Walk the baseline failure set and create Metis backlog items so every gap and defect surfaced by the TCK has a tracked owner, classification, and priority.

## Acceptance Criteria

- [x] Every failing/erroring scenario mapped to one of 12 Metis backlog items ([[GQLITE-T-0218]] through [[GQLITE-T-0229]]).
- [x] Classification rule applied: parse-error / unsupported syntax → `feature`; parses-but-wrong-result → `bug`; crash → `bug` at P0; harness gaps → `tech-debt`.
- [x] Each backlog item references its top affected feature files (counts) and links back to [[GQLITE-I-0037]].
- [x] `docs/tck/triage-2026-05-13.md` published — full mapping table + triage notes.
- [x] Skipped scenarios (90) mapped to harness tech-debt [[GQLITE-T-0228]] with sub-class counts.

## Implementation Notes

### Technical Approach
Goal is durable tracking, not perfect cause analysis — cluster aggressively (one feature-request per missing clause/function is fine; do not file 50 items for 50 scenarios of `OPTIONAL MATCH` if it's simply unimplemented). Get clustering right before filing en masse.

### Dependencies
Depends on [[GQLITE-T-0210]].

## Status Updates

### 2026-05-13 — Completed

**Pre-triage harness fixes.** Treat `Given any graph` as `Given an empty graph` (+619 scenarios unblocked) and load named-graph fixtures from `<name>/<name>.cypher` (+19). Re-ran baseline; skip 723→90, pass 22→109.

**Also added** during this task: a 10s per-execute watchdog in `ExtensionBackend` so hung scenarios (one was looping in `expressions/list/List10`) mark as `error: ExtensionTimeout` instead of stalling the run.

**12 backlog tickets filed**:

| Ticket | Cat | Pri | # | Cluster |
|---|---|---|---:|---|
| [[GQLITE-T-0218]] | bug | P0 | 8 | SIGSEGV in `executor_match.c:187 build_query_results` |
| [[GQLITE-T-0219]] | bug | P1 | 69 | False "WITH requires preceding MATCH" |
| [[GQLITE-T-0220]] | bug | P1 | 43 | "Unknown variable" in legal scopes |
| [[GQLITE-T-0221]] | bug | P2 | 14 | SQLite-internal errors leak through |
| [[GQLITE-T-0222]] | bug | P1 | 66 | Accepts queries spec requires rejected |
| [[GQLITE-T-0223]] | bug | P1 | 56 | Returns rows where empty expected |
| [[GQLITE-T-0224]] | feature | P2 | 303 | Unsupported syntax (omnibus) |
| [[GQLITE-T-0225]] | feature | P2 | 21 | Explicit "not yet implemented" |
| [[GQLITE-T-0226]] | bug | P2 | 17 | Generic "Failed to transform RETURN" |
| [[GQLITE-T-0227]] | tech-debt | P1 | — | **Harness:** decode `cypher()` payload to rows |
| [[GQLITE-T-0228]] | tech-debt | P2 | 90 | **Harness:** close 90-scenario skip gap |
| [[GQLITE-T-0229]] | bug | P2 | 69 | WITH…ORDER BY area cluster |

Triage notes (also in `docs/tck/triage-2026-05-13.md`):
- Fix [[GQLITE-T-0218]] (crash) first — uniquely bad regardless of conformance impact.
- Fix [[GQLITE-T-0227]] (harness decoder) before deep-diving on T-0223/T-0229 — it likely inflates their counts.
- [[GQLITE-T-0224]] is intentionally an umbrella; cut sub-tickets when picked up.

All acceptance criteria met.