# tests/tck/

Minimal Python harness that runs the vendored openCypher TCK
(`vendor/tck/features/*.feature`) against GraphQLite and emits machine-
readable conformance results.

## Layout

- `gherkin.py` — small `.feature` parser (no `behave` / `pytest-bdd` dep).
- `values.py` — parser + comparator for openCypher textual values (nodes,
  relationships, paths, lists, maps, scalars).
- `runner.py` — scenario executor: dispatches Gherkin steps to a backend
  and compares results.
- `backends/` — backend adapters (`Backend` interface + `ExtensionBackend`
  for the SQLite extension). Python and Rust binding adapters land in
  GQLITE-T-0209.
- `__main__.py` — CLI: `python -m tests.tck [--features DIR] [--filter SUB]
  [--backend extension|python|rust|all] [--out FILE] [--debug]`.
- `tests/` — pytest tests for the harness itself (Gherkin parser, value
  comparator, end-to-end smoke against the extension).

## What is checked

Each check is strict (GQLITE-T-0429):

- Result tables: the column names, the row count, the row order when the
  scenario says "in order", and each value with its type (`1` is not `1.0`,
  `true` is not `1`).
- "The result should be empty": no rows, and no error.
- Errors: the class (`SyntaxError`, `TypeError`, ...) and the detail
  (`UndefinedVariable`, ...). The worker reads the class from the start of
  the engine's message, or `SyntaxError` for a parse error; it does not guess
  from words in the message.
- Side effects: `+nodes`, `-nodes`, `+relationships`, `-relationships`,
  `+labels`, `-labels`, `+properties` and `-properties`, from the difference
  of the graph before and after the query (node and relationship ids, the
  label names in use, property triples). The Python and Rust binding
  backends cannot report graph state: there, side effects are not checked.
- Setup: a "having executed" query that fails makes the scenario an error.

## Output schema

JSON written to `build/tck-results.json` (one record per scenario):

```
{
  "feature_file": "clauses/match/Match1.feature",
  "scenario_name": "[1] Match non-existent nodes returns empty",
  "status": "pass" | "fail" | "error" | "skipped",
  "backend": "extension",
  "duration_ms": 12.3,
  "diagnostic": "...",
  "expected": <serializable> | null,
  "actual":   <serializable> | null
}
```

`status` is `skipped` when the scenario uses a step the harness doesn't
yet understand or a value form the comparator can't decode — never crash
on unknown vocabulary.

## Debug noise

The GraphQLite extension emits `[CYPHER_DEBUG]` lines to stderr. The
runner redirects fd 2 to `build/tck-debug.log` for the duration of each
query unless `--debug` is passed. (Follow-up: add a compile-time quiet
switch in the extension itself.)
