"""Metamorphic query testing.

Usage:
    python -m tests.metamorphic [--seed N] [--count M] [--relation NAME]
                                [--ext PATH] [--known FILE] [--out FILE]

Each case is a random graph and a set of query variants that must give the
same result. A failure is a variant that errors, or results that differ.
Failures are grouped by signature; the first case of each group is shrunk
(edges, then nodes, removed while it still fails) and printed with its seed.
`known.json` maps signatures of open bugs to their Kairos task; they are
reported but do not fail the run.
"""

from __future__ import annotations

import argparse
import json
import random
import re
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path

from .engine import Engine, Outcome
from .gen import Graph, random_graph
from .relations import RELATIONS, Case

HERE = Path(__file__).parent


@dataclass
class Failure:
    kind: str                      # error | mismatch
    outcomes: list[tuple[str, str, Outcome]]   # (label, query, outcome)

    def detail(self) -> str:
        lines = []
        for label, query, o in self.outcomes:
            lines.append(f"  [{label}] {query}")
            lines.append(f"      -> {o.key() if o.ok else 'ERROR: ' + o.error}")
        return "\n".join(lines)


def _remap(o: Outcome, keymap: dict[str, str]) -> Outcome:
    if not o.ok or not keymap:
        return o
    return Outcome(True, rows=[{keymap.get(k, k): v for k, v in r.items()} for r in o.rows])


def check(engine: Engine, case: Case, graph: Graph) -> Failure | None:
    setup = graph.create()
    results = []
    conn = None
    for v in case.variants:
        if case.write or conn is None:
            conn = engine.fresh()
            if setup:
                s = engine.run(conn, setup)
                if not s.ok:
                    raise RuntimeError(f"graph setup failed: {s.error}\n{setup}")
        o = engine.run(conn, v.query)
        if o.ok and v.mode == "write":
            o = engine.run(conn, case.readback)
        results.append((v.label, v.query, _remap(o, v.keymap)))
    if any(not o.ok for _, _, o in results):
        return Failure("error", results)
    if len({o.key() for _, _, o in results}) > 1:
        return Failure("mismatch", results)
    return None


def shrink(engine: Engine, case: Case, graph: Graph, kind: str) -> Graph:
    def still(g: Graph) -> bool:
        f = check(engine, case, g)
        return f is not None and f.kind == kind
    changed = True
    while changed:
        changed = False
        for i in range(len(graph.edges)):
            g = graph.without_edge(i)
            if still(g):
                graph, changed = g, True
                break
        if changed:
            continue
        for n in list(graph.nodes):
            g = graph.without_node(n["id"])
            if still(g):
                graph, changed = g, True
                break
    return graph


def signature(case: Case, f: Failure) -> str:
    if f.kind == "error":
        err = next(o.error for _, _, o in f.outcomes if not o.ok)
        err = re.sub(r"\d+", "N", err)
        err = re.sub(r", expecting .*", "", err)            # parser position detail
        err = re.sub(r"_with_N\.\w+", "_with_N.VAR", err)  # generated aliases
        err = re.sub(r"'[^']*'", "'…'", err)[:120]
        base_failed = not f.outcomes[0][2].ok
        # An error in the first variant is a fault of the query, not of the
        # relation: group it across relations.
        return f"query error: {err}" if base_failed else f"{case.relation} error: {err}"
    return f"{case.relation} mismatch"


def known_task(known: list[dict], case: Case, f: Failure) -> str | None:
    queries = " ".join(q for _, q, _ in f.outcomes)
    errors = " ".join(o.error or "" for _, _, o in f.outcomes)
    for k in known:
        if not re.search(k["relation"], case.relation):
            continue
        if k.get("kind") and k["kind"] != f.kind:
            continue
        if k.get("query") and not re.search(k["query"], queries):
            continue
        if k.get("error") and not re.search(k["error"], errors):
            continue
        return k["task"]
    return None


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="tests.metamorphic")
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--count", type=int, default=500)
    p.add_argument("--relation", default=None, help="run only relations whose name contains this")
    p.add_argument("--ext", default="build/graphqlite", type=Path)
    p.add_argument("--known", default=HERE / "known.json", type=Path)
    p.add_argument("--out", default="build/metamorphic-results.json", type=Path)
    p.add_argument("--no-shrink", action="store_true")
    args = p.parse_args(argv)

    engine = Engine(args.ext)
    known = json.loads(args.known.read_text()) if args.known.exists() else []
    names = [n for n in RELATIONS if not args.relation or args.relation in n]
    if not names:
        print(f"no relation matches {args.relation!r}", file=sys.stderr)
        return 2

    groups: dict[str, dict] = {}
    per_relation: Counter = Counter()
    for i in range(args.count):
        seed = args.seed + i
        rng = random.Random(seed)
        name = names[i % len(names)]
        graph = random_graph(rng)
        case = RELATIONS[name](rng)
        per_relation[name] += 1
        f = check(engine, case, graph)
        if f is None:
            continue
        sig = signature(case, f)
        g = groups.setdefault(sig, {"count": 0, "seeds": [], "task": known_task(known, case, f)})
        g["count"] += 1
        g["seeds"].append(seed)
        if g["count"] == 1:
            small = graph if args.no_shrink else shrink(engine, case, graph, f.kind)
            g["graph"] = small.create()
            g["relation"] = case.relation
            g["detail"] = (check(engine, case, small) or f).detail()

    new = {s: g for s, g in groups.items() if not g["task"]}
    old = {s: g for s, g in groups.items() if g["task"]}
    failed = sum(g["count"] for g in groups.values())
    print(f"metamorphic: {args.count} cases (seeds {args.seed}..{args.seed + args.count - 1}), "
          f"{failed} failed, {len(new)} new signature(s), {len(old)} known")
    for sig, g in sorted(old.items(), key=lambda x: -x[1]["count"]):
        print(f"  known {g['task']}: {sig} ({g['count']}x)")
    for sig, g in sorted(new.items(), key=lambda x: -x[1]["count"]):
        print(f"\nNEW: {sig} ({g['count']}x, first seed {g['seeds'][0]})")
        print(f"  graph: {g['graph']}")
        print(g["detail"])

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps({"seed": args.seed, "count": args.count,
                                    "groups": groups}, indent=2))
    return 1 if new else 0


if __name__ == "__main__":
    sys.exit(main())
