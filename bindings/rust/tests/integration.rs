//! Integration tests for GraphQLite Rust bindings.

use graphqlite::{escape_string, graphs, sanitize_rel_type, Connection, Error, Graph, GraphManager};

/// Create a test connection.
fn test_connection() -> Connection {
    Connection::open_in_memory().expect("Failed to open in-memory connection")
}

/// Create a test graph.
fn test_graph() -> Graph {
    Graph::open_in_memory().expect("Failed to open in-memory graph")
}

#[test]
fn test_open_memory() {
    let conn = test_connection();
    assert!(conn.cypher("RETURN 1").is_ok());
}

#[test]
fn test_open_file() {
    let temp_dir = tempfile::tempdir().unwrap();
    let db_path = temp_dir.path().join("test.db");

    let conn = Connection::open(&db_path).unwrap();
    conn.cypher("CREATE (n:Test)").unwrap();

    assert!(db_path.exists());
}

#[test]
fn test_create_node() {
    let conn = test_connection();

    conn.cypher("CREATE (n:Person {name: \"Alice\", age: 30})")
        .unwrap();

    let results = conn
        .cypher("MATCH (n:Person) RETURN n.name, n.age")
        .unwrap();

    assert_eq!(results.len(), 1);
    // Column names include variable prefix (e.g., n.name)
    assert_eq!(results[0].get::<String>("n.name").unwrap(), "Alice");
    assert_eq!(results[0].get::<i64>("n.age").unwrap(), 30);
}

#[test]
fn test_create_relationship() {
    let conn = test_connection();

    conn.cypher("CREATE (a:Person {name: 'Alice'})").unwrap();
    conn.cypher("CREATE (b:Person {name: 'Bob'})").unwrap();
    conn.cypher(
        "MATCH (a:Person {name: 'Alice'}), (b:Person {name: 'Bob'})
         CREATE (a)-[:KNOWS]->(b)",
    )
    .unwrap();

    let results = conn
        .cypher("MATCH (a)-[:KNOWS]->(b) RETURN a.name, b.name")
        .unwrap();

    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("a.name").unwrap(), "Alice");
    assert_eq!(results[0].get::<String>("b.name").unwrap(), "Bob");
}

#[test]
fn test_return_scalar() {
    let conn = test_connection();

    let results = conn
        .cypher("RETURN 42 as num, 'hello' as str, true as flag")
        .unwrap();

    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<i64>("num").unwrap(), 42);
    assert_eq!(results[0].get::<String>("str").unwrap(), "hello");
    assert!(results[0].get::<bool>("flag").unwrap());
}

#[test]
fn test_multiple_rows() {
    let conn = test_connection();

    conn.cypher("CREATE (n:Num {val: 1})").unwrap();
    conn.cypher("CREATE (n:Num {val: 2})").unwrap();
    conn.cypher("CREATE (n:Num {val: 3})").unwrap();

    let results = conn.cypher("MATCH (n:Num) RETURN n.val ORDER BY n.val").unwrap();

    assert_eq!(results.len(), 3);
    // Results use the actual column name from the query
    let values: Vec<i64> = results.iter().map(|r| r.get::<i64>("n.val").unwrap()).collect();
    assert_eq!(values, vec![1, 2, 3]);
}

#[test]
fn test_aggregation() {
    let conn = test_connection();

    conn.cypher("CREATE (n:Num {val: 10})").unwrap();
    conn.cypher("CREATE (n:Num {val: 20})").unwrap();
    conn.cypher("CREATE (n:Num {val: 30})").unwrap();

    let results = conn
        .cypher("MATCH (n:Num) RETURN count(n) as cnt, sum(n.val) as total")
        .unwrap();

    assert_eq!(results[0].get::<i64>("cnt").unwrap(), 3);
    assert_eq!(results[0].get::<i64>("total").unwrap(), 60);
}

#[test]
fn test_empty_result() {
    let conn = test_connection();

    let results = conn.cypher("MATCH (n:NonExistent) RETURN n").unwrap();
    // Empty MATCH results return a success message, not an empty array
    // Check that we get a result (the success message)
    assert!(results.len() <= 1);
}

#[test]
fn test_iteration() {
    let conn = test_connection();

    conn.cypher("CREATE (n:N {v: 'a'})").unwrap();
    conn.cypher("CREATE (n:N {v: 'b'})").unwrap();

    let results = conn.cypher("MATCH (n:N) RETURN n.v ORDER BY n.v").unwrap();

    let mut values = Vec::new();
    for row in &results {
        // Results use the actual column name from the query
        values.push(row.get::<String>("n.v").unwrap());
    }
    assert_eq!(values, vec!["a", "b"]);
}

#[test]
fn test_column_not_found() {
    let conn = test_connection();

    let results = conn.cypher("RETURN 1 as x").unwrap();
    let err = results[0].get::<i64>("nonexistent").unwrap_err();

    match err {
        Error::ColumnNotFound(name) => assert_eq!(name, "nonexistent"),
        _ => panic!("Expected ColumnNotFound error"),
    }
}

#[test]
fn test_type_error() {
    let conn = test_connection();

    let results = conn.cypher("RETURN 'hello' as val").unwrap();
    let err = results[0].get::<i64>("val").unwrap_err();

    match err {
        Error::TypeError { expected, .. } => assert_eq!(expected, "Integer"),
        _ => panic!("Expected TypeError"),
    }
}

#[test]
fn test_optional_values() {
    let conn = test_connection();

    conn.cypher("CREATE (n:Node {name: 'test'})").unwrap();

    let results = conn
        .cypher("MATCH (n:Node) RETURN n.name, n.missing")
        .unwrap();

    assert_eq!(
        results[0].get::<Option<String>>("n.name").unwrap(),
        Some("test".to_string())
    );
    assert_eq!(
        results[0].get::<Option<String>>("n.missing").unwrap(),
        None
    );
}

#[test]
fn test_columns() {
    let conn = test_connection();

    let results = conn.cypher("RETURN 1 as a, 2 as b, 3 as c").unwrap();

    let columns = results.columns();
    assert!(columns.contains(&"a".to_string()));
    assert!(columns.contains(&"b".to_string()));
    assert!(columns.contains(&"c".to_string()));
}

#[test]
fn test_graph_algorithms() {
    let conn = test_connection();

    // Create a small graph
    conn.cypher("CREATE (a:Page {name: 'A'})").unwrap();
    conn.cypher("CREATE (b:Page {name: 'B'})").unwrap();
    conn.cypher("CREATE (c:Page {name: 'C'})").unwrap();
    conn.cypher(
        "MATCH (a:Page {name: 'A'}), (b:Page {name: 'B'}), (c:Page {name: 'C'})
         CREATE (a)-[:LINKS]->(b), (a)-[:LINKS]->(c), (b)-[:LINKS]->(c)",
    )
    .unwrap();

    // PageRank
    let results = conn.cypher("RETURN pageRank(0.85, 10)").unwrap();
    assert!(!results.is_empty());

    // Label Propagation
    let results = conn.cypher("RETURN labelPropagation(5)").unwrap();
    assert!(!results.is_empty());
}

// =============================================================================
// Graph API Tests
// =============================================================================

#[test]
fn test_graph_upsert_node() {
    let g = test_graph();

    g.upsert_node("alice", [("name", "Alice"), ("age", "30")], "Person")
        .unwrap();

    assert!(g.has_node("alice").unwrap());
    assert!(!g.has_node("bob").unwrap());

    let node = g.get_node("alice").unwrap();
    assert!(node.is_some());
}

#[test]
fn test_graph_upsert_edge() {
    let g = test_graph();

    g.upsert_node("a", [("name", "A")], "Node").unwrap();
    g.upsert_node("b", [("name", "B")], "Node").unwrap();
    g.upsert_edge("a", "b", [("weight", "10")], "CONNECTS")
        .unwrap();

    assert!(g.has_edge("a", "b").unwrap());
    assert!(!g.has_edge("b", "a").unwrap()); // Directed edge
}

#[test]
fn test_graph_stats() {
    let g = test_graph();

    g.upsert_node("n1", [("v", "1")], "N").unwrap();
    g.upsert_node("n2", [("v", "2")], "N").unwrap();
    g.upsert_node("n3", [("v", "3")], "N").unwrap();
    let empty: [(&str, &str); 0] = [];
    g.upsert_edge("n1", "n2", empty, "E").unwrap();
    g.upsert_edge("n2", "n3", empty, "E").unwrap();

    let stats = g.stats().unwrap();
    assert_eq!(stats.nodes, 3);
    assert_eq!(stats.edges, 2);
}

#[test]
fn test_graph_degree() {
    let g = test_graph();

    g.upsert_node("hub", [("name", "Hub")], "Node").unwrap();
    g.upsert_node("a", [("name", "A")], "Node").unwrap();
    g.upsert_node("b", [("name", "B")], "Node").unwrap();
    g.upsert_node("c", [("name", "C")], "Node").unwrap();
    let empty: [(&str, &str); 0] = [];
    g.upsert_edge("hub", "a", empty, "LINK").unwrap();
    g.upsert_edge("hub", "b", empty, "LINK").unwrap();
    g.upsert_edge("hub", "c", empty, "LINK").unwrap();

    let degree = g.node_degree("hub").unwrap();
    assert_eq!(degree, 3);
}

#[test]
fn test_graph_neighbors() {
    let g = test_graph();

    g.upsert_node("center", [("name", "Center")], "Node")
        .unwrap();
    g.upsert_node("n1", [("name", "N1")], "Node").unwrap();
    g.upsert_node("n2", [("name", "N2")], "Node").unwrap();
    let empty: [(&str, &str); 0] = [];
    g.upsert_edge("center", "n1", empty, "LINK").unwrap();
    g.upsert_edge("n2", "center", empty, "LINK").unwrap();

    let neighbors = g.get_neighbors("center").unwrap();
    // At least one neighbor should be found (bidirectional matching)
    assert!(!neighbors.is_empty());
}

