"""Graph algorithms checked against NetworkX.

Each test builds the same directed graph in GraphQLite and in NetworkX and
compares the results. The graphs are a set of special shapes (path, cycle,
stars, DAG, complete graph, two components, reciprocal edges) and seeded
random graphs with an isolated node, so the rank of nodes without
out-edges, unreachable pairs and weights from 1 to 9 are always covered.

Each test was written against a known bug first (GQLITE-T-0381 to
GQLITE-T-0385) and passes now.

Semantics that the comparisons pin down:
- betweenness: directed, not normalized
- closeness: harmonic, on the undirected graph, divided by n - 1
- node similarity and knn: Jaccard over out-neighbours
- triangles and clustering: on the undirected graph
- apsp, bfs, astar: hop counts on the directed graph
"""

import itertools
import json
import random

import pytest

from graphqlite import Graph

from test_graph import get_extension_path

nx = pytest.importorskip("networkx")
from networkx.algorithms.community import modularity  # noqa: E402
from networkx.algorithms.link_analysis.pagerank_alg import _pagerank_python  # noqa: E402

TOL = 1e-4


def _special_graphs():
    path = [("a", "b"), ("b", "c"), ("c", "d")]
    cycle = [("a", "b"), ("b", "c"), ("c", "d"), ("d", "a")]
    star_out = [("h", x) for x in "wxyz"]
    star_in = [(x, "h") for x in "wxyz"]
    dag = [("a", "b"), ("a", "c"), ("b", "d"), ("c", "d"), ("d", "e")]
    complete = [(u, v) for u in "abcd" for v in "abcd" if u != v]
    two_parts = [("a", "b"), ("b", "c"), ("c", "a"), ("x", "y"), ("y", "z")]
    reciprocal = [("a", "b"), ("b", "a"), ("b", "c"), ("c", "b"), ("c", "d")]
    graphs = {
        "path": path,
        "cycle": cycle,
        "star_out": star_out,
        "star_in": star_in,
        "dag": dag,
        "complete": complete,
        "two_parts": two_parts,
        "reciprocal": reciprocal,
    }
    out = {}
    for name, edges in graphs.items():
        G = nx.DiGraph()
        for i, (u, v) in enumerate(edges):
            G.add_edge(u, v, w=1 + (i * 7) % 9)
        out[name] = G
    return out


def _random_graph(seed):
    rnd = random.Random(seed)
    n = rnd.randint(5, 18)
    p = rnd.choice([0.1, 0.25, 0.45])
    G = nx.DiGraph()
    names = [f"n{i}" for i in range(n)]
    G.add_nodes_from(names)
    for u in names[:-1]:  # the last node stays isolated
        for v in names[:-1]:
            if u != v and rnd.random() < p:
                G.add_edge(u, v, w=rnd.randint(1, 9))
    return G


GRAPHS = {**_special_graphs(), **{f"random{s}": _random_graph(s) for s in range(10)}}


def _load(G):
    g = Graph(":memory:", extension_path=get_extension_path())
    for node in G:
        g.upsert_node(node, {}, label="N")
    for u, v, data in G.edges(data=True):
        g.upsert_edge(u, v, {"w": data["w"]}, rel_type="E")
    return g


@pytest.fixture(scope="module", params=sorted(GRAPHS))
def pair(request):
    """(NetworkX DiGraph, GraphQLite Graph) holding the same graph."""
    G = GRAPHS[request.param]
    g = _load(G)
    yield G, g
    g.close()


def _failures(check):
    """Run check(G, g) on every graph; return the names where it fails."""
    failed = []
    for name in sorted(GRAPHS):
        g = _load(GRAPHS[name])
        try:
            if not check(GRAPHS[name], g):
                failed.append(name)
        finally:
            g.close()
    return failed


def _by_user(rows, key):
    return {r["user_id"]: r[key] for r in rows}


def _partition(rows, key):
    groups = {}
    for r in rows:
        groups.setdefault(r[key], set()).add(r["user_id"])
    return sorted(sorted(c) for c in groups.values())


def _cypher_list(g, query):
    value = next(iter(g.query(query)[0].values()))
    return json.loads(value) if isinstance(value, str) else value


def _assert_scores(got, expected, tol=TOL):
    assert set(got) == set(expected)
    for node, value in expected.items():
        assert got[node] == pytest.approx(value, abs=tol), node


def _jaccard_out(G, a, b):
    sa, sb = set(G.successors(a)), set(G.successors(b))
    return len(sa & sb) / len(sa | sb) if sa | sb else 0.0


# --- centrality ------------------------------------------------------------

def test_pagerank(pair):
    G, g = pair
    expected = _pagerank_python(G, alpha=0.85, max_iter=1000, tol=1e-12)
    _assert_scores(_by_user(g.pagerank(damping=0.85, iterations=200), "score"), expected)


