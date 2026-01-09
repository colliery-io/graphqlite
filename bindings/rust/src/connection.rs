//! GraphQLite connection wrapper.

use crate::{CypherResult, Error, Result};

#[cfg(not(feature = "bundled-extension"))]
use std::path::PathBuf;
use std::path::Path;

/// A GraphQLite database connection.
///
/// Wraps a SQLite connection with the GraphQLite extension loaded,
/// providing Cypher query support.
pub struct Connection {
    conn: rusqlite::Connection,
}

impl Connection {
    /// Open a database at the given path.
    ///
    /// # Arguments
    ///
    /// * `path` - Path to database file, or ":memory:" for in-memory database
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Connection;
    ///
    /// let conn = Connection::open(":memory:")?;
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn open<P: AsRef<Path>>(path: P) -> Result<Self> {
        let conn = rusqlite::Connection::open(path)?;
        Self::from_rusqlite(conn)
    }

    /// Open an in-memory database.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Connection;
    ///
    /// let conn = Connection::open_in_memory()?;
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn open_in_memory() -> Result<Self> {
        let conn = rusqlite::Connection::open_in_memory()?;
        Self::from_rusqlite(conn)
    }

    /// Create a GraphQLite connection from an existing rusqlite Connection.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Connection;
    /// use rusqlite;
    ///
    /// let sqlite_conn = rusqlite::Connection::open_in_memory()?;
    /// let conn = Connection::from_rusqlite(sqlite_conn)?;
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    #[cfg(feature = "bundled-extension")]
    pub fn from_rusqlite(conn: rusqlite::Connection) -> Result<Self> {
        // Load the bundled extension (extracts from embedded binary)
        crate::platform::load_bundled_extension(&conn)?;
        Ok(Connection { conn })
    }

    /// Create a GraphQLite connection from an existing rusqlite Connection.
    #[cfg(not(feature = "bundled-extension"))]
    pub fn from_rusqlite(conn: rusqlite::Connection) -> Result<Self> {
        let extension_path = find_extension()?;
        load_extension(&conn, &extension_path)?;
        Ok(Connection { conn })
    }

    /// Create a connection with a custom extension path.
    ///
    /// # Arguments
    ///
    /// * `path` - Path to database file
    /// * `extension_path` - Path to the GraphQLite extension (.dylib, .so, or .dll)
    ///
    /// Note: This method is only available when the `bundled-extension` feature is disabled.
    #[cfg(not(feature = "bundled-extension"))]
    pub fn open_with_extension<P: AsRef<Path>, E: AsRef<std::path::Path>>(
        path: P,
        extension_path: E,
    ) -> Result<Self> {
        let conn = rusqlite::Connection::open(path)?;
        load_extension(&conn, extension_path.as_ref())?;
        Ok(Connection { conn })
    }

    /// Execute a Cypher query.
    ///
    /// # Arguments
    ///
    /// * `query` - Cypher query string
    ///
    /// # Returns
    ///
    /// A `CypherResult` containing the query results.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Connection;
    ///
    /// let conn = Connection::open_in_memory()?;
    /// conn.cypher("CREATE (n:Person {name: 'Alice'})")?;
    /// let results = conn.cypher("MATCH (n:Person) RETURN n.name")?;
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn cypher(&self, query: &str) -> Result<CypherResult> {
        let result: Option<String> = self
            .conn
            .query_row("SELECT cypher(?1)", [query], |row| row.get(0))?;

        match result {
            Some(json_str) => {
                // Check for error response
                if json_str.starts_with("Error") {
                    return Err(Error::Cypher(json_str));
                }
                CypherResult::from_json(&json_str)
            }
            None => Ok(CypherResult::empty()),
        }
    }

    /// Execute raw SQL.
    ///
    /// Useful for queries that don't use Cypher, like checking schema
    /// or using algorithm results with `json_each()`.
    pub fn execute(&self, sql: &str) -> Result<usize> {
        Ok(self.conn.execute(sql, [])?)
    }

    /// Access the underlying rusqlite connection.
    pub fn sqlite_connection(&self) -> &rusqlite::Connection {
        &self.conn
    }
}

/// Find the GraphQLite extension library.
#[cfg(not(feature = "bundled-extension"))]
fn find_extension() -> Result<PathBuf> {
    let ext_name = if cfg!(target_os = "macos") {
        "graphqlite.dylib"
    } else if cfg!(target_os = "windows") {
        "graphqlite.dll"
    } else {
        "graphqlite.so"
    };

    // Search paths in order of preference
    let search_paths: Vec<PathBuf> = vec![
        // Environment variable
        std::env::var("GRAPHQLITE_EXTENSION_PATH")
            .ok()
            .map(PathBuf::from)
            .unwrap_or_default(),
        // Current directory build
        PathBuf::from("build").join(ext_name),
        // Relative to crate root (for development)
        PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .parent()
            .unwrap_or(Path::new("."))
            .parent()
            .unwrap_or(Path::new("."))
            .join("build")
            .join(ext_name),
        // System paths
        PathBuf::from("/usr/local/lib").join(ext_name),
        PathBuf::from("/usr/lib").join(ext_name),
    ];

    for path in search_paths {
        if path.exists() {
            return Ok(path);
        }
    }

    Err(Error::ExtensionNotFound(format!(
        "Could not find {}. Build with 'make extension' or set GRAPHQLITE_EXTENSION_PATH",
        ext_name
    )))
}

/// Load the GraphQLite extension into a connection.
#[cfg(not(feature = "bundled-extension"))]
fn load_extension(conn: &rusqlite::Connection, path: &std::path::Path) -> Result<()> {
    // Remove the file extension for SQLite's load_extension
    let load_path = path.with_extension("");

    unsafe {
        conn.load_extension_enable()?;
        conn.load_extension(&load_path, None)?;
        conn.load_extension_disable()?;
    }

    // Verify the extension loaded
    let test: String = conn.query_row("SELECT graphqlite_test()", [], |row| row.get(0))?;
    if !test.to_lowercase().contains("successfully") {
        return Err(Error::ExtensionNotFound(
            "Extension loaded but verification failed".to_string(),
        ));
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[cfg(not(feature = "bundled-extension"))]
    fn get_test_extension_path() -> Option<std::path::PathBuf> {
        let paths = [
            std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"))
                .parent()
                .unwrap()
                .parent()
                .unwrap()
                .join("build/graphqlite.dylib"),
            std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"))
                .parent()
                .unwrap()
                .parent()
                .unwrap()
                .join("build/graphqlite.so"),
        ];

        paths.into_iter().find(|p| p.exists())
    }

    #[test]
    #[cfg(not(feature = "bundled-extension"))]
    fn test_find_extension() {
        // This test may skip if extension isn't built
        if get_test_extension_path().is_none() {
            return;
        }
        assert!(find_extension().is_ok());
    }

    #[test]
    #[cfg(feature = "bundled-extension")]
    fn test_bundled_connection() {
        // Test that bundled extension works
        let conn = Connection::open_in_memory();
        assert!(conn.is_ok(), "Failed to open connection: {:?}", conn.err());
    }
}
