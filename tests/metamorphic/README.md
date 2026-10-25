# Metamorphic query tests

Generated queries checked against rewrites that must not change the result.
The TCK tests one feature at a time; these tests combine features and need
no expected result: equivalent queries must agree.

```
angreal test metamorphic                          # 500 random cases
angreal test metamorphic --mode cover --reps 3    # every relation's covering array
python -m tests.metamorphic --minutes 10 --seed 20261025
python -m tests.metamorphic --replay c:union:31:0:7
```

## How a case is made

Every choice the generator makes goes through a `Chooser` (`space.py`), by
dimension name where the choice is a dimension:

- the graph shape and how many nulls it has
- for each clause: hops, direction, named or anonymous relationship,
  relationship type, labels
- the WITH kind, the RETURN kind
- the expression kind, the predicate kind, the aggregate kind

In **random** mode the chooser picks at random. In **cover** mode it takes
values from a row of a covering array, built per relation over the
dimensions that relation reads. The array covers:

- every pair of values
- every triple of the clause-structure dimensions (`CORE`), which is how
  interactions like "WITH, then a two-hop OPTIONAL MATCH, with an anonymous
  relationship" are reached

A dimension that does not apply (the direction of a clause that is not
there) is not required. A case id rebuilds the case exactly:

- `r:<relation>:<seed>`
- `c:<relation>:<row>:<rep>:<cover seed>`

## Relations (`relations.py`)

| Relation | Must hold |
| --- | --- |
| with_star | A pass-through `WITH *` changes nothing |
| rename, keyword_name | Renaming variables changes nothing; non-reserved keywords work as names |
| predicate_move | A WHERE on the first MATCH, the second, or a WITH gives the same rows |
| reverse_pattern | A pattern written right to left matches the same paths |
| split_path | A two-hop path equals two MATCH clauses with distinct edges |
| varlen | `*m..n` equals the union of fixed-length paths with distinct edges |
| tlp_where | Rows of Q = rows of Q WHERE p, NOT p, p IS NULL |
| tlp_count | count(*), sum() of Q = sums over the three partitions |
| limit_slice | SKIP/LIMIT is a slice of the full ORDER BY; DESC is its reverse |
| distinct_set | RETURN DISTINCT = RETURN without duplicates |
| union | UNION ALL = both results; UNION = both without duplicates |
| distinct_agg | collect/count(DISTINCT e) = the aggregate over WITH DISTINCT |
| alias_shadow | An alias that reuses a variable name changes only the key |
| label_disjunction | `:A\|B` = `WHERE x:A OR x:B` (MATCH, predicate, comprehension) |
| inline_props | `(a {k: v})` = `(a) WHERE a.k = v` |
| optional_filter | OPTIONAL MATCH filtered to matched rows = MATCH |
| optional_null | OPTIONAL MATCH null rows = NOT EXISTS; its count = the comprehension size |
| write_expression | A CREATE property value = the value computed in a WITH = RETURN of it |
| merge_idempotent | MERGE twice = MERGE once = one node per distinct value |
| set_readback | After SET a.k = e, a.k = e |
| delete_count | DETACH DELETE removes exactly the matched nodes or relationships |

Every query is valid Cypher on any generated graph, so any error is a
finding.

## Running at scale

- `runner.py` runs cases in worker processes (`--jobs`), one case at a
  time per worker over a pipe.
- A crash or a hang (`CASE_TIMEOUT`) kills only that worker. The case is
  recorded with its id, and a new worker takes over.
- Failures are grouped by signature. For a new signature the runner shrinks
  the first graph and prints:
  - the replay id
  - the dimension values every failing case shares
  - the variants with their results
- The results file lists the dimension values that were never exercised
  (`gaps`).

## known.json

Each entry maps a failure to the Kairos task of an open bug. It matches by:

- relation (regex)
- kind (`error`, `mismatch`, `crash`)
- and optionally regexes on the queries, the error text and the output

The first entry that matches wins. A known failure is reported but does not
fail the run. When a bug is fixed, delete its entry; when a run shows a new
signature, reproduce it by hand, record a task and add an entry, or fix the
relation if the harness was wrong.

A case that fails for a known bug can hide a second bug in the same query,
so the list should shrink: fix bugs rather than list them.

## CI

- The `tck-conformance` job runs one cover pass and 3000 random cases.
- `metamorphic-nightly.yml` runs cover mode and 20 minutes of random mode
  with a seed from the date, and uploads the results. It does not gate.