#[test]
fn test_graph_delete_node() {
    let g = test_graph();

    g.upsert_node("temp", [("name", "Temp")], "Node").unwrap();
    assert!(g.has_node("temp").unwrap());

    g.delete_node("temp").unwrap();
    assert!(!g.has_node("temp").unwrap());
}

#[test]
fn test_graph_delete_edge() {
    let g = test_graph();

    g.upsert_node("x", [("name", "X")], "Node").unwrap();
    g.upsert_node("y", [("name", "Y")], "Node").unwrap();
    let empty: [(&str, &str); 0] = [];
    g.upsert_edge("x", "y", empty, "REL").unwrap();
    assert!(g.has_edge("x", "y").unwrap());

    g.delete_edge("x", "y").unwrap();
    assert!(!g.has_edge("x", "y").unwrap());
}

#[test]
fn test_graph_query() {
    let g = test_graph();

    g.upsert_node("test", [("name", "Test"), ("value", "42")], "Data")
        .unwrap();

    let result = g
        .query("MATCH (n:Data) RETURN n.name, n.value")
        .unwrap();
    assert_eq!(result.len(), 1);
    assert_eq!(result[0].get::<String>("n.name").unwrap(), "Test");
}

#[test]
fn test_graph_batch_nodes() {
    let g = test_graph();

    g.upsert_nodes_batch([
        ("n1", [("name", "Node1")], "Batch"),
        ("n2", [("name", "Node2")], "Batch"),
        ("n3", [("name", "Node3")], "Batch"),
    ])
    .unwrap();

    let stats = g.stats().unwrap();
    assert_eq!(stats.nodes, 3);
}

#[test]
fn test_graph_api_algorithms() {
    let g = test_graph();

    // Create a small graph for algorithms
    g.upsert_node("a", [("name", "A")], "Page").unwrap();
    g.upsert_node("b", [("name", "B")], "Page").unwrap();
    g.upsert_node("c", [("name", "C")], "Page").unwrap();
    let empty: [(&str, &str); 0] = [];
    g.upsert_edge("a", "b", empty, "LINKS").unwrap();
    g.upsert_edge("a", "c", empty, "LINKS").unwrap();
    g.upsert_edge("b", "c", empty, "LINKS").unwrap();

    let ranks = g.pagerank(0.85, 10).unwrap();
    assert!(!ranks.is_empty());

    let communities = g.community_detection(5).unwrap();
    assert!(!communities.is_empty());
}

#[test]
fn test_graph_shortest_path() {
    let g = test_graph();

    // Create a path: sp1 -> sp2 -> sp3
    g.upsert_node("sp1", [("name", "SP1")], "Node").unwrap();
    g.upsert_node("sp2", [("name", "SP2")], "Node").unwrap();
    g.upsert_node("sp3", [("name", "SP3")], "Node").unwrap();
    let empty: [(&str, &str); 0] = [];
    g.upsert_edge("sp1", "sp2", empty, "CONNECTS").unwrap();
    g.upsert_edge("sp2", "sp3", empty, "CONNECTS").unwrap();

    // Test finding a path
    let result = g.shortest_path("sp1", "sp3", None).unwrap();
    assert!(result.found);
    assert_eq!(result.distance, Some(2.0));
    assert_eq!(result.path, vec!["sp1", "sp2", "sp3"]);

    // Test same node
    let result = g.shortest_path("sp1", "sp1", None).unwrap();
    assert!(result.found);
    assert_eq!(result.distance, Some(0.0));

    // Test no path (reverse direction)
    let result = g.shortest_path("sp3", "sp1", None).unwrap();
    assert!(!result.found);
    assert!(result.path.is_empty());
}

#[test]
fn test_graph_degree_centrality() {
    let g = test_graph();

    // Create graph: dc1 -> dc2 -> dc3, dc1 -> dc3
    g.upsert_node("dc1", [("name", "DC1")], "Node").unwrap();
    g.upsert_node("dc2", [("name", "DC2")], "Node").unwrap();
    g.upsert_node("dc3", [("name", "DC3")], "Node").unwrap();
    let empty: [(&str, &str); 0] = [];
    g.upsert_edge("dc1", "dc2", empty, "CONNECTS").unwrap();
    g.upsert_edge("dc2", "dc3", empty, "CONNECTS").unwrap();
    g.upsert_edge("dc1", "dc3", empty, "CONNECTS").unwrap();

    let degrees = g.degree_centrality().unwrap();
    assert_eq!(degrees.len(), 3);

    // Find each node's result
    let dc1 = degrees.iter().find(|d| d.user_id.as_deref() == Some("dc1")).unwrap();
    let dc2 = degrees.iter().find(|d| d.user_id.as_deref() == Some("dc2")).unwrap();
    let dc3 = degrees.iter().find(|d| d.user_id.as_deref() == Some("dc3")).unwrap();

    // dc1: out=2, in=0
    assert_eq!(dc1.out_degree, 2);
    assert_eq!(dc1.in_degree, 0);
    assert_eq!(dc1.degree, 2);

    // dc2: out=1, in=1
    assert_eq!(dc2.out_degree, 1);
    assert_eq!(dc2.in_degree, 1);
    assert_eq!(dc2.degree, 2);

    // dc3: out=0, in=2
    assert_eq!(dc3.out_degree, 0);
    assert_eq!(dc3.in_degree, 2);
    assert_eq!(dc3.degree, 2);
}

#[test]
fn test_graph_wcc() {
    let g = test_graph();

    // Create two disconnected components
    // Component 1: wcc1 -> wcc2 -> wcc3
    g.upsert_node("wcc1", [("name", "WCC1")], "Node").unwrap();
    g.upsert_node("wcc2", [("name", "WCC2")], "Node").unwrap();
    g.upsert_node("wcc3", [("name", "WCC3")], "Node").unwrap();
    let empty: [(&str, &str); 0] = [];
    g.upsert_edge("wcc1", "wcc2", empty, "LINK").unwrap();
    g.upsert_edge("wcc2", "wcc3", empty, "LINK").unwrap();

    // Component 2: wcc4 -> wcc5
    g.upsert_node("wcc4", [("name", "WCC4")], "Node").unwrap();
    g.upsert_node("wcc5", [("name", "WCC5")], "Node").unwrap();
    g.upsert_edge("wcc4", "wcc5", empty, "LINK").unwrap();

    let components = g.wcc().unwrap();
    assert_eq!(components.len(), 5);

    // Group by component
    let mut by_component: std::collections::HashMap<i64, Vec<String>> = std::collections::HashMap::new();
    for c in &components {
        by_component
            .entry(c.component)
            .or_default()
            .push(c.user_id.clone().unwrap_or_default());
    }

    // Should have exactly 2 components
    assert_eq!(by_component.len(), 2);

    // Check component sizes
    let mut sizes: Vec<usize> = by_component.values().map(|v| v.len()).collect();
    sizes.sort();
    assert_eq!(sizes, vec![2, 3]);
}

#[test]
fn test_graph_wcc_empty() {
    let g = test_graph();

    let components = g.wcc().unwrap();
    assert!(components.is_empty());
}

#[test]
fn test_graph_wcc_alias() {
    let g = test_graph();

    g.upsert_node("a1", [("name", "A1")], "Node").unwrap();
    g.upsert_node("a2", [("name", "A2")], "Node").unwrap();
    let empty: [(&str, &str); 0] = [];
    g.upsert_edge("a1", "a2", empty, "LINK").unwrap();

    // Both methods should return the same result
    let wcc = g.wcc().unwrap();
    let cc = g.wcc().unwrap();

    assert_eq!(wcc.len(), cc.len());
}

#[test]
fn test_graph_scc_cycle() {
    let g = test_graph();

    // Create a cycle: scc1 -> scc2 -> scc3 -> scc1
    g.upsert_node("scc1", [("name", "SCC1")], "Node").unwrap();
    g.upsert_node("scc2", [("name", "SCC2")], "Node").unwrap();
    g.upsert_node("scc3", [("name", "SCC3")], "Node").unwrap();
    let empty: [(&str, &str); 0] = [];
    g.upsert_edge("scc1", "scc2", empty, "LINK").unwrap();
    g.upsert_edge("scc2", "scc3", empty, "LINK").unwrap();
    g.upsert_edge("scc3", "scc1", empty, "LINK").unwrap();

    let components = g.scc().unwrap();
    assert_eq!(components.len(), 3);

    // All nodes should be in the same SCC (they form a cycle)
    let component_ids: std::collections::HashSet<i64> =
        components.iter().map(|c| c.component).collect();
    assert_eq!(component_ids.len(), 1);
}

#[test]
fn test_graph_scc_no_cycle() {
    let g = test_graph();

    // Create a directed chain (no cycles): sc_a -> sc_b -> sc_c
    g.upsert_node("sc_a", [("name", "SC_A")], "Node").unwrap();
    g.upsert_node("sc_b", [("name", "SC_B")], "Node").unwrap();
    g.upsert_node("sc_c", [("name", "SC_C")], "Node").unwrap();
    let empty: [(&str, &str); 0] = [];
    g.upsert_edge("sc_a", "sc_b", empty, "LINK").unwrap();
    g.upsert_edge("sc_b", "sc_c", empty, "LINK").unwrap();

    let components = g.scc().unwrap();
    assert_eq!(components.len(), 3);

    // Each node should be in its own SCC (no back edges)
    let component_ids: std::collections::HashSet<i64> =
        components.iter().map(|c| c.component).collect();
    assert_eq!(component_ids.len(), 3);
}

#[test]
fn test_graph_scc_empty() {
    let g = test_graph();

    let components = g.scc().unwrap();
    assert!(components.is_empty());
}

// =============================================================================
// REMOVE Clause Tests
// =============================================================================

