"""Graph algorithm invariants on random graphs (GQLITE-T-0512).

Each graph is small (0 to 12 nodes, self-loops and parallel edges allowed),
its nodes carry a text `id` (the user_id the algorithms report) and its
edges a `cost`. The checks:

- pageRank: scores sum to 1 and do not change when the nodes are created in
  another order (other internal ids)
- wcc / scc: a partition of the nodes equal to a reference union-find /
  Tarjan in plain Python
- bfs / dfs: reach the same set, the nodes reachable along outgoing edges;
  bfs depth is the hop distance
- dijkstra / astar / apsp: the distances of a reference shortest path
  (hops, and by cost for dijkstra with a weight property)
- degreeCentrality: in and out degree counted in Python
- triangleCount: triangles of the undirected simple graph
- betweenness (directed, not normalized) and closeness (undirected,
  harmonic over n - 1): reference values
- eigenvectorCentrality, nodeSimilarity: do not change with the order
- knn: at most k neighbours, best first, with nodeSimilarity's values
- labelPropagation, louvain: each community lies inside one weak component

Usage: python -m tests.algorithms.run [--seed N] [--count N] [--ext PATH]
Exit code 1 when a check fails.
"""

from __future__ import annotations

import argparse
import heapq
import json
import random
import sqlite3
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EPS = 1e-6


class Graph:
    def __init__(self, n: int, edges: list[tuple[int, int, int]], int_ids: bool = False):
        self.n, self.edges = n, edges
        # text ids, or integer ids (GQLITE-T-0546: those were user_id null)
        self.ids = list(range(n)) if int_ids else [f"n{i}" for i in range(n)]

    def out(self) -> dict[int, list[tuple[int, int]]]:
        adj = defaultdict(list)
        for a, b, c in self.edges:
            adj[a].append((b, c))
        return adj

    def undirected(self) -> dict[int, set[int]]:
        adj = defaultdict(set)
        for a, b, _ in self.edges:
            if a != b:
                adj[a].add(b)
                adj[b].add(a)
        return adj


def random_graph(rng: random.Random) -> Graph:
    n = rng.randint(0, 12)
    m = rng.randint(0, 2 * n) if n else 0
    edges = [(rng.randrange(n), rng.randrange(n), rng.randint(1, 5)) for _ in range(m)]
    return Graph(n, edges, int_ids=rng.random() < 0.5)


def q(v) -> str:
    """An id as a Cypher literal."""
    return str(v) if isinstance(v, int) else f"'{v}'"


def load(ext: str, g: Graph, order: list[int]) -> sqlite3.Connection:
    """The graph in a fresh database; nodes are created in `order`."""
    conn = sqlite3.connect(":memory:")
    conn.enable_load_extension(True)
    conn.load_extension(ext)
    for i in order:
        cy(conn, f"CREATE (:N {{id: {q(g.ids[i])}}})")
    eorder = list(range(len(g.edges)))
    if order != sorted(order):          # the relabelled copy: edges in another order too
        random.Random(len(order) * 7 + 1).shuffle(eorder)
    for k in eorder:
        a, b, c = g.edges[k]
        cy(conn, f"MATCH (a:N {{id: {q(g.ids[a])}}}), (b:N {{id: {q(g.ids[b])}}}) "
                 f"CREATE (a)-[:R {{cost: {c}}}]->(b)")
    return conn


def cy(conn: sqlite3.Connection, q: str):
    raw = conn.execute("SELECT cypher(?)", (q,)).fetchone()[0]
    data = json.loads(raw) if raw else []
    return data if isinstance(data, list) else []


# ------------------------------------------------------------ references

def reach(g: Graph, s: int) -> dict[int, int]:
    """Hop distance along outgoing edges from s."""
    adj, dist, q = g.out(), {s: 0}, [s]
    for u in q:
        for v, _ in adj[u]:
            if v not in dist:
                dist[v] = dist[u] + 1
                q.append(v)
    return dist


