"""Tests for graphqlite.graph module."""

import os
from pathlib import Path

import pytest

import graphqlite
from graphqlite import Graph, graph, escape_string, sanitize_rel_type, CYPHER_RESERVED


def get_extension_path():
    """Get path to the built extension."""
    test_dir = Path(__file__).parent
    build_dir = test_dir.parent.parent.parent / "build"

    if (build_dir / "graphqlite.dylib").exists():
        return str(build_dir / "graphqlite.dylib")
    elif (build_dir / "graphqlite.so").exists():
        return str(build_dir / "graphqlite.so")

    env_path = os.environ.get("GRAPHQLITE_EXTENSION_PATH")
    if env_path and Path(env_path).exists():
        return env_path

    pytest.skip("GraphQLite extension not found. Build with 'make extension'")


@pytest.fixture
def g():
    """Create an in-memory Graph instance."""
    ext_path = get_extension_path()
    graph_instance = Graph(":memory:", extension_path=ext_path)
    yield graph_instance
    graph_instance.close()


# =============================================================================
# Utility Functions
# =============================================================================

def test_escape_string_single_quotes():
    assert escape_string("It's") == "It\\'s"


def test_escape_string_double_quotes():
    assert escape_string('Say "hi"') == 'Say \\"hi\\"'


def test_escape_string_backslash():
    assert escape_string("C:\\path") == "C:\\\\path"


def test_escape_string_newlines():
    assert escape_string("line1\nline2") == "line1 line2"


def test_escape_string_tabs():
    assert escape_string("col1\tcol2") == "col1 col2"


def test_sanitize_rel_type_passthrough():
    assert sanitize_rel_type("KNOWS") == "KNOWS"


def test_sanitize_rel_type_special_chars():
    assert sanitize_rel_type("RELATED-TO") == "RELATED_TO"


def test_sanitize_rel_type_leading_digit():
    result = sanitize_rel_type("123_TYPE")
    assert result.startswith("REL_")


def test_sanitize_rel_type_reserved_word():
    result = sanitize_rel_type("CREATE")
    assert result == "REL_CREATE"


def test_cypher_reserved_contains_keywords():
    assert "CREATE" in CYPHER_RESERVED
    assert "MATCH" in CYPHER_RESERVED
    assert "RETURN" in CYPHER_RESERVED


# =============================================================================
# Graph Initialization
# =============================================================================

def test_graph_create():
    ext_path = get_extension_path()
    g = Graph(":memory:", extension_path=ext_path)
    assert g is not None
    g.close()


def test_graph_context_manager():
    ext_path = get_extension_path()
    with Graph(":memory:", extension_path=ext_path) as g:
        g.upsert_node("test", {"name": "Test"})
        assert g.has_node("test")


def test_graph_factory_function():
    ext_path = get_extension_path()
    g = graph(":memory:", extension_path=ext_path)
    assert isinstance(g, Graph)
    g.close()


def test_graph_connection_property(g):
    conn = g.connection
    assert isinstance(conn, graphqlite.Connection)


def test_graph_namespace(g):
    assert g.namespace == "default"


# =============================================================================
# Node Operations
# =============================================================================

def test_has_node_false(g):
    assert not g.has_node("nonexistent")


def test_upsert_node_creates(g):
    g.upsert_node("alice", {"name": "Alice", "age": 30}, label="Person")
    assert g.has_node("alice")


def test_get_node(g):
    g.upsert_node("bob", {"name": "Bob"}, label="Person")
    node = g.get_node("bob")
    assert node is not None


def test_get_node_nonexistent(g):
    assert g.get_node("nonexistent") is None


def test_upsert_node_updates(g):
    g.upsert_node("carol", {"name": "Carol", "age": 25}, label="Person")
    g.upsert_node("carol", {"name": "Carol", "age": 26}, label="Person")
    assert g.stats()["nodes"] == 1