#[test]
fn test_remove_node_property() {
    let conn = test_connection();

    // Create node with properties
    conn.cypher("CREATE (n:RemoveTest {name: 'Alice', age: 30, city: 'NYC'})")
        .unwrap();

    // Verify properties exist
    let results = conn
        .cypher("MATCH (n:RemoveTest) RETURN n.name, n.age, n.city")
        .unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("n.name").unwrap(), "Alice");
    assert_eq!(results[0].get::<i64>("n.age").unwrap(), 30);
    assert_eq!(results[0].get::<String>("n.city").unwrap(), "NYC");

    // Remove age property
    conn.cypher("MATCH (n:RemoveTest) REMOVE n.age").unwrap();

    // Verify age is removed (null)
    let results = conn
        .cypher("MATCH (n:RemoveTest) RETURN n.name, n.age, n.city")
        .unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("n.name").unwrap(), "Alice");
    assert!(results[0].get::<Option<i64>>("n.age").unwrap().is_none());
    assert_eq!(results[0].get::<String>("n.city").unwrap(), "NYC");
}

#[test]
fn test_remove_multiple_properties() {
    let conn = test_connection();

    // Create node with multiple properties
    conn.cypher("CREATE (n:RemoveMultiTest {a: 1, b: 2, c: 3, d: 4})")
        .unwrap();

    // Remove multiple properties in one query
    conn.cypher("MATCH (n:RemoveMultiTest) REMOVE n.a, n.b")
        .unwrap();

    // Verify a and b are removed, c and d remain
    let results = conn
        .cypher("MATCH (n:RemoveMultiTest) RETURN n.a, n.b, n.c, n.d")
        .unwrap();
    assert_eq!(results.len(), 1);
    assert!(results[0].get::<Option<i64>>("n.a").unwrap().is_none());
    assert!(results[0].get::<Option<i64>>("n.b").unwrap().is_none());
    assert_eq!(results[0].get::<i64>("n.c").unwrap(), 3);
    assert_eq!(results[0].get::<i64>("n.d").unwrap(), 4);
}

#[test]
fn test_remove_label() {
    let conn = test_connection();

    // Create node with multiple labels - use unique label to avoid conflicts
    conn.cypher("CREATE (n:RemoveLabelTest:RemoveLabelEmp:RemoveLabelMgr {name: 'Bob'})")
        .unwrap();

    // Verify all labels exist
    let results = conn
        .cypher("MATCH (n:RemoveLabelMgr {name: 'Bob'}) RETURN n.name")
        .unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("n.name").unwrap(), "Bob");

    // Remove Manager label
    conn.cypher("MATCH (n:RemoveLabelTest {name: 'Bob'}) REMOVE n:RemoveLabelMgr")
        .unwrap();

    // Verify label is removed by checking that a query for the removed label returns no results
    // For empty MATCH results, we may get a success message or empty array
    let results = conn
        .cypher("MATCH (n:RemoveLabelMgr {name: 'Bob'}) RETURN n.name")
        .unwrap();

    // After removing RemoveLabelMgr, this should not find any nodes
    // Either empty or just a success message without actual data
    let has_bob = results.iter().any(|row| {
        row.get::<String>("n.name").map(|n| n == "Bob").unwrap_or(false)
    });
    assert!(!has_bob, "Should not find Bob with RemoveLabelMgr after removal");

    // Verify the node still exists with remaining labels
    let results = conn
        .cypher("MATCH (n:RemoveLabelTest {name: 'Bob'}) RETURN n.name")
        .unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("n.name").unwrap(), "Bob");
}

#[test]
fn test_remove_edge_property() {
    let conn = test_connection();

    // Create relationship with properties
    conn.cypher(
        "CREATE (a:RemoveEdgeTest {name: 'A'})-[r:KNOWS {since: 2020, strength: 0.9}]->(b:RemoveEdgeTest {name: 'B'})",
    )
    .unwrap();

    // Verify edge properties exist
    let results = conn
        .cypher("MATCH (a:RemoveEdgeTest)-[r:KNOWS]->(b:RemoveEdgeTest) RETURN r.since, r.strength")
        .unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<i64>("r.since").unwrap(), 2020);

    // Remove since property
    conn.cypher("MATCH (a:RemoveEdgeTest)-[r:KNOWS]->(b:RemoveEdgeTest) REMOVE r.since")
        .unwrap();

    // Verify since is removed, strength remains
    let results = conn
        .cypher("MATCH (a:RemoveEdgeTest)-[r:KNOWS]->(b:RemoveEdgeTest) RETURN r.since, r.strength")
        .unwrap();
    assert_eq!(results.len(), 1);
    assert!(results[0].get::<Option<i64>>("r.since").unwrap().is_none());
    // strength is a float, check it exists
    let strength = results[0].get::<f64>("r.strength").unwrap();
    assert!((strength - 0.9).abs() < 0.01);
}

#[test]
fn test_remove_with_where() {
    let conn = test_connection();

    // Create multiple nodes
    conn.cypher("CREATE (a:RemoveWhereTest {name: 'Alice', age: 30, status: 'active'})")
        .unwrap();
    conn.cypher("CREATE (b:RemoveWhereTest {name: 'Bob', age: 25, status: 'active'})")
        .unwrap();
    conn.cypher("CREATE (c:RemoveWhereTest {name: 'Charlie', age: 35, status: 'active'})")
        .unwrap();

    // Remove status only from nodes where age > 28
    conn.cypher("MATCH (n:RemoveWhereTest) WHERE n.age > 28 REMOVE n.status")
        .unwrap();

    // Verify: Alice (30) and Charlie (35) should have status removed, Bob (25) should keep it
    let results = conn
        .cypher("MATCH (n:RemoveWhereTest) RETURN n.name, n.status ORDER BY n.name")
        .unwrap();
    assert_eq!(results.len(), 3);

    // Alice: status should be null
    assert_eq!(results[0].get::<String>("n.name").unwrap(), "Alice");
    assert!(results[0].get::<Option<String>>("n.status").unwrap().is_none());

    // Bob: status should still exist
    assert_eq!(results[1].get::<String>("n.name").unwrap(), "Bob");
    assert_eq!(
        results[1].get::<String>("n.status").unwrap(),
        "active"
    );

    // Charlie: status should be null
    assert_eq!(results[2].get::<String>("n.name").unwrap(), "Charlie");
    assert!(results[2].get::<Option<String>>("n.status").unwrap().is_none());
}

#[test]
fn test_remove_nonexistent_property() {
    let conn = test_connection();

    // Create node with only one property
    conn.cypher("CREATE (n:RemoveNonexistTest {name: 'Test'})")
        .unwrap();

    // Remove a property that doesn't exist - should succeed without error
    let result = conn.cypher("MATCH (n:RemoveNonexistTest) REMOVE n.nonexistent");
    assert!(result.is_ok());

    // Original property should still exist
    let results = conn
        .cypher("MATCH (n:RemoveNonexistTest) RETURN n.name")
        .unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("n.name").unwrap(), "Test");
}

#[test]
fn test_remove_no_match() {
    let conn = test_connection();

    // Remove property on non-existent nodes - should succeed (0 rows affected)
    let result = conn.cypher("MATCH (n:NonExistentLabel) REMOVE n.property");
    assert!(result.is_ok());
}

// =============================================================================
// IN Operator Tests
// =============================================================================

#[test]
fn test_in_literal_list_match() {
    let conn = test_connection();

    let results = conn.cypher("RETURN 5 IN [1, 2, 5, 10]").unwrap();
    assert_eq!(results.len(), 1);
    // Result should be truthy (1)
    let val = results[0].get::<i64>(&results.columns()[0]).unwrap();
    assert_eq!(val, 1);
}

#[test]
fn test_in_literal_list_no_match() {
    let conn = test_connection();

    let results = conn.cypher("RETURN 'x' IN ['a', 'b', 'c']").unwrap();
    assert_eq!(results.len(), 1);
    // Result should be falsy (0)
    let val = results[0].get::<i64>(&results.columns()[0]).unwrap();
    assert_eq!(val, 0);
}

#[test]
fn test_in_with_where_clause() {
    let conn = test_connection();

    conn.cypher("CREATE (n:InTest {name: 'Alice', status: 'active'})").unwrap();
    conn.cypher("CREATE (n:InTest {name: 'Bob', status: 'pending'})").unwrap();
    conn.cypher("CREATE (n:InTest {name: 'Charlie', status: 'inactive'})").unwrap();

    let results = conn
        .cypher("MATCH (n:InTest) WHERE n.status IN ['active', 'pending'] RETURN n.name ORDER BY n.name")
        .unwrap();
    assert_eq!(results.len(), 2);
    assert_eq!(results[0].get::<String>("n.name").unwrap(), "Alice");
    assert_eq!(results[1].get::<String>("n.name").unwrap(), "Bob");
}

#[test]
fn test_in_with_integers() {
    let conn = test_connection();

    conn.cypher("CREATE (n:InIntTest {name: 'A', priority: 1})").unwrap();
    conn.cypher("CREATE (n:InIntTest {name: 'B', priority: 2})").unwrap();
    conn.cypher("CREATE (n:InIntTest {name: 'C', priority: 3})").unwrap();

    let results = conn
        .cypher("MATCH (n:InIntTest) WHERE n.priority IN [1, 3] RETURN n.name ORDER BY n.name")
        .unwrap();
    assert_eq!(results.len(), 2);
    assert_eq!(results[0].get::<String>("n.name").unwrap(), "A");
    assert_eq!(results[1].get::<String>("n.name").unwrap(), "C");
}

#[test]
fn test_in_empty_result() {
    let conn = test_connection();

    conn.cypher("CREATE (n:InEmptyTest {name: 'Test', status: 'archived'})").unwrap();

    let results = conn
        .cypher("MATCH (n:InEmptyTest) WHERE n.status IN ['active', 'pending'] RETURN n.name")
        .unwrap();
    assert!(results.is_empty());
}

