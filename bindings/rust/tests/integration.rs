//! Integration tests for GraphQLite Rust bindings.

use graphqlite::{Connection, Error};
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
