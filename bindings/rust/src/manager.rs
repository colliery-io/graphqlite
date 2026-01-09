//! Multi-graph management for GraphQLite.
//!
//! The `GraphManager` struct provides management of multiple graph databases
//! in a directory, with cross-graph query support via ATTACH.

use crate::{Connection, CypherResult, Error, Graph, Result};
use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};

/// Manager for multiple graph databases in a directory.
///
/// Provides ergonomic multi-graph management using separate SQLite files
/// per graph, with cross-graph query support via ATTACH.
///
/// # Example
///
/// ```no_run
/// use graphqlite::GraphManager;
///
/// let mut gm = GraphManager::open("./data")?;
///
/// // Create graphs
/// let social = gm.create("social")?;
/// social.upsert_node("alice", [("name", "Alice")], "Person")?;
///
/// let products = gm.create("products")?;
/// products.upsert_node("phone", [("name", "Phone")], "Product")?;
///
/// // List graphs
/// println!("{:?}", gm.list()?);  // ["products", "social"]
///
/// // Cross-graph query
/// let result = gm.query(
///     "MATCH (n:Person) FROM social RETURN n.name",
///     &["social"]
/// )?;
/// # Ok::<(), graphqlite::Error>(())
/// ```
pub struct GraphManager {
    base_path: PathBuf,
    open_graphs: HashMap<String, Graph>,
    coordinator: Option<Connection>,
}

impl GraphManager {
    /// Open a GraphManager for a directory.
    ///
    /// # Arguments
    ///
    /// * `base_path` - Directory where graph .db files are stored
    pub fn open<P: AsRef<Path>>(base_path: P) -> Result<Self> {
        let base_path = base_path.as_ref().to_path_buf();

        // Ensure base directory exists
        fs::create_dir_all(&base_path)?;

        Ok(GraphManager {
            base_path,
            open_graphs: HashMap::new(),
            coordinator: None,
        })
    }

    /// Get the file path for a graph.
    fn graph_path(&self, name: &str) -> PathBuf {
        self.base_path.join(format!("{}.db", name))
    }

    /// Get or create the coordinator connection for cross-graph queries.
    fn ensure_coordinator(&mut self) -> Result<&Connection> {
        if self.coordinator.is_none() {
            let conn = Connection::open_in_memory()?;
            self.coordinator = Some(conn);
        }
        Ok(self.coordinator.as_ref().unwrap())
    }

    /// List all available graphs in the base directory.
    ///
    /// # Returns
    ///
    /// List of graph names (without .db extension), sorted alphabetically.
    pub fn list(&self) -> Result<Vec<String>> {
        let mut graphs = Vec::new();

        for entry in fs::read_dir(&self.base_path)? {
            let entry = entry?;
            let path = entry.path();
            if path.extension().is_some_and(|ext| ext == "db") {
                if let Some(stem) = path.file_stem() {
                    if let Some(name) = stem.to_str() {
                        graphs.push(name.to_string());
                    }
                }
            }
        }

        graphs.sort();
        Ok(graphs)
    }

    /// Check if a graph exists.
    ///
    /// # Arguments
    ///
    /// * `name` - Graph name
    pub fn exists(&self, name: &str) -> bool {
        self.graph_path(name).exists()
    }

    /// Create a new graph.
    ///
    /// # Arguments
    ///
    /// * `name` - Graph name (will create {name}.db file)
    ///
    /// # Returns
    ///
    /// Reference to the new Graph instance.
    ///
    /// # Errors
    ///
    /// Returns an error if the graph already exists.
    pub fn create(&mut self, name: &str) -> Result<&Graph> {
        let path = self.graph_path(name);
        if path.exists() {
            return Err(Error::GraphExists(name.to_string()));
        }

        let graph = Graph::open(&path)?;
        self.open_graphs.insert(name.to_string(), graph);
        Ok(self.open_graphs.get(name).unwrap())
    }

