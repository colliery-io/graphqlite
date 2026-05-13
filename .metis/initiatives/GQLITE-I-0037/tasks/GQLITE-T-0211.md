---
id: tck-06-triage-tck-failures-into
level: task
title: "TCK-06: Triage TCK failures into Metis backlog items"
short_code: "GQLITE-T-0211"
created_at: 2026-05-13T12:51:05.505574+00:00
updated_at: 2026-05-13T12:51:05.505574+00:00
parent: GQLITE-I-0037
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0037
---

# TCK-06: Triage TCK failures into Metis backlog items

## Parent Initiative

[[GQLITE-I-0037]]

## Objective

Walk the baseline failure set and create Metis backlog items so every gap and defect surfaced by the TCK has a tracked owner, classification, and priority.

## Acceptance Criteria

- [ ] Every failing scenario in the baseline report is associated with exactly one Metis backlog item (a backlog item may cover N related failures; mapping is recorded).
- [ ] Classification rule applied: parse error / unsupported syntax → `backlog_category: feature`; parses-but-wrong-result → `backlog_category: bug`; crash/panic/non-graceful error → `backlog_category: bug` at P1 minimum.
- [ ] Each new backlog item references the TCK feature file + scenario name(s) in its body and links back to [[GQLITE-I-0037]].
- [ ] A `docs/tck/triage-<date>.md` summary lists every backlog item created, classification, and the failure count it covers.
- [ ] Skipped scenarios are reviewed: if skipped due to unsupported step vocabulary, file a harness bug (parent: this initiative); if skipped due to known unimplemented features, file feature-requests like real failures.

## Implementation Notes

### Technical Approach
Goal is durable tracking, not perfect cause analysis — cluster aggressively (one feature-request per missing clause/function is fine; do not file 50 items for 50 scenarios of `OPTIONAL MATCH` if it's simply unimplemented). Get clustering right before filing en masse.

### Dependencies
Depends on [[GQLITE-T-0210]].

## Status Updates

*To be added during implementation*
