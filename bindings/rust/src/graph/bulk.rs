//! Bulk insert operations for high-performance graph construction.
//!
//! These methods bypass Cypher query parsing and use direct SQL for maximum throughput.
//! They are designed for building graphs from external data sources where you have
//! full control over node IDs and don't need upsert semantics.
//!
//! # Example
//!
//! ```no_run
//! use graphqlite::Graph;
//!
//! let g = Graph::open_in_memory()?;
//!
//! // Bulk insert nodes - returns mapping of external ID -> internal rowid
//! let id_map = g.insert_nodes_bulk([
//!     ("alice", vec![("name", "Alice"), ("age", "30")], "Person"),
//!     ("bob", vec![("name", "Bob"), ("age", "25")], "Person"),
//!     ("charlie", vec![("name", "Charlie"), ("age", "35")], "Person"),
//! ])?;
//!
//! // Bulk insert edges using the ID map - no MATCH queries needed
//! g.insert_edges_bulk(
//!     [
//!         ("alice", "bob", vec![("since", "2020")], "KNOWS"),
//!         ("bob", "charlie", vec![("since", "2021")], "KNOWS"),
//!     ],
//!     &id_map,
//! )?;
//! # Ok::<(), graphqlite::Error>(())
//! ```

use std::collections::HashMap;

use rusqlite::params;

use super::Graph;
use crate::{Error, Result};

/// Result of a bulk insert operation.
#[derive(Debug, Clone)]
pub struct BulkInsertResult {
    /// Number of nodes inserted.
    pub nodes_inserted: usize,
    /// Number of edges inserted.
    pub edges_inserted: usize,
    /// Mapping from external node IDs to internal SQLite rowids.
    pub id_map: HashMap<String, i64>,
}

impl Graph {
    /// Insert multiple nodes in a single transaction with minimal overhead.
    ///
    /// Returns a map of external_id -> internal_rowid for subsequent edge insertion.
    /// This bypasses Cypher parsing entirely for maximum performance.
    ///
    /// # Arguments
    ///
    /// * `nodes` - Iterator of (external_id, properties, label) tuples
    ///
    /// # Returns
    ///
    /// A `HashMap<String, i64>` mapping external IDs to internal SQLite rowids.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open_in_memory()?;
    /// let id_map = g.insert_nodes_bulk([
    ///     ("node1", vec![("name", "Node 1")], "Label"),
    ///     ("node2", vec![("name", "Node 2")], "Label"),
    /// ])?;
    /// assert!(id_map.contains_key("node1"));
    /// assert!(id_map.contains_key("node2"));
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn insert_nodes_bulk<I, N, P, K, V, L>(&self, nodes: I) -> Result<HashMap<String, i64>>
    where
        I: IntoIterator<Item = (N, P, L)>,
        N: AsRef<str>,
        P: IntoIterator<Item = (K, V)>,
        K: AsRef<str>,
        V: AsRef<str>,
        L: AsRef<str>,
    {
        let conn = self.connection().sqlite_connection();
        let mut id_map = HashMap::new();

        // Collect nodes into a vec so we can work with them
        let nodes: Vec<_> = nodes.into_iter().collect();
        if nodes.is_empty() {
            return Ok(id_map);
        }

        // Begin transaction
        conn.execute("BEGIN IMMEDIATE", [])?;

        // Get or create property key for 'id'
        let id_key_id = self.ensure_property_key(conn, "id")?;

        // Prepare statements
        let mut insert_node_stmt = conn.prepare_cached("INSERT INTO nodes DEFAULT VALUES")?;
        let mut insert_label_stmt =
            conn.prepare_cached("INSERT OR IGNORE INTO node_labels (node_id, label) VALUES (?, ?)")?;
        let mut insert_text_prop_stmt = conn.prepare_cached(
            "INSERT OR REPLACE INTO node_props_text (node_id, key_id, value) VALUES (?, ?, ?)",
        )?;
        let mut insert_int_prop_stmt = conn.prepare_cached(
            "INSERT OR REPLACE INTO node_props_int (node_id, key_id, value) VALUES (?, ?, ?)",
        )?;
        let mut insert_real_prop_stmt = conn.prepare_cached(
            "INSERT OR REPLACE INTO node_props_real (node_id, key_id, value) VALUES (?, ?, ?)",
        )?;
        let mut insert_bool_prop_stmt = conn.prepare_cached(
            "INSERT OR REPLACE INTO node_props_bool (node_id, key_id, value) VALUES (?, ?, ?)",
        )?;

        // Property key cache within this transaction
        let mut prop_key_cache: HashMap<String, i64> = HashMap::new();
        prop_key_cache.insert("id".to_string(), id_key_id);

        for (external_id, props, label) in nodes {
            let external_id = external_id.as_ref();
            let label = label.as_ref();

            // Insert node row
            insert_node_stmt.execute([])?;
            let node_id = conn.last_insert_rowid();

            // Store mapping
            id_map.insert(external_id.to_string(), node_id);

            // Insert label
            insert_label_stmt.execute(params![node_id, label])?;

            // Insert 'id' property (the external ID)
            insert_text_prop_stmt.execute(params![node_id, id_key_id, external_id])?;

            // Insert other properties
            for (key, value) in props {
                let key = key.as_ref();
                let value = value.as_ref();

                // Get or create property key ID
                let key_id = if let Some(&cached_id) = prop_key_cache.get(key) {
                    cached_id
                } else {
                    let key_id = self.ensure_property_key(conn, key)?;
                    prop_key_cache.insert(key.to_string(), key_id);
                    key_id
                };

                // Determine value type and insert
                if let Ok(int_val) = value.parse::<i64>() {
                    insert_int_prop_stmt.execute(params![node_id, key_id, int_val])?;
                } else if let Ok(real_val) = value.parse::<f64>() {
                    insert_real_prop_stmt.execute(params![node_id, key_id, real_val])?;
                } else if value == "true" {
                    insert_bool_prop_stmt.execute(params![node_id, key_id, 1])?;
                } else if value == "false" {
                    insert_bool_prop_stmt.execute(params![node_id, key_id, 0])?;
                } else {
                    insert_text_prop_stmt.execute(params![node_id, key_id, value])?;
                }
            }
        }

        // Commit transaction
        conn.execute("COMMIT", [])?;

        Ok(id_map)
    }

