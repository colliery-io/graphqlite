"""Metamorphic query testing.

Usage:
    python -m tests.metamorphic [--mode random|cover] [options]

random: --count cases from --seed (or --minutes), relations in turn.
cover:  for each relation, a covering array over the dimensions it reads
        (all pairs; all triples of the clause-structure dimensions), each
        row run --reps times with different random fill-ins.
--replay ID reruns one case and prints it.

Each case is a random graph and query variants that must give the same
result. A failure is a variant that errors, or results that differ. Open
bugs are listed in known.json with their Kairos task; they are reported but
do not fail the run. Exit 1 on a new signature, a crash or a timeout.
"""

from __future__ import annotations

import argparse
import itertools
import json
import sys
import time
from pathlib import Path

from .engine import Engine
from .relations import RELATIONS
from .runner import (build, check, cover_rows, default_jobs, keyfn, print_report,
                     report, run_all, shrink)

HERE = Path(__file__).parent


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="tests.metamorphic")
    p.add_argument("--mode", choices=["random", "cover"], default="random")
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--count", type=int, default=500, help="random mode: number of cases")
    p.add_argument("--minutes", type=float, default=None, help="random mode: run until this budget")
    p.add_argument("--reps", type=int, default=2, help="cover mode: runs per covering row")
    p.add_argument("--strength", type=int, default=3, choices=[2, 3],
                   help="cover mode: 3 adds all triples of the clause-structure dimensions")
    p.add_argument("--relation", default=None, help="only relations whose name contains this")
    p.add_argument("--jobs", type=int, default=default_jobs())
    p.add_argument("--ext", default="build/graphqlite", type=Path)
    p.add_argument("--known", default=HERE / "known.json", type=Path)
    p.add_argument("--out", default="build/metamorphic-results.json", type=Path)
    p.add_argument("--replay", default=None, help="rerun one case id and print it")
    p.add_argument("--no-shrink", action="store_true")
    args = p.parse_args(argv)

    engine = Engine(args.ext)
    known = json.loads(args.known.read_text()) if args.known.exists() else []

    if args.replay:
        ch, graph, case = build(args.replay, args.strength)
        print(f"case {args.replay} ({case.relation})")
        print("dims: " + ", ".join(f"{k}={x}" for k, x in ch.used.items()))
        f = check(engine, case, graph)
        if f and not args.no_shrink:
            graph = shrink(engine, case, graph, f.kind)
            f = check(engine, case, graph)
        print(f"graph: {graph.create()}")
        if f is None:
            print("passes")
            for var in case.variants:
                for q in var.queries:
                    print(f"  [{var.label}] {q}")
            return 0
        print(f"{f.kind}:\n" + f.detail(keyfn(case.compare)))
        return 1

    names = [n for n in RELATIONS if not args.relation or args.relation in n]
    if not names:
        print(f"no relation matches {args.relation!r}", file=sys.stderr)
        return 2

    t0 = time.time()
    if args.mode == "random":
        if args.minutes:
            ids = (f"r:{names[i % len(names)]}:{args.seed + i}" for i in itertools.count())
        else:
            ids = (f"r:{names[i % len(names)]}:{args.seed + i}" for i in range(args.count))
        header = f"metamorphic random (seed {args.seed})"
    else:
        ids_list = []
        for n in names:
            rows = cover_rows(n, args.seed, args.strength)
            ids_list += [f"c:{n}:{j}:{k}:{args.seed}" for j in range(len(rows)) for k in range(args.reps)]
        print(f"covering arrays built in {time.time() - t0:.1f}s: {len(ids_list)} cases "
              f"over {len(names)} relations", flush=True)
        ids = iter(ids_list)
        header = f"metamorphic cover (seed {args.seed}, strength {args.strength}, reps {args.reps})"
    deadline = time.time() + args.minutes * 60 if args.minutes else None
    results = run_all(args.ext, known, ids, args.jobs, args.strength, deadline,
                      progress=lambda n: print(f"  {n} cases, {time.time() - t0:.0f}s", flush=True))
    report.known = known
    rep = report(results, engine, args.strength, shrink_new=not args.no_shrink)
    print(f"ran in {time.time() - t0:.1f}s on {args.jobs} worker(s)")
    rc = print_report(rep, header)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(rep, indent=1, default=str))
    return rc


if __name__ == "__main__":
    sys.exit(main())
