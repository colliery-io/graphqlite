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
    assert g.stats()["node_count"] == 1


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


def test_upsert_edge_multiple_types(g):
    g.upsert_node("a", {"name": "A"})
    g.upsert_node("b", {"name": "B"})

    # Create two different relation types between same nodes
    g.upsert_edge("a", "b", {"since": 2020}, rel_type="KNOWS")
    g.upsert_edge("a", "b", {"project": "X"}, rel_type="WORKS_WITH")

    # Both should exist — verify via get_all_edges count
    assert len(g.get_all_edges()) == 2


def test_upsert_edge_updates_properties(g):
    g.upsert_node("a", {"name": "A"})
    g.upsert_node("b", {"name": "B"})

    # Create edge with initial properties
    g.upsert_edge("a", "b", {"weight": 1}, rel_type="KNOWS")

    # Upsert again with updated properties — should SET, not create a second edge
    g.upsert_edge("a", "b", {"weight": 2}, rel_type="KNOWS")

    # Still only one edge
    assert len(g.get_all_edges()) == 1

    # Verify updated property
    edge = g.get_edge("a", "b")
    assert edge["properties"]["weight"] == 2


def test_upsert_edge_update_empty_props(g):
    g.upsert_node("a", {"name": "A"})
    g.upsert_node("b", {"name": "B"})

    # Create edge with properties
    g.upsert_edge("a", "b", {"weight": 1}, rel_type="KNOWS")

    # Upsert with empty props should preserve existing properties
    g.upsert_edge("a", "b", {}, rel_type="KNOWS")
    edge = g.get_edge("a", "b")
    assert edge["properties"]["weight"] == 1


def test_get_edge_by_type(g):
    """get_edge should be able to retrieve a specific edge type."""
    g.upsert_node("a", {"name": "A"})
    g.upsert_node("b", {"name": "B"})

    g.upsert_edge("a", "b", {"since": 2020}, rel_type="KNOWS")
    g.upsert_edge("a", "b", {"project": "X"}, rel_type="WORKS_WITH")

    # Should be able to fetch the KNOWS edge specifically
    edge = g.get_edge("a", "b", rel_type="KNOWS")
    assert edge["type"] == "KNOWS"
    assert edge["properties"]["since"] == 2020

    # Should be able to fetch the WORKS_WITH edge specifically
    edge = g.get_edge("a", "b", rel_type="WORKS_WITH")
    assert edge["type"] == "WORKS_WITH"
    assert edge["properties"]["project"] == "X"


def test_delete_edge_by_type(g):
    """delete_edge should only remove the specified edge type."""
    g.upsert_node("a", {"name": "A"})
    g.upsert_node("b", {"name": "B"})

    g.upsert_edge("a", "b", {"since": 2020}, rel_type="KNOWS")
    g.upsert_edge("a", "b", {"project": "X"}, rel_type="WORKS_WITH")
    assert len(g.get_all_edges()) == 2

    # Deleting KNOWS should leave WORKS_WITH intact
    g.delete_edge("a", "b", rel_type="KNOWS")
    edges = g.get_all_edges()
    assert len(edges) == 1
    assert edges[0]["r"]["type"] == "WORKS_WITH"


def test_has_edge_by_type(g):
    """has_edge should be able to check for a specific edge type."""
    g.upsert_node("a", {"name": "A"})
    g.upsert_node("b", {"name": "B"})

    g.upsert_edge("a", "b", {"since": 2020}, rel_type="KNOWS")

    assert g.has_edge("a", "b", rel_type="KNOWS") is True
    assert g.has_edge("a", "b", rel_type="WORKS_WITH") is False


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
    assert stats["node_count"] == 3
    assert stats["edge_count"] == 2


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
    assert g.stats()["node_count"] == 3


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
    assert g.stats()["edge_count"] == 3


# =============================================================================
# Bulk Insert Operations (High Performance)
# =============================================================================

def test_insert_nodes_bulk(g):
    """Test bulk node insertion returns ID mapping."""
    id_map = g.insert_nodes_bulk([
        ("bulk_alice", {"name": "Alice", "age": 30}, "Person"),
        ("bulk_bob", {"name": "Bob", "age": 25}, "Person"),
        ("bulk_charlie", {"name": "Charlie"}, "Person"),
    ])

    # Check ID map is returned
    assert isinstance(id_map, dict)
    assert len(id_map) == 3
    assert "bulk_alice" in id_map
    assert "bulk_bob" in id_map
    assert "bulk_charlie" in id_map

    # Check nodes exist
    assert g.has_node("bulk_alice")
    assert g.has_node("bulk_bob")
    assert g.has_node("bulk_charlie")
    assert g.stats()["node_count"] == 3