def test_delete_node(g):
    g.upsert_node("dave", {"name": "Dave"}, label="Person")
    assert g.has_node("dave")
    g.delete_node("dave")
    assert not g.has_node("dave")


def test_get_all_nodes(g):
    g.upsert_node("n1", {"v": 1}, label="Num")
    g.upsert_node("n2", {"v": 2}, label="Num")
    g.upsert_node("n3", {"v": 3}, label="Str")
    assert len(g.get_all_nodes()) == 3


def test_get_all_nodes_by_label(g):
    g.upsert_node("p1", {"name": "Person1"}, label="Person")
    g.upsert_node("p2", {"name": "Person2"}, label="Person")
    g.upsert_node("o1", {"name": "Org1"}, label="Organization")
    assert len(g.get_all_nodes(label="Person")) == 2


# =============================================================================
# Edge Operations
# =============================================================================

def test_has_edge_false(g):
    g.upsert_node("a", {"name": "A"})
    g.upsert_node("b", {"name": "B"})
    assert not g.has_edge("a", "b")


def test_upsert_edge_creates(g):
    g.upsert_node("a", {"name": "A"})
    g.upsert_node("b", {"name": "B"})
    g.upsert_edge("a", "b", {"weight": 1.0}, rel_type="CONNECTS")
    assert g.has_edge("a", "b")


def test_get_edge(g):
    g.upsert_node("x", {"name": "X"})
    g.upsert_node("y", {"name": "Y"})
    g.upsert_edge("x", "y", {"since": 2020}, rel_type="LINKED")
    assert g.get_edge("x", "y") is not None


def test_delete_edge(g):
    g.upsert_node("m", {"name": "M"})
    g.upsert_node("n", {"name": "N"})
    g.upsert_edge("m", "n", {}, rel_type="TEST")
    assert g.has_edge("m", "n")
    g.delete_edge("m", "n")
    assert not g.has_edge("m", "n")


def test_get_all_edges(g):
    g.upsert_node("e1", {"name": "E1"})
    g.upsert_node("e2", {"name": "E2"})
    g.upsert_node("e3", {"name": "E3"})
    g.upsert_edge("e1", "e2", {}, rel_type="R1")
    g.upsert_edge("e2", "e3", {}, rel_type="R2")
    assert len(g.get_all_edges()) == 2


# =============================================================================
# Graph Queries
# =============================================================================

def test_node_degree(g):
    g.upsert_node("hub", {"name": "Hub"})
    g.upsert_node("s1", {"name": "Spoke1"})
    g.upsert_node("s2", {"name": "Spoke2"})
    g.upsert_node("s3", {"name": "Spoke3"})
    g.upsert_edge("hub", "s1", {}, rel_type="TO")
    g.upsert_edge("hub", "s2", {}, rel_type="TO")
    g.upsert_edge("hub", "s3", {}, rel_type="TO")
    assert g.node_degree("hub") == 3


def test_get_neighbors(g):
    g.upsert_node("center", {"name": "Center"})
    g.upsert_node("nb1", {"name": "Neighbor1"})
    g.upsert_node("nb2", {"name": "Neighbor2"})
    g.upsert_edge("center", "nb1", {}, rel_type="NEAR")
    g.upsert_edge("center", "nb2", {}, rel_type="NEAR")
    assert len(g.get_neighbors("center")) == 2


def test_stats(g):
    g.upsert_node("s1", {"v": 1})
    g.upsert_node("s2", {"v": 2})
    g.upsert_node("s3", {"v": 3})
    g.upsert_edge("s1", "s2", {})
    g.upsert_edge("s2", "s3", {})
    stats = g.stats()
    assert stats["nodes"] == 3
    assert stats["edges"] == 2


def test_query_raw_cypher(g):
    g.upsert_node("q1", {"name": "Query1", "value": 10})
    results = g.query("MATCH (n {id: 'q1'}) RETURN n.name, n.value")
    assert len(results) == 1
    assert results[0]["n.name"] == "Query1"
    assert results[0]["n.value"] == 10


