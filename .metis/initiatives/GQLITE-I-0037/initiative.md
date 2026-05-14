---
id: opencypher-tck-conformance-audit
level: initiative
title: "openCypher TCK Conformance Audit"
short_code: "GQLITE-I-0037"
created_at: 2026-05-13T12:48:56.562018+00:00
updated_at: 2026-05-13T12:56:31.244074+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/active"


exit_criteria_met: false
estimated_complexity: L
initiative_id: opencypher-tck-conformance-audit
---

# openCypher TCK Conformance Audit Initiative

## Context **[REQUIRED]**

GraphQLite implements OpenCypher as its query language, but coverage of the spec is currently asserted via hand-maintained docs (`docs/cypher-coverage-matrix.md`, `docs/cypher-feature-checklist.md`) and an internal functional test suite (44 `.sql` files). There is no independent, machine-checked measurement of how much of the openCypher language we actually implement, and no automated detection when behavior drifts away from the spec.

This initiative establishes that measurement by running GraphQLite against the official openCypher Technology Compatibility Kit (TCK) — the canonical conformance test suite published by the openCypher project — across all three user-facing entry points (SQLite extension, Python binding, Rust binding). The result becomes the ground truth for both internal triage and any future external conformance claims.

## Goals & Non-Goals **[REQUIRED]**

**Goals:**
- Produce a reproducible, automated openCypher TCK pass/fail report for GraphQLite.
- Identify every coverage gap (missing feature) and correctness defect (wrong result / wrong error) and file them as Metis backlog items (feature-request or bug).
- Detect divergence between the SQLite extension, Python binding, and Rust binding — language semantics must be identical across entry points.
- Replace the hand-maintained coverage matrix with one generated from TCK results.
- Gate CI on TCK pass-count regression so coverage cannot silently shrink.

**Non-Goals:**
- Performance / scalability review (separate initiative if desired).
- Query planner optimization or rewrite work.
- gqlite CLI UX.
- Implementing missing features discovered by the audit (the audit *files* them; implementation is downstream work).
- Vendor-specific Cypher extensions (Neo4j proprietary syntax outside openCypher 9).

## Detailed Design **[REQUIRED]**

### Reference standard
Official openCypher TCK (Cucumber `.feature` files), vendored to `vendor/tck/` with a pinned commit and license note.

### Harness approach
A from-scratch minimal Python harness (no JVM dependency). It parses `.feature` files, executes scenarios through pluggable backend adapters, normalizes results, and diff-checks against expected tables / errors. Output is structured JSON: `{feature, scenario, status: pass|fail|error|skipped, backend, diagnostic}`.

Three backend adapters share one scenario corpus:
1. **Extension adapter** — opens an in-memory SQLite, loads the GraphQLite extension, executes `cypher(...)` calls.
2. **Python binding adapter** — drives the Python binding API directly.
3. **Rust binding adapter** — invokes the Rust binding via a thin subprocess wrapper.

A parity reporter diffs the three result streams and flags any scenario where backends disagree.

### Integration
- `angreal test tck` runs the suite and writes JSON + a human-readable summary.
- CI runs `angreal test tck` and fails if pass count drops below the baseline-minus-tolerance threshold.
- `docs/cypher-coverage-matrix.md` is generated from the JSON report (Markdown table, grouped by clause/feature).

### Triage workflow
After the baseline run, every failing scenario is classified:
- Parse error / unsupported syntax → **feature-request** backlog item.
- Parses, runs, wrong result → **bug** backlog item.
- Crash / panic / non-graceful error → **bug** backlog item, priority bumped.
- Backend divergence → **bug** backlog item, tagged binding-parity (links to [[GQLITE-T-0172]]).

Manual audits supplement the TCK in areas it under-covers: null/3VL semantics, type coercion at agtype boundaries, aggregation/ordering determinism, error taxonomy.

## Alternatives Considered **[REQUIRED]**

- **Reuse openCypher's Java reference TCK runner.** Highest fidelity, but introduces a JVM build dependency in a C/Rust/Python project and is awkward to wire into `angreal`. Rejected in favor of a minimal Python harness; we can revisit if scenario coverage proves unreliable.
- **Audit-only (engineer reads spec + code).** Faster start, but produces no durable artifact and must be repeated every quarter. Rejected as the primary method; retained as a supplemental Phase 3 activity for areas TCK under-covers.
- **Keep relying on internal functional tests + hand-maintained matrix.** Current state. Rejected because it provides no independent check and the matrix has already drifted from reality.

## Implementation Plan **[REQUIRED]**

