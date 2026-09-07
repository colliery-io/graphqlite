---
id: c1-text-path-json-formatter-in
level: task
title: "C1: text-path JSON formatter in extension.c does not escape control characters"
short_code: "GQLITE-T-0367"
created_at: 2026-09-07T01:25:17.689516+00:00
updated_at: 2026-09-07T01:47:00.656652+00:00
parent: GQLITE-I-0051
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0051
---

# C1: text-path JSON formatter in extension.c does not escape control characters

## Parent Initiative

[[GQLITE-I-0051]]

## Objective **[REQUIRED]**

Correctness issue from the review: the non-agtype JSON formatter in `extension.c` escapes only `"` and `\\`, so any string containing a newline/tab/control character (and all `EXPLAIN` output) is invalid JSON.

## Validation

Validated 2026-09-06 on macOS with a release build via `tests/performance/python` (harness patched for macOS in H1). `RETURN 'a\\nb' AS s` returned a raw newline inside the JSON string; `json.loads` of `EXPLAIN` output raised "Invalid control character".

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] Text path escapes `\\n \\r \\t \\b \\f` and other `< 0x20` as `\\u00XX`; buffer sized for the 6x worst case.
- [ ] Functional `40_json_escaping.sql` (json_valid + round trip + EXPLAIN); Python `test_control_characters_are_escaped`, `test_explain_output_is_valid_json`.

## Status Updates **[REQUIRED]**

- 2026-09-06: created, validated, implemented.
- 2026-09-06: Fixed. Discovered while testing: the params JSON parser does not decode `\uXXXX` escapes (`\u0001` becomes the text `u0001`), and stored scalars returned via the agtype path come back with control characters replaced by a space. Both are separate pre-existing issues; tests use a raw control character inside the params JSON (functional 40.2 builds it with `char(1)`).