# =============================================================================
# String Escaping in Operations
# =============================================================================

def test_node_with_single_quotes(g):
    g.upsert_node("quote_test", {"text": "It's a test"})
    assert g.has_node("quote_test")


def test_node_with_double_quotes(g):
    g.upsert_node("dquote_test", {"text": 'Say "hello"'})
    assert g.has_node("dquote_test")


def test_node_with_backslash(g):
    g.upsert_node("backslash_test", {"path": "C:\\Users\\Test"})
    assert g.has_node("backslash_test")


def test_edge_with_reserved_word_rel_type(g):
    g.upsert_node("rw1", {"name": "RW1"})
    g.upsert_node("rw2", {"name": "RW2"})
    g.upsert_edge("rw1", "rw2", {}, rel_type="CREATE")
    assert g.has_edge("rw1", "rw2")


def test_edge_with_special_char_rel_type(g):
    g.upsert_node("sc1", {"name": "SC1"})
    g.upsert_node("sc2", {"name": "SC2"})
    g.upsert_edge("sc1", "sc2", {}, rel_type="RELATED-TO")
    assert g.has_edge("sc1", "sc2")


# =============================================================================
# Batch Operations
# =============================================================================

def test_upsert_nodes_batch(g):
    nodes = [
        ("batch1", {"name": "Batch1"}, "Item"),
        ("batch2", {"name": "Batch2"}, "Item"),
        ("batch3", {"name": "Batch3"}, "Item"),
    ]
    g.upsert_nodes_batch(nodes)
    assert g.has_node("batch1")
    assert g.has_node("batch2")
    assert g.has_node("batch3")
    assert g.stats()["nodes"] == 3


def test_upsert_edges_batch(g):
    g.upsert_node("be1", {"name": "BE1"})
    g.upsert_node("be2", {"name": "BE2"})
    g.upsert_node("be3", {"name": "BE3"})
    edges = [
        ("be1", "be2", {"w": 1}, "LINK"),
        ("be2", "be3", {"w": 2}, "LINK"),
        ("be1", "be3", {"w": 3}, "LINK"),
    ]
    g.upsert_edges_batch(edges)
    assert g.has_edge("be1", "be2")
    assert g.has_edge("be2", "be3")
    assert g.has_edge("be1", "be3")
    assert g.stats()["edges"] == 3


# =============================================================================
# Graph Algorithms
# =============================================================================

def test_pagerank(g):
    g.upsert_node("pr1", {"name": "PR1"})
    g.upsert_node("pr2", {"name": "PR2"})
    g.upsert_node("pr3", {"name": "PR3"})
    g.upsert_edge("pr1", "pr2", {})
    g.upsert_edge("pr1", "pr3", {})
    g.upsert_edge("pr2", "pr3", {})
    ranks = g.pagerank(damping=0.85, iterations=10)
    assert isinstance(ranks, list)
    assert len(ranks) == 3
    # Check structure
    for r in ranks:
        assert "node_id" in r
        assert "user_id" in r
        assert "score" in r
    # Check user_ids are present
    user_ids = {r["user_id"] for r in ranks}
    assert user_ids == {"pr1", "pr2", "pr3"}


def test_community_detection(g):
    g.upsert_node("cd1", {"name": "CD1"})
    g.upsert_node("cd2", {"name": "CD2"})
    g.upsert_edge("cd1", "cd2", {})
    communities = g.community_detection(iterations=5)
    assert isinstance(communities, list)
    assert len(communities) == 2
    # Check structure
    for c in communities:
        assert "node_id" in c
        assert "user_id" in c
        assert "community" in c
    # Check user_ids are present
    user_ids = {c["user_id"] for c in communities}
    assert user_ids == {"cd1", "cd2"}


