---
id: c2-decide-and-document-generated
level: task
title: "C2: Decide and document generated-files (parser/scanner) policy + CI gate"
short_code: "GQLITE-T-0287"
created_at: 2026-05-19T14:48:18.601517+00:00
updated_at: 2026-05-19T14:48:18.601517+00:00
parent: GQLITE-I-0041
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0041
---

# C2: Decide and document generated-files policy + CI gate

## Parent Initiative

[[GQLITE-I-0041]]

## Objective

Pick Option A (generated files in git + CI staleness check) or Option B (gitignore + always regenerate). Initiative recommends A so `git clone && make` works without flex/bison locally. Whichever wins: document in CONTRIBUTING and implement the CI check or build wiring.

## Acceptance Criteria

- [ ] Decision documented in `CONTRIBUTING.md`.
- [ ] If Option A: CI step runs `make parser && git diff --exit-code src/generated/` and fails when stale.
- [ ] If Option B: `.gitignore` updated, Makefile guarantees generated files are present before each build.

## Status Updates

*To be added during implementation*