def dijkstra_ref(g: Graph, s: int, weighted: bool) -> dict[int, float]:
    adj, dist, h = g.out(), {s: 0}, [(0, s)]
    while h:
        d, u = heapq.heappop(h)
        if d > dist.get(u, float("inf")):
            continue
        for v, c in adj[u]:
            nd = d + (c if weighted else 1)
            if nd < dist.get(v, float("inf")):
                dist[v] = nd
                heapq.heappush(h, (nd, v))
    return dist


def wcc_ref(g: Graph) -> list[set[int]]:
    parent = list(range(g.n))

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x
    for a, b, _ in g.edges:
        parent[find(a)] = find(b)
    groups = defaultdict(set)
    for i in range(g.n):
        groups[find(i)].add(i)
    return list(groups.values())


def scc_ref(g: Graph) -> list[set[int]]:
    adj = g.out()
    index, low, on, stack, out, counter = {}, {}, set(), [], [], [0]

    def visit(v):
        index[v] = low[v] = counter[0]
        counter[0] += 1
        stack.append(v)
        on.add(v)
        for w, _ in adj[v]:
            if w not in index:
                visit(w)
                low[v] = min(low[v], low[w])
            elif w in on:
                low[v] = min(low[v], index[w])
        if low[v] == index[v]:
            comp = set()
            while True:
                w = stack.pop()
                on.discard(w)
                comp.add(w)
                if w == v:
                    break
            out.append(comp)
    for v in range(g.n):
        if v not in index:
            visit(v)
    return out


def betweenness_ref(g: Graph) -> list[float]:
    """Brandes, directed, unweighted; each parallel edge is its own path
    (as the engine counts them), self-loops are no path."""
    adj = {u: [v for v, _ in vs if v != u] for u, vs in g.out().items()}
    cb = [0.0] * g.n
    for s in range(g.n):
        stack, pred = [], defaultdict(list)
        sigma, dist = defaultdict(float), {s: 0}
        sigma[s] = 1.0
        q = [s]
        for v in q:
            stack.append(v)
            for w in adj.get(v, []):
                if w not in dist:
                    dist[w] = dist[v] + 1
                    q.append(w)
                if dist[w] == dist[v] + 1:
                    sigma[w] += sigma[v]
                    pred[w].append(v)
        delta = defaultdict(float)
        while stack:
            w = stack.pop()
            for v in pred[w]:
                delta[v] += sigma[v] / sigma[w] * (1 + delta[w])
            if w != s:
                cb[w] += delta[w]
    return cb


def closeness_ref(g: Graph) -> list[float]:
    """Harmonic closeness on the undirected graph, over n - 1."""
    adj, out = g.undirected(), []
    for s in range(g.n):
        dist, q = {s: 0}, [s]
        for u in q:
            for v in adj[u]:
                if v not in dist:
                    dist[v] = dist[u] + 1
                    q.append(v)
        h = sum(1 / d for v, d in dist.items() if v != s)
        out.append(h / (g.n - 1) if g.n > 1 else 0.0)
    return out


def triangles_ref(g: Graph) -> list[int]:
    adj = g.undirected()
    return [sum(1 for a in adj[u] for b in adj[u] if a < b and b in adj[a]) for u in range(g.n)]


# ------------------------------------------------------------ checks

def by_user(rows, key) -> dict:
    return {r["user_id"]: r[key] for r in rows}


def partition(rows, g: Graph, key="component") -> list[set[int]]:
    groups = defaultdict(set)
    for r in rows:
        groups[r[key]].add(g.ids.index(r["user_id"]))
    return list(groups.values())


def same_partition(a, b) -> bool:
    return sorted(sorted(x) for x in a) == sorted(sorted(x) for x in b)


def close(a, b) -> bool:
    if a is None or b is None:
        return a is b
    return abs(a - b) <= EPS * max(1.0, abs(a), abs(b))


