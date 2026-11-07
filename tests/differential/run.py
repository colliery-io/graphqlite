"""Differential test against Neo4j (GQLITE-T-0529): the metamorphic
harness's random graphs and queries on Neo4j Community and on GraphQLite,
results compared.

    python -m tests.differential.run --start [--count 2000] [--seed 1]

Needs the `neo4j` Python driver (angreal test differential makes a venv in
build/differential/venv). With --start it runs Neo4j Community in Docker
(image NEO4J_IMAGE, heap 1 GB, container memory 2 GB, Bolt on 127.0.0.1
only) and removes the container at the end; without it, it uses the server
at --bolt. Neo4j is GPL-3.0: it is only run as a service, nothing of it is
in this repository.

For each case it builds the graph on both engines, runs the case's first
plain variant (a write case: the write, then its read-back) and compares
the rows the way the harness does (bag or set order, lists sorted, floats
rounded, 1 / 1.0 / true apart). Nodes, relationships and paths are compared
without their ids, which each engine numbers its own way; a query that
returns id() is skipped. Rows that differ in a known Neo4j-only way are
counted, not failed (KNOWN). Results go to build/differential/results.json.
"""

from __future__ import annotations

import argparse
import json
import math
import random
import re
import subprocess
import sys
import time
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tests.metamorphic.engine import Engine, Outcome, canon_row  # noqa: E402
from tests.metamorphic.relations import RELATIONS  # noqa: E402
from tests.metamorphic.runner import build  # noqa: E402

NEO4J_IMAGE = "neo4j:5.26-community"
CONTAINER = "gqlite-neo4j-differential"
PORT = 17687

# Known differences, by a regex on the query: (pattern, why).
KNOWN: list[tuple[str, str]] = [
    # Neo4j 5 refuses `:A|B` and `:A:B` in one query (GQL label expressions
    # do not mix with colon conjunction); openCypher takes both.
    (r"(?=.*\(\w*:[A-Za-z_]+\|)(?=.*(?:\(\w*|\b\w+):[A-Za-z_]+:[A-Za-z_])",
     "Neo4j: label | and : : not mixed in one query"),
]


# ------------------------------------------------------------------ values

def _entity(d: dict) -> dict:
    """A GraphQLite node / relationship / path without its ids."""
    if "labels" in d and "properties" in d:
        return {"labels": sorted(d["labels"]), "properties": d["properties"]}
    if "type" in d and "properties" in d and ("startNode" in d or "startNodeId" in d):
        return {"type": d["type"], "properties": d["properties"]}
    if set(d) >= {"nodes", "rels"}:
        return {"path": [norm_gql(n) for n in d["nodes"]] + [norm_gql(r) for r in d["rels"]]}
    if "_iso8601" in d:
        return d["_iso8601"]
    return None


def norm_gql(v):
    if isinstance(v, dict):
        e = _entity(v)
        if e is not None:
            return e
        return {k: norm_gql(x) for k, x in v.items()}
    if isinstance(v, list):
        return [norm_gql(x) for x in v]
    return v


def norm_neo(v):
    from neo4j.graph import Node, Path, Relationship
    from neo4j.time import Date, DateTime, Duration, Time
    if isinstance(v, Node):
        return {"labels": sorted(v.labels), "properties": {k: norm_neo(x) for k, x in v.items()}}
    if isinstance(v, Relationship):
        return {"type": v.type, "properties": {k: norm_neo(x) for k, x in v.items()}}
    if isinstance(v, Path):
        return {"path": [norm_neo(n) for n in v.nodes] + [norm_neo(r) for r in v.relationships]}
    if isinstance(v, (Date, DateTime, Time)):
        return v.iso_format()
    if isinstance(v, Duration):
        return v.iso_format()
    if isinstance(v, dict):
        return {k: norm_neo(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm_neo(x) for x in v]
    return v


def _nan_safe(v):
    """NaN to a token, so two NaN compare equal."""
    if isinstance(v, float) and math.isnan(v):
        return "NaN"
    if isinstance(v, float) and math.isinf(v):
        return "Infinity" if v > 0 else "-Infinity"
    if isinstance(v, dict):
        return {k: _nan_safe(x) for k, x in v.items()}
    if isinstance(v, list):
        return [_nan_safe(x) for x in v]
    return v


def key(rows: list, compare: str) -> str:
    rs = [canon_row(_nan_safe(r)) for r in rows]
    if compare in ("bag", "set"):
        rs = sorted(set(rs)) if compare == "set" else sorted(rs)
    return json.dumps(rs)


# ------------------------------------------------------------------ engines

class Neo:
    def __init__(self, bolt: str):
        from neo4j import GraphDatabase
        self.driver = GraphDatabase.driver(bolt, auth=None, notifications_min_severity="OFF")
        self.driver.verify_connectivity()

    def reset(self, setup: str | None):
        with self.driver.session() as s:
            s.run("MATCH (n) DETACH DELETE n").consume()
            if setup:
                s.run(setup).consume()

    def run(self, q: str) -> Outcome:
        try:
            with self.driver.session() as s:
                res = s.run(q)
                rows = [{k: norm_neo(r[k]) for k in r.keys()} for r in res]
            return Outcome(True, rows=rows)
        except Exception as e:  # noqa: BLE001 - the error is the result
            return Outcome(False, error=f"{type(e).__name__}: {str(e)[:200]}")

    def close(self):
        self.driver.close()


def start_neo4j() -> str:
    subprocess.run(["docker", "rm", "-f", CONTAINER], capture_output=True)
    cmd = ["docker", "run", "-d", "--name", CONTAINER, "--memory", "2g",
           "-p", f"127.0.0.1:{PORT}:7687",
           "-e", "NEO4J_AUTH=none",
           "-e", "NEO4J_server_memory_heap_initial__size=512m",
           "-e", "NEO4J_server_memory_heap_max__size=1G",
           "-e", "NEO4J_server_memory_pagecache_size=256m",
           NEO4J_IMAGE]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"docker run failed: {r.stderr}")
    bolt = f"bolt://127.0.0.1:{PORT}"
    from neo4j import GraphDatabase
    for _ in range(120):
        try:
            d = GraphDatabase.driver(bolt, auth=None)
            d.verify_connectivity()
            d.close()
            return bolt
        except Exception:  # noqa: BLE001 - not up yet
            time.sleep(2)
    raise SystemExit("Neo4j did not come up in 4 minutes")


