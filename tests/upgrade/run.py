"""Upgrade test (GQLITE-T-0527): databases written by earlier releases open
with this build and answer the same.

    python -m tests.upgrade.run [--ext build/graphqlite] [--releases v0.9.4,...]

For each release: export its source with `git archive` into
build/upgrade/<tag>/src and build the extension there (kept between runs),
write a fixture graph with it into build/upgrade/<tag>/fixture.db, and run
QUERIES against it, still with the old build. Then open the same file with
this build and:

  * run QUERIES and compare with this build's answers on a fixture it
    wrote itself: a difference means the old file is read differently
    (or the old release stored a value differently, listed in OLD_WRITES);
  * report where the old release's own answers differ (bugs fixed since;
    information, not a failure);
  * check gql_check_storage() and gql_check_constraints();
  * write to it (WRITES), and check the storage again.

Exit code 1 when a release differs or fails.
"""

from __future__ import annotations

import argparse
import json
import math
import shutil
import sqlite3
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RELEASES = ["v0.9.4", "v0.10.0", "v0.11.0", "v0.12.0"]

# The fixture: every value type, labels, relationships with properties, an
# index and a constraint (statements an old release does not know are
# skipped and reported).
FIXTURE = [
    "CREATE (:Person:Admin {name: 'Robin', age: 41, score: 3.5, active: true, "
    "tags: ['a', 'b'], meta: {k: 1, l: [1, 2]}, nothing: null, text_num: '12', "
    "text_json: '[1]', neg: -7, big: 9007199254740993, tiny: 0.000001})",
    "CREATE (:Person {name: 'Sam', age: 29, active: false, score: -0.5})",
    "CREATE (:Person {name: 'Jamie', age: 35})",
    "CREATE (:City {name: 'Lyon', founded: date('2020-01-02'), "
    "at: datetime('2021-06-01T10:30:00Z'), local: localdatetime('2021-06-01T10:30:00'), "
    "t: time('10:30:00+01:00'), lt: localtime('10:30:00'), d: duration('P1Y2M3DT4H5M6S')})",
    "CREATE (:Num {inf: 1.0 / 0.0, ninf: -1.0 / 0.0, nan: 0.0 / 0.0, whole: 2.0})",
    "MATCH (a:Person {name: 'Robin'}), (b:Person {name: 'Sam'}) "
    "CREATE (a)-[:KNOWS {since: 2019, w: 0.5, on: true, via: ['x'], note: 'n'}]->(b)",
    "MATCH (b:Person {name: 'Sam'}), (c:Person {name: 'Jamie'}) CREATE (b)-[:KNOWS {since: 2021}]->(c)",
    "MATCH (a:Person {name: 'Robin'}), (l:City) CREATE (a)-[:LIVES_IN]->(l)",
    "MATCH (c:Person {name: 'Jamie'}) CREATE (c)-[:SELF]->(c)",
    "CREATE INDEX FOR (p:Person) ON (p.name)",
    "CREATE CONSTRAINT FOR (c:City) REQUIRE c.name IS UNIQUE",
]

QUERIES = [
    "MATCH (n) RETURN labels(n) AS l, properties(n) AS p ORDER BY p.name, l",
    "MATCH (a)-[r]->(b) RETURN a.name AS a, type(r) AS t, properties(r) AS p, b.name AS b ORDER BY a, t, b",
    "MATCH (p:Person) WHERE p.age > 30 RETURN p.name AS n ORDER BY n",
    "MATCH (p:Person {name: 'Robin'}) RETURN p.tags[1] AS t, p.meta.l AS l, p.big AS big, p.tiny AS tiny, "
    "p.text_num AS tn, p.text_json AS tj, p.active AS a, p.nothing AS z",
    "MATCH (c:City) RETURN c.founded AS f, c.founded.year AS y, c.d AS d, c.at AS at",
    "MATCH (n:Num) RETURN n.inf AS i, n.ninf AS ni, isNaN(n.nan) AS nan, n.whole AS w",
    "MATCH (a:Person)-[:KNOWS*1..2]->(b) RETURN a.name AS a, collect(b.name) AS bs ORDER BY a",
    "MATCH (p:Person) RETURN count(*) AS c, avg(p.age) AS avg, collect(p.name) AS names",
    "MATCH (c:Person) WHERE (c)-[:SELF]->(c) RETURN c.name AS n",
]