def test_shortest_path(g):
    # Create a simple path: sp1 -> sp2 -> sp3
    g.upsert_node("sp1", {"name": "SP1"})
    g.upsert_node("sp2", {"name": "SP2"})
    g.upsert_node("sp3", {"name": "SP3"})
    g.upsert_edge("sp1", "sp2", {})
    g.upsert_edge("sp2", "sp3", {})

    result = g.shortest_path("sp1", "sp3")
    assert isinstance(result, dict)
    assert result["found"] is True
    assert result["distance"] == 2
    assert result["path"] == ["sp1", "sp2", "sp3"]


def test_shortest_path_no_path(g):
    # Create disconnected nodes
    g.upsert_node("iso1", {"name": "ISO1"})
    g.upsert_node("iso2", {"name": "ISO2"})
    # No edge between them

    result = g.shortest_path("iso1", "iso2")
    assert isinstance(result, dict)
    assert result["found"] is False
    assert result["path"] == []
    assert result["distance"] is None


def test_shortest_path_same_node(g):
    g.upsert_node("same1", {"name": "Same"})

    result = g.shortest_path("same1", "same1")
    assert result["found"] is True
    assert result["distance"] == 0
    assert result["path"] == ["same1"]


def test_degree_centrality(g):
    # Create a simple graph: dc1 -> dc2 -> dc3, dc1 -> dc3
    g.upsert_node("dc1", {"name": "DC1"})
    g.upsert_node("dc2", {"name": "DC2"})
    g.upsert_node("dc3", {"name": "DC3"})
    g.upsert_edge("dc1", "dc2", {})
    g.upsert_edge("dc2", "dc3", {})
    g.upsert_edge("dc1", "dc3", {})

    degrees = g.degree_centrality()
    assert isinstance(degrees, list)
    assert len(degrees) == 3

    # Check structure
    for d in degrees:
        assert "node_id" in d
        assert "user_id" in d
        assert "in_degree" in d
        assert "out_degree" in d
        assert "degree" in d

    # Check specific values by user_id
    by_user = {d["user_id"]: d for d in degrees}

    # dc1: out=2, in=0, total=2
    assert by_user["dc1"]["out_degree"] == 2
    assert by_user["dc1"]["in_degree"] == 0
    assert by_user["dc1"]["degree"] == 2

    # dc2: out=1, in=1, total=2
    assert by_user["dc2"]["out_degree"] == 1
    assert by_user["dc2"]["in_degree"] == 1
    assert by_user["dc2"]["degree"] == 2

    # dc3: out=0, in=2, total=2
    assert by_user["dc3"]["out_degree"] == 0
    assert by_user["dc3"]["in_degree"] == 2
    assert by_user["dc3"]["degree"] == 2


# =============================================================================
# Connected Components (WCC/SCC)
# =============================================================================

def test_weakly_connected_components(g):
    # Create two disconnected components
    # Component 1: wcc1 <-> wcc2 <-> wcc3
    g.upsert_node("wcc1", {"name": "WCC1"})
    g.upsert_node("wcc2", {"name": "WCC2"})
    g.upsert_node("wcc3", {"name": "WCC3"})
    g.upsert_edge("wcc1", "wcc2", {})
    g.upsert_edge("wcc2", "wcc3", {})

    # Component 2: wcc4 <-> wcc5
    g.upsert_node("wcc4", {"name": "WCC4"})
    g.upsert_node("wcc5", {"name": "WCC5"})
    g.upsert_edge("wcc4", "wcc5", {})

    components = g.weakly_connected_components()
    assert isinstance(components, list)
    assert len(components) == 5

    # Check structure
    for c in components:
        assert "node_id" in c
        assert "user_id" in c
        assert "component" in c

    # Group by component
    by_component = {}
    for c in components:
        comp_id = c["component"]
        if comp_id not in by_component:
            by_component[comp_id] = set()
        by_component[comp_id].add(c["user_id"])

    # Should have exactly 2 components
    assert len(by_component) == 2

    # One component should have wcc1,2,3, the other wcc4,5
    comp_sizes = sorted([len(v) for v in by_component.values()])
    assert comp_sizes == [2, 3]