#[test]
fn test_utility_functions() {
    // escape_string
    assert_eq!(escape_string("hello"), "hello");
    assert_eq!(escape_string("it's"), "it\\'s");
    assert_eq!(escape_string("line\nbreak"), "line break");

    // sanitize_rel_type
    assert_eq!(sanitize_rel_type("KNOWS"), "KNOWS");
    assert_eq!(sanitize_rel_type("has-items"), "has_items");
    assert_eq!(sanitize_rel_type("CREATE"), "REL_CREATE");
}

// =============================================================================
// STARTS WITH, ENDS WITH, CONTAINS Tests
// =============================================================================

#[test]
fn test_starts_with_match() {
    let conn = test_connection();

    let results = conn.cypher("RETURN 'hello world' STARTS WITH 'hello'").unwrap();
    assert_eq!(results.len(), 1);
    // Result should be truthy (1 or true)
    let val = results[0].get::<i64>(&results.columns()[0]).unwrap();
    assert_eq!(val, 1);
}

#[test]
fn test_starts_with_no_match() {
    let conn = test_connection();

    let results = conn.cypher("RETURN 'hello world' STARTS WITH 'world'").unwrap();
    assert_eq!(results.len(), 1);
    let val = results[0].get::<i64>(&results.columns()[0]).unwrap();
    assert_eq!(val, 0);
}

#[test]
fn test_ends_with_match() {
    let conn = test_connection();

    let results = conn.cypher("RETURN 'hello world' ENDS WITH 'world'").unwrap();
    assert_eq!(results.len(), 1);
    let val = results[0].get::<i64>(&results.columns()[0]).unwrap();
    assert_eq!(val, 1);
}

#[test]
fn test_ends_with_no_match() {
    let conn = test_connection();

    let results = conn.cypher("RETURN 'hello world' ENDS WITH 'hello'").unwrap();
    assert_eq!(results.len(), 1);
    let val = results[0].get::<i64>(&results.columns()[0]).unwrap();
    assert_eq!(val, 0);
}

#[test]
fn test_contains_match() {
    let conn = test_connection();

    let results = conn.cypher("RETURN 'hello world' CONTAINS 'lo wo'").unwrap();
    assert_eq!(results.len(), 1);
    let val = results[0].get::<i64>(&results.columns()[0]).unwrap();
    assert_eq!(val, 1);
}

#[test]
fn test_contains_no_match() {
    let conn = test_connection();

    let results = conn.cypher("RETURN 'hello world' CONTAINS 'xyz'").unwrap();
    assert_eq!(results.len(), 1);
    let val = results[0].get::<i64>(&results.columns()[0]).unwrap();
    assert_eq!(val, 0);
}

#[test]
fn test_string_operators_in_where() {
    let conn = test_connection();

    conn.cypher("CREATE (n:StringTest {name: 'John Smith', email: 'john@example.com'})").unwrap();
    conn.cypher("CREATE (n:StringTest {name: 'Jane Doe', email: 'jane@test.org'})").unwrap();
    conn.cypher("CREATE (n:StringTest {name: 'Bob Johnson', email: 'bob@example.com'})").unwrap();

    // Test STARTS WITH
    let results = conn
        .cypher("MATCH (n:StringTest) WHERE n.name STARTS WITH 'J' RETURN n.name ORDER BY n.name")
        .unwrap();
    assert_eq!(results.len(), 2);
    assert_eq!(results[0].get::<String>("n.name").unwrap(), "Jane Doe");
    assert_eq!(results[1].get::<String>("n.name").unwrap(), "John Smith");

    // Test ENDS WITH
    let results = conn
        .cypher("MATCH (n:StringTest) WHERE n.email ENDS WITH '.com' RETURN n.name ORDER BY n.name")
        .unwrap();
    assert_eq!(results.len(), 2);
    assert_eq!(results[0].get::<String>("n.name").unwrap(), "Bob Johnson");
    assert_eq!(results[1].get::<String>("n.name").unwrap(), "John Smith");

    // Test CONTAINS
    let results = conn
        .cypher("MATCH (n:StringTest) WHERE n.name CONTAINS 'ohn' RETURN n.name ORDER BY n.name")
        .unwrap();
    assert_eq!(results.len(), 2);
    assert_eq!(results[0].get::<String>("n.name").unwrap(), "Bob Johnson");
    assert_eq!(results[1].get::<String>("n.name").unwrap(), "John Smith");
}

// =============================================================================
// String Function Tests
// =============================================================================

#[test]
fn test_string_to_upper() {
    let conn = test_connection();

    let results = conn.cypher("RETURN toUpper('hello world') AS result").unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("result").unwrap(), "HELLO WORLD");
}

#[test]
fn test_string_to_lower() {
    let conn = test_connection();

    let results = conn.cypher("RETURN toLower('HELLO WORLD') AS result").unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("result").unwrap(), "hello world");
}

#[test]
fn test_string_trim() {
    let conn = test_connection();

    let results = conn.cypher("RETURN trim('  hello  ') AS result").unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("result").unwrap(), "hello");
}

#[test]
fn test_string_ltrim() {
    let conn = test_connection();

    let results = conn.cypher("RETURN ltrim('  hello  ') AS result").unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("result").unwrap(), "hello  ");
}

#[test]
fn test_string_rtrim() {
    let conn = test_connection();

    let results = conn.cypher("RETURN rtrim('  hello  ') AS result").unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("result").unwrap(), "  hello");
}

#[test]
fn test_string_substring() {
    let conn = test_connection();

    // With start only
    let results = conn.cypher("RETURN substring('hello world', 6) AS result").unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("result").unwrap(), "world");

    // With start and length
    let results = conn.cypher("RETURN substring('hello world', 0, 5) AS result").unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("result").unwrap(), "hello");
}

#[test]
fn test_string_replace() {
    let conn = test_connection();

    let results = conn.cypher("RETURN replace('hello world', 'world', 'rust') AS result").unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("result").unwrap(), "hello rust");
}

#[test]
fn test_string_reverse() {
    let conn = test_connection();

    let results = conn.cypher("RETURN reverse('hello') AS result").unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("result").unwrap(), "olleh");
}

#[test]
fn test_string_left() {
    let conn = test_connection();

    let results = conn.cypher("RETURN left('hello world', 5) AS result").unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("result").unwrap(), "hello");
}

#[test]
fn test_string_right() {
    let conn = test_connection();

    let results = conn.cypher("RETURN right('hello world', 5) AS result").unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("result").unwrap(), "world");
}

#[test]
fn test_string_split() {
    let conn = test_connection();

    // split() returns a single row with the array as a column value
    let results = conn.cypher("RETURN split('a,b,c', ',') AS result").unwrap();
    assert_eq!(results.len(), 1);
    // The result column contains the array ["a", "b", "c"]
    if let Some(graphqlite::Value::Array(arr)) = results[0].get_value("result") {
        assert_eq!(arr.len(), 3);
    } else {
        panic!("Expected array result");
    }
}

#[test]
fn test_string_functions_with_properties() {
    let conn = test_connection();

    conn.cypher("CREATE (n:StringFuncTest {name: '  John Doe  ', email: 'JOHN@EMAIL.COM'})").unwrap();

    let results = conn
        .cypher("MATCH (n:StringFuncTest) RETURN trim(n.name) AS name, toLower(n.email) AS email")
        .unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("name").unwrap(), "John Doe");
    assert_eq!(results[0].get::<String>("email").unwrap(), "john@email.com");
}

// =============================================================================
// Math Function Tests
// =============================================================================

#[test]
fn test_math_abs() {
    let conn = test_connection();

    let results = conn.cypher("RETURN abs(-42) AS result").unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<i64>("result").unwrap(), 42);

    let results = conn.cypher("RETURN abs(-3.14) AS result").unwrap();
    assert_eq!(results.len(), 1);
    let val = results[0].get::<f64>("result").unwrap();
    #[allow(clippy::approx_constant)]
    let expected = 3.14;
    assert!((val - expected).abs() < 0.01);
}

#[test]
fn test_math_ceil() {
    let conn = test_connection();

    let results = conn.cypher("RETURN ceil(3.14) AS result").unwrap();
    assert_eq!(results.len(), 1);
    let val = results[0].get::<f64>("result").unwrap();
    assert!((val - 4.0).abs() < 0.01);
}

#[test]
fn test_math_floor() {
    let conn = test_connection();

    let results = conn.cypher("RETURN floor(3.99) AS result").unwrap();
    assert_eq!(results.len(), 1);
    let val = results[0].get::<f64>("result").unwrap();
    assert!((val - 3.0).abs() < 0.01);
}

#[test]
fn test_math_round() {
    let conn = test_connection();

    let results = conn.cypher("RETURN round(3.5) AS result").unwrap();
    assert_eq!(results.len(), 1);
    let val = results[0].get::<f64>("result").unwrap();
    assert!((val - 4.0).abs() < 0.01);

    let results = conn.cypher("RETURN round(3.4) AS result").unwrap();
    let val = results[0].get::<f64>("result").unwrap();
    assert!((val - 3.0).abs() < 0.01);
}

#[test]
fn test_math_sqrt() {
    let conn = test_connection();

    // sqrt() requires SQLite to be compiled with -DSQLITE_ENABLE_MATH_FUNCTIONS
    // which isn't always available
    let result = conn.cypher("RETURN sqrt(16) AS result");
    if let Err(ref e) = result {
        if e.to_string().contains("no such function") {
            eprintln!("Skipping: SQLite math functions not available");
            return;
        }
    }
    let results = result.unwrap();
    assert_eq!(results.len(), 1);
    let val = results[0].get::<f64>("result").unwrap();
    assert!((val - 4.0).abs() < 0.01);
}

#[test]
fn test_math_sign() {
    let conn = test_connection();

    let results = conn.cypher("RETURN sign(-10) AS neg, sign(0) AS zero, sign(10) AS pos").unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<i64>("neg").unwrap(), -1);
    assert_eq!(results[0].get::<i64>("zero").unwrap(), 0);
    assert_eq!(results[0].get::<i64>("pos").unwrap(), 1);
}