WRITES = [
    "CREATE (:Person {name: 'New', age: 1, tags: ['z']})",
    "MATCH (p:Person {name: 'Robin'}) SET p.age = p.age + 1, p.tags = p.tags + ['c'] REMOVE p.nothing",
    "MATCH (p:Person {name: 'Sam'}) DETACH DELETE p",
    "MERGE (c:City {name: 'Lyon'}) ON MATCH SET c.seen = true",
    "MATCH (a:Person {name: 'Jamie'}), (b:Person {name: 'New'}) CREATE (a)-[:KNOWS {since: 2026}]->(b)",
]

# Values an old release stored differently, so this build reads them back
# differently from a fixture it wrote itself: (query index, release prefix)
# -> why. Listed differences are reported but do not fail.
OLD_WRITES: dict[tuple[int, str], str] = {
    # Up to 0.12.0 a CREATE of 1.0 / 0.0 stored no property at all; the
    # file reads back as written (GQLITE-T-0484 stores Infinity).
    # Up to 0.12.0 a time or datetime kept zero seconds as written
    # ('10:30:00'); this build prints one form ('10:30', GQLITE-T-0550). The
    # stored text reads back as it was written.
    (0, "v0."): "Infinity / -Infinity were not stored (GQLITE-T-0484); zero seconds of stored "
                "times kept as written (GQLITE-T-0550)",
    (4, "v0."): "zero seconds of a stored datetime kept as written (GQLITE-T-0550)",
    (5, "v0."): "Infinity / -Infinity were not stored (fixed after 0.12.0, GQLITE-T-0484)",
    # 0.9.4 and 0.10.0 stored the key of `{on: true}` as `On`.
    (1, "v0.9."): "the property key `on` was stored as `On` (fixed in 0.11.0)",
    (1, "v0.10."): "the property key `on` was stored as `On` (fixed in 0.11.0)",
}


def resolve(ext: str) -> str:
    for suffix in ("", ".dylib", ".so", ".dll"):
        if Path(ext + suffix).is_file():
            return str(Path(ext + suffix).resolve())
    raise SystemExit(f"extension not found: {ext}")


def open_db(path: Path, ext: str) -> sqlite3.Connection:
    conn = sqlite3.connect(str(path))
    conn.enable_load_extension(True)
    conn.load_extension(ext)
    return conn


def cypher(conn: sqlite3.Connection, q: str):
    try:
        raw = conn.execute("SELECT cypher(?)", (q,)).fetchone()[0]
    except sqlite3.Error as e:
        return {"error": str(e)[:300]}
    conn.commit()
    try:
        return json.loads(raw, parse_constant=float) if raw else []
    except ValueError:
        return {"raw": raw}


def canon(x):
    """Comparable form: floats by value (NaN equal to NaN), dicts by key."""
    if isinstance(x, float):
        return "NaN" if math.isnan(x) else round(x, 12)
    if isinstance(x, dict):
        return {k: canon(v) for k, v in sorted(x.items())}
    if isinstance(x, list):
        return [canon(v) for v in x]
    return x


def build_release(tag: str, work: Path) -> str | None:
    src = work / tag / "src"
    lib = None
    for suffix in (".dylib", ".so"):
        cand = src / "build" / f"graphqlite{suffix}"
        if cand.is_file():
            lib = cand
    if lib:
        return str(lib)
    shutil.rmtree(src, ignore_errors=True)
    src.mkdir(parents=True)
    archive = subprocess.run(["git", "-C", str(ROOT), "archive", tag], capture_output=True)
    if archive.returncode != 0:
        print(f"{tag}: git archive failed: {archive.stderr.decode()[-300:]}")
        return None
    subprocess.run(["tar", "-x", "-C", str(src)], input=archive.stdout, check=True)
    r = subprocess.run(["make", "extension"], cwd=src, capture_output=True, text=True)
    if r.returncode != 0:
        print(f"{tag}: build failed:\n{r.stdout[-800:]}{r.stderr[-800:]}")
        return None
    for suffix in (".dylib", ".so"):
        cand = src / "build" / f"graphqlite{suffix}"
        if cand.is_file():
            return str(cand)
    return None


