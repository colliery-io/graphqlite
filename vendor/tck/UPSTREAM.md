# openCypher TCK — Upstream Pin

This directory is a **vendor snapshot** of the openCypher Technology
Compatibility Kit. It is not a git submodule. The snapshot is pinned to a
specific upstream commit so conformance runs are reproducible and offline-
capable.

## Source

- **Upstream repository:** https://github.com/opencypher/openCypher
- **Subdirectory vendored:** `tck/` (specifically `tck/features/` and `tck/graphs/`)
- **Pinned commit:** `677cbafabb8c3c5eed458fd3b1ec0daec8d67d23`
- **Date pinned:** 2026-05-13
- **License:** Apache-2.0 (see `LICENSE` and `NOTICE` in this directory)

## What's here

- `features/` — Cucumber `.feature` files (the TCK scenarios), grouped by
  `clauses/`, `expressions/`, and `useCases/`.
- `graphs/` — fixture graph data referenced by feature files (`Given the <name> graph`).
- `LICENSE` — Apache-2.0 license text from upstream.
- `NOTICE` — upstream attribution notice.
- `UPSTREAM-README.adoc` — upstream's own TCK readme, copied verbatim.

## Refreshing the snapshot

Run `scripts/refresh-tck.sh` from the repository root. It performs a shallow
sparse clone of upstream, copies `tck/features` and `tck/graphs` into
`vendor/tck/`, refreshes `LICENSE` / `NOTICE` / `UPSTREAM-README.adoc`, and
rewrites the pinned commit in this file.

## Update policy

- Bump the snapshot only as a deliberate, reviewed commit — never as an
  automated side effect of CI.
- When bumping, re-run `angreal test tck` and reconcile the new baseline
  before merging.
- Do not edit vendored files in-place. If a scenario must be excluded, do it
  in the runner's filter list, not by deleting the upstream file.