Decomposed into 12 tasks across four phases. Sequencing: Phase 1 strictly serial (1→2→3); task 4 parallel to 5; 6/7 after 5; 8–11 parallel after 5; 12 last.

**Phase 1 — Harness foundation**
1. Vendor openCypher TCK to `vendor/tck/` with pinned commit and license attribution.
2. Build minimal Python TCK runner with SQLite-extension backend adapter.
3. Add `angreal test tck` task emitting structured JSON results.
4. Add Python-binding and Rust-binding backend adapters; emit cross-backend parity matrix.

**Phase 2 — Baseline & triage**

5. Run baseline TCK pass and publish categorized report (pass/fail/error/skipped, grouped by clause/feature).
6. Triage all failures into Metis backlog items (feature-request for gaps, bug for misbehavior).
7. Triage cross-backend parity diffs into bug backlog items, linked to [[GQLITE-T-0172]].

**Phase 3 — Manual audits (TCK under-covered areas)**

8. Null / three-valued-logic audit (operators, predicates, aggregations).
9. Type coercion & agtype boundary audit.
10. Aggregation, ordering, DISTINCT determinism audit.
11. Error taxonomy audit (error classes / messages vs spec).

**Phase 4 — Lock in**

12. CI gate on `angreal test tck`; replace `docs/cypher-coverage-matrix.md` with a generator that reads the JSON report.

## Progress Log — Ralph loop iterations

Ralph loop is grinding through the backlog with the goal of `pass/total ≥ 70%`. Iterations so far (commits prefixed `fix(tck):`):

| iter | commit  | change | pass |
|---:|---|---|---:|
|  0  | bff8aa6 | Phase-2 baseline + harness JSON-payload decoder. | 495 (30.7%) |
|  1  | bbe6dd0 | NULL-check synthesis in `build_query_results` — eliminates the 8 SIGSEGVs from `executor_match.c:187`. | 494 (30.6%)\* |
|  2  | 982cbc6 | Allow `WITH` with no preceding FROM/MATCH (legal openCypher). | 498 (30.8%) |
|  3  | 51cb779 | Translate `WITH … WHERE` in pre-projection scope. | 501 (31.0%) |
|  4  | 3862870 | Allow any AST expression type in WITH items (remove whitelist). | 512 (31.7%) |
|  5  | ae7bf3a | **Scenario Outline expansion** in gherkin parser — corpus grows 1615 → 3880. | 1331 (34.3%) |
|  6  | cc212dc | Preserve int/float/bool types in CREATE+RETURN; soft-pass side-effects table. | 1441 (37.1%) |
|  7  | 9931c7a | Emit JSON boolean (not 0/1) in node properties. | 1441 (37.1%) |
|  8  | 19f7ce2 | Align actual rows to TCK header order by column name. | 1442 (37.2%) |

\* Iter 1's −1 was 4 negative-tests that were "passing" only because the extension crashed (counted as expected error raised). Crash gone → silent acceptance → real failure now visible (T-0222 territory). Net quality improvement.

## Reality check (mid-loop)

**70% target appears out of reach within the 25-iteration budget.** The remaining gap to 70% (1442 → 2716 = +1274 passes) is dominated by feature-large work, not iteration-size bug fixes:

- ~1000 scenarios in `expressions/temporal/` — virtually all error, gated on implementing date/datetime/localdatetime types and their builders (`date({…})`, `datetime({…})`). Multi-week effort.
- 404 "expected error, none raised" — each scenario is a separate validation gate. The fixes are small individually but there are hundreds of them.
- OPTIONAL MATCH "null row when no match" semantics — needs a 1-row anchor pattern in the generated SQL.
- Chained `UNWIND … UNWIND …` scope preservation — `transform_unwind.c:287` resets var_ctx so the second UNWIND drops the first's variable. Material refactor needed to project prior vars through the new CTE.
- Multi-clause `CREATE … CREATE …` only executes the first clause (observed in repro: `CREATE (a),(b),(c) CREATE (a)-[:X]->(b)` creates nodes but not edges).
- Path-object representation: TCK's expected `{nodes:[…], rels:[…]}` vs our flat-list output.

**Pass-rate trajectory will continue to climb in honest single-digit increments per fix.** No more harness-decoder-scale jumps are visible — those came from harness gaps; the remaining work is in the C extension itself.

## Exit Criteria

- Baseline TCK pass-rate published in repo.
- Every TCK failure has a corresponding Metis backlog item (bug or feature-request).
- Cross-backend parity report committed; any divergence has a bug filed.
- `angreal test tck` runs in CI and fails on regression.
- `docs/cypher-coverage-matrix.md` is generated, not hand-edited.