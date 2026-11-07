"""Interface parity (GQLITE-T-0532): the same queries through the cypher()
SQL function, the Python bindings and the Rust bindings give the same values
of the same types.

    python -m tests.parity.run [--ext build/graphqlite]

It drives the three TCK backends (tests/tck/backends): extension (raw
cypher() JSON), python (graphqlite.Connection) and rust (the tck_runner
example; built here when missing). Each value is compared with its type:
an integer is not a float, a boolean is not an integer, a string that
looks like a number or JSON is still a string. Exit code 1 on a difference.
"""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tests.tck.backends.base import BackendError, QueryResult  # noqa: E402

SETUP = [
    "CREATE (:P {i: 1, f: 2.5, whole: 2.0, neg: -7, big: 9007199254740993, s: 'text', "
    "snum: '12', sfloat: '1.5', sbool: 'true', sjson: '[1, 2]', smap: '{\"a\": 1}', "
    "nl: 'a\\nb\\tc', quote: 'say \"hi\"', uni: 'naïve ✓', empty: '', b: true, nb: false, "
    "l: [1, 'a', true, null, 2.0], m: {k: 1, s: 'x', b: false, l: [1]}, "
    "d: date('2024-02-29'), dt: datetime('2024-01-01T10:00:00Z'), dur: duration('P1DT2H')})"
    "-[:R {w: 0.5, t: 'r'}]->(:Q {id: 2})",
]

CORPUS = [
    # scalars from properties
    "MATCH (n:P) RETURN n.i AS i, n.f AS f, n.whole AS whole, n.neg AS neg, n.big AS big",
    "MATCH (n:P) RETURN n.s AS s, n.snum AS snum, n.sfloat AS sfloat, n.sbool AS sbool, "
    "n.sjson AS sjson, n.smap AS smap, n.empty AS empty",
    "MATCH (n:P) RETURN n.nl AS nl, n.quote AS quote, n.uni AS uni",
    "MATCH (n:P) RETURN n.b AS b, n.nb AS nb, n.missing AS missing",
    "MATCH (n:P) RETURN n.l AS l, n.m AS m, n.m.l AS ml, n.l[2] AS l2",
    "MATCH (n:P) RETURN n.d AS d, n.dt AS dt, n.dur AS dur",
    # literals and expressions
    "RETURN 1 AS i, 1.0 AS f, 2.5e10 AS e, -0.0 AS nz, 1 / 2 AS idiv, 1 / 2.0 AS fdiv",
    "RETURN 0.0 / 0.0 AS nan, 1.0 / 0.0 AS inf, -1.0 / 0.0 AS ninf",
    "RETURN true AS t, false AS f, null AS n, 1 = 1 AS eq, 1 < 2 AS lt, NOT true AS nt",
    "RETURN '12' AS s, toString(12) AS ts, toInteger('12') AS ti, toFloat('1') AS tf, toBoolean('true') AS tb",
    "RETURN [1, 2.0, 'a', true, null, [1], {k: 1}] AS l, {a: 1, b: [true], c: {d: null}} AS m",
    "RETURN [] AS el, {} AS em, '' AS es, [null] AS ln",
    "RETURN date('2024-02-29') AS d, localtime('10:00:00') AS lt, duration({days: 3}) AS dur",
    "UNWIND [1, 2.0, 'x', true, null, [1], {a: 1}] AS v RETURN v",
    "WITH '1' AS s, 1 AS i, true AS b RETURN s, i, b, [s, i, b] AS l",
    # aggregates
    "UNWIND [1, 2, 3] AS x RETURN sum(x) AS s, avg(x) AS a, min(x) AS mn, max(x) AS mx, "
    "collect(x) AS c, count(*) AS n",
    "UNWIND [1.5, 2.5] AS x RETURN sum(x) AS s, avg(x) AS a",
    "UNWIND [true, false] AS x RETURN collect(x) AS c, max(x = true) AS m",
    "UNWIND ['1', '2'] AS x RETURN collect(x) AS c, max(x) AS m",
    # entities and paths
    "MATCH (n:P) RETURN labels(n) AS l, keys(n) AS k",
    "MATCH (n:Q) RETURN n",
    "MATCH ()-[r:R]->() RETURN r, type(r) AS t, r.w AS w",
    "MATCH p = (:P)-->(:Q) RETURN length(p) AS len, [x IN nodes(p) | labels(x)] AS ls",
    "MATCH (n:Q) RETURN n {.id, extra: 1.5, flag: true} AS proj",
    # writes with RETURN
    "CREATE (x:W {f: 3.0, s: '4', b: false, l: [1.5]}) RETURN x.f AS f, x.s AS s, x.b AS b, x.l AS l",
    "MATCH (x:W) SET x.g = 1.0, x.h = 'true' RETURN x.g AS g, x.h AS h",
]