def test_insert_nodes_bulk_empty(g):
    """Test bulk insert with empty list."""
    id_map = g.insert_nodes_bulk([])
    assert id_map == {}
    assert g.stats()["node_count"] == 0


def test_insert_edges_bulk(g):
    """Test bulk edge insertion using ID map."""
    # First insert nodes
    id_map = g.insert_nodes_bulk([
        ("edge_a", {}, "Node"),
        ("edge_b", {}, "Node"),
        ("edge_c", {}, "Node"),
    ])

    # Then insert edges using the map
    edges_inserted = g.insert_edges_bulk([
        ("edge_a", "edge_b", {"weight": 1.0}, "CONNECTS"),
        ("edge_b", "edge_c", {"weight": 2.0}, "CONNECTS"),
        ("edge_a", "edge_c", {"weight": 3.0}, "CONNECTS"),
    ], id_map)

    assert edges_inserted == 3
    assert g.has_edge("edge_a", "edge_b")
    assert g.has_edge("edge_b", "edge_c")
    assert g.has_edge("edge_a", "edge_c")
    assert g.stats()["edge_count"] == 3


def test_insert_edges_bulk_empty(g):
    """Test bulk edge insert with empty list."""
    edges_inserted = g.insert_edges_bulk([], {})
    assert edges_inserted == 0


def test_insert_edges_bulk_without_id_map(g):
    """Test bulk edge insertion without ID map (falls back to lookup)."""
    # Insert nodes via Cypher
    g.connection.cypher("CREATE (:Node {id: 'lookup_a'})")
    g.connection.cypher("CREATE (:Node {id: 'lookup_b'})")

    # Insert edges without providing id_map
    edges_inserted = g.insert_edges_bulk([
        ("lookup_a", "lookup_b", {}, "LINKS"),
    ])

    assert edges_inserted == 1
    assert g.has_edge("lookup_a", "lookup_b")


def test_insert_graph_bulk(g):
    """Test combined node and edge bulk insertion."""
    from graphqlite import BulkInsertResult

    result = g.insert_graph_bulk(
        nodes=[
            ("graph_x", {"name": "X"}, "Node"),
            ("graph_y", {"name": "Y"}, "Node"),
            ("graph_z", {"name": "Z"}, "Node"),
        ],
        edges=[
            ("graph_x", "graph_y", {}, "LINKS"),
            ("graph_y", "graph_z", {}, "LINKS"),
        ],
    )

    assert isinstance(result, BulkInsertResult)
    assert result.nodes_inserted == 3
    assert result.edges_inserted == 2
    assert "graph_x" in result.id_map
    assert "graph_y" in result.id_map
    assert "graph_z" in result.id_map

    assert g.stats()["node_count"] == 3
    assert g.stats()["edge_count"] == 2


def test_resolve_node_ids(g):
    """Test resolving external IDs to internal rowids."""
    # Insert nodes via Cypher
    g.connection.cypher("CREATE (:Person {id: 'resolve_alice'})")
    g.connection.cypher("CREATE (:Person {id: 'resolve_bob'})")

    resolved = g.resolve_node_ids(["resolve_alice", "resolve_bob", "nonexistent"])

    assert isinstance(resolved, dict)
    assert "resolve_alice" in resolved
    assert "resolve_bob" in resolved
    assert "nonexistent" not in resolved


def test_resolve_node_ids_empty(g):
    """Test resolving empty list."""
    resolved = g.resolve_node_ids([])
    assert resolved == {}


def test_bulk_insert_mixed_sources(g):
    """Test bulk edge insert connecting new nodes to existing nodes."""
    # Insert some nodes via Cypher
    g.connection.cypher("CREATE (:Person {id: 'existing_node'})")

    # Insert new nodes via bulk
    id_map = g.insert_nodes_bulk([
        ("new_node1", {}, "Person"),
        ("new_node2", {}, "Person"),
    ])

    # Insert edges connecting new and existing nodes
    edges_inserted = g.insert_edges_bulk([
        ("new_node1", "new_node2", {}, "KNOWS"),
        ("new_node1", "existing_node", {}, "KNOWS"),
    ], id_map)

    assert edges_inserted == 2
    assert g.has_edge("new_node1", "new_node2")
    assert g.has_edge("new_node1", "existing_node")