    /// Open an existing graph.
    ///
    /// # Arguments
    ///
    /// * `name` - Graph name
    ///
    /// # Returns
    ///
    /// Reference to the Graph instance.
    ///
    /// # Errors
    ///
    /// Returns an error if the graph doesn't exist.
    pub fn open_graph(&mut self, name: &str) -> Result<&Graph> {
        // Return cached graph if already open
        if self.open_graphs.contains_key(name) {
            return Ok(self.open_graphs.get(name).unwrap());
        }

        let path = self.graph_path(name);
        if !path.exists() {
            let available = self.list()?;
            return Err(Error::GraphNotFound {
                name: name.to_string(),
                available,
            });
        }

        let graph = Graph::open(&path)?;
        self.open_graphs.insert(name.to_string(), graph);
        Ok(self.open_graphs.get(name).unwrap())
    }

    /// Open a graph, creating it if it doesn't exist.
    ///
    /// # Arguments
    ///
    /// * `name` - Graph name
    pub fn open_or_create(&mut self, name: &str) -> Result<&Graph> {
        if self.exists(name) {
            self.open_graph(name)
        } else {
            self.create(name)
        }
    }

    /// Get a mutable reference to an open graph.
    ///
    /// # Arguments
    ///
    /// * `name` - Graph name
    ///
    /// # Returns
    ///
    /// Mutable reference to the Graph, or None if not open.
    pub fn get_mut(&mut self, name: &str) -> Option<&mut Graph> {
        self.open_graphs.get_mut(name)
    }

    /// Delete a graph and its database file.
    ///
    /// # Arguments
    ///
    /// * `name` - Graph name
    ///
    /// # Errors
    ///
    /// Returns an error if the graph doesn't exist.
    pub fn drop(&mut self, name: &str) -> Result<()> {
        let path = self.graph_path(name);
        if !path.exists() {
            let available = self.list()?;
            return Err(Error::GraphNotFound {
                name: name.to_string(),
                available,
            });
        }

        // Remove from cache
        self.open_graphs.remove(name);

        // Detach from coordinator if attached
        if let Some(ref conn) = self.coordinator {
            // Ignore errors if not attached
            let _ = conn.sqlite_connection().execute(&format!("DETACH DATABASE {}", name), []);
        }

        // Delete file
        fs::remove_file(path)?;
        Ok(())
    }

    /// Execute a cross-graph Cypher query.
    ///
    /// Uses the FROM clause syntax to query across multiple graphs.
    /// Graphs are automatically attached to the coordinator connection.
    ///
    /// # Arguments
    ///
    /// * `cypher` - Cypher query with FROM clauses specifying graphs
    /// * `graph_names` - List of graph names to attach
    ///
    /// # Returns
    ///
    /// CypherResult with query results.
    pub fn query(&mut self, cypher: &str, graph_names: &[&str]) -> Result<CypherResult> {
        // Collect graph paths first (before borrowing coordinator)
        let mut graph_paths: Vec<(String, PathBuf)> = Vec::new();
        for name in graph_names {
            let path = self.graph_path(name);
            if !path.exists() {
                let available = self.list()?;
                return Err(Error::GraphNotFound {
                    name: name.to_string(),
                    available,
                });
            }
            graph_paths.push((name.to_string(), path));
        }

        // Now borrow coordinator and attach graphs
        self.ensure_coordinator()?;
        let coord = self.coordinator.as_ref().unwrap().sqlite_connection();

        for (name, path) in &graph_paths {
            let attach_sql = format!(
                "ATTACH DATABASE '{}' AS {}",
                path.display(),
                name
            );
            if let Err(e) = coord.execute(&attach_sql, []) {
                let err_str = e.to_string().to_lowercase();
                if !err_str.contains("already in use") {
                    return Err(e.into());
                }
            }
        }

        // Execute query
        let result: Option<String> = coord.query_row(
            "SELECT cypher(?1)",
            [cypher],
            |row| row.get(0),
        )?;

        match result {
            Some(json_str) => {
                if json_str.starts_with("Error") {
                    return Err(Error::Cypher(json_str));
                }
                CypherResult::from_json(&json_str)
            }
            None => Ok(CypherResult::empty()),
        }
    }

