# /// script
# dependencies = ["networkx"]
# ///
"""Cross-check `yen()` against brute-force / NetworkX oracles.

Dev-time oracle for `src/backend/executor/graph_algo_yen.c`: generates
random simple directed graphs whose node IDs exercise JSON escaping
(quotes, backslashes, newlines, tabs), builds each one through the
extension, and checks

- the unbounded result against `networkx.shortest_simple_paths`,
- the hop-capped result against bruteforce enumeration
  (`networkx.all_simple_paths` with `cutoff=H`), and
- that the two optional arguments are order-independent and the capped
  result obeys the cap.

Parallel edges are covered by the CUnit suite instead - NetworkX refuses
multi-edges.

Run from the repository root after `angreal build extension`:

    uv run scripts/check-yen-oracle.py [--cases N] [--seed S]
"""

import argparse
import json
import random
import sqlite3
import sys
from pathlib import Path

import networkx as nx

ROOT = Path(__file__).resolve().parent.parent
EXTENSION_ENTRYPOINT = "sqlite3_graphqlite_init"
WEIGHT = "w"
WEIGHT_CHOICES = [0, 1, 2, 3, 0.5, 1.5, 2.5, 4]
NODE_ID_SUFFIXES = ["", '"q', "\\b", "\nnl", "\ttab"]


def make_node_id(rng, index):
    return f"n{index}" + rng.choice(NODE_ID_SUFFIXES)


def extension_path() -> Path:
    for name in ("graphqlite.dylib", "graphqlite.so", "graphqlite.dll"):
        path = ROOT / "build" / name
        if path.exists():
            return path
    sys.exit("built extension not found; run `angreal build extension` first")


def connect() -> sqlite3.Connection:
    conn = sqlite3.connect(":memory:")
    conn.enable_load_extension(True)
    conn.load_extension(str(extension_path()), entrypoint=EXTENSION_ENTRYPOINT)
    conn.enable_load_extension(False)
    return conn


def cypher(conn, query, params=None):
    if params is None:
        row = conn.execute("SELECT cypher(?1)", (query,)).fetchone()
    else:
        row = conn.execute(
            "SELECT cypher(?1, ?2)", (query, json.dumps(params))
        ).fetchone()
    return row[0]


def run_yen(conn, source, target, k, weight, max_hops=None, hops_first=True):
    if max_hops is None:
        query = "RETURN yen($s, $t, $k, $w)"
        params = {"s": source, "t": target, "k": k, "w": weight}
    elif hops_first:
        query = "RETURN yen($s, $t, $k, $h, $w)"
        params = {"s": source, "t": target, "k": k, "h": max_hops, "w": weight}
    else:
        query = "RETURN yen($s, $t, $k, $w, $h)"
        params = {"s": source, "t": target, "k": k, "w": weight, "h": max_hops}
    raw = cypher(conn, query, params)
    return raw, json.loads(raw)[0]["column_0"]


def run_dijkstra(conn, source, target, weight):
    raw = cypher(
        conn,
        "RETURN dijkstra($s, $t, $w)",
        {"s": source, "t": target, "w": weight},
    )
    return json.loads(raw)[0]["column_0"]


def build_random_graph(conn, rng, nodes):
    for node in nodes:
        cypher(conn, "CREATE (:N {id: $id})", {"id": node})
    edges = []  # (rowid, source, target, weight)
    rowid = 1
    for u in nodes:
        for v in nodes:
            if u == v or rng.random() >= 0.35:
                continue
            weight = rng.choice(WEIGHT_CHOICES)
            cypher(
                conn,
                "MATCH (a:N {id: $u}), (b:N {id: $v}) CREATE (a)-[:E {w: $w}]->(b)",
                {"u": u, "v": v, "w": weight},
            )
            edges.append((rowid, u, v, float(weight)))
            rowid += 1
    return edges


def build_nx_graph(edges, nodes):
    graph = nx.DiGraph()
    graph.add_nodes_from(nodes)
    for _, u, v, weight in edges:
        graph.add_edge(u, v, w=weight)
    return graph


def path_cost(graph, path):
    return sum(graph[u][v]["w"] for u, v in zip(path, path[1:]))


def oracle_paths(graph, source, target, k):
    if not graph.has_node(source) or not graph.has_node(target):
        return []
    try:
        paths = []
        for path in nx.shortest_simple_paths(graph, source, target, weight=WEIGHT):
            paths.append(path)
            if len(paths) >= k:
                break
        return paths
    except nx.NetworkXNoPath:
        return []


def oracle_bounded_paths(graph, source, target, k, max_hops):
    """Brute-force: every simple path within the hop budget, cheapest first."""
    if not graph.has_node(source) or not graph.has_node(target):
        return []
    paths = list(nx.all_simple_paths(graph, source, target, cutoff=max_hops))
    paths.sort(key=lambda path: path_cost(graph, path))
    return paths[:k]


def fail(seed, message, detail):
    print(f"FAIL seed={seed}: {message}")
    print(detail)
    sys.exit(1)