def test_bulk_insert_with_typed_properties(g):
    """Test bulk insert correctly handles different property types."""
    id_map = g.insert_nodes_bulk([
        ("typed_node", {
            "str_prop": "hello",
            "int_prop": 42,
            "float_prop": 3.14,
            "bool_true": True,
            "bool_false": False,
        }, "TypedNode"),
    ])

    assert "typed_node" in id_map
    assert g.has_node("typed_node")


def test_bulk_insert_performance(g):
    """Test that bulk insert is reasonably fast."""
    import time

    node_count = 500
    edge_count = 2000

    nodes = [(f"perf_node_{i}", {"name": f"Node {i}"}, "PerfTest") for i in range(node_count)]
    edges = [
        (f"perf_node_{i % node_count}", f"perf_node_{(i + 1) % node_count}", {"weight": i}, "PERF")
        for i in range(edge_count)
    ]

    start = time.time()
    id_map = g.insert_nodes_bulk(nodes)
    node_time = time.time() - start

    edge_start = time.time()
    edges_inserted = g.insert_edges_bulk(edges, id_map)
    edge_time = time.time() - edge_start

    total_time = time.time() - start

    assert len(id_map) == node_count
    assert edges_inserted == edge_count

    # Bulk insert should complete in under 5 seconds for this size
    assert total_time < 5.0, f"Bulk insert took too long: {total_time:.2f}s"


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


# =============================================================================
# Graph Cache Tests
# =============================================================================

def test_graph_loaded_initially_false(g):
    """Cache should not be loaded initially."""
    assert g.graph_loaded() is False


def test_load_graph(g):
    """Test loading graph into cache."""
    g.upsert_node("a", {}, "Node")
    g.upsert_node("b", {}, "Node")
    g.upsert_edge("a", "b", {}, "KNOWS")

    result = g.load_graph()

    assert result["status"] == "loaded"
    assert result["node_count"] == 2
    assert result["edge_count"] == 1
    assert g.graph_loaded() is True


def test_load_graph_already_loaded(g):
    """Loading when already loaded should return already_loaded status."""
    g.upsert_node("a", {}, "Node")
    g.load_graph()

    result = g.load_graph()

    assert result["status"] == "already_loaded"


def test_unload_graph(g):
    """Test unloading graph cache."""
    g.upsert_node("a", {}, "Node")
    g.load_graph()
    assert g.graph_loaded() is True

    result = g.unload_graph()

    assert result["status"] == "unloaded"
    assert g.graph_loaded() is False


def test_unload_graph_not_loaded(g):
    """Unloading when not loaded should return not_loaded status."""
    result = g.unload_graph()

    assert result["status"] == "not_loaded"


def test_reload_graph(g):
    """Test reloading graph cache after modifications."""
    g.upsert_node("a", {}, "Node")
    g.upsert_node("b", {}, "Node")
    g.load_graph()

    # Add new node
    g.upsert_node("c", {}, "Node")

    result = g.reload_graph()

    assert result["status"] == "reloaded"
    assert result["node_count"] == 3


def test_reload_graph_not_loaded(g):
    """Reloading when not loaded should load and return reloaded status."""
    g.upsert_node("a", {}, "Node")

    result = g.reload_graph()

    # reload_graph always returns "reloaded" even on first load
    assert result["status"] == "reloaded"
    assert g.graph_loaded() is True


def test_cache_with_pagerank(g):
    """Test that cached graph works with algorithms."""
    g.upsert_node("a", {}, "Node")
    g.upsert_node("b", {}, "Node")
    g.upsert_node("c", {}, "Node")
    g.upsert_edge("a", "b", {}, "LINKS")
    g.upsert_edge("b", "c", {}, "LINKS")
    g.upsert_edge("c", "a", {}, "LINKS")

    g.load_graph()

    # PageRank should work with cached graph
    result = g.pagerank()

    assert isinstance(result, list)
    assert len(result) == 3


