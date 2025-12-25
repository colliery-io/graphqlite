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
