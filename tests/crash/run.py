"""Crash test (GQLITE-T-0530): kill a writer at random points, then check
that the database is whole and every write is all there or not there.

    python -m tests.crash.run --ext build/graphqlite [--rounds 200] [--seed 1]

Each round starts tests/crash/writer.py on one database file (the file is
kept across rounds, so each round starts from what the last crash left),
sends it SIGKILL after a random delay, reopens the file and checks:

  * PRAGMA integrity_check is ok, and gql_check_storage() is [];
  * every batch is whole: 20 :B, 20 :E and 10 :R, 10 :M, 3 :T, 15 :Bulk
    and 10 :BR per batch (or 0 of each);
  * the nodes of a :B batch all have the same s (a SET is whole).

Rounds alternate journal_mode WAL and DELETE. Exit code 1 when a round
finds a problem; the database of the first bad round is kept.
"""

from __future__ import annotations

import argparse
import json
import os
import random
import shutil
import signal
import sqlite3
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WRITER = Path(__file__).with_name("writer.py")

WHOLE = {  # label or type -> rows per batch
    ("node", "B"): 20, ("node", "E"): 20, ("rel", "R"): 10, ("node", "M"): 10,
    ("node", "T"): 3, ("node", "Bulk"): 15, ("rel", "BR"): 10,
}


def cypher(conn: sqlite3.Connection, q: str) -> list[dict]:
    raw = conn.execute("SELECT cypher(?)", (q,)).fetchone()[0]
    data = json.loads(raw) if raw else []
    return data if isinstance(data, list) else []


def check(db: Path, ext: str) -> list[str]:
    problems: list[str] = []
    conn = sqlite3.connect(str(db))
    try:
        conn.enable_load_extension(True)
        conn.load_extension(ext)
        ic = conn.execute("PRAGMA integrity_check").fetchall()
        if ic != [("ok",)]:
            problems.append(f"integrity_check: {ic[:5]}")
        st = conn.execute("SELECT gql_check_storage()").fetchone()[0]
        if st != "[]":
            problems.append(f"storage: {st}")
        for (kind, name), whole in WHOLE.items():
            q = (f"MATCH (n:{name}) RETURN n.batch AS b, count(*) AS c" if kind == "node"
                 else f"MATCH ()-[r:{name}]->() RETURN r.batch AS b, count(*) AS c")
            for row in cypher(conn, q):
                if row["c"] != whole:
                    problems.append(f"partial {name} batch {row['b']}: {row['c']} of {whole}")
        for row in cypher(conn, "MATCH (n:B) RETURN n.batch AS b, "
                                "collect(DISTINCT coalesce(n.s, -1)) AS s"):
            if len(row["s"]) != 1:
                problems.append(f"partial SET on B batch {row['b']}: {row['s']}")
    finally:
        conn.close()
    return problems


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="tests.crash.run")
    p.add_argument("--ext", default=str(ROOT / "build" / "graphqlite"))
    p.add_argument("--rounds", type=int, default=200)
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--max-delay", type=float, default=0.6, help="seconds before the kill")
    p.add_argument("--dir", default=str(ROOT / "build" / "crash"))
    args = p.parse_args(argv)
    # the Python bindings want the file itself (graphqlite.dylib / .so)
    for suffix in ("", ".dylib", ".so", ".dll"):
        if Path(args.ext + suffix).is_file():
            args.ext = str(Path(args.ext + suffix).resolve())
            break

    rng = random.Random(args.seed)
    work = Path(args.dir)
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    db = work / "crash.db"
    bad = 0
    killed_mid_write = 0
    t0 = time.monotonic()
    for rnd in range(1, args.rounds + 1):
        journal = "wal" if rnd % 2 else "delete"
        proc = subprocess.Popen(
            [sys.executable, str(WRITER), str(db), args.ext, str(rnd), journal, str(rng.randrange(1 << 30))],
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        time.sleep(rng.uniform(0.05, args.max_delay))
        if proc.poll() is None:
            os.kill(proc.pid, signal.SIGKILL)
            killed_mid_write += 1
        _, err = proc.communicate()
        if proc.returncode not in (-signal.SIGKILL,):
            # the writer died by itself: a write failed
            print(f"round {rnd}: writer exited {proc.returncode}: {err.decode()[-400:]}", flush=True)
            bad += 1
        problems = check(db, args.ext)
        if problems:
            bad += 1
            print(f"round {rnd} ({journal}): " + "; ".join(problems[:5]), flush=True)
            keep = work / f"bad-round-{rnd}.db"
            if not keep.exists():
                shutil.copy(db, keep)
            break
    conn = sqlite3.connect(str(db))
    conn.enable_load_extension(True)
    conn.load_extension(args.ext)
    n = cypher(conn, "MATCH (n) RETURN count(*) AS c")[0]["c"]
    conn.close()
    print(f"crash test: {rnd} rounds ({killed_mid_write} killed while writing), "
          f"{n} nodes at the end, {bad} bad, {time.monotonic() - t0:.0f}s", flush=True)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