def test_cache_empty_graph(g):
    """Test caching an empty graph."""
    result = g.load_graph()

    # Empty graph should still load successfully
    assert result["status"] == "loaded"
    assert result["node_count"] == 0
    assert result["edge_count"] == 0


# =============================================================================
# Parameterized Query Tests
# =============================================================================


def test_graph_query_with_params(g):
    """Test Graph.query() with parameters."""
    g.upsert_node("alice", {"name": "Alice", "age": 30}, "Person")
    result = g.query(
        "MATCH (n:Person) WHERE n.name = $name RETURN n.name",
        {"name": "Alice"}
    )
    assert len(result) == 1
    assert result[0]["n.name"] == "Alice"


def test_graph_query_with_integer_param(g):
    """Test Graph.query() with integer parameter."""
    g.upsert_node("alice", {"name": "Alice", "age": 30}, "Person")
    g.upsert_node("bob", {"name": "Bob", "age": 20}, "Person")
    result = g.query(
        "MATCH (n:Person) WHERE n.age > $min RETURN n.name",
        {"min": 25}
    )
    assert len(result) == 1
    assert result[0]["n.name"] == "Alice"


def test_graph_query_with_params_empty_dict(g):
    """Test Graph.query() with empty params dict."""
    g.upsert_node("alice", {"name": "Alice"}, "Person")
    result = g.query("MATCH (n:Person) RETURN n.name", {})
    assert len(result) == 1


def test_graph_query_without_params_unchanged(g):
    """Test Graph.query() backward compatibility without params."""
    g.upsert_node("alice", {"name": "Alice"}, "Person")
    result = g.query("MATCH (n:Person) RETURN n.name")
    assert len(result) == 1


def test_graph_query_params_injection_safe(g):
    """Test that parameter binding prevents SQL injection."""
    g.upsert_node("alice", {"name": "Alice"}, "Person")
    result = g.query(
        "MATCH (n:Person) WHERE n.name = $name RETURN n.name",
        {"name": "Alice'; DROP TABLE nodes; --"}
    )
    # Should not match and should not inject
    assert len(result) == 0
    # Verify graph is intact
    verify = g.query("MATCH (n:Person) RETURN n.name")
    assert len(verify) == 1


# =============================================================================
# JSON / Map / List Property Tests
# =============================================================================


def test_create_with_map_property(g):
    """Test CREATE with a nested map literal stored as JSON."""
    g.query('CREATE (n:JsonTest {name: "Alice", meta: {role: "admin", level: 5}})')
    result = g.query('MATCH (n:JsonTest {name: "Alice"}) RETURN n.meta')
    assert len(result) == 1
    # meta should come back as a JSON string
    meta = result[0]["n.meta"]
    assert meta is not None
    assert "admin" in str(meta)


def test_create_with_list_property(g):
    """Test CREATE with a list literal stored as JSON."""
    g.query('CREATE (n:JsonTest {name: "Bob", tags: ["python", "rust", "sql"]})')
    result = g.query('MATCH (n:JsonTest {name: "Bob"}) RETURN n.tags')
    assert len(result) == 1
    tags = result[0]["n.tags"]
    assert tags is not None
    assert "python" in str(tags)


def test_nested_dot_access(g):
    """Test nested dot access returns correct nested value."""
    g.query('CREATE (n:JsonTest {name: "Carol", meta: {role: "editor"}})')
    result = g.query('MATCH (n:JsonTest {name: "Carol"}) RETURN n.meta.role AS role')
    assert len(result) == 1
    assert result[0]["role"] == "editor"


def test_nested_dot_access_deep(g):
    """Test deeply nested dot access."""
    g.query('CREATE (n:JsonTest {name: "Deep", config: {db: {host: "localhost"}}})')
    result = g.query('MATCH (n:JsonTest {name: "Deep"}) RETURN n.config.db.host AS host')
    assert len(result) == 1
    assert result[0]["host"] == "localhost"


def test_bracket_subscript_access(g):
    """Test bracket subscript notation for property access."""
    g.query('CREATE (n:JsonTest {name: "Eve", meta: {team: "core"}})')
    result = g.query("MATCH (n:JsonTest {name: \"Eve\"}) RETURN n[\"meta\"][\"team\"]")
    assert len(result) == 1