PARAM_CORPUS = [
    ("RETURN $i AS i, $f AS f, $s AS s, $b AS b, $n AS n, $l AS l, $m AS m",
     {"i": 1, "f": 1.0, "s": "1", "b": True, "n": None, "l": [1, "a", False], "m": {"k": 2.5}}),
    ("UNWIND $rows AS r RETURN r.a AS a, r.b AS b", {"rows": [{"a": 1, "b": "x"}, {"a": 2.0, "b": True}]}),
    ("CREATE (n:Par {v: $v, w: $w}) RETURN n.v AS v, n.w AS w", {"v": 2.0, "w": "2"}),
]


def typed(x):
    """A value with its JSON type, so 1, 1.0 and true differ."""
    if isinstance(x, bool):
        return ["bool", x]
    if isinstance(x, int):
        return ["int", x]
    if isinstance(x, float):
        if math.isnan(x):
            return ["float", "NaN"]
        if math.isinf(x):
            return ["float", "Infinity" if x > 0 else "-Infinity"]
        return ["float", x]
    if isinstance(x, str):
        return ["str", x]
    if x is None:
        return ["null"]
    if isinstance(x, list):
        return ["list", [typed(v) for v in x]]
    if isinstance(x, dict):
        return ["map", {k: typed(v) for k, v in sorted(x.items())}]
    return [type(x).__name__, repr(x)]


def canon(res: QueryResult):
    if res.error:
        return {"error": res.error}
    return {"columns": res.headers, "rows": [[typed(v) for v in row] for row in res.rows]}


def backends(ext: str | None):
    from tests.tck.backends.extension import ExtensionBackend
    from tests.tck.backends.python_binding import PythonBindingBackend
    from tests.tck.backends.rust_binding import RUNNER_REL, RustBindingBackend
    runner = ROOT / RUNNER_REL
    if not runner.exists():
        print("building the Rust tck_runner example ...", flush=True)
        subprocess.run(["cargo", "build", "-q", "--example", "tck_runner",
                        "--manifest-path", str(ROOT / "bindings/rust/Cargo.toml")], check=True)
    return [ExtensionBackend(Path(ext) if ext else None), PythonBindingBackend(), RustBindingBackend(runner)]


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="tests.parity.run")
    p.add_argument("--ext", default=None)
    args = p.parse_args(argv)
    import os
    os.chdir(ROOT)
    bs = backends(args.ext)
    for b in bs:
        b.reset()
        for q in SETUP:
            r = b.execute(q)
            if r.error:
                print(f"{b.name}: setup failed: {r.error}: {r.error_message}")
                return 2
    diffs = 0
    cases = [(q, None) for q in CORPUS] + PARAM_CORPUS
    for q, params in cases:
        got = {}
        for b in bs:
            got[b.name] = canon(b.execute(q, params))
        names = list(got)
        ref = json.dumps(got[names[0]], sort_keys=True)
        if any(json.dumps(got[n], sort_keys=True) != ref for n in names[1:]):
            diffs += 1
            print(f"DIFF: {q[:110]}" + (f"  params={json.dumps(params)}" if params else ""))
            for n in names:
                print(f"  {n:9}: {json.dumps(got[n])[:600]}")
    for b in bs:
        b.close()
    print(f"interface parity: {len(cases)} queries, {diffs} differ "
          f"({', '.join(b.name for b in bs)})")
    return 1 if diffs else 0


if __name__ == "__main__":
    sys.exit(main())
