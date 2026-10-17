"""Write speed checks (GQLITE-T-0431).

#143 sent more write shapes to the row-wise write pipeline, which runs a
MATCH per row; with the row's values in the SQL text, each row prepared new
statements, and `UNWIND $rows MERGE ...` became 28x slower. Two kinds of
check:

- deterministic: the statements a per-row write runs do not grow with the
  number of rows (the SQL text does not carry the row's values);
- timing: each shape against a simple write in the same run, with wide
  bounds (the old ratios were 39x and 14x).
"""

import json
import os
import sqlite3
import time
from pathlib import Path

import pytest


def get_extension_path():
    build_dir = Path(__file__).parent.parent.parent.parent / "build"
    for name in ("graphqlite.dylib", "graphqlite.so", "graphqlite.dll"):
        if (build_dir / name).exists():
            return str(build_dir / name)
    env_path = os.environ.get("GRAPHQLITE_EXTENSION_PATH")
    if env_path and Path(env_path).exists():
        return env_path
    pytest.skip("GraphQLite extension not found. Build with 'make extension'")


def _conn():
    c = sqlite3.connect(":memory:")
    if not hasattr(c, "enable_load_extension"):
        pytest.skip("this Python's sqlite3 cannot load extensions")
    c.enable_load_extension(True)
    c.load_extension(get_extension_path())
    return c


def _distinct_statements(setup, query, params=None):
    c = _conn()
    for q in setup:
        c.execute("SELECT cypher(?)", (q,))
    seen = set()
    c.set_trace_callback(seen.add)
    if params:
        c.execute("SELECT cypher(?, ?)", (query, json.dumps(params)))
    else:
        c.execute("SELECT cypher(?)", (query,))
    c.set_trace_callback(None)
    c.close()
    return len(seen)


@pytest.mark.parametrize("query", [
    "MATCH (n:S) MERGE (:C {k: n.id % 10})",
    "MATCH (n:S) WITH n, n.id AS i MERGE (:C {k: i % 10})",
    "MATCH (n:S) SET n.x = 1 WITH n RETURN count(n) AS c",
    "MATCH (n:S) CREATE (m:T {id: n.id}) WITH m MATCH (o:S {id: m.id}) RETURN count(o) AS c",
])
def test_statements_do_not_grow_with_rows(query):
    small = _distinct_statements(["UNWIND range(1, 40) AS i CREATE (:S {id: i})"], query)
    large = _distinct_statements(["UNWIND range(1, 120) AS i CREATE (:S {id: i})"], query)
    assert large <= small + 3, f"{small} statements for 40 rows, {large} for 120"


def _time(setup, query, params=None):
    c = _conn()
    for q in setup:
        c.execute("SELECT cypher(?)", (q,))
    t0 = time.perf_counter()
    if params:
        c.execute("SELECT cypher(?, ?)", (query, json.dumps(params)))
    else:
        c.execute("SELECT cypher(?)", (query,))
    t = time.perf_counter() - t0
    c.close()
    return t


N = 2000
ROWS = {"rows": [{"id": i, "v": i} for i in range(N)]}
SEED = f"UNWIND range(1, {N}) AS i CREATE (:S {{id: i}})"


def _best(fn, n=3):
    return min(fn() for _ in range(n))


def test_unwind_merge_speed():
    create = _best(lambda: _time([], "UNWIND $rows AS r CREATE (:I {id: r.id, v: r.v})", ROWS))
    merge = _best(lambda: _time([], "UNWIND $rows AS r MERGE (n:I {id: r.id}) ON CREATE SET n.v = r.v", ROWS))
    assert merge < 8 * create, f"MERGE {merge * 1000:.0f} ms, CREATE {create * 1000:.0f} ms"


def test_set_before_with_speed():
    plain = _best(lambda: _time([SEED], "MATCH (n:S) SET n.x = 1"))
    with_ = _best(lambda: _time([SEED], "MATCH (n:S) SET n.x = 1 WITH n RETURN count(n) AS c"))
    assert with_ < 6 * plain + 0.01, f"SET .. WITH {with_ * 1000:.0f} ms, SET {plain * 1000:.0f} ms"


# MERGE per matched row is covered by test_statements_do_not_grow_with_rows:
# against a batch CREATE its time ratio differs too much between machines
# (7x local, 23x on the macOS CI runner) to bound it.
