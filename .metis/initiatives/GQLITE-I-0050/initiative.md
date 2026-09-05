---
id: issue-triage-batch-0-7-0-bindings
level: initiative
title: "Issue triage batch 0.7.0 — bindings correctness + core stats/similarity fixes (#104–#116)"
short_code: "GQLITE-I-0050"
created_at: 2026-09-05T13:13:58.758467+00:00
updated_at: 2026-09-05T13:34:57.847154+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/active"


exit_criteria_met: false
estimated_complexity: M
initiative_id: issue-triage-batch-0-7-0-bindings
---

# Issue triage batch 0.7.0 — bindings correctness + core stats/similarity fixes (#104–#116) Initiative

## Context **[REQUIRED]**

GitHub issues #104–#116 (filed 2026-09-02) are a triage batch covering the Python and Rust bindings plus two core items. Every report was re-validated on 2026-09-05 against a fresh extension build and all 13 reproduce exactly as filed (see each task's *Validation* section). Older feature requests #16 and #17 are already tracked as [[GQLITE-T-0192]] and [[GQLITE-T-0100]] and are out of scope here.

Root-cause clusters:
- **column_0 unwrap reimplemented per method** (#104, #105, #106): eight algorithm entry points never see the array/object the core returns.
- **nodeSimilarity argument handling** (#107 core, #108 bindings).
- **Binding input hygiene** (#109 id hijack, #110 identifier injection, #111 path traversal, #112 misleading `graphs=None`).
- **API consistency / dead code** (#113 namespace, #114 tuple vs dict, #115 duplicate sanitizer).
- **Core output contract** (#116 status string → JSON object).

## Goals & Non-Goals **[REQUIRED]**

**Goals:**
- One Metis task per issue (three column_0 issues share one task because they share one fix), each validated before work starts.
- Fix all 13 issues in one PR against `main`, ship as **0.7.0** (minor bump: #113/#114/#112/#116 are observable API changes).
- Regression tests in the harness for every fix: Python (`angreal test python`), Rust (`angreal test rust`), CUnit (`angreal test unit`), functional SQL.

**Non-Goals:**
- #16 diagnostics / #17 capability metadata (separate backlog tasks).
- A second `cypher_stats()` function; the status string was never a contract, so it is replaced outright (#116 option 1).
- Auto-detecting graphs from a Cypher FROM clause in the bindings (#112 explicitly rejects this).

## Detailed Design **[REQUIRED]**

- **Algorithm results (T-0345):** Python `_parsing.extract_algo_array` becomes the single unwrap for array results and a new `extract_algo_object` for dijkstra/astar object results; `ALGO_COLUMN_NAMES` collapses to `column_0` (probe on 2026-09-05 showed every algorithm emits `column_0`). Rust gets `algo_rows(&CypherResult)` / `algo_object(&CypherResult)` in `algorithms/parsing.rs`; all methods route through them.
- **Core (T-0346, T-0355):** `detect_graph_algorithm()` reads `args[0]` as threshold in the two-numeric-arg branch. `extension.c` emits `{"nodes_created":N,"relationships_created":N,"nodes_deleted":N,"relationships_deleted":N,"properties_set":N}` for RETURN-less writes. Consumers updated: CUnit `test_output_format.c`, TCK worker `_decode_payload` (treat stats object as status, not a data row), `docs/src/reference/sql-interface.md`.
- **Input hygiene:** `assert_identifier()` helper in each binding (`^[A-Za-z_][A-Za-z0-9_]*$`), applied to labels, property keys, and astar coordinate props. Relationship types keep the existing public `sanitize_rel_type` contract (they are never interpolated raw). GraphManager validates names with the same rule plus a resolved-path containment check. Rust gets `Error::InvalidIdentifier`, `Error::InvalidGraphName`, `Error::InvalidArgument`.
- **API changes:** `Graph(db_path, extension_path=None)` (namespace removed); `GraphManager.query(cypher, graphs, params=None)` with `graphs` required and non-empty; `get_node_edges` returns dicts; bulk uses `utils.sanitize_rel_type`.

## Alternatives Considered **[REQUIRED]**

- Unwrapping `column_0` inside `Connection.cypher()` / `CypherResult::from_json` — rejected: it would change the raw `query()` output for every algorithm call and for list-of-map literals.
- Keeping the status string and adding `cypher_stats()` (#116 option 2) — rejected per the issue's own recommendation; minor bump covers it.
- Raising on non-identifier relationship types (#110 wording) — rejected: `sanitize_rel_type` is a public, tested contract and #115 depends on it.

## Implementation Plan **[REQUIRED]**

1. Core fixes first (T-0346, T-0355) with CUnit + functional coverage; rebuild extension.
2. Bindings, Python then Rust, task by task (T-0345, T-0347–T-0354), each with its regression tests.
3. T-0356: bump to 0.7.0, changelog, run unit → functional → python → rust → TCK, open one PR, wait for CI before tagging.