    /// Insert multiple edges using pre-resolved internal IDs.
    ///
    /// Uses the mapping returned from `insert_nodes_bulk` to resolve external IDs
    /// to internal rowids without any database queries. For nodes not in the map,
    /// falls back to a database lookup.
    ///
    /// # Arguments
    ///
    /// * `edges` - Iterator of (source_external_id, target_external_id, properties, rel_type) tuples
    /// * `id_map` - Mapping from external IDs to internal rowids (from `insert_nodes_bulk`)
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open_in_memory()?;
    /// let id_map = g.insert_nodes_bulk([
    ///     ("a", vec![], "Node"),
    ///     ("b", vec![], "Node"),
    /// ])?;
    ///
    /// g.insert_edges_bulk(
    ///     [("a", "b", vec![("weight", "1.0")], "CONNECTS")],
    ///     &id_map,
    /// )?;
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn insert_edges_bulk<I, S, T, P, K, V, R>(
        &self,
        edges: I,
        id_map: &HashMap<String, i64>,
    ) -> Result<usize>
    where
        I: IntoIterator<Item = (S, T, P, R)>,
        S: AsRef<str>,
        T: AsRef<str>,
        P: IntoIterator<Item = (K, V)>,
        K: AsRef<str>,
        V: AsRef<str>,
        R: AsRef<str>,
    {
        let conn = self.connection().sqlite_connection();

        // Collect edges into a vec
        let edges: Vec<_> = edges.into_iter().collect();
        if edges.is_empty() {
            return Ok(0);
        }

        // Begin transaction
        conn.execute("BEGIN IMMEDIATE", [])?;

        // Prepare statements
        let mut insert_edge_stmt = conn.prepare_cached(
            "INSERT INTO edges (source_id, target_id, type) VALUES (?, ?, ?)",
        )?;
        let mut insert_text_prop_stmt = conn.prepare_cached(
            "INSERT OR REPLACE INTO edge_props_text (edge_id, key_id, value) VALUES (?, ?, ?)",
        )?;
        let mut insert_int_prop_stmt = conn.prepare_cached(
            "INSERT OR REPLACE INTO edge_props_int (edge_id, key_id, value) VALUES (?, ?, ?)",
        )?;
        let mut insert_real_prop_stmt = conn.prepare_cached(
            "INSERT OR REPLACE INTO edge_props_real (edge_id, key_id, value) VALUES (?, ?, ?)",
        )?;
        let mut insert_bool_prop_stmt = conn.prepare_cached(
            "INSERT OR REPLACE INTO edge_props_bool (edge_id, key_id, value) VALUES (?, ?, ?)",
        )?;

        // Property key cache
        let mut prop_key_cache: HashMap<String, i64> = HashMap::new();

        // Cache for looking up node IDs not in the provided map
        let mut fallback_cache: HashMap<String, i64> = HashMap::new();

        let mut edges_inserted = 0;

        for (source, target, props, rel_type) in edges {
            let source = source.as_ref();
            let target = target.as_ref();
            let rel_type = crate::sanitize_rel_type(rel_type.as_ref());

            // Resolve source ID
            let source_id = if let Some(&id) = id_map.get(source) {
                id
            } else if let Some(&id) = fallback_cache.get(source) {
                id
            } else {
                let id = self.lookup_node_id(conn, source)?;
                fallback_cache.insert(source.to_string(), id);
                id
            };

            // Resolve target ID
            let target_id = if let Some(&id) = id_map.get(target) {
                id
            } else if let Some(&id) = fallback_cache.get(target) {
                id
            } else {
                let id = self.lookup_node_id(conn, target)?;
                fallback_cache.insert(target.to_string(), id);
                id
            };

            // Insert edge
            insert_edge_stmt.execute(params![source_id, target_id, rel_type])?;
            let edge_id = conn.last_insert_rowid();
            edges_inserted += 1;

            // Insert edge properties
            for (key, value) in props {
                let key = key.as_ref();
                let value = value.as_ref();

                // Get or create property key ID
                let key_id = if let Some(&cached_id) = prop_key_cache.get(key) {
                    cached_id
                } else {
                    let key_id = self.ensure_property_key(conn, key)?;
                    prop_key_cache.insert(key.to_string(), key_id);
                    key_id
                };

                // Determine value type and insert
                if let Ok(int_val) = value.parse::<i64>() {
                    insert_int_prop_stmt.execute(params![edge_id, key_id, int_val])?;
                } else if let Ok(real_val) = value.parse::<f64>() {
                    insert_real_prop_stmt.execute(params![edge_id, key_id, real_val])?;
                } else if value == "true" {
                    insert_bool_prop_stmt.execute(params![edge_id, key_id, 1])?;
                } else if value == "false" {
                    insert_bool_prop_stmt.execute(params![edge_id, key_id, 0])?;
                } else {
                    insert_text_prop_stmt.execute(params![edge_id, key_id, value])?;
                }
            }
        }

        // Commit transaction
        conn.execute("COMMIT", [])?;

        Ok(edges_inserted)
    }