def stop_neo4j() -> None:
    subprocess.run(["docker", "rm", "-f", CONTAINER], capture_output=True)


# ------------------------------------------------------------------ cases

def plain_variant(case):
    for var in case.variants:
        if var.query and not var.parts and not var.combine and not var.post and not var.keymap \
                and var.repeat == 1 and not getattr(var, "expect_error", False):
            if case.write and var.mode != "write":
                continue
            return var
    return None


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="tests.differential.run")
    p.add_argument("--start", action="store_true", help="run Neo4j in Docker for this run")
    p.add_argument("--bolt", default=f"bolt://127.0.0.1:{PORT}")
    p.add_argument("--ext", default=str(ROOT / "build" / "graphqlite"))
    p.add_argument("--count", type=int, default=2000)
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--show", type=int, default=3, help="examples per signature")
    p.add_argument("--out", default=str(ROOT / "build" / "differential" / "results.json"))
    args = p.parse_args(argv)

    bolt = start_neo4j() if args.start else args.bolt
    neo = Neo(bolt)
    gql = Engine(Path(args.ext))
    rng = random.Random(args.seed)
    names = sorted(RELATIONS)
    stats = Counter()
    diffs: dict[str, list] = {}
    known = Counter()
    try:
        for i in range(args.count):
            name = rng.choice(names)
            case_id = f"r:{name}:{rng.randrange(1 << 30)}"
            try:
                _, graph, case = build(case_id)
            except Exception:  # noqa: BLE001 - a generator gap, not a result
                stats["build error"] += 1
                continue
            var = plain_variant(case)
            if var is None or re.search(r"\bid\(|elementId", var.query + (case.readback or "")):
                stats["skipped"] += 1
                continue
            setup = graph.create()
            # GraphQLite
            conn = gql.fresh()
            if setup:
                gql.run(conn, setup)
            g = gql.run(conn, var.query)
            if case.write and g.ok:
                g = gql.run(conn, case.readback)
            conn.close()
            # Neo4j
            neo.reset(setup)
            n = neo.run(var.query)
            if case.write and n.ok:
                n = neo.run(case.readback)
            stats["compared"] += 1
            if not g.ok and not n.ok:
                stats["both error"] += 1
                continue
            if g.ok and n.ok:
                gk = key([norm_gql(r) for r in g.rows], case.compare)
                nk = key(n.rows, case.compare)
                if gk == nk:
                    stats["same"] += 1
                    continue
                kind = "rows differ"
            else:
                kind = "only GraphQLite errors" if not g.ok else "only Neo4j errors"
            why = next((w for pat, w in KNOWN if re.search(pat, var.query)), None)
            if why:
                known[why] += 1
                continue
            sig = f"{case.relation.split('/')[0]}: {kind}"
            diffs.setdefault(sig, []).append({
                "case": case_id, "setup": setup, "query": var.query, "readback": case.readback if case.write else None,
                "graphqlite": g.rows if g.ok else g.error, "neo4j": n.rows if n.ok else n.error})
            stats["differ"] += 1
    finally:
        neo.close()
        if args.start:
            stop_neo4j()

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps({"stats": stats, "known": known, "diffs": diffs}, indent=1, default=str))
    for sig, items in sorted(diffs.items(), key=lambda kv: -len(kv[1])):
        print(f"{len(items):5d}  {sig}")
        for it in items[:args.show]:
            print(f"         {it['query'][:160]}")
            print(f"           graphqlite: {json.dumps(it['graphqlite'], default=str)[:220]}")
            print(f"           neo4j:      {json.dumps(it['neo4j'], default=str)[:220]}")
    print(f"differential: {dict(stats)}; known {dict(known)}; results in {out.relative_to(ROOT)}")
    return 1 if diffs else 0


if __name__ == "__main__":
    sys.exit(main())
