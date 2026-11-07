"""Crash-test writer (GQLITE-T-0530): writes tagged batches until killed.

    python writer.py DB EXT ROUND JOURNAL SEED

Every write tags what it writes with a batch id, so the checker can tell a
whole write (all of its rows) from a partial one. The kinds:

  unwind  UNWIND range(1, 20) ... CREATE (:B {batch})          20 :B nodes
  rel     UNWIND range(1, 10) ... CREATE (:E)-[:R]->(:E)       20 :E, 10 :R
  merge   FOREACH (i IN range(1, 10) | MERGE (:M {batch, i}))  10 :M nodes
  set     MATCH (n:B {batch: old}) SET n.s = batch             one value per :B batch
  detach  MATCH (n:E {batch: old}) DETACH DELETE n             an :E batch goes whole
  txn     BEGIN; 3 x CREATE (:T {batch}); COMMIT               3 :T nodes
  bulk    insert_nodes_bulk(15 :Bulk) then insert_edges_bulk(10 :BR)
"""

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "bindings" / "python" / "src"))

import graphqlite  # noqa: E402


def main() -> None:
    db_path, ext, rnd, journal, seed = sys.argv[1:6]
    rng = random.Random(int(seed))
    g = graphqlite.graph(db_path, extension_path=ext)
    conn = g.connection
    sq = conn.sqlite_connection
    sq.isolation_level = None                 # autocommit; BEGIN / COMMIT by hand
    sq.execute(f"PRAGMA journal_mode = {journal}")
    sq.execute("PRAGMA synchronous = FULL")
    batch = int(rnd) * 100000
    made: dict[str, list[int]] = {"B": [], "E": []}
    while True:
        batch += 1
        b = batch
        kind = rng.choice(["unwind", "rel", "merge", "set", "detach", "txn", "bulk"])
        if kind == "unwind":
            conn.cypher("UNWIND range(1, 20) AS i CREATE (:B {batch: $b, i: i})", {"b": b})
            made["B"].append(b)
        elif kind == "rel":
            conn.cypher("UNWIND range(1, 10) AS i "
                        "CREATE (:E {batch: $b, i: i})-[:R {batch: $b}]->(:E {batch: $b, i: -i})", {"b": b})
            made["E"].append(b)
        elif kind == "merge":
            conn.cypher("FOREACH (i IN range(1, 10) | MERGE (:M {batch: $b, i: i}))", {"b": b})
        elif kind == "set" and made["B"]:
            conn.cypher("MATCH (n:B) WHERE n.batch = $old SET n.s = $b",
                        {"old": rng.choice(made["B"]), "b": b})
        elif kind == "detach" and made["E"]:
            old = made["E"].pop(rng.randrange(len(made["E"])))
            conn.cypher("MATCH (n:E) WHERE n.batch = $old DETACH DELETE n", {"old": old})
        elif kind == "txn":
            sq.execute("BEGIN")
            for step in (1, 2, 3):
                conn.cypher("CREATE (:T {batch: $b, step: $s})", {"b": b, "s": step})
            sq.execute("COMMIT")
        elif kind == "bulk":
            nodes = [(f"x{b}_{i}", {"batch": b, "i": i}, "Bulk") for i in range(15)]
            ids = g.insert_nodes_bulk(nodes)
            edges = [(f"x{b}_{i}", f"x{b}_{i + 1}", {"batch": b}, "BR") for i in range(10)]
            g.insert_edges_bulk(edges, ids)


if __name__ == "__main__":
    main()
