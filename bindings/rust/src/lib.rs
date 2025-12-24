//! GraphQLite - SQLite extension for graph queries using Cypher
//!
//! This crate provides Rust bindings for GraphQLite, allowing you to use
//! Cypher graph queries in SQLite databases.
//!
//! # Example
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

mod connection;
mod error;
mod result;

pub use connection::Connection;
pub use error::Error;
pub use result::{CypherResult, Row, Value};

/// Result type for GraphQLite operations.
pub type Result<T> = std::result::Result<T, Error>;