def check_graph(ext: str, g: Graph, rng: random.Random) -> list[tuple[str, str]]:
    fails = []
    order = list(range(g.n))
    perm = order[:]
    rng.shuffle(perm)
    c1, c2 = load(ext, g, order), load(ext, g, perm)

    def fail(name, detail):
        fails.append((name, detail))

    pr1, pr2 = cy(c1, "CALL pageRank()"), cy(c2, "CALL pageRank()")
    if g.n:
        s = sum(r["score"] for r in pr1)
        if abs(s - 1) > 1e-3:
            fail("pagerank_sum", f"sum {s}")
    a, b = by_user(pr1, "score"), by_user(pr2, "score")
    if a.keys() != b.keys() or any(not close(a[k], b[k]) for k in a):
        fail("pagerank_order", f"{a} vs {b}")

    if not same_partition(partition(cy(c1, "CALL wcc()"), g), wcc_ref(g)):
        fail("wcc", f"{cy(c1, 'CALL wcc()')}")
    if not same_partition(partition(cy(c1, "CALL scc()"), g), scc_ref(g)):
        fail("scc", f"{cy(c1, 'CALL scc()')}")

    for s in rng.sample(range(g.n), min(3, g.n)):
        sid = g.ids[s]
        ref = reach(g, s)
        bfs = {g.ids.index(r["user_id"]): r["depth"] for r in cy(c1, f"CALL bfs({q(sid)})")}
        dfs = {g.ids.index(r["user_id"]) for r in cy(c1, f"CALL dfs({q(sid)})")}
        if set(bfs) != set(ref):
            fail("bfs_reach", f"from {sid}: {sorted(bfs)} vs {sorted(ref)}")
        elif bfs != ref:
            fail("bfs_depth", f"from {sid}: {bfs} vs {ref}")
        if dfs != set(ref):
            fail("dfs_reach", f"from {sid}: {sorted(dfs)} vs {sorted(ref)}")
        hops, cost = dijkstra_ref(g, s, False), dijkstra_ref(g, s, True)
        for t in rng.sample(range(g.n), min(3, g.n)):
            tid = g.ids[t]
            d = cy(c1, f"CALL dijkstra({q(sid)}, {q(tid)})")
            got = d[0]["distance"] if d and d[0].get("found") else None
            if got != hops.get(t):
                fail("dijkstra_hops", f"{sid}->{tid}: {d} vs {hops.get(t)}")
            dw = cy(c1, f"CALL dijkstra({q(sid)}, {q(tid)}, 'cost')")
            gotw = dw[0]["distance"] if dw and dw[0].get("found") else None
            if not close(gotw, cost.get(t)):
                fail("dijkstra_cost", f"{sid}->{tid}: {dw} vs {cost.get(t)}")
            da = cy(c1, f"CALL astar({q(sid)}, {q(tid)})")
            gota = da[0]["distance"] if da and da[0].get("found") else None
            if not close(gota, hops.get(t)):
                fail("astar", f"{sid}->{tid}: {da} vs {hops.get(t)}")

    apsp = {(r["source"], r["target"]): r["distance"] for r in cy(c1, "CALL apsp()")}
    ref_apsp = {(g.ids[s], g.ids[t]): d for s in range(g.n) for t, d in reach(g, s).items() if t != s}
    if apsp != ref_apsp:
        fail("apsp", f"{sorted(set(apsp.items()) ^ set(ref_apsp.items()))[:6]}")

    deg = {r["user_id"]: (r["in_degree"], r["out_degree"], r["degree"]) for r in cy(c1, "CALL degreeCentrality()")}
    ind, outd = defaultdict(int), defaultdict(int)
    for x, y, _ in g.edges:
        outd[x] += 1
        ind[y] += 1
    ref_deg = {g.ids[i]: (ind[i], outd[i], ind[i] + outd[i]) for i in range(g.n)}
    if deg != ref_deg:
        fail("degree", f"{deg} vs {ref_deg}")

    tri = by_user(cy(c1, "CALL triangleCount()"), "triangles")
    ref_tri = dict(zip(g.ids, triangles_ref(g)))
    if tri != ref_tri:
        fail("triangles", f"{tri} vs {ref_tri}")

    bt = by_user(cy(c1, "CALL betweennessCentrality()"), "score")
    ref_bt = dict(zip(g.ids, betweenness_ref(g)))
    if bt.keys() != ref_bt.keys() or any(not close(bt[k], ref_bt[k]) for k in bt):
        fail("betweenness", f"{bt} vs {ref_bt}")
    cl = by_user(cy(c1, "CALL closenessCentrality()"), "score")
    ref_cl = dict(zip(g.ids, closeness_ref(g)))
    if cl.keys() != ref_cl.keys() or any(not close(cl[k], ref_cl[k]) for k in cl):
        fail("closeness", f"{cl} vs {ref_cl}")

    e1, e2 = by_user(cy(c1, "CALL eigenvectorCentrality()"), "score"), \
        by_user(cy(c2, "CALL eigenvectorCentrality()"), "score")
    if e1.keys() != e2.keys() or any(not close(e1[k], e2[k]) for k in e1):
        fail("eigenvector_order", f"{e1} vs {e2}")
    ns = lambda c: {tuple(sorted((r["node1"], r["node2"]))): r["similarity"] for r in cy(c, "CALL nodeSimilarity()")}
    s1, s2 = ns(c1), ns(c2)
    if s1.keys() != s2.keys() or any(not close(s1[k], s2[k]) for k in s1):
        fail("similarity_order", f"{sorted(set(s1.items()) ^ set(s2.items()))[:6]}")

    # knn: at most k neighbours, best first, each with the similarity that
    # nodeSimilarity gives the pair
    for src in rng.sample(range(g.n), min(2, g.n)):
        k = rng.randint(1, 4)
        rows = cy(c1, f"CALL knn({q(g.ids[src])}, {k})")
        sims = [r["similarity"] for r in rows]
        bad = len(rows) > k or sims != sorted(sims, reverse=True) or any(
            not close(r["similarity"], s1.get(tuple(sorted((g.ids[src], r["neighbor"]))), 0.0))
            for r in rows)
        if bad:
            fail("knn", f"knn('{g.ids[src]}', {k}) = {rows}")

    comp = {i: k for k, grp in enumerate(wcc_ref(g)) for i in grp}
    for algo in ("labelPropagation", "louvain"):
        for grp in partition(cy(c1, f"CALL {algo}()"), g, key="community"):
            if len({comp[i] for i in grp}) > 1:
                fail(algo, f"community {sorted(g.ids[i] for i in grp)} spans components")
                break
    c1.close()
    c2.close()
    return fails


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="tests.algorithms.run")
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--count", type=int, default=300)
    p.add_argument("--ext", default=str(ROOT / "build" / "graphqlite"))
    args = p.parse_args(argv)
    rng = random.Random(args.seed)
    by_check = defaultdict(list)
    for k in range(args.count):
        g = random_graph(rng)
        try:
            for name, detail in check_graph(args.ext, g, rng):
                by_check[name].append((k, g, detail))
        except Exception as e:      # an error of the engine is a finding too
            by_check["error"].append((k, g, repr(e)))
    for name, items in sorted(by_check.items()):
        k, g, detail = items[0]
        print(f"FAIL {name}: {len(items)} graph(s); first: graph {k}, n={g.n}, "
              f"edges={[(g.ids[a], g.ids[b], c) for a, b, c in g.edges]}\n  {detail[:600]}")
    print(f"algorithm invariants (seed {args.seed}): {args.count} graphs, "
          f"{sum(len(v) for v in by_check.values())} failure(s) in {len(by_check)} check(s)")
    return 1 if by_check else 0


if __name__ == "__main__":
    sys.exit(main())