def test_weakly_connected_components_single_node(g):
    g.upsert_node("solo", {"name": "Solo"})

    components = g.weakly_connected_components()
    assert len(components) == 1
    assert components[0]["user_id"] == "solo"
    assert components[0]["component"] == 0


def test_weakly_connected_components_empty_graph(g):
    components = g.weakly_connected_components()
    assert components == []


def test_connected_components_alias(g):
    """Test that connected_components is an alias for weakly_connected_components."""
    g.upsert_node("alias1", {"name": "A1"})
    g.upsert_node("alias2", {"name": "A2"})
    g.upsert_edge("alias1", "alias2", {})

    # Both should return the same result
    wcc = g.weakly_connected_components()
    cc = g.connected_components()

    assert len(wcc) == len(cc) == 2
    assert {c["user_id"] for c in wcc} == {c["user_id"] for c in cc}


def test_strongly_connected_components_with_cycle(g):
    # Create a cycle: scc1 -> scc2 -> scc3 -> scc1
    g.upsert_node("scc1", {"name": "SCC1"})
    g.upsert_node("scc2", {"name": "SCC2"})
    g.upsert_node("scc3", {"name": "SCC3"})
    g.upsert_edge("scc1", "scc2", {})
    g.upsert_edge("scc2", "scc3", {})
    g.upsert_edge("scc3", "scc1", {})

    components = g.strongly_connected_components()
    assert isinstance(components, list)
    assert len(components) == 3

    # Check structure
    for c in components:
        assert "node_id" in c
        assert "user_id" in c
        assert "component" in c

    # All nodes should be in the same SCC (they form a cycle)
    component_ids = {c["component"] for c in components}
    assert len(component_ids) == 1


def test_strongly_connected_components_no_cycle(g):
    # Create a directed chain (no cycles): scc_a -> scc_b -> scc_c
    g.upsert_node("scc_a", {"name": "SCC_A"})
    g.upsert_node("scc_b", {"name": "SCC_B"})
    g.upsert_node("scc_c", {"name": "SCC_C"})
    g.upsert_edge("scc_a", "scc_b", {})
    g.upsert_edge("scc_b", "scc_c", {})

    components = g.strongly_connected_components()
    assert len(components) == 3

    # Each node should be in its own SCC (no back edges)
    component_ids = {c["component"] for c in components}
    assert len(component_ids) == 3


def test_strongly_connected_components_empty_graph(g):
    components = g.strongly_connected_components()
    assert components == []


def test_strongly_connected_components_mixed(g):
    # Create a graph with multiple SCCs:
    # SCC 1: m1 <-> m2 (cycle)
    # SCC 2: m3 alone (pointed to by SCC 1)
    g.upsert_node("m1", {"name": "M1"})
    g.upsert_node("m2", {"name": "M2"})
    g.upsert_node("m3", {"name": "M3"})
    g.upsert_edge("m1", "m2", {})
    g.upsert_edge("m2", "m1", {})  # Creates cycle between m1, m2
    g.upsert_edge("m2", "m3", {})  # m3 is separate SCC

    components = g.strongly_connected_components()
    assert len(components) == 3

    # Group by component
    by_component = {}
    for c in components:
        comp_id = c["component"]
        if comp_id not in by_component:
            by_component[comp_id] = set()
        by_component[comp_id].add(c["user_id"])

    # Should have exactly 2 SCCs
    assert len(by_component) == 2

    # One SCC has m1,m2 and another has m3
    comp_sizes = sorted([len(v) for v in by_component.values()])
    assert comp_sizes == [1, 2]


# =============================================================================
# Rustworkx Export
# =============================================================================