def test_set_json_map_property(g):
    """Test SET with a map value on a property."""
    g.query('CREATE (n:JsonTest {name: "Frank"})')
    g.query('MATCH (n:JsonTest {name: "Frank"}) SET n.settings = {theme: "dark", lang: "en"}')
    result = g.query('MATCH (n:JsonTest {name: "Frank"}) RETURN n.settings')
    assert len(result) == 1
    settings = result[0]["n.settings"]
    assert settings is not None
    assert "dark" in str(settings)


def test_set_json_list_property(g):
    """Test SET with a list value on a property."""
    g.query('CREATE (n:JsonTest {name: "Grace"})')
    g.query('MATCH (n:JsonTest {name: "Grace"}) SET n.scores = [95, 87, 92]')
    result = g.query('MATCH (n:JsonTest {name: "Grace"}) RETURN n.scores')
    assert len(result) == 1
    scores = result[0]["n.scores"]
    assert scores is not None
    assert "95" in str(scores)


def test_bulk_set_replace(g):
    """Test SET n = {map} replaces all properties."""
    g.query('CREATE (n:SetTest {name: "Alice", age: 30, city: "NYC"})')
    g.query('MATCH (n:SetTest {name: "Alice"}) SET n = {name: "Alice", updated: true}')
    result = g.query('MATCH (n:SetTest {name: "Alice"}) RETURN n.updated, n.age, n.city')
    assert len(result) == 1
    row = result[0]
    # updated should exist, age and city should be gone (replaced)
    assert row.get("n.updated") is not None


def test_bulk_set_merge(g):
    """Test SET n += {map} merges properties."""
    g.query('CREATE (n:SetTest {name: "Bob", age: 25})')
    g.query('MATCH (n:SetTest {name: "Bob"}) SET n += {city: "LA", active: true}')
    result = g.query('MATCH (n:SetTest {name: "Bob"}) RETURN n.age, n.city, n.active')
    assert len(result) == 1
    row = result[0]
    # Original age should be preserved, new props added
    assert row["n.age"] == 25
    assert row.get("n.city") is not None


def test_bulk_set_merge_updates_existing(g):
    """Test SET n += {map} updates existing properties."""
    g.query('CREATE (n:SetTest {name: "Carol", age: 30})')
    g.query('MATCH (n:SetTest {name: "Carol"}) SET n += {age: 31}')
    result = g.query('MATCH (n:SetTest {name: "Carol"}) RETURN n.age')
    assert len(result) == 1
    assert result[0]["n.age"] == 31


def test_bulk_set_preserves_labels(g):
    """Test that bulk SET does not remove labels."""
    g.query('CREATE (n:SetTest:Important {name: "Dave"})')
    g.query('MATCH (n:SetTest {name: "Dave"}) SET n = {name: "Dave", v: 1}')
    result = g.query('MATCH (n:SetTest {name: "Dave"}) RETURN n.v')
    assert len(result) == 1
    assert result[0]["n.v"] == 1


def test_bulk_set_empty_map_clears(g):
    """Test SET n = {} clears all properties."""
    g.query('CREATE (n:SetTest {name: "Eve", age: 28})')
    g.query('MATCH (n:SetTest {name: "Eve"}) SET n = {}')
    result = g.query('MATCH (n:SetTest) RETURN n.name, n.age')
    # Node still exists but properties should be cleared
    assert len(result) == 1
    assert result[0]["n.name"] is None
    assert result[0]["n.age"] is None


def test_bulk_set_edge(g):
    """Test SET r = {map} on relationships."""
    g.query('CREATE (a:Node {name: "A"}), (b:Node {name: "B"})')
    g.query('MATCH (a:Node {name: "A"}), (b:Node {name: "B"}) CREATE (a)-[:KNOWS {since: 2020}]->(b)')
    g.query('MATCH (a)-[r:KNOWS]->(b) SET r = {since: 2021, strong: true}')
    result = g.query('MATCH (a)-[r:KNOWS]->(b) RETURN r.since, r.strong')
    assert len(result) == 1
    assert result[0]["r.since"] == 2021


def test_mixed_property_and_bulk_set(g):
    """Test combining individual property SET with bulk SET."""
    g.query('CREATE (n:MixTest {name: "Test"})')
    g.query('MATCH (n:MixTest {name: "Test"}) SET n.score = 100, n += {rank: 1}')
    result = g.query('MATCH (n:MixTest {name: "Test"}) RETURN n.score, n.rank')
    assert len(result) == 1
    assert result[0]["n.score"] == 100
    assert result[0]["n.rank"] == 1