    /// Bulk insert both nodes and edges in a single operation.
    ///
    /// This is a convenience method that combines `insert_nodes_bulk` and `insert_edges_bulk`.
    ///
    /// # Arguments
    ///
    /// * `nodes` - Iterator of (external_id, properties, label) tuples
    /// * `edges` - Iterator of (source_external_id, target_external_id, properties, rel_type) tuples
    ///
    /// # Returns
    ///
    /// A `BulkInsertResult` with counts and the ID mapping.
    pub fn insert_graph_bulk<NI, N, NP, NK, NV, NL, EI, S, T, EP, EK, EV, R>(
        &self,
        nodes: NI,
        edges: EI,
    ) -> Result<BulkInsertResult>
    where
        NI: IntoIterator<Item = (N, NP, NL)>,
        N: AsRef<str>,
        NP: IntoIterator<Item = (NK, NV)>,
        NK: AsRef<str>,
        NV: AsRef<str>,
        NL: AsRef<str>,
        EI: IntoIterator<Item = (S, T, EP, R)>,
        S: AsRef<str>,
        T: AsRef<str>,
        EP: IntoIterator<Item = (EK, EV)>,
        EK: AsRef<str>,
        EV: AsRef<str>,
        R: AsRef<str>,
    {
        let id_map = self.insert_nodes_bulk(nodes)?;
        let nodes_inserted = id_map.len();
        let edges_inserted = self.insert_edges_bulk(edges, &id_map)?;

        Ok(BulkInsertResult {
            nodes_inserted,
            edges_inserted,
            id_map,
        })
    }

