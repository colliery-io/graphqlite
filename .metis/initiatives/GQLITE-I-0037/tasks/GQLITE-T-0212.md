---
id: tck-07-triage-cross-backend-parity
level: task
title: "TCK-07: Triage cross-backend parity divergences"
short_code: "GQLITE-T-0212"
created_at: 2026-05-13T12:51:06.627958+00:00
updated_at: 2026-05-13T17:13:14.764424+00:00
parent: GQLITE-I-0037
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/active"


exit_criteria_met: false
initiative_id: GQLITE-I-0037
---

# TCK-07: Triage cross-backend parity divergences

## Parent Initiative

[[GQLITE-I-0037]]

## Objective

For every scenario where the extension, Python binding, and Rust binding disagree, file a binding-parity bug so the bindings are held to the same semantic standard as the extension.

## Acceptance Criteria

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

### 2026-05-13 — Paused (binding work deprioritized)

Per user direction: the bindings (Python and Rust) are OOP wrappers around the same Cypher execution path in the SQLite extension. Language-implementation conformance is measured by the extension baseline ([[GQLITE-T-0210]]); binding parity is a secondary signal about *wrapper-level* fidelity, not Cypher semantics.

**Work done while this task was active that's worth preserving:**

- `RustBindingBackend` now has the same supervisor + 10-second per-execute watchdog + crash-respawn pattern as `ExtensionBackend` (was unsupervised before — a hot loop in the underlying extension would hang the run).
- `PythonBindingBackend` was refactored from in-process to a worker subprocess (`tests/tck/_python_binding_worker.py`) so the same watchdog + respawn pattern applies — previously the binding loaded the extension in-process and any infinite loop took down the whole harness.
- Smoke runs against `clauses/match/Match1.feature` confirm both adapters produce signal: `python` returns pass/fail; `rust` errors out across the board (separate signal worth follow-up).

**What's NOT done (the original acceptance criteria):**

- No full TCK run against the Python or Rust binding.
- No `build/tck-parity.json` from a coordinated multi-backend run.
- No backlog items filed for binding parity divergences.

**Resume condition.** Pick this up when the extension baseline has had enough bug-fixing churn that parity becomes meaningful — most likely after [[GQLITE-T-0218]] (crash) and [[GQLITE-T-0227]] (harness decoder) land, since both materially change what "the extension does" and therefore what "parity with the extension" means.