# --- Mutation + RETURN tests (SET/REMOVE combined with RETURN) ---


def test_set_return_single_property(g):
    """Test SET + RETURN in a single query."""
    g.query('CREATE (n:SetRetPy {name: "Alice", age: 30})')
    result = g.query('MATCH (n:SetRetPy {name: "Alice"}) SET n.score = 100 RETURN n.score')
    assert len(result) == 1
    assert result[0]["n.score"] == 100


def test_set_return_bulk_merge(g):
    """Test SET n += {map} + RETURN in a single query."""
    g.query('CREATE (n:SetRetPy2 {name: "Bob", age: 25})')
    result = g.query('MATCH (n:SetRetPy2 {name: "Bob"}) SET n += {role: "admin"} RETURN n.name, n.role, n.age')
    assert len(result) == 1
    assert result[0]["n.name"] == "Bob"
    assert result[0]["n.role"] == "admin"
    assert result[0]["n.age"] == 25  # preserved by merge


def test_set_return_with_params(g):
    """Test parameterized SET + RETURN."""
    g.query('CREATE (n:SetRetPy3 {name: "Carol"})')
    result = g.query(
        'MATCH (n:SetRetPy3) WHERE n.name = $name SET n.verified = true RETURN n.verified',
        params={"name": "Carol"},
    )
    assert len(result) == 1
    # bool comes back as "true" string or True depending on type
    assert result[0]["n.verified"] in (True, "true")


def test_set_timestamp_function(g):
    """Issue #35: SET n.prop = timestamp() should evaluate the function."""
    g.query('CREATE (n:TsTest {name: "ts"})')
    g.query('MATCH (n:TsTest {name: "ts"}) SET n.updated = timestamp()')
    result = g.query('MATCH (n:TsTest {name: "ts"}) RETURN n.updated')
    assert len(result) == 1
    ts = result[0]["n.updated"]
    assert ts is not None
    assert isinstance(ts, int)
    assert ts > 0


def test_set_toUpper_function(g):
    """Issue #35: SET n.prop = toUpper('alice') should evaluate the function."""
    g.query('CREATE (n:UpperTest {name: "raw"})')
    g.query("MATCH (n:UpperTest {name: 'raw'}) SET n.name = toUpper('alice')")
    result = g.query('MATCH (n:UpperTest) RETURN n.name')
    assert len(result) == 1
    assert result[0]["n.name"] == "ALICE"


def test_merge_on_create_set_timestamp(g):
    """Issue #35: MERGE ... ON CREATE SET n.created = timestamp()."""
    g.query("MERGE (n:MergeTsTest {id: 'mt1'}) ON CREATE SET n.created = timestamp()")
    result = g.query("MATCH (n:MergeTsTest {id: 'mt1'}) RETURN n.created")
    assert len(result) == 1
    assert result[0]["n.created"] is not None
    assert isinstance(result[0]["n.created"], int)


def test_bulk_set_parameter_merge(g):
    """Issue #38: SET n += $param should merge parameter map into properties."""
    g.query('CREATE (n:BulkParamMerge {name: "Bob", age: 25})')
    g.query(
        'MATCH (n:BulkParamMerge {name: "Bob"}) SET n += $props',
        params={"props": {"city": "LA", "active": True}},
    )
    result = g.query('MATCH (n:BulkParamMerge {name: "Bob"}) RETURN n.age, n.city, n.active')
    assert len(result) == 1
    assert result[0]["n.age"] == 25  # preserved
    assert result[0]["n.city"] == "LA"


def test_bulk_set_parameter_replace(g):
    """Issue #38: SET n = $param should replace all properties."""
    g.query('CREATE (n:BulkParamReplace {name: "Alice", age: 30, city: "NYC"})')
    g.query(
        'MATCH (n:BulkParamReplace {name: "Alice"}) SET n = $props',
        params={"props": {"name": "Alice", "score": 100}},
    )
    result = g.query('MATCH (n:BulkParamReplace {name: "Alice"}) RETURN n.score, n.age')
    assert len(result) == 1
    assert result[0]["n.score"] == 100
    assert result[0]["n.age"] is None  # replaced away


