//! High-level graph operations for GraphQLite.
//!
//! The `Graph` struct provides an ergonomic interface for common graph operations.

mod batch;
mod edges;
mod nodes;
mod queries;

use crate::{Connection, CypherResult, Result};
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
}

/// Create a new Graph instance (convenience function).
pub fn graph<P: AsRef<Path>>(path: P) -> Result<Graph> {
    Graph::open(path)
}
