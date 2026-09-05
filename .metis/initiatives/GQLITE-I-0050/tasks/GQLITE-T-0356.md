---
id: release-0-7-0-version-bump
level: task
title: "Release 0.7.0: version bump, changelog, full test matrix, PR"
short_code: "GQLITE-T-0356"
created_at: 2026-09-05T13:18:50.452900+00:00
updated_at: 2026-09-05T13:33:31.298378+00:00
parent: GQLITE-I-0050
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/active"


exit_criteria_met: false
initiative_id: GQLITE-I-0050
---

# Release 0.7.0: version bump, changelog, full test matrix, PR

## Parent Initiative

[[GQLITE-I-0050]]

## Objective **[REQUIRED]**

Ship the batch as 0.7.0 in a single PR.

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] Version bumped in `bindings/rust/Cargo.toml` (+ `Cargo.lock`) and `bindings/python/src/graphqlite/__init__.py`.
- [ ] `CHANGELOG.md` gains a `[0.7.0]` section listing every issue, with a **Breaking** subsection (#112, #113, #114, #116).
- [ ] Full matrix green, each as its own run: `angreal test unit`, `angreal test functional`, `angreal test python`, `angreal test rust`, `angreal test tck` (pass count ≥ 3788 / 3876 baseline from 0.6.0).
- [ ] PR opened against `main` with `Closes #104 … #116`; tag only after CI is green.

## Status Updates **[REQUIRED]**

- 2026-09-05: created.- 2026-09-05: version bumped to 0.7.0 (Cargo.toml, Cargo.lock, __init__.py), CHANGELOG [0.7.0] written, full matrix green (unit 945/945, functional 8/8, python 383, rust 301 + clippy/fmt clean, TCK 3788/3876 = baseline). Commit 07aee2e pushed; PR #117 opened against main. Remaining: wait for CI, merge, tag v0.7.0 (tag only after CI is green).