def storage(conn: sqlite3.Connection) -> list[str]:
    out = []
    try:
        v = conn.execute("SELECT gql_check_storage()").fetchone()[0]
        if v != "[]":
            out.append(f"storage: {v}")
        conn.execute("SELECT gql_check_constraints()").fetchone()
    except sqlite3.Error as e:
        out.append(f"check: {e}")
    return out


def write_fixture(db: Path, ext: str) -> list[str]:
    db.unlink(missing_ok=True)
    conn = open_db(db, ext)
    skipped = []
    for q in FIXTURE:
        r = cypher(conn, q)
        if isinstance(r, dict) and "error" in r:
            skipped.append(q.split("(")[0].strip()[:40] + f" ({r['error'][:80]})")
    conn.close()
    return skipped


def answers(db: Path, ext: str) -> list:
    conn = open_db(db, ext)
    out = [cypher(conn, q) for q in QUERIES]
    conn.close()
    return out


def diff_line(i: int, a_name: str, a, b_name: str, b) -> str:
    return (f"  Q{i}: {QUERIES[i][:70]}\n    {a_name}: {json.dumps(a)[:400]}\n"
            f"    {b_name}: {json.dumps(b)[:400]}")


def run_release(tag: str, new_ext: str, work: Path, reference: list) -> bool:
    old_ext = build_release(tag, work)
    if not old_ext:
        return False
    db = work / tag / "fixture.db"
    skipped = write_fixture(db, old_ext)
    old_answers = answers(db, old_ext)
    now = answers(db, new_ext)
    ok = True
    fails, notes, fixed = [], [], []
    for i, (ref, got, was) in enumerate(zip(reference, now, old_answers)):
        if canon(ref) != canon(got):
            why = next((w for (qi, pre), w in OLD_WRITES.items() if qi == i and tag.startswith(pre)), None)
            line = diff_line(i, "new file", ref, f"{tag} file", got)
            if why:
                notes.append(line + f"\n    ({tag} stored it differently: {why})")
            else:
                ok = False
                fails.append(line)
        if canon(was) != canon(got):
            fixed.append(i)
    conn = open_db(db, new_ext)
    version = conn.execute("PRAGMA user_version").fetchone()[0]
    problems = storage(conn)
    for q in WRITES:
        r = cypher(conn, q)
        if isinstance(r, dict) and "error" in r:
            problems.append(f"write failed: {q[:60]}: {r['error']}")
    problems += storage(conn)
    conn.close()
    if problems:
        ok = False
    print(f"{tag}: {'ok' if ok else 'DIFFERS'} (user_version {version}; "
          f"{len(FIXTURE) - len(skipped)}/{len(FIXTURE)} fixture statements; "
          f"{tag} itself answered {len(fixed)} queries differently: {', '.join(f'Q{i}' for i in fixed) or 'none'})")
    for s_ in skipped:
        print(f"  not in {tag}: {s_}")
    for n in fails + notes:
        print(n)
    for p_ in problems:
        print(f"  {p_}")
    return ok


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="tests.upgrade.run")
    p.add_argument("--ext", default=str(ROOT / "build" / "graphqlite"))
    p.add_argument("--releases", default=",".join(RELEASES))
    p.add_argument("--dir", default=str(ROOT / "build" / "upgrade"))
    args = p.parse_args(argv)
    new_ext = resolve(args.ext)
    work = Path(args.dir)
    work.mkdir(parents=True, exist_ok=True)
    ref_db = work / "reference.db"
    skipped = write_fixture(ref_db, new_ext)
    if skipped:
        print(f"this build skipped fixture statements: {skipped}")
        return 1
    reference = answers(ref_db, new_ext)
    results = [run_release(tag, new_ext, work, reference) for tag in args.releases.split(",")]
    print(f"upgrade test: {sum(results)}/{len(results)} releases ok")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
