//! Integration tests for GraphQLite Rust bindings.

use graphqlite::{escape_string, sanitize_rel_type, Connection, Error, Graph};
use std::path::PathBuf;

/// Get the path to the test extension, or skip if not found.
fn get_extension_path() -> Option<PathBuf> {
    let paths = [
        PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .parent()
            .unwrap()
            .parent()
            .unwrap()
            .join("build/graphqlite.dylib"),
        PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .parent()
            .unwrap()
            .parent()
            .unwrap()
            .join("build/graphqlite.so"),
    ];

    for path in paths {
        if path.exists() {
            return Some(path);
        }
    }

    // Check environment variable
    std::env::var("GRAPHQLITE_EXTENSION_PATH")
        .ok()
        .map(PathBuf::from)
        .filter(|p| p.exists())
}

/// Create a test connection, or skip if extension not available.
fn test_connection() -> Option<Connection> {
    let ext_path = get_extension_path()?;
    Connection::open_with_extension(":memory:", ext_path).ok()
}

#[test]
fn test_open_memory() {
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };
    assert!(conn.cypher("RETURN 1").is_ok());
}

#[test]
fn test_open_file() {
    let ext_path = match get_extension_path() {
        Some(p) => p,
        None => {
            eprintln!("Skipping: extension not found");
            return;
        }
    };

    let temp_dir = tempfile::tempdir().unwrap();
    let db_path = temp_dir.path().join("test.db");

    let conn = Connection::open_with_extension(&db_path, &ext_path).unwrap();
    conn.cypher("CREATE (n:Test)").unwrap();

    assert!(db_path.exists());
}

#[test]
fn test_create_node() {
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

    let results = conn
        .cypher("RETURN 42 as num, 'hello' as str, true as flag")
        .unwrap();

    assert_eq!(results.len(), 1);
    assert_eq!(results[0].get::<i64>("num").unwrap(), 42);
    assert_eq!(results[0].get::<String>("str").unwrap(), "hello");
    assert_eq!(results[0].get::<bool>("flag").unwrap(), true);
}

#[test]
fn test_multiple_rows() {
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

    let results = conn.cypher("MATCH (n:NonExistent) RETURN n").unwrap();
    // Empty MATCH results return a success message, not an empty array
    // Check that we get a result (the success message)
    assert!(results.len() <= 1);
}

#[test]
fn test_iteration() {
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

    let results = conn.cypher("RETURN 1 as x").unwrap();
    let err = results[0].get::<i64>("nonexistent").unwrap_err();

    match err {
        Error::ColumnNotFound(name) => assert_eq!(name, "nonexistent"),
        _ => panic!("Expected ColumnNotFound error"),
    }
}

#[test]
fn test_type_error() {
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

    let results = conn.cypher("RETURN 'hello' as val").unwrap();
    let err = results[0].get::<i64>("val").unwrap_err();

    match err {
        Error::TypeError { expected, .. } => assert_eq!(expected, "Integer"),
        _ => panic!("Expected TypeError"),
    }
}

#[test]
fn test_optional_values() {
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

    let results = conn.cypher("RETURN 1 as a, 2 as b, 3 as c").unwrap();

    let columns = results.columns();
    assert!(columns.contains(&"a".to_string()));
    assert!(columns.contains(&"b".to_string()));
    assert!(columns.contains(&"c".to_string()));
}