def validate_result(seed, result, expected, graph, edge_by_rowid, max_hops=None):
    routes = result["paths"]
    detail = (
        f"expected: {expected}\n"
        f"actual:   {routes}\n"
        f"max_hops: {max_hops}"
    )

    if len(routes) != len(expected):
        fail(seed, f"route count {len(routes)} != oracle {len(expected)}", detail)

    if bool(routes) != bool(result["found"]):
        fail(seed, "found flag disagrees with the route list", detail)

    distances = [route["distance"] for route in routes]
    if distances != sorted(distances):
        fail(seed, "distances are not ordered", detail)

    expected_costs = sorted(path_cost(graph, path) for path in expected)
    for got, want in zip(sorted(distances), expected_costs):
        if abs(got - want) > 1e-9:
            fail(seed, f"cost multiset {sorted(distances)} != oracle {expected_costs}", detail)

    # Routes tied at the k-th cost may be picked in any order, so only the
    # cost multiset is pinned.  Every returned route must be a distinct,
    # loopless, real path with a matching cost.
    seen = set()
    for route in routes:
        path = route["path"]
        key = tuple(path)
        if key in seen:
            fail(seed, f"duplicate route {key}", detail)
        seen.add(key)
        if len(key) != len(set(key)):
            fail(seed, f"route {key} repeats a node", detail)
        if max_hops is not None and len(key) - 1 > max_hops:
            fail(seed, f"route {key} exceeds the hop cap {max_hops}", detail)

        rowids = route["edges"]
        if len(rowids) != len(path) - 1:
            fail(seed, f"edge count does not match path {path}", detail)
        for index, rowid in enumerate(rowids):
            if rowid not in edge_by_rowid:
                fail(seed, f"unknown edge rowid {rowid}", detail)
            u, v, weight = edge_by_rowid[rowid]
            if (u, v) != (path[index], path[index + 1]):
                fail(seed, f"edge {rowid} ({u}->{v}) does not match path {path}", detail)
        if abs(route["distance"] - path_cost(graph, path)) > 1e-9:
            fail(seed, f"distance does not match edge weights for {path}", detail)


def check_case(seed, case):
    rng = random.Random(f"{seed}:{case}")
    node_count = rng.randint(2, 8)
    nodes = [make_node_id(rng, i) for i in range(node_count)]
    source, target = rng.sample(nodes, 2)
    k = rng.randint(1, 6)
    max_hops = rng.randint(1, 5)

    conn = connect()
    edges = build_random_graph(conn, rng, nodes)
    graph = build_nx_graph(edges, nodes)
    edge_by_rowid = {rowid: (u, v, weight) for rowid, u, v, weight in edges}

    # Unbounded: match NetworkX and be deterministic.
    first_raw, result = run_yen(conn, source, target, k, WEIGHT)
    second_raw, _ = run_yen(conn, source, target, k, WEIGHT)
    if first_raw != second_raw:
        fail(seed, f"non-deterministic result (seed {seed}, case {case})",
             f"first:  {first_raw}\nsecond: {second_raw}")

    expected = oracle_paths(graph, source, target, k)
    validate_result(seed, result, expected, graph, edge_by_rowid)

    # k = 1 must agree with dijkstra().
    dijkstra = run_dijkstra(conn, source, target, WEIGHT)
    routes = result["paths"]
    if not routes:
        if dijkstra["found"]:
            fail(seed, "yen found nothing but dijkstra found a path",
                 f"dijkstra: {dijkstra}")
    else:
        if not dijkstra["found"]:
            fail(seed, "dijkstra found nothing but yen found a path",
                 f"yen: {routes}")
        if routes[0]["distance"] != dijkstra["distance"]:
            fail(seed, "k=1 distance differs between yen and dijkstra",
                 f"yen: {routes[0]}\ndijkstra: {dijkstra}")

    # Hop-capped: match brute force, obey the cap, accept both argument orders.
    hops_raw, hops_result = run_yen(conn, source, target, k, WEIGHT,
                                    max_hops=max_hops, hops_first=True)
    weight_raw, weight_result = run_yen(conn, source, target, k, WEIGHT,
                                        max_hops=max_hops, hops_first=False)
    if hops_raw != weight_raw:
        fail(seed, "argument order changed the result",
             f"hops-first:   {hops_raw}\nweight-first: {weight_raw}")
    if weight_result != hops_result:
        fail(seed, "argument order changed the parsed result", "")

    # Determinism under the cap (the hops-first run already re-ran it).
    repeat_raw, _ = run_yen(conn, source, target, k, WEIGHT,
                            max_hops=max_hops, hops_first=True)
    if repeat_raw != hops_raw:
        fail(seed, "non-deterministic bounded result",
             f"first:  {hops_raw}\nsecond: {repeat_raw}")

    expected_bounded = oracle_bounded_paths(graph, source, target, k, max_hops)
    validate_result(seed, hops_result, expected_bounded, graph, edge_by_rowid,
                    max_hops=max_hops)

    conn.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", type=int, default=300)
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()

    for case in range(args.cases):
        check_case(args.seed, case)
    print(f"ok: {args.cases} random cases match the oracles (seed {args.seed})")


if __name__ == "__main__":
    main()
