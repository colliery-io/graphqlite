//! Error types for GraphQLite operations.

use thiserror::Error;

/// Error type for GraphQLite operations.
#[derive(Error, Debug)]
pub enum Error {
    /// SQLite error from rusqlite.
    #[error("SQLite error: {0}")]
    Sqlite(#[from] rusqlite::Error),

    /// JSON parsing error.
    #[error("JSON error: {0}")]
    Json(#[from] serde_json::Error),

    /// Cypher query error returned by the extension.
    #[error("Cypher error: {0}")]
    Cypher(String),

    /// Extension not found.
    #[error("GraphQLite extension not found: {0}")]
    ExtensionNotFound(String),

    /// Type conversion error.
    #[error("Type error: expected {expected}, got {actual}")]
    TypeError {
        expected: &'static str,
        actual: String,
    },

    /// Column not found in result row.
    #[error("Column not found: {0}")]
    ColumnNotFound(String),

    /// Graph already exists.
    #[error("Graph '{0}' already exists")]
    GraphExists(String),

    /// Graph not found.
    #[error("Graph '{name}' not found. Available: {available:?}")]
    GraphNotFound {
        name: String,
        available: Vec<String>,
    },

    /// IO error.
    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),
}
