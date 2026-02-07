//! High-level graph operations for GraphQLite.
//!
//! The `Graph` struct provides an ergonomic interface for common graph operations.

mod batch;
mod bulk;
mod edges;
mod nodes;
mod queries;

pub use bulk::BulkInsertResult;

use crate::{Connection, CypherResult, Result};
use crate::query_builder::CypherQuery;
use serde::{Deserialize, Serialize};
use std::path::Path;

/// Graph statistics containing node and edge counts.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GraphStats {
    /// Total number of nodes in the graph.
    pub nodes: i64,
    /// Total number of edges in the graph.
    pub edges: i64,
}

/// High-level graph operations.
///
/// Provides ergonomic node/edge CRUD, graph queries, and algorithm wrappers
/// on top of the raw Cypher interface.
pub struct Graph {
    conn: Connection,
}

impl Graph {
    /// Open a graph database.
    ///
    /// # Arguments
    ///
    /// * `path` - Path to database file, or ":memory:" for in-memory
    pub fn open<P: AsRef<Path>>(path: P) -> Result<Self> {
        let conn = Connection::open(path)?;
        Ok(Graph { conn })
    }

    /// Open a graph database with a custom extension path.
    ///
    /// # Arguments
    ///
    /// * `path` - Path to database file
    /// * `extension_path` - Path to the GraphQLite extension
    ///
    /// Note: This method is only available when the `bundled-extension` feature is disabled.
    #[cfg(not(feature = "bundled-extension"))]
    pub fn open_with_extension<P: AsRef<Path>, E: AsRef<Path>>(
        path: P,
        extension_path: E,
    ) -> Result<Self> {
        let conn = Connection::open_with_extension(path, extension_path)?;
        Ok(Graph { conn })
    }

    /// Open an in-memory graph database.
    pub fn open_in_memory() -> Result<Self> {
        let conn = Connection::open_in_memory()?;
        Ok(Graph { conn })
    }

    /// Create a Graph from an existing [`Connection`].
    pub fn from_connection(conn: Connection) -> Self {
        Graph { conn }
    }

    /// Access the underlying Connection.
    pub fn connection(&self) -> &Connection {
        &self.conn
    }

    /// Execute a raw Cypher query.
    pub fn query(&self, cypher: &str) -> Result<CypherResult> {
        self.conn.cypher(cypher)
    }

    /// Execute a raw Cypher query with named parameters.
    ///
    /// # Arguments
    ///
    /// * `cypher` - Cypher query string with `$param` placeholders
    /// * `params` - Parameter values as a `serde_json::Value` (must be an object)
    #[deprecated(since = "0.4.0", note = "Use query_builder() instead")]
    pub fn query_with_params(&self, cypher: &str, params: &serde_json::Value) -> Result<CypherResult> {
        self.conn.execute_cypher_with_params(cypher, params)
    }

    /// Create a builder for a parameterized Cypher query.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open_in_memory()?;
    /// g.query("CREATE (n:Person {name: 'Alice'})")?;
    /// let results = g.query_builder("MATCH (n:Person) WHERE n.name = $name RETURN n")
    ///     .param("name", "Alice")
    ///     .run()?;
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn query_builder<'a>(&'a self, cypher: &'a str) -> CypherQuery<'a> {
        self.conn.cypher_builder(cypher)
    }

    // Cache management methods for algorithm acceleration

    /// Load the graph into an in-memory CSR cache for fast algorithm execution.
    ///
    /// When the cache is loaded, graph algorithms run ~28x faster by avoiding
    /// repeated SQLite I/O. The cache persists until explicitly unloaded or
    /// the connection is closed.
    ///
    /// # Returns
    ///
    /// A `CacheStatus` with the cache status and graph statistics.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open_in_memory()?;
    /// g.query("CREATE (:Person {id: 'alice'})-[:KNOWS]->(:Person {id: 'bob'})")?;
    /// let status = g.load_graph()?;
    /// assert_eq!(status.status, "loaded");
    /// // Now pagerank() will run ~28x faster
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn load_graph(&self) -> Result<CacheStatus> {
        let json: String = self.conn.sqlite_connection()
            .query_row("SELECT gql_load_graph()", [], |row| row.get(0))?;
        let status: CacheStatus = serde_json::from_str(&json)?;
        Ok(status)
    }

    /// Free the cached graph from memory.
    ///
    /// Call this after algorithm execution to reclaim memory, or when the
    /// graph has been modified and you want to invalidate the cache.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open_in_memory()?;
    /// g.load_graph()?;
    /// // ... run algorithms ...
    /// let status = g.unload_graph()?;
    /// assert_eq!(status.status, "unloaded");
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn unload_graph(&self) -> Result<CacheStatus> {
        let json: String = self.conn.sqlite_connection()
            .query_row("SELECT gql_unload_graph()", [], |row| row.get(0))?;
        let status: CacheStatus = serde_json::from_str(&json)?;
        Ok(status)
    }

    /// Reload the graph cache with the latest data.
    ///
    /// Use this after modifying the graph (adding/removing nodes/edges)
    /// to refresh the cache with the current state.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open_in_memory()?;
    /// g.load_graph()?;
    /// g.query("CREATE (:Person {id: 'charlie'})")?;  // Graph modified
    /// let status = g.reload_graph()?;  // Refresh cache
    /// assert_eq!(status.status, "reloaded");
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn reload_graph(&self) -> Result<CacheStatus> {
        let json: String = self.conn.sqlite_connection()
            .query_row("SELECT gql_reload_graph()", [], |row| row.get(0))?;
        let status: CacheStatus = serde_json::from_str(&json)?;
        Ok(status)
    }

    /// Check if the graph cache is currently loaded.
    ///
    /// # Returns
    ///
    /// `true` if the cache is loaded, `false` otherwise.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open_in_memory()?;
    /// assert!(!g.graph_loaded()?);
    /// g.load_graph()?;
    /// assert!(g.graph_loaded()?);
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn graph_loaded(&self) -> Result<bool> {
        let json: String = self.conn.sqlite_connection()
            .query_row("SELECT gql_graph_loaded()", [], |row| row.get(0))?;
        let status: CacheLoadedStatus = serde_json::from_str(&json)?;
        Ok(status.loaded)
    }
}

/// Cache operation status returned by load/unload/reload operations.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CacheStatus {
    /// Operation status: "loaded", "unloaded", "reloaded", or "already_loaded"
    pub status: String,
    /// Number of nodes in the cached graph (if loaded)
    #[serde(default)]
    pub nodes: Option<i64>,
    /// Number of edges in the cached graph (if loaded)
    #[serde(default)]
    pub edges: Option<i64>,
}

/// Response from graph_loaded() query.
#[derive(Debug, Clone, Deserialize)]
struct CacheLoadedStatus {
    loaded: bool,
}

/// Create a new Graph instance (convenience function).
pub fn graph<P: AsRef<Path>>(path: P) -> Result<Graph> {
    Graph::open(path)
}