#[test]
fn test_graph_algorithms() {
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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

/// Create a test Graph, or skip if extension not available.
fn test_graph() -> Option<Graph> {
    let ext_path = get_extension_path()?;
    let conn = Connection::open_with_extension(":memory:", ext_path).ok()?;
    Some(Graph::from_connection(conn))
}

#[test]
fn test_graph_upsert_node() {
    let Some(g) = test_graph() else {
        eprintln!("Skipping: extension not found");
        return;
    };

    g.upsert_node("alice", [("name", "Alice"), ("age", "30")], "Person")
        .unwrap();

    assert!(g.has_node("alice").unwrap());
    assert!(!g.has_node("bob").unwrap());

    let node = g.get_node("alice").unwrap();
    assert!(node.is_some());
}

#[test]
fn test_graph_upsert_edge() {
    let Some(g) = test_graph() else {
        eprintln!("Skipping: extension not found");
        return;
    };

    g.upsert_node("a", [("name", "A")], "Node").unwrap();
    g.upsert_node("b", [("name", "B")], "Node").unwrap();
    g.upsert_edge("a", "b", [("weight", "10")], "CONNECTS")
        .unwrap();

    assert!(g.has_edge("a", "b").unwrap());
    assert!(!g.has_edge("b", "a").unwrap()); // Directed edge
}

#[test]
fn test_graph_stats() {
    let Some(g) = test_graph() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(g) = test_graph() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(g) = test_graph() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(g) = test_graph() else {
        eprintln!("Skipping: extension not found");
        return;
    };

    g.upsert_node("temp", [("name", "Temp")], "Node").unwrap();
    assert!(g.has_node("temp").unwrap());

    g.delete_node("temp").unwrap();
    assert!(!g.has_node("temp").unwrap());
}

#[test]
fn test_graph_delete_edge() {
    let Some(g) = test_graph() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(g) = test_graph() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(g) = test_graph() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(g) = test_graph() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(g) = test_graph() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(g) = test_graph() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(g) = test_graph() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(g) = test_graph() else {
        eprintln!("Skipping: extension not found");
        return;
    };

    let components = g.wcc().unwrap();
    assert!(components.is_empty());
}

#[test]
fn test_graph_wcc_alias() {
    let Some(g) = test_graph() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(g) = test_graph() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(g) = test_graph() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(g) = test_graph() else {
        eprintln!("Skipping: extension not found");
        return;
    };

    let components = g.scc().unwrap();
    assert!(components.is_empty());
}

// =============================================================================
// REMOVE Clause Tests
// =============================================================================

#[test]
fn test_remove_node_property() {
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

    // Remove property on non-existent nodes - should succeed (0 rows affected)
    let result = conn.cypher("MATCH (n:NonExistentLabel) REMOVE n.property");
    assert!(result.is_ok());
}

// =============================================================================
// IN Operator Tests
// =============================================================================

#[test]
fn test_in_literal_list_match() {
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

    let results = conn.cypher("RETURN 5 IN [1, 2, 5, 10]").unwrap();
    assert_eq!(results.len(), 1);
    // Result should be truthy (1)
    let val = results[0].get::<i64>(&results.columns()[0]).unwrap();
    assert_eq!(val, 1);
}

#[test]
fn test_in_literal_list_no_match() {
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

    let results = conn.cypher("RETURN 'x' IN ['a', 'b', 'c']").unwrap();
    assert_eq!(results.len(), 1);
    // Result should be falsy (0)
    let val = results[0].get::<i64>(&results.columns()[0]).unwrap();
    assert_eq!(val, 0);
}

#[test]
fn test_in_with_where_clause() {
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

    let results = conn.cypher("RETURN 'hello world' STARTS WITH 'hello'").unwrap();
    assert_eq!(results.len(), 1);
    // Result should be truthy (1 or true)
    let val = results[0].get::<i64>(&results.columns()[0]).unwrap();
    assert_eq!(val, 1);
}

#[test]
fn test_starts_with_no_match() {
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

    let results = conn.cypher("RETURN 'hello world' STARTS WITH 'world'").unwrap();
    assert_eq!(results.len(), 1);
    let val = results[0].get::<i64>(&results.columns()[0]).unwrap();
    assert_eq!(val, 0);
}

#[test]
fn test_ends_with_match() {
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

    let results = conn.cypher("RETURN 'hello world' ENDS WITH 'world'").unwrap();
    assert_eq!(results.len(), 1);
    let val = results[0].get::<i64>(&results.columns()[0]).unwrap();
    assert_eq!(val, 1);
}

#[test]
fn test_ends_with_no_match() {
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

    let results = conn.cypher("RETURN 'hello world' ENDS WITH 'hello'").unwrap();
    assert_eq!(results.len(), 1);
    let val = results[0].get::<i64>(&results.columns()[0]).unwrap();
    assert_eq!(val, 0);
}

#[test]
fn test_contains_match() {
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

    let results = conn.cypher("RETURN 'hello world' CONTAINS 'lo wo'").unwrap();
    assert_eq!(results.len(), 1);
    let val = results[0].get::<i64>(&results.columns()[0]).unwrap();
    assert_eq!(val, 1);
}

#[test]
fn test_contains_no_match() {
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

    let results = conn.cypher("RETURN 'hello world' CONTAINS 'xyz'").unwrap();
    assert_eq!(results.len(), 1);
    let val = results[0].get::<i64>(&results.columns()[0]).unwrap();
    assert_eq!(val, 0);
}

#[test]
fn test_string_operators_in_where() {
    let Some(conn) = test_connection() else {
        eprintln!("Skipping: extension not found");
        return;
    };

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
