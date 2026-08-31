# Contributing to GraphQLite

GraphQLite is a Cypher-on-SQLite engine shipped as a SQLite extension plus
Python, Rust, and TypeScript bindings. This document covers how to build,
test, and ship changes.

## Build

All operational tasks go through [angreal](https://github.com/colliery-io/angreal).
Don't invoke `make`, `gcc`, or `pytest` directly — angreal encodes the
project's flags, dependency ordering, and platform handling.

```
angreal build extension   # SQLite extension (.dylib / .so / .dll)
angreal build app         # gqlite CLI
angreal build all         # both
```

Add `--release` for optimized builds.

After changing parser sources (`*.y` / `*.l`) or adding `ast_node_type`
enum values, run `angreal dev clean` before rebuilding — incremental builds
can corrupt enum-tagged structs.

## Test taxonomy

| Suite | Command | Run when |
|---|---|---|
| Unit (CUnit, 926 tests) | `angreal test unit` | Touching parser, transform, executor, helpers |
| Functional (44 SQL files) | `angreal test functional` | SQL semantics / round-trip behavior |
| TCK (openCypher conformance) | `angreal test tck` | Anything in `src/backend/transform/` or `src/backend/executor/` |
| Rust binding | `angreal test rust` | Rust binding surface area |
| Python binding | `angreal test python` | Python binding surface area |
| TypeScript binding | `angreal test typescript` | TypeScript binding surface area |
| CLI | `angreal test cli` | `gqlite` interactive entry point |
| Constraints (expected-fail) | `angreal test constraints` | Negative test corpus |
| Everything | `angreal test all` | Pre-release smoke |

Backend parity (extension vs Rust vs Python vs TypeScript) is verified
post-hoc. Run backends independently and reconcile divergences in their own
tickets — do not gate one backend on another in the same commit.

### Before push

```
angreal test unit && angreal test functional && angreal test tck
```

The TCK output JSON (`tests/tck/baseline.json`) is the ground truth for
conformance. Any scenario movement (pass → fail or vice versa) must be
intentional and called out in the commit body.

## Generated files

Parser/scanner output (`cypher_gram.tab.c`, `cypher_gram.tab.h`,
`cypher_scanner.c`) is regenerated into `build/parser/` by the Makefile.
**Do not commit generated artifacts.** `src/generated/` is gitignored.

When changing `src/backend/parser/cypher_gram.y` or `cypher_scanner.l`:

1. `angreal dev clean`
2. `angreal build extension`
3. Run the full pre-push suite — grammar changes routinely shift TCK.

## Source layout

```
src/
├── backend/
│   ├── parser/       # Bison GLR grammar + Flex scanner + AST
│   ├── transform/    # AST → SQL
│   ├── executor/     # SQL execution + result projection
│   └── runtime/      # Helper UDFs (post-I-0040)
├── include/          # Public headers
└── extension.c       # SQLite extension entry point
```

## Header ownership conventions

- Public headers live under `src/include/`.
- One header per `.c` file in `transform_func_*` and `executor_*` families.
- Caller-owned vs borrowed-pointer ownership is documented inline in the
  declaration when not obvious from the function name.
- Forward-declaring symbols from another translation unit (`extern int
  foo(...);` in a `.c` file) is a code smell — add a header or use the
  existing one.

## Commit hygiene

- One logical change per commit. Refactors and behavior changes do not
  share a commit.
- For TCK-affecting work, include the scenario delta in the commit message
  (e.g. `+8 / -0`).
- Don't bypass hooks (`--no-verify`) or signing without explicit reason.

## Reporting issues

File issues at https://github.com/colliery-io/graphqlite/issues. Include:

- Reproducer (`sqlite3 :memory: <<EOF` block preferred for extension bugs)
- Expected vs actual output
- `angreal --version` and platform