def test_top_pagerank(pair):
    G, g = pair
    expected = sorted(_pagerank_python(G, alpha=0.85, max_iter=1000, tol=1e-12).values(), reverse=True)
    top = _cypher_list(g, "RETURN topPageRank(3, 0.85, 100)")
    assert [t["score"] for t in top] == pytest.approx(expected[:3], abs=TOL)


def test_personalized_pagerank(pair):
    G, g = pair
    seed = sorted(G)[0]
    ids = {r["user_id"]: int(r["node_id"]) for r in g.degree_centrality()}
    names = {v: k for k, v in ids.items()}
    expected = _pagerank_python(G, alpha=0.85, personalization={seed: 1}, dangling={seed: 1},
                                max_iter=1000, tol=1e-12)
    rows = _cypher_list(g, f"RETURN personalizedPageRank('[{ids[seed]}]', 0.85, 100)")
    _assert_scores({names[r["node_id"]]: r["score"] for r in rows}, expected, tol=1e-3)


def test_degree_centrality(pair):
    G, g = pair
    got = {r["user_id"]: (r["in_degree"], r["out_degree"], r["degree"]) for r in g.degree_centrality()}
    assert got == {n: (G.in_degree(n), G.out_degree(n), G.degree(n)) for n in G}


def test_betweenness_centrality(pair):
    G, g = pair
    expected = nx.betweenness_centrality(G, normalized=False)
    _assert_scores(_by_user(g.betweenness_centrality(), "score"), expected, tol=1e-3)


def test_closeness_centrality(pair):
    G, g = pair
    n = G.number_of_nodes()
    expected = {k: v / (n - 1) for k, v in nx.harmonic_centrality(G.to_undirected()).items()}
    _assert_scores(_by_user(g.closeness_centrality(), "score"), expected, tol=1e-3)


def test_eigenvector_centrality():
    def check(G, g):
        try:
            expected = nx.eigenvector_centrality(G, max_iter=10000, tol=1e-12)
        except nx.PowerIterationFailedConvergence:
            return True  # undefined on this graph (no cycles)
        got = _by_user(g.eigenvector_centrality(iterations=1000), "score")
        return all(abs(got[n] - v) <= 1e-2 for n, v in expected.items())
    assert _failures(check) == []


# --- components and communities -------------------------------------------

def test_weakly_connected_components(pair):
    G, g = pair
    expected = sorted(sorted(c) for c in nx.weakly_connected_components(G))
    assert _partition(g.weakly_connected_components(), "component") == expected


def test_strongly_connected_components(pair):
    G, g = pair
    expected = sorted(sorted(c) for c in nx.strongly_connected_components(G))
    assert _partition(g.strongly_connected_components(), "component") == expected


def _undirected_weighted(G):
    """Each directed edge adds 1 to the weight of its undirected edge."""
    U = nx.Graph()
    U.add_nodes_from(G)
    for u, v in G.edges():
        weight = U[u][v]["weight"] + 1 if U.has_edge(u, v) else 1
        U.add_edge(u, v, weight=weight)
    return U


def test_louvain_modularity():
    def check(G, g):
        U = _undirected_weighted(G)
        if U.number_of_edges() == 0:
            return True
        groups = [set(c) for c in _partition(g.louvain(resolution=1.0), "community")]
        q = modularity(U, groups, weight="weight")
        singletons = modularity(U, [{n} for n in U], weight="weight")
        best = max(modularity(U, nx.community.louvain_communities(U, weight="weight", seed=s), weight="weight")
                   for s in range(5))
        # Louvain starts from singletons and only makes moves that raise modularity.
        return q >= singletons - 1e-9 and q >= best - 0.05
    assert _failures(check) == []


def test_louvain_covers_every_node_once(pair):
    G, g = pair
    rows = g.louvain(resolution=1.0)
    assert sorted(r["user_id"] for r in rows) == sorted(G)


def test_label_propagation_covers_every_node_once(pair):
    G, g = pair
    rows = g.community_detection(iterations=10)
    assert sorted(r["user_id"] for r in rows) == sorted(G)


def test_label_propagation_communities_are_connected():
    def check(G, g):
        U = G.to_undirected()
        return all(nx.is_connected(U.subgraph(c)) for c in _partition(g.community_detection(iterations=10), "community"))
    assert _failures(check) == []


# --- paths and traversal ----------------------------------------------------

def test_dijkstra_every_pair(pair):
    G, g = pair
    for s, t in itertools.permutations(G, 2):
        got = g.shortest_path(s, t, weight_property="w")
        try:
            expected = nx.dijkstra_path_length(G, s, t, weight="w")
        except nx.NetworkXNoPath:
            assert got["found"] is False, (s, t)
            continue
        assert got["found"] is True, (s, t)
        assert got["distance"] == pytest.approx(expected), (s, t)
        path = got["path"]
        assert path[0] == s and path[-1] == t
        assert sum(G[u][v]["w"] for u, v in zip(path, path[1:])) == pytest.approx(expected)


