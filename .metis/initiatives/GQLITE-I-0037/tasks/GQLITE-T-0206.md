---
id: tck-01-vendor-opencypher-tck-to
level: task
title: "TCK-01: Vendor openCypher TCK to vendor/tck/"
short_code: "GQLITE-T-0206"
created_at: 2026-05-13T12:50:59.814423+00:00
updated_at: 2026-05-13T13:17:25.365186+00:00
parent: GQLITE-I-0037
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0037
---

# TCK-01: Vendor openCypher TCK to vendor/tck/

## Parent Initiative

[[GQLITE-I-0037]]

## Objective

Vendor the official openCypher Technology Compatibility Kit `.feature` files into the repo so the conformance harness has a stable, version-pinned reference corpus that does not require network access to run.

## Acceptance Criteria

- [x] openCypher TCK `.feature` files are copied under `vendor/tck/` with directory structure preserved.
- [x] The exact upstream commit SHA is recorded in `vendor/tck/UPSTREAM.md` along with the upstream repository URL and the date pinned.
- [x] License attribution (Apache-2.0) is captured in `vendor/tck/LICENSE` matching the upstream file.
- [x] A short `vendor/tck/README.md` explains how to refresh the snapshot (one command) and the project's update policy.
- [x] No build-system changes other than ensuring `vendor/tck/` is included in source distributions.

## Implementation Notes

### Technical Approach
This is a vendor-snapshot, not a git submodule, to keep `angreal test tck` runnable offline and reproducible. Refresh by re-running a documented `git archive` / `cp` script.

### Dependencies
None — first task in the chain.

## Status Updates

### 2026-05-13 — Completed

Vendored openCypher TCK at upstream commit `677cbafabb8c3c5eed458fd3b1ec0daec8d67d23`.

- `vendor/tck/features/` — 220 `.feature` files, grouped by `clauses/`, `expressions/`, `useCases/`.
- `vendor/tck/graphs/` — fixture graphs referenced by feature scenarios.
- `vendor/tck/LICENSE`, `vendor/tck/NOTICE` — Apache-2.0 + upstream attribution.
- `vendor/tck/UPSTREAM.md` — pin (commit + date + source + refresh policy).
- `vendor/tck/UPSTREAM-README.adoc`, `vendor/tck/README.md`.
- `scripts/refresh-tck.sh` — one-command refresh (shallow sparse clone, copy features+graphs, refresh license/notice, rewrite pin).

Total vendored size ~1.9 MB. All acceptance criteria met.