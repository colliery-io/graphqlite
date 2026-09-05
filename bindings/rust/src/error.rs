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

    /// A label, property key, or coordinate property name is not a plain
    /// identifier and cannot be interpolated into a query (GitHub #110).
    #[error("Invalid identifier {0:?}: must match ^[A-Za-z_][A-Za-z0-9_]*$")]
    InvalidIdentifier(String),

    /// A graph name is not a plain identifier or would resolve outside the
    /// manager's base directory (GitHub #111).
    #[error("Invalid graph name {0:?}: must match ^[A-Za-z_][A-Za-z0-9_]*$ and stay inside the base directory")]
    InvalidGraphName(String),

    /// A caller-supplied argument is unusable (for example an empty graph list).
    #[error("Invalid argument: {0}")]
    InvalidArgument(String),
}