def test_to_rustworkx(g):
    rx = pytest.importorskip("rustworkx")

    g.upsert_node("rx1", {"name": "RX1"}, label="Test")
    g.upsert_node("rx2", {"name": "RX2"}, label="Test")
    g.upsert_node("rx3", {"name": "RX3"}, label="Test")
    g.upsert_edge("rx1", "rx2", {}, rel_type="CONNECTS")
    g.upsert_edge("rx2", "rx3", {}, rel_type="CONNECTS")

    G, node_map = g.to_rustworkx()

    assert isinstance(G, rx.PyDiGraph)
    assert G.num_nodes() == 3
    assert G.num_edges() == 2
    assert "rx1" in node_map
    assert "rx2" in node_map
    assert "rx3" in node_map


def test_to_rustworkx_empty_graph(g):
    rx = pytest.importorskip("rustworkx")

    G, node_map = g.to_rustworkx()

    assert isinstance(G, rx.PyDiGraph)
    assert G.num_nodes() == 0
    assert G.num_edges() == 0
    assert node_map == {}


def test_to_rustworkx_preserves_properties(g):
    rx = pytest.importorskip("rustworkx")

    g.upsert_node("prop1", {"name": "PropNode", "value": 42}, label="Test")

    G, node_map = g.to_rustworkx()

    assert "prop1" in node_map
    node_idx = node_map["prop1"]
    node_data = G[node_idx]
    assert node_data["name"] == "PropNode"
    assert node_data["value"] == 42


# =============================================================================
# Leiden Community Detection
# =============================================================================

def test_leiden_communities(g):
    pytest.importorskip("rustworkx")
    pytest.importorskip("graspologic")

    # Create a graph with clear community structure
    # Community 1: ld1, ld2, ld3 (densely connected)
    g.upsert_node("ld1", {"name": "LD1"}, label="Test")
    g.upsert_node("ld2", {"name": "LD2"}, label="Test")
    g.upsert_node("ld3", {"name": "LD3"}, label="Test")
    g.upsert_edge("ld1", "ld2", {}, rel_type="CONNECTS")
    g.upsert_edge("ld2", "ld3", {}, rel_type="CONNECTS")
    g.upsert_edge("ld1", "ld3", {}, rel_type="CONNECTS")

    # Community 2: ld4, ld5, ld6 (densely connected)
    g.upsert_node("ld4", {"name": "LD4"}, label="Test")
    g.upsert_node("ld5", {"name": "LD5"}, label="Test")
    g.upsert_node("ld6", {"name": "LD6"}, label="Test")
    g.upsert_edge("ld4", "ld5", {}, rel_type="CONNECTS")
    g.upsert_edge("ld5", "ld6", {}, rel_type="CONNECTS")
    g.upsert_edge("ld4", "ld6", {}, rel_type="CONNECTS")

    # Single weak link between communities
    g.upsert_edge("ld3", "ld4", {}, rel_type="BRIDGE")

    communities = g.leiden_communities(random_seed=42)

    assert isinstance(communities, list)
    assert len(communities) > 0

    # Check structure of results
    for c in communities:
        assert "node_id" in c
        assert "community" in c


def test_leiden_communities_empty_graph(g):
    pytest.importorskip("rustworkx")
    pytest.importorskip("graspologic")

    communities = g.leiden_communities()

    assert communities == []


def test_leiden_communities_with_resolution(g):
    pytest.importorskip("rustworkx")
    pytest.importorskip("graspologic")

    g.upsert_node("r1", {"name": "R1"}, label="Test")
    g.upsert_node("r2", {"name": "R2"}, label="Test")
    g.upsert_edge("r1", "r2", {}, rel_type="CONNECTS")

    # Higher resolution should produce more communities
    communities = g.leiden_communities(resolution=2.0, random_seed=42)

    assert isinstance(communities, list)
    node_ids = {c["node_id"] for c in communities}
    assert "r1" in node_ids
    assert "r2" in node_ids