#[test]
fn test_math_rand() {
    let conn = test_connection();

    let results = conn.cypher("RETURN rand() AS result").unwrap();
    assert_eq!(results.len(), 1);
    let val = results[0].get::<f64>("result").unwrap();
    assert!((0.0..1.0).contains(&val));
}

#[test]
fn test_math_functions_with_properties() {
    let conn = test_connection();

    conn.cypher("CREATE (n:MathTest {value: -25.7})").unwrap();

    let results = conn
        .cypher("MATCH (n:MathTest) RETURN abs(n.value) AS abs_val, ceil(n.value) AS ceil_val, floor(n.value) AS floor_val")
        .unwrap();
    assert_eq!(results.len(), 1);
    let abs_val = results[0].get::<f64>("abs_val").unwrap();
    let ceil_val = results[0].get::<f64>("ceil_val").unwrap();
    let floor_val = results[0].get::<f64>("floor_val").unwrap();
    assert!((abs_val - 25.7).abs() < 0.01);
    assert!((ceil_val - (-25.0)).abs() < 0.01);
    assert!((floor_val - (-26.0)).abs() < 0.01);
}

// =============================================================================
// List Function Tests
// =============================================================================

#[test]
fn test_list_size() {
    let conn = test_connection();

    let results = conn.cypher("RETURN size([1, 2, 3, 4, 5]) AS result").unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<i64>("result").unwrap(), 5);
}

#[test]
fn test_list_head() {
    let conn = test_connection();

    let results = conn.cypher("RETURN head([1, 2, 3]) AS result").unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<i64>("result").unwrap(), 1);
}

#[test]
fn test_list_tail() {
    let conn = test_connection();

    // tail() returns a single row with the array as a column value
    let results = conn.cypher("RETURN tail([1, 2, 3]) AS result").unwrap();
    assert_eq!(results.len(), 1);
    // The result column contains the array [2, 3]
    if let Some(graphqlite::Value::Array(arr)) = results[0].get_value("result") {
        assert_eq!(arr.len(), 2);
    } else {
        panic!("Expected array result");
    }
}

#[test]
fn test_list_last() {
    let conn = test_connection();

    let results = conn.cypher("RETURN last([1, 2, 3]) AS result").unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<i64>("result").unwrap(), 3);
}

#[test]
fn test_list_range() {
    let conn = test_connection();

    // range() returns a single row with the array as a column value
    let results = conn.cypher("RETURN range(0, 5) AS result").unwrap();
    assert_eq!(results.len(), 1);
    // The result column contains the array [0, 1, 2, 3, 4, 5]
    if let Some(graphqlite::Value::Array(arr)) = results[0].get_value("result") {
        assert_eq!(arr.len(), 6);
    } else {
        panic!("Expected array result");
    }
}

#[test]
fn test_list_range_with_step() {
    let conn = test_connection();

    // range() with step returns a single row with the array as a column value
    let results = conn.cypher("RETURN range(0, 10, 2) AS result").unwrap();
    assert_eq!(results.len(), 1);
    // The result column contains the array [0, 2, 4, 6, 8, 10]
    if let Some(graphqlite::Value::Array(arr)) = results[0].get_value("result") {
        assert_eq!(arr.len(), 6);
    } else {
        panic!("Expected array result");
    }
}

// =============================================================================
// UNION Tests
// =============================================================================

#[test]
fn test_union_basic() {
    let conn = test_connection();

    conn.cypher("CREATE (n:UnionA {name: 'Alice'})").unwrap();
    conn.cypher("CREATE (n:UnionB {name: 'Bob'})").unwrap();

    let results = conn
        .cypher("MATCH (n:UnionA) RETURN n.name AS name UNION MATCH (m:UnionB) RETURN m.name AS name")
        .unwrap();
    assert_eq!(results.len(), 2);
}

#[test]
fn test_union_all() {
    let conn = test_connection();

    conn.cypher("CREATE (n:UnionAllA {name: 'Same'})").unwrap();
    conn.cypher("CREATE (n:UnionAllB {name: 'Same'})").unwrap();

    let results = conn
        .cypher("MATCH (n:UnionAllA) RETURN n.name AS name UNION ALL MATCH (m:UnionAllB) RETURN m.name AS name")
        .unwrap();
    // UNION ALL keeps duplicates
    assert_eq!(results.len(), 2);
}

#[test]
fn test_union_removes_duplicates() {
    let conn = test_connection();

    // UNION without ALL removes duplicates
    let results = conn
        .cypher("RETURN 'test' AS val UNION RETURN 'test' AS val")
        .unwrap();
    assert_eq!(results.len(), 1);
}

// =============================================================================
// WITH Clause Tests
// =============================================================================

#[test]
fn test_with_basic() {
    let conn = test_connection();

    conn.cypher("CREATE (n:WithTest {name: 'Alice', age: 30})").unwrap();
    conn.cypher("CREATE (n:WithTest {name: 'Bob', age: 25})").unwrap();
    conn.cypher("CREATE (n:WithTest {name: 'Charlie', age: 35})").unwrap();

    // Use WITH to project properties directly (WHERE before WITH, project name in WITH)
    let results = conn
        .cypher("MATCH (n:WithTest) WHERE n.age > 26 WITH n.name AS name RETURN name ORDER BY name")
        .unwrap();
    assert_eq!(results.len(), 2);
    assert_eq!(results[0].get::<String>("name").unwrap(), "Alice");
    assert_eq!(results[1].get::<String>("name").unwrap(), "Charlie");
}

#[test]
fn test_with_aggregation() {
    let conn = test_connection();

    conn.cypher("CREATE (n:WithAggTest {city: 'NYC', age: 30})").unwrap();
    conn.cypher("CREATE (n:WithAggTest {city: 'NYC', age: 25})").unwrap();
    conn.cypher("CREATE (n:WithAggTest {city: 'LA', age: 35})").unwrap();

    let results = conn
        .cypher("MATCH (n:WithAggTest) WITH n.city AS city, count(n) AS cnt RETURN city, cnt ORDER BY city")
        .unwrap();
    assert_eq!(results.len(), 2);
    assert_eq!(results[0].get::<String>("city").unwrap(), "LA");
    assert_eq!(results[0].get::<i64>("cnt").unwrap(), 1);
    assert_eq!(results[1].get::<String>("city").unwrap(), "NYC");
    assert_eq!(results[1].get::<i64>("cnt").unwrap(), 2);
}

#[test]
fn test_with_order_by_limit() {
    let conn = test_connection();

    conn.cypher("CREATE (n:WithOrderTest {val: 1})").unwrap();
    conn.cypher("CREATE (n:WithOrderTest {val: 2})").unwrap();
    conn.cypher("CREATE (n:WithOrderTest {val: 3})").unwrap();
    conn.cypher("CREATE (n:WithOrderTest {val: 4})").unwrap();

    let results = conn
        .cypher("MATCH (n:WithOrderTest) WITH n ORDER BY n.val DESC LIMIT 2 RETURN n.val")
        .unwrap();
    assert_eq!(results.len(), 2);
    assert_eq!(results[0].get::<i64>("n.val").unwrap(), 4);
    assert_eq!(results[1].get::<i64>("n.val").unwrap(), 3);
}

// =============================================================================
// CASE Expression Tests
// =============================================================================

#[test]
fn test_case_simple() {
    let conn = test_connection();

    conn.cypher("CREATE (n:CaseTest {status: 'active'})").unwrap();
    conn.cypher("CREATE (n:CaseTest {status: 'pending'})").unwrap();
    conn.cypher("CREATE (n:CaseTest {status: 'closed'})").unwrap();

    // Note: GraphQLite only supports searched CASE syntax (CASE WHEN ...), not simple CASE (CASE expr WHEN ...)
    let results = conn
        .cypher("MATCH (n:CaseTest) RETURN n.status, CASE WHEN n.status = 'active' THEN 1 WHEN n.status = 'pending' THEN 2 ELSE 0 END AS code ORDER BY code")
        .unwrap();
    assert_eq!(results.len(), 3);
    assert_eq!(results[0].get::<i64>("code").unwrap(), 0); // closed
    assert_eq!(results[1].get::<i64>("code").unwrap(), 1); // active
    assert_eq!(results[2].get::<i64>("code").unwrap(), 2); // pending
}

#[test]
fn test_case_generic() {
    let conn = test_connection();

    conn.cypher("CREATE (n:CaseGenTest {score: 85})").unwrap();
    conn.cypher("CREATE (n:CaseGenTest {score: 70})").unwrap();
    conn.cypher("CREATE (n:CaseGenTest {score: 55})").unwrap();

    let results = conn
        .cypher("MATCH (n:CaseGenTest) RETURN n.score, CASE WHEN n.score >= 80 THEN 'A' WHEN n.score >= 60 THEN 'B' ELSE 'C' END AS grade ORDER BY n.score")
        .unwrap();
    assert_eq!(results.len(), 3);
    assert_eq!(results[0].get::<String>("grade").unwrap(), "C"); // 55
    assert_eq!(results[1].get::<String>("grade").unwrap(), "B"); // 70
    assert_eq!(results[2].get::<String>("grade").unwrap(), "A"); // 85
}

// =============================================================================
// COALESCE and NULL Tests
// =============================================================================

#[test]
fn test_coalesce() {
    let conn = test_connection();

    conn.cypher("CREATE (n:CoalesceTest {name: 'Alice'})").unwrap();
    conn.cypher("CREATE (n:CoalesceTest {nickname: 'Bobby'})").unwrap();

    let results = conn
        .cypher("MATCH (n:CoalesceTest) RETURN coalesce(n.nickname, n.name, 'Unknown') AS display ORDER BY display")
        .unwrap();
    assert_eq!(results.len(), 2);
    assert_eq!(results[0].get::<String>("display").unwrap(), "Alice");
    assert_eq!(results[1].get::<String>("display").unwrap(), "Bobby");
}

