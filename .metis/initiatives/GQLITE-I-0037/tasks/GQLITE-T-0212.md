---
id: tck-07-triage-cross-backend-parity
level: task
title: "TCK-07: Triage cross-backend parity divergences"
short_code: "GQLITE-T-0212"
created_at: 2026-05-13T12:51:06.627958+00:00
updated_at: 2026-05-13T12:51:06.627958+00:00
parent: GQLITE-I-0037
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0037
---

# TCK-07: Triage cross-backend parity divergences

## Parent Initiative

[[GQLITE-I-0037]]

## Objective

For every scenario where the extension, Python binding, and Rust binding disagree, file a binding-parity bug so the bindings are held to the same semantic standard as the extension.

## Acceptance Criteria

- [ ] Every `divergence: true` row in `build/tck-parity.json` is mapped to a Metis bug backlog item.
- [ ] Each parity bug describes which backends disagree and how, references the offending scenario, and links to [[GQLITE-T-0172]] (REC-17 API consistency).
- [ ] Cross-cutting divergences (same root cause across many scenarios) are clustered into a single item with a representative scenario plus the full list.
- [ ] A `docs/tck/parity-<date>.md` summary tabulates the divergences.

## Implementation Notes

### Technical Approach
If the extension is wrong and the bindings are wrong identically, that is not a parity bug — it is covered by [[GQLITE-T-0211]]. Parity is strictly about disagreement between entry points.

### Dependencies
Depends on [[GQLITE-T-0209]] and [[GQLITE-T-0210]].

## Status Updates

*To be added during implementation*