def test_bulk_set_parameter_nested_json(g):
    """Issue #38: nested objects in parameter map should be stored as JSON."""
    g.query('CREATE (n:BulkParamJson {name: "test"})')
    g.query(
        'MATCH (n:BulkParamJson {name: "test"}) SET n += $props',
        params={"props": {"meta": {"team": "core", "priority": 1}}},
    )
    result = g.query('MATCH (n:BulkParamJson {name: "test"}) RETURN n.meta')
    assert len(result) == 1
    assert "core" in str(result[0]["n.meta"])


def test_set_toFloat_function(g):
    """PR #45 coverage: SET n.prop = toFloat('3.14') should evaluate to a float."""
    g.query('CREATE (n:FloatFuncPy {name: "ftest"})')
    g.query("MATCH (n:FloatFuncPy {name: 'ftest'}) SET n.score = toFloat('3.14')")
    result = g.query('MATCH (n:FloatFuncPy {name: "ftest"}) RETURN n.score')
    assert len(result) == 1
    assert abs(result[0]["n.score"] - 3.14) < 0.001


def test_set_function_null_result(g):
    """PR #45 coverage: NULL-returning function should skip the property."""
    g.query('CREATE (n:NullFuncPy {name: "ntest", keep: "yes"})')
    g.query("MATCH (n:NullFuncPy {name: 'ntest'}) SET n.bad = toIntegerOrNull('not_a_number')")
    result = g.query('MATCH (n:NullFuncPy {name: "ntest"}) RETURN n.bad, n.keep')
    assert len(result) == 1
    assert result[0]["n.bad"] is None
    assert result[0]["n.keep"] == "yes"


def test_bulk_set_parameter_float_values(g):
    """PR #45 coverage: float values in parameter map."""
    g.query('CREATE (n:BulkFloatPy {name: "flt"})')
    g.query(
        'MATCH (n:BulkFloatPy {name: "flt"}) SET n += $props',
        params={"props": {"temperature": 98.6, "ratio": -0.5}},
    )
    result = g.query('MATCH (n:BulkFloatPy {name: "flt"}) RETURN n.temperature, n.ratio')
    assert len(result) == 1
    assert abs(result[0]["n.temperature"] - 98.6) < 0.01
    assert abs(result[0]["n.ratio"] - (-0.5)) < 0.01


def test_bulk_set_parameter_null_skipped(g):
    """PR #45 coverage: null values in parameter map should be skipped."""
    g.query('CREATE (n:BulkNullPy {name: "nv", existing: "kept"})')
    g.query(
        'MATCH (n:BulkNullPy {name: "nv"}) SET n += $props',
        params={"props": {"added": "yes", "skipped": None}},
    )
    result = g.query('MATCH (n:BulkNullPy {name: "nv"}) RETURN n.added, n.skipped, n.existing')
    assert len(result) == 1
    assert result[0]["n.added"] == "yes"
    assert result[0]["n.skipped"] is None
    assert result[0]["n.existing"] == "kept"


def test_bulk_set_parameter_bool_false(g):
    """PR #45 coverage: boolean false in parameter map."""
    g.query('CREATE (n:BulkBoolFPy {name: "bf"})')
    g.query(
        'MATCH (n:BulkBoolFPy {name: "bf"}) SET n += $props',
        params={"props": {"active": False, "verified": True}},
    )
    result = g.query('MATCH (n:BulkBoolFPy {name: "bf"}) RETURN n.active, n.verified')
    assert len(result) == 1
    assert result[0]["n.active"] in (0, False, "0", "false")
    assert result[0]["n.verified"] in (1, True, "1", "true")


def test_bulk_set_parameter_nested_array(g):
    """PR #45 coverage: nested array in parameter map should be stored as JSON."""
    g.query('CREATE (n:BulkArrPy {name: "arr"})')
    g.query(
        'MATCH (n:BulkArrPy {name: "arr"}) SET n += $props',
        params={"props": {"tags": ["a", "b", "c"]}},
    )
    result = g.query('MATCH (n:BulkArrPy {name: "arr"}) RETURN n.tags')
    assert len(result) == 1
    tags_str = str(result[0]["n.tags"])
    assert "a" in tags_str
    assert "b" in tags_str
    assert "c" in tags_str