    /// Resolve multiple external node IDs to internal rowids in a single query.
    ///
    /// This is useful when you need to insert edges between nodes that were
    /// inserted in previous sessions or via Cypher.
    ///
    /// # Arguments
    ///
    /// * `external_ids` - Iterator of external node IDs to resolve
    ///
    /// # Returns
    ///
    /// A `HashMap<String, i64>` mapping external IDs to internal rowids.
    /// IDs that don't exist in the database will be missing from the map.
    pub fn resolve_node_ids<I, S>(&self, external_ids: I) -> Result<HashMap<String, i64>>
    where
        I: IntoIterator<Item = S>,
        S: AsRef<str>,
    {
        let conn = self.connection().sqlite_connection();
        let mut result = HashMap::new();

        // Get the 'id' property key
        let id_key_id: Option<i64> = conn
            .query_row(
                "SELECT id FROM property_keys WHERE key = 'id'",
                [],
                |row| row.get(0),
            )
            .ok();

        let id_key_id = match id_key_id {
            Some(id) => id,
            None => return Ok(result), // No 'id' property key means no nodes
        };

        // Prepare lookup statement
        let mut stmt = conn.prepare_cached(
            "SELECT node_id, value FROM node_props_text WHERE key_id = ? AND value = ?",
        )?;

        for external_id in external_ids {
            let external_id = external_id.as_ref();
            if let Ok((node_id, _)) = stmt.query_row(params![id_key_id, external_id], |row| {
                Ok((row.get::<_, i64>(0)?, row.get::<_, String>(1)?))
            }) {
                result.insert(external_id.to_string(), node_id);
            }
        }

        Ok(result)
    }

    // Helper: Ensure a property key exists and return its ID
    fn ensure_property_key(&self, conn: &rusqlite::Connection, key: &str) -> Result<i64> {
        // Try to find existing key
        let existing: Option<i64> = conn
            .query_row(
                "SELECT id FROM property_keys WHERE key = ?",
                params![key],
                |row| row.get(0),
            )
            .ok();

        if let Some(id) = existing {
            return Ok(id);
        }

        // Insert new key
        conn.execute("INSERT INTO property_keys (key) VALUES (?)", params![key])?;
        Ok(conn.last_insert_rowid())
    }