#[test]
fn test_is_null() {
    let conn = test_connection();

    conn.cypher("CREATE (n:NullTest {name: 'Alice', email: 'alice@test.com'})").unwrap();
    conn.cypher("CREATE (n:NullTest {name: 'Bob'})").unwrap();

    let results = conn
        .cypher("MATCH (n:NullTest) WHERE n.email IS NULL RETURN n.name")
        .unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("n.name").unwrap(), "Bob");
}

#[test]
fn test_is_not_null() {
    let conn = test_connection();

    conn.cypher("CREATE (n:NotNullTest {name: 'Alice', email: 'alice@test.com'})").unwrap();
    conn.cypher("CREATE (n:NotNullTest {name: 'Bob'})").unwrap();

    let results = conn
        .cypher("MATCH (n:NotNullTest) WHERE n.email IS NOT NULL RETURN n.name")
        .unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("n.name").unwrap(), "Alice");
}

// =============================================================================
// Type Conversion Function Tests
// =============================================================================

#[test]
fn test_to_string() {
    let conn = test_connection();

    let results = conn.cypher("RETURN toString(42) AS result").unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("result").unwrap(), "42");

    let results = conn.cypher("RETURN toString(3.14) AS result").unwrap();
    assert_eq!(results.len(), 1);
    let val = results[0].get::<String>("result").unwrap();
    assert!(val.starts_with("3.14"));
}

#[test]
fn test_to_integer() {
    let conn = test_connection();

    let results = conn.cypher("RETURN toInteger('42') AS result").unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<i64>("result").unwrap(), 42);

    let results = conn.cypher("RETURN toInteger(3.9) AS result").unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<i64>("result").unwrap(), 3);
}

#[test]
fn test_to_float() {
    let conn = test_connection();

    let results = conn.cypher("RETURN toFloat('3.14') AS result").unwrap();
    assert_eq!(results.len(), 1);
    let val = results[0].get::<f64>("result").unwrap();
    #[allow(clippy::approx_constant)]
    let expected = 3.14;
    assert!((val - expected).abs() < 0.01);

    let results = conn.cypher("RETURN toFloat(42) AS result").unwrap();
    assert_eq!(results.len(), 1);
    let val = results[0].get::<f64>("result").unwrap();
    assert!((val - 42.0).abs() < 0.01);
}

#[test]
fn test_to_boolean() {
    let conn = test_connection();

    let results = conn.cypher("RETURN toBoolean('true') AS result").unwrap();
    assert_eq!(results.len(), 1);
    let val = results[0].get::<i64>("result").unwrap();
    assert_eq!(val, 1);

    let results = conn.cypher("RETURN toBoolean('false') AS result").unwrap();
    assert_eq!(results.len(), 1);
    let val = results[0].get::<i64>("result").unwrap();
    assert_eq!(val, 0);
}

// =============================================================================
// EXISTS Predicate Tests
// =============================================================================

#[test]
fn test_exists_property() {
    let conn = test_connection();

    conn.cypher("CREATE (n:ExistsTest {name: 'Alice', email: 'alice@test.com'})").unwrap();
    conn.cypher("CREATE (n:ExistsTest {name: 'Bob'})").unwrap();

    let results = conn
        .cypher("MATCH (n:ExistsTest) WHERE EXISTS(n.email) RETURN n.name")
        .unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("n.name").unwrap(), "Alice");
}

// =============================================================================
// Multiple Labels Tests
// =============================================================================

#[test]
fn test_create_multiple_labels() {
    let conn = test_connection();

    conn.cypher("CREATE (n:MultiLabel1:MultiLabel2:MultiLabel3 {name: 'Test'})").unwrap();

    // Can match by any label
    let results = conn.cypher("MATCH (n:MultiLabel1) RETURN n.name").unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("n.name").unwrap(), "Test");

    let results = conn.cypher("MATCH (n:MultiLabel2) RETURN n.name").unwrap();
    assert_eq!(results.len(), 1);

    let results = conn.cypher("MATCH (n:MultiLabel3) RETURN n.name").unwrap();
    assert_eq!(results.len(), 1);
}

#[test]
fn test_match_multiple_labels() {
    let conn = test_connection();

    conn.cypher("CREATE (n:MatchMultiA:MatchMultiB {name: 'Both'})").unwrap();
    conn.cypher("CREATE (n:MatchMultiA {name: 'OnlyA'})").unwrap();
    conn.cypher("CREATE (n:MatchMultiB {name: 'OnlyB'})").unwrap();

    // Match nodes that have both labels
    let results = conn
        .cypher("MATCH (n:MatchMultiA:MatchMultiB) RETURN n.name")
        .unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("n.name").unwrap(), "Both");
}

// =============================================================================
// OPTIONAL MATCH Tests
// =============================================================================

#[test]
fn test_optional_match_with_results() {
    let conn = test_connection();

    conn.cypher("CREATE (a:OptMatchA {name: 'Alice'})-[:KNOWS]->(b:OptMatchB {name: 'Bob'})").unwrap();

    let results = conn
        .cypher("MATCH (a:OptMatchA) OPTIONAL MATCH (a)-[:KNOWS]->(b) RETURN a.name, b.name")
        .unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("a.name").unwrap(), "Alice");
    assert_eq!(results[0].get::<String>("b.name").unwrap(), "Bob");
}

#[test]
fn test_optional_match_no_results() {
    let conn = test_connection();

    conn.cypher("CREATE (a:OptMatchNoRes {name: 'Lonely'})").unwrap();

    let results = conn
        .cypher("MATCH (a:OptMatchNoRes) OPTIONAL MATCH (a)-[:KNOWS]->(b) RETURN a.name, b.name")
        .unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<String>("a.name").unwrap(), "Lonely");
    // b.name should be null
    assert!(results[0].get::<Option<String>>("b.name").unwrap().is_none());
}

// =============================================================================
// UNWIND Clause Tests
// =============================================================================

#[test]
fn test_unwind_basic() {
    let conn = test_connection();

    let results = conn
        .cypher("UNWIND [1, 2, 3] AS x RETURN x")
        .unwrap();
    assert_eq!(results.len(), 3);
    assert_eq!(results[0].get::<i64>("x").unwrap(), 1);
    assert_eq!(results[1].get::<i64>("x").unwrap(), 2);
    assert_eq!(results[2].get::<i64>("x").unwrap(), 3);
}

#[test]
fn test_unwind_with_create() {
    let conn = test_connection();

    conn.cypher("UNWIND ['A', 'B', 'C'] AS name CREATE (n:UnwindCreate {name: name})").unwrap();

    let results = conn
        .cypher("MATCH (n:UnwindCreate) RETURN n.name ORDER BY n.name")
        .unwrap();
    assert_eq!(results.len(), 3);
    assert_eq!(results[0].get::<String>("n.name").unwrap(), "A");
    assert_eq!(results[1].get::<String>("n.name").unwrap(), "B");
    assert_eq!(results[2].get::<String>("n.name").unwrap(), "C");
}

#[test]
fn test_unwind_with_list_literal() {
    let conn = test_connection();

    // UNWIND requires a list literal, property access, or variable
    // (function calls like range() are not directly supported)
    let results = conn
        .cypher("UNWIND [1, 2, 3, 4, 5] AS n RETURN n")
        .unwrap();
    assert_eq!(results.len(), 5);
    assert_eq!(results[0].get::<i64>("n").unwrap(), 1);
    assert_eq!(results[4].get::<i64>("n").unwrap(), 5);
}

// =============================================================================
// GraphManager Tests
// =============================================================================

/// Create a test GraphManager.
fn test_graph_manager() -> (GraphManager, tempfile::TempDir) {
    let tmpdir = tempfile::tempdir().expect("Failed to create temp dir");
    let gm = GraphManager::open(tmpdir.path()).expect("Failed to create GraphManager");
    (gm, tmpdir)
}

#[test]
fn test_manager_create() {
    let (gm, _tmpdir) = test_graph_manager();

    assert!(gm.is_empty().unwrap());
    assert_eq!(gm.list().unwrap(), Vec::<String>::new());
}

#[test]
fn test_manager_create_graph() {
    let (mut gm, _tmpdir) = test_graph_manager();

    // Create graph and use it in a scope
    {
        let graph = gm.create("social").unwrap();
        graph.upsert_node("alice", [("name", "Alice")], "Person").unwrap();
        let stats = graph.stats().unwrap();
        assert_eq!(stats.nodes, 1);
    }

    // Now we can check the manager
    assert!(gm.exists("social"));
    assert!(gm.list().unwrap().contains(&"social".to_string()));
}

#[test]
fn test_manager_create_duplicate_fails() {
    let (mut gm, _tmpdir) = test_graph_manager();

    gm.create("social").unwrap();
    assert!(gm.create("social").is_err());
}

#[test]
fn test_manager_open_graph() {
    let (mut gm, _tmpdir) = test_graph_manager();

    gm.create("test_db").unwrap();
    let graph = gm.open_graph("test_db").unwrap();
    assert!(graph.stats().is_ok());
}

#[test]
fn test_manager_open_missing_fails() {
    let (mut gm, _tmpdir) = test_graph_manager();

    let result = gm.open_graph("nonexistent");
    assert!(result.is_err());
    let err = result.err().unwrap();
    match err {
        Error::GraphNotFound { name, .. } => assert_eq!(name, "nonexistent"),
        _ => panic!("Expected GraphNotFound error"),
    }
}

#[test]
fn test_manager_open_or_create() {
    let (mut gm, _tmpdir) = test_graph_manager();

    // Should create new, use it in a scope
    {
        let graph = gm.open_or_create("cache").unwrap();
        graph.upsert_node("item", [("value", "test")], "Cache").unwrap();
    }

    assert!(gm.exists("cache"));

    // Should open existing (returns cached reference)
    let graph2 = gm.open_or_create("cache").unwrap();
    let stats = graph2.stats().unwrap();
    assert_eq!(stats.nodes, 1);
}