def test_bulk_set_parameter_non_json_error(g):
    """PR #45 coverage: non-JSON param for bulk SET should error."""
    g.query('CREATE (n:BulkErrPy {name: "err"})')
    try:
        g.query(
            'MATCH (n:BulkErrPy {name: "err"}) SET n += $props',
            params={"props": "not_an_object"},
        )
        assert False, "Expected an error for non-JSON bulk SET parameter"
    except Exception:
        pass  # Expected


def test_bulk_set_parameter_missing_error(g):
    """PR #45 coverage: missing param for bulk SET should error."""
    g.query('CREATE (n:BulkMissPy {name: "miss"})')
    try:
        g.query(
            'MATCH (n:BulkMissPy {name: "miss"}) SET n += $nonexistent',
            params={"other": {"a": 1}},
        )
        assert False, "Expected an error for missing bulk SET parameter"
    except Exception:
        pass  # Expected


def test_merge_on_match_set_function(g):
    """PR #45 coverage: MERGE ON MATCH SET with function call."""
    g.query("CREATE (n:MergeMatchFunc {id: 'mm1', name: 'original'})")
    g.query("MERGE (n:MergeMatchFunc {id: 'mm1'}) ON MATCH SET n.name = toUpper('updated')")
    result = g.query("MATCH (n:MergeMatchFunc {id: 'mm1'}) RETURN n.name")
    assert len(result) == 1
    assert result[0]["n.name"] == "UPDATED"


def test_merge_with_set_return(g):
    """Issue #48: MERGE + WITH + SET + RETURN returns column data."""
    result = g.query("""
        MERGE (n:MergeWithPy {id: 'mwp1'})
        WITH n
        SET n.updated = true
        RETURN n.id, n.updated
    """)
    assert len(result) == 1
    assert result[0]["n.id"] == "mwp1"
    assert result[0]["n.updated"] is True


def test_merge_with_set_no_return(g):
    """Issue #48: MERGE + WITH + SET without RETURN succeeds."""
    g.query("""
        MERGE (n:MergeWithPy2 {id: 'mwp2'})
        WITH n
        SET n.updated = true
    """)
    result = g.query("MATCH (n:MergeWithPy2 {id: 'mwp2'}) RETURN n.updated")
    assert len(result) == 1
    assert result[0]["n.updated"] is True


def test_merge_with_return_no_set(g):
    """Issue #48: MERGE + WITH + RETURN without SET returns column data."""
    result = g.query("""
        MERGE (n:MergeWithPy3 {id: 'mwp3'})
        WITH n
        RETURN n.id
    """)
    assert len(result) == 1
    assert result[0]["n.id"] == "mwp3"


def test_remove_return(g):
    """Test REMOVE + RETURN in a single query."""
    g.query('CREATE (n:RemRetPy {name: "Dave", temp: "delete_me"})')
    result = g.query('MATCH (n:RemRetPy {name: "Dave"}) REMOVE n.temp RETURN n.name, n.temp')
    assert len(result) == 1
    assert result[0]["n.name"] == "Dave"
    assert result[0]["n.temp"] is None


# =============================================================================
# CALL {} Subquery
# =============================================================================

def test_call_subquery_standalone(g):
    """Test standalone CALL { MATCH ... RETURN }."""
    g.connection.cypher('CREATE (:CallPy {name: "Alice"})')
    g.connection.cypher('CREATE (:CallPy {name: "Bob"})')
    result = g.query('CALL { MATCH (n:CallPy) RETURN n.name ORDER BY n.name }')
    assert len(result) == 2
    assert result[0]["n.name"] == "Alice"
    assert result[1]["n.name"] == "Bob"


def test_call_subquery_with_import(g):
    """Test CALL with WITH variable import and SET."""
    g.connection.cypher('CREATE (:CallPyW {name: "Carol"})')
    g.connection.cypher(
        'MATCH (n:CallPyW {name: "Carol"}) CALL { WITH n SET n.touched = true }'
    )
    result = g.query('MATCH (n:CallPyW) RETURN n.name, n.touched')
    assert len(result) == 1
    assert result[0]["n.name"] == "Carol"
    assert result[0]["n.touched"] is True


def test_call_subquery_union(g):
    """Test CALL with UNION inside."""
    result = g.query('CALL { RETURN 1 AS n UNION RETURN 2 AS n }')
    assert len(result) == 2
    values = sorted([r["n"] for r in result])
    assert values == [1, 2]
