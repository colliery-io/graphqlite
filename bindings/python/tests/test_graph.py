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