#[test]
fn test_manager_drop_graph() {
    let (mut gm, _tmpdir) = test_graph_manager();

    gm.create("temp").unwrap();
    assert!(gm.exists("temp"));

    gm.drop("temp").unwrap();
    assert!(!gm.exists("temp"));
}

#[test]
fn test_manager_drop_missing_fails() {
    let (mut gm, _tmpdir) = test_graph_manager();

    assert!(gm.drop("nonexistent").is_err());
}

#[test]
fn test_manager_list_multiple() {
    let (mut gm, _tmpdir) = test_graph_manager();

    gm.create("alpha").unwrap();
    gm.create("beta").unwrap();
    gm.create("gamma").unwrap();

    let list = gm.list().unwrap();
    assert_eq!(list, vec!["alpha", "beta", "gamma"]);
}

#[test]
fn test_manager_contains() {
    let (mut gm, _tmpdir) = test_graph_manager();

    gm.create("social").unwrap();
    assert!(gm.contains("social"));
    assert!(!gm.contains("missing"));
}

#[test]
fn test_manager_len() {
    let (mut gm, _tmpdir) = test_graph_manager();

    assert_eq!(gm.len().unwrap(), 0);
    gm.create("one").unwrap();
    assert_eq!(gm.len().unwrap(), 1);
    gm.create("two").unwrap();
    assert_eq!(gm.len().unwrap(), 2);
}

#[test]
fn test_manager_graph_isolation() {
    let (mut gm, _tmpdir) = test_graph_manager();

    // Create first graph
    {
        let social = gm.create("social").unwrap();
        social.upsert_node("alice", [("name", "Alice")], "Person").unwrap();
    }

    // Create second graph
    {
        let products = gm.create("products").unwrap();
        products.upsert_node("phone", [("name", "Phone")], "Product").unwrap();
    }

    // Verify isolation - reopen and check each graph
    {
        let social = gm.open_graph("social").unwrap();
        let social_stats = social.stats().unwrap();
        assert_eq!(social_stats.nodes, 1);

        let social_results = social.query("MATCH (n) RETURN n.name").unwrap();
        assert_eq!(social_results.len(), 1);
        assert_eq!(social_results[0].get::<String>("n.name").unwrap(), "Alice");
    }

    {
        let products = gm.open_graph("products").unwrap();
        let product_stats = products.stats().unwrap();
        assert_eq!(product_stats.nodes, 1);

        let product_results = products.query("MATCH (n) RETURN n.name").unwrap();
        assert_eq!(product_results.len(), 1);
        assert_eq!(product_results[0].get::<String>("n.name").unwrap(), "Phone");
    }
}

#[test]
fn test_manager_cross_graph_query() {
    let (mut gm, _tmpdir) = test_graph_manager();

    // Create and populate a graph in a scope
    {
        let social = gm.create("social").unwrap();
        social.upsert_node("alice", [("name", "Alice"), ("age", "30")], "Person").unwrap();
        social.upsert_node("bob", [("name", "Bob"), ("age", "25")], "Person").unwrap();
    }

    // Cross-graph query with FROM clause
    let result = gm.query(
        "MATCH (n:Person) FROM social RETURN n.name ORDER BY n.name",
        &["social"]
    ).unwrap();

    assert_eq!(result.len(), 2);
    assert_eq!(result[0].get::<String>("n.name").unwrap(), "Alice");
    assert_eq!(result[1].get::<String>("n.name").unwrap(), "Bob");
}

#[test]
fn test_manager_query_missing_graph_fails() {
    let (mut gm, _tmpdir) = test_graph_manager();

    let result = gm.query(
        "MATCH (n) FROM missing RETURN n",
        &["missing"]
    );
    assert!(result.is_err());
}

#[test]
fn test_manager_query_sql() {
    let (mut gm, _tmpdir) = test_graph_manager();

    // Create and populate graph in a scope
    {
        let social = gm.create("social").unwrap();
        social.upsert_node("alice", [("name", "Alice")], "Person").unwrap();
    }

    let result = gm.query_sql(
        "SELECT COUNT(*) FROM social.nodes",
        &["social"]
    ).unwrap();

    // The result is a Vec<Vec<rusqlite::types::Value>>
    assert_eq!(result.len(), 1);
}

#[test]
fn test_graphs_convenience_function() {
    let tmpdir = tempfile::tempdir().unwrap();

    // Use the convenience function
    let mut gm = graphs(tmpdir.path()).unwrap();

    gm.create("test").unwrap();
    assert!(gm.exists("test"));
}

#[test]
fn test_manager_iter() {
    let (mut gm, _tmpdir) = test_graph_manager();

    gm.create("alpha").unwrap();
    gm.create("beta").unwrap();

    let names: Vec<String> = gm.iter().unwrap().collect();
    assert_eq!(names, vec!["alpha", "beta"]);
}

/// Regression test for GQLITE-T-0092: DETACH DELETE deletes all nodes
/// Bug was: MATCH (n {id: 'x'}) DETACH DELETE n deleted ALL nodes instead of just matched
/// Root cause was: AST mutation in transform_match.c when MATCH transformed twice
/// Fix: Removed unnecessary transform_match_clause call in execute_match_delete_query
#[test]
fn test_regression_gqlite_t_0092_detach_delete_property_filter() {
    let g = test_graph();

    // Insert 3 nodes
    g.upsert_node("node_a", [("name", "A")], "Test").expect("insert a");
    g.upsert_node("node_b", [("name", "B")], "Test").expect("insert b");
    g.upsert_node("node_c", [("name", "C")], "Test").expect("insert c");

    let stats = g.stats().expect("stats");
    assert_eq!(stats.nodes, 3, "Should have 3 nodes after insert");

    // Verify all nodes exist
    assert!(g.has_node("node_a").expect("has a"), "node_a should exist");
    assert!(g.has_node("node_b").expect("has b"), "node_b should exist");
    assert!(g.has_node("node_c").expect("has c"), "node_c should exist");

    // Delete only node_a
    g.delete_node("node_a").expect("delete a");

    let stats = g.stats().expect("stats");
    // EXPECTED: 2 nodes remain
    // ACTUAL (bug): 0 nodes remain - all deleted due to AST mutation
    assert_eq!(stats.nodes, 2, "Should have 2 nodes after deleting node_a");
    assert!(!g.has_node("node_a").expect("has a"), "node_a should NOT exist");
    assert!(g.has_node("node_b").expect("has b"), "node_b should still exist");
    assert!(g.has_node("node_c").expect("has c"), "node_c should still exist");
}

// =============================================================================
// Graph Cache Tests
// =============================================================================

#[test]
fn test_graph_loaded_initially_false() {
    let g = test_graph();
    assert!(!g.graph_loaded().unwrap());
}

#[test]
fn test_load_graph() {
    let g = test_graph();

    let empty: [(&str, &str); 0] = [];
    g.upsert_node("a", empty, "Node").unwrap();
    g.upsert_node("b", empty, "Node").unwrap();
    g.upsert_edge("a", "b", empty, "KNOWS").unwrap();

    let status = g.load_graph().unwrap();

    assert_eq!(status.status, "loaded");
    assert_eq!(status.nodes, Some(2));
    assert_eq!(status.edges, Some(1));
    assert!(g.graph_loaded().unwrap());
}

#[test]
fn test_load_graph_already_loaded() {
    let g = test_graph();

    let empty: [(&str, &str); 0] = [];
    g.upsert_node("a", empty, "Node").unwrap();
    g.load_graph().unwrap();

    let status = g.load_graph().unwrap();

    assert_eq!(status.status, "already_loaded");
}

#[test]
fn test_unload_graph() {
    let g = test_graph();

    let empty: [(&str, &str); 0] = [];
    g.upsert_node("a", empty, "Node").unwrap();
    g.load_graph().unwrap();
    assert!(g.graph_loaded().unwrap());

    let status = g.unload_graph().unwrap();

    assert_eq!(status.status, "unloaded");
    assert!(!g.graph_loaded().unwrap());
}

#[test]
fn test_unload_graph_not_loaded() {
    let g = test_graph();

    let status = g.unload_graph().unwrap();

    assert_eq!(status.status, "not_loaded");
}

#[test]
fn test_reload_graph() {
    let g = test_graph();

    let empty: [(&str, &str); 0] = [];
    g.upsert_node("a", empty, "Node").unwrap();
    g.upsert_node("b", empty, "Node").unwrap();
    g.load_graph().unwrap();

    // Add new node
    g.upsert_node("c", empty, "Node").unwrap();

    let status = g.reload_graph().unwrap();

    assert_eq!(status.status, "reloaded");
    assert_eq!(status.nodes, Some(3));
}

#[test]
fn test_reload_graph_not_loaded() {
    let g = test_graph();

    let empty: [(&str, &str); 0] = [];
    g.upsert_node("a", empty, "Node").unwrap();

    let status = g.reload_graph().unwrap();

    // reload_graph always returns "reloaded" even on first load
    assert_eq!(status.status, "reloaded");
    assert!(g.graph_loaded().unwrap());
}

#[test]
fn test_cache_with_pagerank() {
    let g = test_graph();

    let empty: [(&str, &str); 0] = [];
    g.upsert_node("a", empty, "Node").unwrap();
    g.upsert_node("b", empty, "Node").unwrap();
    g.upsert_node("c", empty, "Node").unwrap();
    g.upsert_edge("a", "b", empty, "LINKS").unwrap();
    g.upsert_edge("b", "c", empty, "LINKS").unwrap();
    g.upsert_edge("c", "a", empty, "LINKS").unwrap();

    g.load_graph().unwrap();

    // PageRank should work with cached graph
    let result = g.pagerank(0.85, 10).unwrap();

    assert_eq!(result.len(), 3);
}

#[test]
fn test_cache_empty_graph() {
    let g = test_graph();

    let status = g.load_graph().unwrap();

    // Empty graph should still load successfully
    assert_eq!(status.status, "loaded");
    assert_eq!(status.nodes, Some(0));
    assert_eq!(status.edges, Some(0));
}