def test_astar_hop_distance(pair):
    G, g = pair
    for s, t in itertools.permutations(G, 2):
        got = g.astar(s, t)
        try:
            expected = nx.shortest_path_length(G, s, t)
        except nx.NetworkXNoPath:
            assert got["found"] is False, (s, t)
            continue
        assert got["found"] is True and got["distance"] == pytest.approx(expected), (s, t)


def test_all_pairs_shortest_path(pair):
    G, g = pair
    got = {(r["source"], r["target"]): r["distance"] for r in g.all_pairs_shortest_path()}
    expected = {(s, t): d for s, lengths in nx.all_pairs_shortest_path_length(G)
                for t, d in lengths.items() if s != t}
    assert got == expected


def test_bfs_depths(pair):
    G, g = pair
    for source in G:
        got = {r["user_id"]: r["depth"] for r in g.bfs(source, max_depth=100)}
        assert got == nx.single_source_shortest_path_length(G, source), source


def test_bfs_max_depth(pair):
    G, g = pair
    for source in G:
        got = {r["user_id"]: r["depth"] for r in g.bfs(source, max_depth=1)}
        assert got == nx.single_source_shortest_path_length(G, source, cutoff=1), source


def test_dfs_reaches_descendants(pair):
    G, g = pair
    for source in G:
        rows = g.dfs(source, max_depth=100)
        assert {r["user_id"] for r in rows} == nx.descendants(G, source) | {source}, source
        assert sorted(r["order"] for r in rows) == list(range(len(rows)))


# --- similarity and triangles -------------------------------------------------

def test_node_similarity_all_pairs(pair):
    G, g = pair
    got = {frozenset((r["node1"], r["node2"])): r["similarity"] for r in g.node_similarity()}
    for a, b in itertools.combinations(G, 2):
        assert got.get(frozenset((a, b)), 0.0) == pytest.approx(_jaccard_out(G, a, b), abs=1e-6), (a, b)


def test_knn(pair):
    G, g = pair
    for source in G:
        got = g.knn(source, k=3)
        assert len(got) <= 3
        expected = sorted((_jaccard_out(G, source, x) for x in G if x != source), reverse=True)
        assert [r["similarity"] for r in got] == pytest.approx(expected[:len(got)], abs=1e-6), source
        assert all(r["similarity"] > 0 for r in got)
        assert [r["rank"] for r in got] == list(range(1, len(got) + 1))


def test_triangle_count_and_clustering(pair):
    G, g = pair
    U = G.to_undirected()
    triangles, clustering = nx.triangles(U), nx.clustering(U)
    for r in g.triangle_count():
        assert r["triangles"] == triangles[r["user_id"]], r["user_id"]
        assert r["clustering_coefficient"] == pytest.approx(clustering[r["user_id"]], abs=1e-6)


# --- graphs that the random set does not produce ------------------------------

def test_label_propagation_sql_form_matches(pair):
    """Outside a plain RETURN the SQL form runs; it must give the same communities."""
    G, g = pair
    direct = _partition(g.community_detection(iterations=10), "community")
    first = sorted(G)[0]
    rows = _cypher_list(g, f"MATCH (n:N {{id: '{first}'}}) WITH n RETURN labelPropagation(10) AS l")
    assert _partition(rows, "community") == direct


@pytest.fixture
def g():
    graph = Graph(":memory:", extension_path=get_extension_path())
    yield graph
    graph.close()


def test_triangle_count_ignores_self_loops(g):
    g.upsert_node("a", {}, label="N")
    g.upsert_node("b", {}, label="N")
    g.upsert_edge("a", "a", {}, rel_type="E")
    g.upsert_edge("a", "b", {}, rel_type="E")
    assert {r["user_id"]: r["triangles"] for r in g.triangle_count()} == {"a": 0, "b": 0}


def test_algorithms_on_a_graph_without_edges(g):
    for node in "abc":
        g.upsert_node(node, {}, label="N")
    assert _by_user(g.pagerank(), "score") == pytest.approx({n: 1 / 3 for n in "abc"}, abs=TOL)
    assert _partition(g.weakly_connected_components(), "component") == [["a"], ["b"], ["c"]]
    assert g.all_pairs_shortest_path() == []
    assert g.shortest_path("a", "b")["found"] is False
    assert [r["user_id"] for r in g.bfs("a")] == ["a"]


def test_algorithms_on_a_single_node(g):
    g.upsert_node("a", {}, label="N")
    assert _by_user(g.pagerank(), "score") == pytest.approx({"a": 1.0}, abs=TOL)
    assert g.shortest_path("a", "a") == {"path": ["a"], "distance": 0, "found": True}
    assert g.node_similarity() == []
    assert g.knn("a", 2) == []


def test_algorithms_on_an_empty_graph(g):
    assert g.pagerank() == []
    assert g.louvain() == []
    assert g.all_pairs_shortest_path() == []
