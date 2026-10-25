# Metamorphic query tests

Generated queries checked against rewrites that must not change the result.
The TCK tests one feature at a time; these tests combine features at random
and need no expected result: two equivalent queries must agree.

```
angreal test metamorphic                      # 500 cases from seed 1
python -m tests.metamorphic --seed 1000 --count 20000
python -m tests.metamorphic --relation write  # one relation
```

## Files

- `gen.py`: random graphs, patterns and expressions. Queries are templates
  with variables written `⟨a⟩`, rendered through a name map.
- `relations.py`: the relations. Each builds a `Case`: variants that must
  give the same rows (a multiset; lists are compared sorted, since `collect`
  order is unspecified). Write cases run each variant on a fresh database
  and compare a read-back query.
- `engine.py`: runs `cypher()` and normalises the result.
- `__main__.py`: the runner. It groups failures by signature, shrinks the
  first graph of each group, and prints the seed and the variants.
- `known.json`: signatures of open bugs and their Kairos task. A known
  failure is reported but does not fail the run.

## A new failure

1. Rerun its seed: `--seed N --count 1`.
2. Reproduce it by hand in `sqlite3` and check which variant is right.
3. A real bug: record a task, add a line to `known.json`. A harness error:
   fix the relation.
4. When a bug is fixed, delete its line from `known.json`.

A case that fails for a known bug can hide a second bug in the same query,
so keep `known.json` short: fix bugs rather than list them.

## Adding a relation

Write a function `rng -> Case` in `relations.py` and add it to `RELATIONS`.
Make each variant valid Cypher on any graph the generator makes, so any
error is a finding.