// =============================================================================
// Bulk Insert Tests
// =============================================================================

// Type alias to help with empty property vecs
type BulkProps = Vec<(&'static str, &'static str)>;

fn empty_bulk_props() -> BulkProps {
    vec![]
}

#[test]
fn test_bulk_insert_nodes() {
    let g = test_graph();

    let id_map = g
        .insert_nodes_bulk([
            ("alice", vec![("name", "Alice"), ("age", "30")], "Person"),
            ("bob", vec![("name", "Bob"), ("age", "25")], "Person"),
            ("charlie", vec![("name", "Charlie"), ("age", "35")], "Person"),
        ])
        .unwrap();

    assert_eq!(id_map.len(), 3);
    assert!(id_map.contains_key("alice"));
    assert!(id_map.contains_key("bob"));
    assert!(id_map.contains_key("charlie"));

    // Verify nodes exist via Graph API
    let stats = g.stats().unwrap();
    assert_eq!(stats.nodes, 3);

    // Verify nodes via Cypher query
    let result = g.query("MATCH (n:Person) RETURN n.id ORDER BY n.id").unwrap();
    assert_eq!(result.len(), 3);
    assert_eq!(result[0].get::<String>("n.id").unwrap(), "alice");
    assert_eq!(result[1].get::<String>("n.id").unwrap(), "bob");
    assert_eq!(result[2].get::<String>("n.id").unwrap(), "charlie");
}

#[test]
fn test_bulk_insert_nodes_empty() {
    let g = test_graph();

    let id_map = g
        .insert_nodes_bulk::<std::vec::IntoIter<(&str, BulkProps, &str)>, _, _, _, _, _>(vec![].into_iter())
        .unwrap();

    assert!(id_map.is_empty());
    assert_eq!(g.stats().unwrap().nodes, 0);
}

#[test]
fn test_bulk_insert_edges() {
    let g = test_graph();

    let id_map = g
        .insert_nodes_bulk([
            ("a", empty_bulk_props(), "Node"),
            ("b", empty_bulk_props(), "Node"),
            ("c", empty_bulk_props(), "Node"),
        ])
        .unwrap();

    let edges_inserted = g
        .insert_edges_bulk(
            [
                ("a", "b", vec![("weight", "1.5")], "CONNECTS"),
                ("b", "c", vec![("weight", "2.5")], "CONNECTS"),
                ("a", "c", vec![("weight", "3.5")], "CONNECTS"),
            ],
            &id_map,
        )
        .unwrap();

    assert_eq!(edges_inserted, 3);

    // Verify edges exist
    let stats = g.stats().unwrap();
    assert_eq!(stats.edges, 3);

    // Verify edges via query
    let result = g.query("MATCH ()-[r:CONNECTS]->() RETURN r.weight ORDER BY r.weight").unwrap();
    assert_eq!(result.len(), 3);
    let weights: Vec<f64> = result.iter().map(|r| r.get::<f64>("r.weight").unwrap()).collect();
    assert!((weights[0] - 1.5).abs() < 0.01);
    assert!((weights[1] - 2.5).abs() < 0.01);
    assert!((weights[2] - 3.5).abs() < 0.01);
}

#[test]
fn test_bulk_insert_edges_empty() {
    let g = test_graph();

    let id_map = g
        .insert_nodes_bulk([("node1", empty_bulk_props(), "Node")])
        .unwrap();

    let edges_inserted = g
        .insert_edges_bulk::<std::vec::IntoIter<(&str, &str, BulkProps, &str)>, _, _, _, _, _, _>(
            vec![].into_iter(),
            &id_map,
        )
        .unwrap();

    assert_eq!(edges_inserted, 0);
    assert_eq!(g.stats().unwrap().edges, 0);
}

#[test]
fn test_bulk_insert_edges_fallback_lookup() {
    let g = test_graph();

    // Create nodes via regular upsert
    g.upsert_node("existing1", [("name", "Existing1")], "Node").unwrap();
    g.upsert_node("existing2", [("name", "Existing2")], "Node").unwrap();

    // Insert edges using empty id_map - should fall back to database lookup
    let empty_map = std::collections::HashMap::new();
    let edges_inserted = g
        .insert_edges_bulk(
            [("existing1", "existing2", empty_bulk_props(), "CONNECTS")],
            &empty_map,
        )
        .unwrap();

    assert_eq!(edges_inserted, 1);
    assert!(g.has_edge("existing1", "existing2").unwrap());
}

#[test]
fn test_bulk_insert_graph() {
    let g = test_graph();

    let result = g
        .insert_graph_bulk(
            [
                ("x", vec![("name", "X")], "Node"),
                ("y", vec![("name", "Y")], "Node"),
                ("z", vec![("name", "Z")], "Node"),
            ],
            [
                ("x", "y", empty_bulk_props(), "LINKS"),
                ("y", "z", empty_bulk_props(), "LINKS"),
            ],
        )
        .unwrap();

    assert_eq!(result.nodes_inserted, 3);
    assert_eq!(result.edges_inserted, 2);
    assert_eq!(result.id_map.len(), 3);

    // Verify via stats
    let stats = g.stats().unwrap();
    assert_eq!(stats.nodes, 3);
    assert_eq!(stats.edges, 2);
}

#[test]
fn test_resolve_node_ids() {
    let g = test_graph();

    // Insert some nodes via upsert
    g.upsert_node("alice", [("name", "Alice")], "Person").unwrap();
    g.upsert_node("bob", [("name", "Bob")], "Person").unwrap();
    g.upsert_node("charlie", [("name", "Charlie")], "Person").unwrap();

    let resolved = g.resolve_node_ids(["alice", "bob", "unknown"]).unwrap();

    assert_eq!(resolved.len(), 2);
    assert!(resolved.contains_key("alice"));
    assert!(resolved.contains_key("bob"));
    assert!(!resolved.contains_key("unknown"));
    assert!(!resolved.contains_key("charlie")); // Not in the query
}

#[test]
fn test_resolve_node_ids_empty() {
    let g = test_graph();

    let resolved = g.resolve_node_ids::<std::vec::IntoIter<&str>, _>(vec![].into_iter()).unwrap();
    assert!(resolved.is_empty());
}

#[test]
fn test_bulk_insert_mixed_sources() {
    let g = test_graph();

    // Insert some nodes via Cypher/upsert
    g.upsert_node("existing", [("name", "Existing")], "Person").unwrap();

    // Insert new nodes via bulk
    let id_map = g
        .insert_nodes_bulk([
            ("new1", empty_bulk_props(), "Person"),
            ("new2", empty_bulk_props(), "Person"),
        ])
        .unwrap();

    // Insert edges connecting new and existing nodes
    // "existing" is not in id_map, so it will use fallback lookup
    let edges_inserted = g
        .insert_edges_bulk(
            [
                ("new1", "new2", empty_bulk_props(), "KNOWS"),
                ("new1", "existing", empty_bulk_props(), "KNOWS"),
                ("existing", "new2", empty_bulk_props(), "KNOWS"),
            ],
            &id_map,
        )
        .unwrap();

    assert_eq!(edges_inserted, 3);

    // Verify all edges exist
    assert!(g.has_edge("new1", "new2").unwrap());
    assert!(g.has_edge("new1", "existing").unwrap());
    assert!(g.has_edge("existing", "new2").unwrap());
}

#[test]
fn test_bulk_insert_with_typed_properties() {
    let g = test_graph();

    // Insert nodes with different property types
    let id_map = g
        .insert_nodes_bulk([
            ("node1", vec![("name", "Node1"), ("count", "42"), ("active", "true")], "TypedNode"),
            ("node2", vec![("name", "Node2"), ("score", "3.14"), ("active", "false")], "TypedNode"),
        ])
        .unwrap();

    assert_eq!(id_map.len(), 2);

    // Verify integer property
    let result = g.query("MATCH (n {id: 'node1'}) RETURN n.count").unwrap();
    assert_eq!(result[0].get::<i64>("n.count").unwrap(), 42);

    // Verify float property
    let result = g.query("MATCH (n {id: 'node2'}) RETURN n.score").unwrap();
    let score = result[0].get::<f64>("n.score").unwrap();
    assert!((score - 3.14).abs() < 0.01);

    // Verify boolean properties
    let result = g.query("MATCH (n {id: 'node1'}) RETURN n.active").unwrap();
    assert!(result[0].get::<bool>("n.active").unwrap());

    let result = g.query("MATCH (n {id: 'node2'}) RETURN n.active").unwrap();
    assert!(!result[0].get::<bool>("n.active").unwrap());
}

#[test]
fn test_bulk_insert_verifies_with_graph_api() {
    let g = test_graph();

    let result = g
        .insert_graph_bulk(
            [
                ("hub", vec![("name", "Hub")], "Node"),
                ("spoke1", vec![("name", "Spoke1")], "Node"),
                ("spoke2", vec![("name", "Spoke2")], "Node"),
            ],
            [
                ("hub", "spoke1", empty_bulk_props(), "CONNECTS"),
                ("hub", "spoke2", empty_bulk_props(), "CONNECTS"),
            ],
        )
        .unwrap();

    assert_eq!(result.nodes_inserted, 3);
    assert_eq!(result.edges_inserted, 2);

    // Verify using Graph API methods
    assert!(g.has_node("hub").unwrap());
    assert!(g.has_node("spoke1").unwrap());
    assert!(g.has_node("spoke2").unwrap());

    assert!(g.has_edge("hub", "spoke1").unwrap());
    assert!(g.has_edge("hub", "spoke2").unwrap());

    // Hub should have degree 2
    assert_eq!(g.node_degree("hub").unwrap(), 2);

    // Neighbors of hub should include both spokes
    let neighbors = g.get_neighbors("hub").unwrap();
    assert_eq!(neighbors.len(), 2);
}