    /// Execute a raw SQL query across attached graphs.
    ///
    /// For power users who need direct SQL access to cross-graph data.
    ///
    /// # Arguments
    ///
    /// * `sql` - SQL query with graph-prefixed table names
    /// * `graph_names` - List of graph names to attach
    pub fn query_sql(&mut self, sql: &str, graph_names: &[&str]) -> Result<Vec<Vec<rusqlite::types::Value>>> {
        // Collect graph paths first (before borrowing coordinator)
        let mut graph_paths: Vec<(String, PathBuf)> = Vec::new();
        for name in graph_names {
            let path = self.graph_path(name);
            if !path.exists() {
                let available = self.list()?;
                return Err(Error::GraphNotFound {
                    name: name.to_string(),
                    available,
                });
            }
            graph_paths.push((name.to_string(), path));
        }

        // Now borrow coordinator and attach graphs
        self.ensure_coordinator()?;
        let coord = self.coordinator.as_ref().unwrap().sqlite_connection();

        for (name, path) in &graph_paths {
            let attach_sql = format!(
                "ATTACH DATABASE '{}' AS {}",
                path.display(),
                name
            );
            if let Err(e) = coord.execute(&attach_sql, []) {
                let err_str = e.to_string().to_lowercase();
                if !err_str.contains("already in use") {
                    return Err(e.into());
                }
            }
        }

        // Execute query
        let mut stmt = coord.prepare(sql)?;
        let column_count = stmt.column_count();

        let rows = stmt.query_map([], |row| {
            let mut values = Vec::with_capacity(column_count);
            for i in 0..column_count {
                values.push(row.get(i)?);
            }
            Ok(values)
        })?;

        let mut results = Vec::new();
        for row in rows {
            results.push(row?);
        }
        Ok(results)
    }

    /// Get the number of graphs in the directory.
    pub fn len(&self) -> Result<usize> {
        Ok(self.list()?.len())
    }

    /// Check if the directory is empty (no graphs).
    pub fn is_empty(&self) -> Result<bool> {
        Ok(self.len()? == 0)
    }

    /// Check if a graph name is in the directory.
    pub fn contains(&self, name: &str) -> bool {
        self.exists(name)
    }

    /// Iterate over graph names.
    pub fn iter(&self) -> Result<impl Iterator<Item = String>> {
        Ok(self.list()?.into_iter())
    }
}

impl Drop for GraphManager {
    fn drop(&mut self) {
        // Close all open graphs
        self.open_graphs.clear();
        // Coordinator connection is automatically closed when dropped
    }
}

/// Create a new GraphManager instance (convenience function).
pub fn graphs<P: AsRef<Path>>(base_path: P) -> Result<GraphManager> {
    GraphManager::open(base_path)
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    #[test]
    fn test_create_manager() {
        let tmpdir = TempDir::new().unwrap();
        let gm = GraphManager::open(tmpdir.path()).unwrap();
        assert!(gm.is_empty().unwrap());
    }

    #[test]
    fn test_list_empty() {
        let tmpdir = TempDir::new().unwrap();
        let gm = GraphManager::open(tmpdir.path()).unwrap();
        assert_eq!(gm.list().unwrap(), Vec::<String>::new());
    }

    #[test]
    fn test_create_graph() {
        let tmpdir = TempDir::new().unwrap();
        let mut gm = GraphManager::open(tmpdir.path()).unwrap();
        gm.create("social").unwrap();
        assert!(gm.exists("social"));
        assert!(gm.list().unwrap().contains(&"social".to_string()));
    }

    #[test]
    fn test_create_duplicate_fails() {
        let tmpdir = TempDir::new().unwrap();
        let mut gm = GraphManager::open(tmpdir.path()).unwrap();
        gm.create("social").unwrap();
        assert!(gm.create("social").is_err());
    }

    #[test]
    fn test_open_missing_fails() {
        let tmpdir = TempDir::new().unwrap();
        let mut gm = GraphManager::open(tmpdir.path()).unwrap();
        assert!(gm.open_graph("nonexistent").is_err());
    }

    #[test]
    fn test_drop_graph() {
        let tmpdir = TempDir::new().unwrap();
        let mut gm = GraphManager::open(tmpdir.path()).unwrap();
        gm.create("social").unwrap();
        assert!(gm.exists("social"));
        gm.drop("social").unwrap();
        assert!(!gm.exists("social"));
    }

    #[test]
    fn test_list_multiple() {
        let tmpdir = TempDir::new().unwrap();
        let mut gm = GraphManager::open(tmpdir.path()).unwrap();
        gm.create("alpha").unwrap();
        gm.create("beta").unwrap();
        gm.create("gamma").unwrap();
        let list = gm.list().unwrap();
        assert_eq!(list, vec!["alpha", "beta", "gamma"]);
    }
}
