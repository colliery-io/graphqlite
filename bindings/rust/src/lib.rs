//! GraphQLite - SQLite extension for graph queries using Cypher
//!
//! This crate provides Rust bindings for GraphQLite, allowing you to use
//! Cypher graph queries in SQLite databases.
//!
//! # High-Level Graph API
//!
//! The [`Graph`] struct provides an ergonomic interface for common graph operations:
//!
//! ```no_run
//! use graphqlite::Graph;
//!
//! let g = Graph::open(":memory:")?;
//!
//! // Add nodes
//! g.upsert_node("alice", [("name", "Alice"), ("age", "30")], "Person")?;
//! g.upsert_node("bob", [("name", "Bob"), ("age", "25")], "Person")?;
//!
//! // Add edge
//! g.upsert_edge("alice", "bob", [("since", "2020")], "KNOWS")?;
//!
//! // Query
//! println!("{:?}", g.stats()?);
//! println!("{:?}", g.get_neighbors("alice")?);
//!
//! // Graph algorithms
//! let ranks = g.pagerank(0.85, 20)?;
//! let communities = g.community_detection(10)?;
//! # Ok::<(), graphqlite::Error>(())
//! ```
//!
//! # Low-Level Cypher API
//!
//! The [`Connection`] struct provides direct Cypher query access:
//!
//! ```no_run
//! use graphqlite::Connection;
//!
//! let conn = Connection::open(":memory:")?;
//!
//! // Create nodes
//! conn.cypher("CREATE (n:Person {name: 'Alice', age: 30})")?;
//!
//! // Query the graph
//! let results = conn.cypher("MATCH (n:Person) RETURN n.name, n.age")?;
//! for row in &results {
//!     println!("{}: {}", row.get::<String>("n.name")?, row.get::<i64>("n.age")?);
//! }
//! # Ok::<(), graphqlite::Error>(())
//! ```

mod algorithms;
mod connection;
mod error;
mod graph;
mod manager;
#[cfg(feature = "bundled-extension")]
mod platform;
mod result;
mod utils;

pub use connection::Connection;
pub use error::Error;
pub use graph::{graph, BulkInsertResult, CacheStatus, Graph, GraphStats};
pub use manager::{graphs, GraphManager};
pub use result::{CypherResult, Row, Value};
pub use utils::{escape_string, format_value, sanitize_rel_type, CYPHER_RESERVED};

// Algorithm result types
pub use algorithms::{
    ApspResult, AStarResult, BetweennessCentralityResult, ClosenessCentralityResult,
    CommunityResult, ComponentResult, DegreeCentralityResult, EigenvectorCentralityResult,
    KnnResult, NodeSimilarityResult, PageRankResult, ShortestPathResult, TraversalResult,
    TriangleCountResult,
};

/// Result type for GraphQLite operations.
pub type Result<T> = std::result::Result<T, Error>;