    // Helper: Look up a node's internal ID by external ID
    fn lookup_node_id(&self, conn: &rusqlite::Connection, external_id: &str) -> Result<i64> {
        // Get the 'id' property key
        let id_key_id: i64 = conn.query_row(
            "SELECT id FROM property_keys WHERE key = 'id'",
            [],
            |row| row.get(0),
        )?;

        // Look up the node
        let node_id: i64 = conn
            .query_row(
                "SELECT node_id FROM node_props_text WHERE key_id = ? AND value = ?",
                params![id_key_id, external_id],
                |row| row.get(0),
            )
            .map_err(|_| {
                Error::Cypher(format!("Node with id '{}' not found", external_id))
            })?;

        Ok(node_id)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // Type alias to help with empty property vecs
    type Props = Vec<(&'static str, &'static str)>;

    fn empty_props() -> Props {
        vec![]
    }

    #[test]
    fn test_bulk_insert_nodes() {
        let g = Graph::open_in_memory().unwrap();

        let id_map = g
            .insert_nodes_bulk([
                ("alice", vec![("name", "Alice"), ("age", "30")], "Person"),
                ("bob", vec![("name", "Bob"), ("age", "25")], "Person"),
            ])
            .unwrap();

        assert_eq!(id_map.len(), 2);
        assert!(id_map.contains_key("alice"));
        assert!(id_map.contains_key("bob"));

        // Verify nodes exist via Cypher
        let result = g.query("MATCH (n:Person) RETURN n.id ORDER BY n.id").unwrap();
        assert_eq!(result.len(), 2);
    }

    #[test]
    fn test_bulk_insert_edges() {
        let g = Graph::open_in_memory().unwrap();

        let id_map = g
            .insert_nodes_bulk([
                ("a", empty_props(), "Node"),
                ("b", empty_props(), "Node"),
                ("c", empty_props(), "Node"),
            ])
            .unwrap();

        let edges_inserted = g
            .insert_edges_bulk(
                [
                    ("a", "b", vec![("weight", "1.0")], "CONNECTS"),
                    ("b", "c", vec![("weight", "2.0")], "CONNECTS"),
                ],
                &id_map,
            )
            .unwrap();

        assert_eq!(edges_inserted, 2);

        // Verify edges exist via Cypher
        let result = g.query("MATCH ()-[r]->() RETURN count(r) AS cnt").unwrap();
        let cnt: i64 = result[0].get("cnt").unwrap();
        assert_eq!(cnt, 2);
    }

    #[test]
    fn test_bulk_insert_graph() {
        let g = Graph::open_in_memory().unwrap();

        let result = g
            .insert_graph_bulk(
                [
                    ("x", vec![("name", "X")], "Node"),
                    ("y", vec![("name", "Y")], "Node"),
                ],
                [("x", "y", empty_props(), "LINKS")],
            )
            .unwrap();

        assert_eq!(result.nodes_inserted, 2);
        assert_eq!(result.edges_inserted, 1);
    }

    #[test]
    fn test_resolve_node_ids() {
        let g = Graph::open_in_memory().unwrap();

        // Insert some nodes via Cypher
        g.query("CREATE (:Person {id: 'alice', name: 'Alice'})").unwrap();
        g.query("CREATE (:Person {id: 'bob', name: 'Bob'})").unwrap();

        let resolved = g.resolve_node_ids(["alice", "bob", "unknown"]).unwrap();

        assert_eq!(resolved.len(), 2);
        assert!(resolved.contains_key("alice"));
        assert!(resolved.contains_key("bob"));
        assert!(!resolved.contains_key("unknown"));
    }

    #[test]
    fn test_bulk_insert_mixed_sources() {
        let g = Graph::open_in_memory().unwrap();

        // Insert some nodes via Cypher
        g.query("CREATE (:Person {id: 'existing', name: 'Existing'})").unwrap();

        // Insert new nodes via bulk
        let id_map = g
            .insert_nodes_bulk([
                ("new1", empty_props(), "Person"),
                ("new2", empty_props(), "Person"),
            ])
            .unwrap();

        // Insert edges connecting new and existing nodes
        let edges_inserted = g
            .insert_edges_bulk(
                [
                    ("new1", "new2", empty_props(), "KNOWS"),
                    ("new1", "existing", empty_props(), "KNOWS"), // existing not in id_map
                ],
                &id_map,
            )
            .unwrap();

        assert_eq!(edges_inserted, 2);
    }

    #[test]
    fn test_bulk_insert_performance() {
        use std::time::Instant;

        let g = Graph::open_in_memory().unwrap();

        // Generate test data - 1000 nodes, 5000 edges
        let node_count = 1000;
        let edge_count = 5000;

        let nodes: Vec<_> = (0..node_count)
            .map(|i| {
                (
                    format!("node_{}", i),
                    vec![("name", format!("Node {}", i))],
                    "TestNode".to_string(),
                )
            })
            .collect();

        let edges: Vec<_> = (0..edge_count)
            .map(|i| {
                (
                    format!("node_{}", i % node_count),
                    format!("node_{}", (i + 1) % node_count),
                    vec![("weight", format!("{}", i))],
                    "CONNECTS".to_string(),
                )
            })
            .collect();

        // Time bulk insert
        let start = Instant::now();

        let id_map = g
            .insert_nodes_bulk(
                nodes
                    .iter()
                    .map(|(id, props, label)| (id.as_str(), props.clone(), label.as_str())),
            )
            .unwrap();

        let node_time = start.elapsed();

        let edge_start = Instant::now();
        let edges_inserted = g
            .insert_edges_bulk(
                edges.iter().map(|(s, t, props, rel)| {
                    (s.as_str(), t.as_str(), props.clone(), rel.as_str())
                }),
                &id_map,
            )
            .unwrap();

        let edge_time = edge_start.elapsed();
        let total_time = start.elapsed();

        assert_eq!(id_map.len(), node_count);
        assert_eq!(edges_inserted, edge_count);

        // Print performance stats (visible with cargo test -- --nocapture)
        println!("\n=== Bulk Insert Performance ===");
        println!("Nodes: {} in {:?} ({:.0} nodes/sec)",
            node_count, node_time,
            node_count as f64 / node_time.as_secs_f64());
        println!("Edges: {} in {:?} ({:.0} edges/sec)",
            edge_count, edge_time,
            edge_count as f64 / edge_time.as_secs_f64());
        println!("Total: {:?}", total_time);
        println!("===============================\n");

        // Sanity check: bulk insert should be fast (< 1 second for this size)
        assert!(total_time.as_secs() < 5, "Bulk insert took too long: {:?}", total_time);
    }
}
