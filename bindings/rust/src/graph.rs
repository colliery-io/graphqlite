//! High-level graph operations for GraphQLite.
//!
//! The `Graph` struct provides an ergonomic interface for common graph operations,
//! similar to the Python `Graph` class.
//!
//! # Example
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
//! # Ok::<(), graphqlite::Error>(())
//! ```

use crate::{Connection, CypherResult, Result, Value};
use serde::{Deserialize, Serialize};
use std::collections::HashSet;
use std::path::Path;

/// Cypher reserved keywords that can't be used as relationship types.
pub static CYPHER_RESERVED: &[&str] = &[
    // Clauses
    "CREATE", "MATCH", "RETURN", "WHERE", "DELETE", "SET", "REMOVE",
    "ORDER", "BY", "SKIP", "LIMIT", "WITH", "UNWIND", "AS", "AND", "OR",
    "NOT", "IN", "IS", "NULL", "TRUE", "FALSE", "MERGE", "ON", "CALL",
    "YIELD", "DETACH", "OPTIONAL", "UNION", "ALL", "CASE", "WHEN", "THEN",
    "ELSE", "END", "EXISTS", "FOREACH",
    // Aggregate functions
    "COUNT", "SUM", "AVG", "MIN", "MAX", "COLLECT",
    // List functions
    "REDUCE", "FILTER", "EXTRACT", "ANY", "NONE", "SINGLE",
    // Other reserved
    "STARTS", "ENDS", "CONTAINS", "XOR", "DISTINCT", "LOAD", "CSV",
    "USING", "PERIODIC", "COMMIT", "CONSTRAINT", "INDEX", "DROP", "ASSERT",
];

/// Escape a string for use in Cypher queries.
///
/// Handles backslashes, quotes, and whitespace characters.
pub fn escape_string(s: &str) -> String {
    s.replace('\\', "\\\\")
        .replace('\'', "\\'")
        .replace('"', "\\\"")
        .replace('\n', " ")
        .replace('\r', " ")
        .replace('\t', " ")
}

/// Sanitize a relationship type for use in Cypher.
///
/// Ensures the type is a valid Cypher identifier and not a reserved word.
pub fn sanitize_rel_type(rel_type: &str) -> String {
    let safe: String = rel_type
        .chars()
        .map(|c| if c.is_alphanumeric() || c == '_' { c } else { '_' })
        .collect();

    let safe = if safe.is_empty() || safe.chars().next().map_or(false, |c| c.is_numeric()) {
        format!("REL_{}", safe)
    } else {
        safe
    };

    let reserved: HashSet<&str> = CYPHER_RESERVED.iter().copied().collect();
    if reserved.contains(safe.to_uppercase().as_str()) {
        format!("REL_{}", safe)
    } else {
        safe
    }
}

/// Graph statistics containing node and edge counts.
///
/// Returned by [`Graph::stats`].
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GraphStats {
    /// Total number of nodes in the graph.
    pub nodes: i64,
    /// Total number of edges in the graph.
    pub edges: i64,
}

/// PageRank result for a single node.
///
/// Returned as part of the vector from [`Graph::pagerank`].
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PageRankResult {
    /// Internal node identifier.
    pub node_id: String,
    /// User-defined node identifier (from the `id` property).
    pub user_id: Option<String>,
    /// PageRank score (higher = more important).
    pub score: f64,
}

/// Community detection result for a single node.
///
/// Returned as part of the vector from [`Graph::community_detection`].
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CommunityResult {
    /// Internal node identifier.
    pub node_id: String,
    /// User-defined node identifier (from the `id` property).
    pub user_id: Option<String>,
    /// Community label (nodes with the same value belong to the same community).
    pub community: i64,
}

/// High-level graph operations.
///
/// Provides ergonomic node/edge CRUD, graph queries, and algorithm wrappers
/// on top of the raw Cypher interface.
pub struct Graph {
    conn: Connection,
    namespace: String,
}

impl Graph {
    /// Open a graph database.
    ///
    /// # Arguments
    ///
    /// * `path` - Path to database file, or ":memory:" for in-memory
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open(":memory:")?;
    /// let g = Graph::open("my_graph.db")?;
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn open<P: AsRef<Path>>(path: P) -> Result<Self> {
        let conn = Connection::open(path)?;
        Ok(Graph {
            conn,
            namespace: "default".to_string(),
        })
    }

    /// Open an in-memory graph database.
    ///
    /// Equivalent to `Graph::open(":memory:")`.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open_in_memory()?;
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn open_in_memory() -> Result<Self> {
        let conn = Connection::open_in_memory()?;
        Ok(Graph {
            conn,
            namespace: "default".to_string(),
        })
    }

    /// Create a Graph from an existing [`Connection`].
    ///
    /// Useful when you need both high-level Graph API and low-level Connection
    /// access, or when using a custom extension path.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::{Connection, Graph};
    ///
    /// let conn = Connection::open_with_extension(":memory:", "/path/to/graphqlite.dylib")?;
    /// let g = Graph::from_connection(conn);
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn from_connection(conn: Connection) -> Self {
        Graph {
            conn,
            namespace: "default".to_string(),
        }
    }

    /// Set the namespace for this graph (reserved for future use).
    pub fn with_namespace(mut self, namespace: &str) -> Self {
        self.namespace = namespace.to_string();
        self
    }

    /// Access the underlying Connection.
    pub fn connection(&self) -> &Connection {
        &self.conn
    }

    // -------------------------------------------------------------------------
    // Node Operations
    // -------------------------------------------------------------------------

    /// Check if a node with the given ID exists.
    ///
    /// # Arguments
    ///
    /// * `node_id` - The unique identifier to look up
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open_in_memory()?;
    /// assert!(!g.has_node("alice")?);
    /// g.upsert_node("alice", [("name", "Alice")], "Person")?;
    /// assert!(g.has_node("alice")?);
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn has_node(&self, node_id: &str) -> Result<bool> {
        let query = format!(
            "MATCH (n {{id: '{}'}}) RETURN count(n) AS cnt",
            escape_string(node_id)
        );
        let result = self.conn.cypher(&query)?;
        if result.is_empty() {
            return Ok(false);
        }
        let cnt: i64 = result[0].get("cnt").unwrap_or(0);
        Ok(cnt > 0)
    }

    /// Get a node by ID.
    ///
    /// Returns the node as a [`Value`], or `None` if not found.
    ///
    /// # Arguments
    ///
    /// * `node_id` - The unique identifier to look up
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open_in_memory()?;
    /// g.upsert_node("alice", [("name", "Alice")], "Person")?;
    /// if let Some(node) = g.get_node("alice")? {
    ///     println!("Found: {:?}", node);
    /// }
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn get_node(&self, node_id: &str) -> Result<Option<Value>> {
        let query = format!(
            "MATCH (n {{id: '{}'}}) RETURN n",
            escape_string(node_id)
        );
        let result = self.conn.cypher(&query)?;
        if result.is_empty() {
            return Ok(None);
        }
        Ok(result[0].get_value("n").cloned())
    }

    /// Create or update a node.
    ///
    /// If a node with the given id exists, its properties are updated.
    /// Otherwise, a new node is created.
    ///
    /// # Arguments
    ///
    /// * `node_id` - Unique identifier for the node
    /// * `props` - Properties as key-value pairs
    /// * `label` - Node label (only used on creation)
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open_in_memory()?;
    /// g.upsert_node("alice", [("name", "Alice"), ("age", "30")], "Person")?;
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn upsert_node<I, K, V>(&self, node_id: &str, props: I, label: &str) -> Result<()>
    where
        I: IntoIterator<Item = (K, V)>,
        K: AsRef<str>,
        V: AsRef<str>,
    {
        let props: Vec<(String, String)> = props
            .into_iter()
            .map(|(k, v)| (k.as_ref().to_string(), v.as_ref().to_string()))
            .collect();

        if self.has_node(node_id)? {
            // Update existing node
            for (k, v) in props {
                let val = format_value(&v);
                let query = format!(
                    "MATCH (n {{id: '{}'}}) SET n.{} = {} RETURN n",
                    escape_string(node_id),
                    k,
                    val
                );
                self.conn.cypher(&query)?;
            }
        } else {
            // Create new node
            let mut prop_parts = vec![format!("id: '{}'", escape_string(node_id))];
            for (k, v) in props {
                prop_parts.push(format!("{}: {}", k, format_value(&v)));
            }
            let prop_str = prop_parts.join(", ");
            let query = format!("CREATE (n:{} {{{}}})", label, prop_str);
            self.conn.cypher(&query)?;
        }
        Ok(())
    }

    /// Delete a node and all its relationships.
    ///
    /// Uses `DETACH DELETE` to remove the node and any connected edges.
    /// Does nothing if the node doesn't exist.
    ///
    /// # Arguments
    ///
    /// * `node_id` - The unique identifier of the node to delete
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open_in_memory()?;
    /// g.upsert_node("alice", [("name", "Alice")], "Person")?;
    /// g.delete_node("alice")?;
    /// assert!(!g.has_node("alice")?);
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn delete_node(&self, node_id: &str) -> Result<()> {
        let query = format!(
            "MATCH (n {{id: '{}'}}) DETACH DELETE n",
            escape_string(node_id)
        );
        self.conn.cypher(&query)?;
        Ok(())
    }

    /// Get all nodes, optionally filtered by label.
    ///
    /// # Arguments
    ///
    /// * `label` - Optional label to filter by (e.g., "Person")
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open_in_memory()?;
    /// g.upsert_node("alice", [("name", "Alice")], "Person")?;
    /// g.upsert_node("acme", [("name", "Acme Inc")], "Company")?;
    ///
    /// let all = g.get_all_nodes(None)?;        // Returns both
    /// let people = g.get_all_nodes(Some("Person"))?;  // Returns just Alice
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn get_all_nodes(&self, label: Option<&str>) -> Result<Vec<Value>> {
        let query = match label {
            Some(l) => format!("MATCH (n:{}) RETURN n", l),
            None => "MATCH (n) RETURN n".to_string(),
        };
        let result = self.conn.cypher(&query)?;
        let mut nodes = Vec::new();
        for row in result.iter() {
            if let Some(n) = row.get_value("n") {
                nodes.push(n.clone());
            }
        }
        Ok(nodes)
    }

    // -------------------------------------------------------------------------
    // Edge Operations
    // -------------------------------------------------------------------------

    /// Check if a directed edge exists from source to target.
    ///
    /// # Arguments
    ///
    /// * `source_id` - ID of the source node
    /// * `target_id` - ID of the target node
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open_in_memory()?;
    /// g.upsert_node("a", [], "Node")?;
    /// g.upsert_node("b", [], "Node")?;
    /// g.upsert_edge("a", "b", [], "CONNECTS")?;
    ///
    /// assert!(g.has_edge("a", "b")?);
    /// assert!(!g.has_edge("b", "a")?);  // Directed edge
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn has_edge(&self, source_id: &str, target_id: &str) -> Result<bool> {
        let query = format!(
            "MATCH (a {{id: '{}'}})-[r]->(b {{id: '{}'}}) RETURN count(r) AS cnt",
            escape_string(source_id),
            escape_string(target_id)
        );
        let result = self.conn.cypher(&query)?;
        if result.is_empty() {
            return Ok(false);
        }
        let cnt: i64 = result[0].get("cnt").unwrap_or(0);
        Ok(cnt > 0)
    }

    /// Get edge properties between two nodes.
    ///
    /// Returns the edge as a [`Value`], or `None` if no edge exists.
    ///
    /// # Arguments
    ///
    /// * `source_id` - ID of the source node
    /// * `target_id` - ID of the target node
    pub fn get_edge(&self, source_id: &str, target_id: &str) -> Result<Option<Value>> {
        let query = format!(
            "MATCH (a {{id: '{}'}})-[r]->(b {{id: '{}'}}) RETURN r",
            escape_string(source_id),
            escape_string(target_id)
        );
        let result = self.conn.cypher(&query)?;
        if result.is_empty() {
            return Ok(None);
        }
        Ok(result[0].get_value("r").cloned())
    }

    /// Create an edge between two nodes.
    ///
    /// If an edge already exists, this is a no-op.
    /// Both source and target nodes must exist.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open_in_memory()?;
    /// g.upsert_node("alice", [("name", "Alice")], "Person")?;
    /// g.upsert_node("bob", [("name", "Bob")], "Person")?;
    /// g.upsert_edge("alice", "bob", [("since", "2020")], "KNOWS")?;
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn upsert_edge<I, K, V>(
        &self,
        source_id: &str,
        target_id: &str,
        props: I,
        rel_type: &str,
    ) -> Result<()>
    where
        I: IntoIterator<Item = (K, V)>,
        K: AsRef<str>,
        V: AsRef<str>,
    {
        if self.has_edge(source_id, target_id)? {
            return Ok(());
        }

        let safe_rel_type = sanitize_rel_type(rel_type);
        let esc_source = escape_string(source_id);
        let esc_target = escape_string(target_id);

        let props: Vec<(String, String)> = props
            .into_iter()
            .map(|(k, v)| (k.as_ref().to_string(), v.as_ref().to_string()))
            .collect();

        let query = if props.is_empty() {
            format!(
                "MATCH (a {{id: '{}'}}), (b {{id: '{}'}}) CREATE (a)-[r:{}]->(b)",
                esc_source, esc_target, safe_rel_type
            )
        } else {
            let prop_parts: Vec<String> = props
                .iter()
                .map(|(k, v)| format!("{}: {}", k, format_value(v)))
                .collect();
            let prop_str = prop_parts.join(", ");
            format!(
                "MATCH (a {{id: '{}'}}), (b {{id: '{}'}}) CREATE (a)-[r:{} {{{}}}]->(b)",
                esc_source, esc_target, safe_rel_type, prop_str
            )
        };

        self.conn.cypher(&query)?;
        Ok(())
    }

    /// Delete the directed edge between two nodes.
    ///
    /// Does nothing if no edge exists.
    ///
    /// # Arguments
    ///
    /// * `source_id` - ID of the source node
    /// * `target_id` - ID of the target node
    pub fn delete_edge(&self, source_id: &str, target_id: &str) -> Result<()> {
        let query = format!(
            "MATCH (a {{id: '{}'}})-[r]->(b {{id: '{}'}}) DELETE r",
            escape_string(source_id),
            escape_string(target_id)
        );
        self.conn.cypher(&query)?;
        Ok(())
    }

    /// Get all edges in the graph.
    ///
    /// Returns a [`CypherResult`] with columns: `source`, `target`, `r` (edge properties).
    pub fn get_all_edges(&self) -> Result<CypherResult> {
        self.conn.cypher("MATCH (a)-[r]->(b) RETURN a.id AS source, b.id AS target, r")
    }

    // -------------------------------------------------------------------------
    // Graph Queries
    // -------------------------------------------------------------------------

    /// Get the degree (number of connections) of a node.
    ///
    /// Counts both incoming and outgoing edges.
    ///
    /// # Arguments
    ///
    /// * `node_id` - ID of the node
    pub fn node_degree(&self, node_id: &str) -> Result<i64> {
        let query = format!(
            "MATCH (n {{id: '{}'}})-[r]-() RETURN count(r) AS degree",
            escape_string(node_id)
        );
        let result = self.conn.cypher(&query)?;
        if result.is_empty() {
            return Ok(0);
        }
        Ok(result[0].get("degree").unwrap_or(0))
    }

    /// Get all neighboring nodes (connected via any edge direction).
    ///
    /// # Arguments
    ///
    /// * `node_id` - ID of the node
    ///
    /// # Returns
    ///
    /// Vector of neighboring nodes as [`Value`] objects.
    pub fn get_neighbors(&self, node_id: &str) -> Result<Vec<Value>> {
        let query = format!(
            "MATCH (n {{id: '{}'}})-[]-(m) RETURN DISTINCT m",
            escape_string(node_id)
        );
        let result = self.conn.cypher(&query)?;
        let mut neighbors = Vec::new();
        for row in result.iter() {
            if let Some(m) = row.get_value("m") {
                neighbors.push(m.clone());
            }
        }
        Ok(neighbors)
    }

    /// Get graph statistics (node and edge counts).
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open_in_memory()?;
    /// g.upsert_node("a", [], "Node")?;
    /// g.upsert_node("b", [], "Node")?;
    /// g.upsert_edge("a", "b", [], "E")?;
    ///
    /// let stats = g.stats()?;
    /// assert_eq!(stats.nodes, 2);
    /// assert_eq!(stats.edges, 1);
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn stats(&self) -> Result<GraphStats> {
        let nodes_result = self.conn.cypher("MATCH (n) RETURN count(n) AS cnt")?;
        let edges_result = self.conn.cypher("MATCH ()-[r]->() RETURN count(r) AS cnt")?;

        let nodes = if nodes_result.is_empty() {
            0
        } else {
            nodes_result[0].get("cnt").unwrap_or(0)
        };

        let edges = if edges_result.is_empty() {
            0
        } else {
            edges_result[0].get("cnt").unwrap_or(0)
        };

        Ok(GraphStats { nodes, edges })
    }

    /// Execute a raw Cypher query.
    ///
    /// Use this for complex queries not covered by the high-level API.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open_in_memory()?;
    /// let results = g.query("MATCH (n) RETURN n.name ORDER BY n.name")?;
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn query(&self, cypher: &str) -> Result<CypherResult> {
        self.conn.cypher(cypher)
    }

    // -------------------------------------------------------------------------
    // Graph Algorithms
    // -------------------------------------------------------------------------

    /// Run the PageRank algorithm to find important nodes.
    ///
    /// PageRank measures node importance based on the structure of incoming links.
    /// Nodes with many high-quality incoming connections score higher.
    ///
    /// # Arguments
    ///
    /// * `damping` - Damping factor, typically 0.85. Higher values give more weight
    ///   to link structure vs. random jumping.
    /// * `iterations` - Number of iterations, typically 20. More iterations give
    ///   more accurate results but take longer.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open_in_memory()?;
    /// // ... add nodes and edges ...
    /// let ranks = g.pagerank(0.85, 20)?;
    /// for r in &ranks {
    ///     println!("{}: {:.4}", r.node_id, r.score);
    /// }
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn pagerank(&self, damping: f64, iterations: i32) -> Result<Vec<PageRankResult>> {
        let query = format!("RETURN pageRank({}, {})", damping, iterations);
        let result = self.conn.cypher(&query)?;

        let mut ranks = Vec::new();
        for row in result.iter() {
            let node_id = row.get_value("node_id");
            let user_id = row.get_value("user_id");
            let score = row.get_value("score");

            let nid = match node_id {
                Some(Value::String(s)) => s.clone(),
                Some(Value::Integer(i)) => i.to_string(),
                _ => continue,
            };
            let uid = match user_id {
                Some(Value::String(u)) => Some(u.clone()),
                _ => None,
            };
            let score_val = match score {
                Some(Value::Float(f)) => *f,
                Some(Value::Integer(i)) => *i as f64,
                _ => 0.0,
            };
            ranks.push(PageRankResult {
                node_id: nid,
                user_id: uid,
                score: score_val,
            });
        }

        Ok(ranks)
    }

    /// Run community detection using label propagation.
    ///
    /// Label propagation identifies clusters of densely connected nodes.
    /// Nodes in the same community have the same `community` value.
    ///
    /// # Arguments
    ///
    /// * `iterations` - Number of iterations, typically 10. More iterations
    ///   allow labels to propagate further but may not improve results.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open_in_memory()?;
    /// // ... add nodes and edges ...
    /// let communities = g.community_detection(10)?;
    /// for c in &communities {
    ///     println!("Node {} in community {}", c.node_id, c.community);
    /// }
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn community_detection(&self, iterations: i32) -> Result<Vec<CommunityResult>> {
        let query = format!("RETURN labelPropagation({})", iterations);
        let result = self.conn.cypher(&query)?;

        let mut communities = Vec::new();
        for row in result.iter() {
            let node_id = row.get_value("node_id");
            let user_id = row.get_value("user_id");
            let community = row.get_value("community");

            let nid = match node_id {
                Some(Value::String(s)) => s.clone(),
                Some(Value::Integer(i)) => i.to_string(),
                _ => continue,
            };
            let uid = match user_id {
                Some(Value::String(u)) => Some(u.clone()),
                _ => None,
            };
            let comm_val = match community {
                Some(Value::Integer(i)) => *i,
                Some(Value::Float(f)) => *f as i64,
                _ => 0,
            };
            communities.push(CommunityResult {
                node_id: nid,
                user_id: uid,
                community: comm_val,
            });
        }

        Ok(communities)
    }

    // -------------------------------------------------------------------------
    // Batch Operations
    // -------------------------------------------------------------------------

    /// Batch upsert multiple nodes.
    ///
    /// More convenient than calling [`upsert_node`](Self::upsert_node) in a loop.
    ///
    /// # Arguments
    ///
    /// * `nodes` - Iterator of `(node_id, properties, label)` tuples
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open_in_memory()?;
    /// g.upsert_nodes_batch([
    ///     ("n1", [("name", "Node 1")], "Type"),
    ///     ("n2", [("name", "Node 2")], "Type"),
    /// ])?;
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn upsert_nodes_batch<I, N, P, K, V, L>(&self, nodes: I) -> Result<()>
    where
        I: IntoIterator<Item = (N, P, L)>,
        N: AsRef<str>,
        P: IntoIterator<Item = (K, V)>,
        K: AsRef<str>,
        V: AsRef<str>,
        L: AsRef<str>,
    {
        for (node_id, props, label) in nodes {
            self.upsert_node(node_id.as_ref(), props, label.as_ref())?;
        }
        Ok(())
    }

    /// Batch upsert multiple edges.
    ///
    /// More convenient than calling [`upsert_edge`](Self::upsert_edge) in a loop.
    ///
    /// # Arguments
    ///
    /// * `edges` - Iterator of `(source_id, target_id, properties, rel_type)` tuples
    ///
    /// # Example
    ///
    /// ```no_run
    /// use graphqlite::Graph;
    ///
    /// let g = Graph::open_in_memory()?;
    /// g.upsert_nodes_batch([
    ///     ("a", [], "Node"),
    ///     ("b", [], "Node"),
    ///     ("c", [], "Node"),
    /// ])?;
    /// g.upsert_edges_batch([
    ///     ("a", "b", [("weight", "1")], "CONNECTS"),
    ///     ("b", "c", [("weight", "2")], "CONNECTS"),
    /// ])?;
    /// # Ok::<(), graphqlite::Error>(())
    /// ```
    pub fn upsert_edges_batch<I, S, T, P, K, V, R>(&self, edges: I) -> Result<()>
    where
        I: IntoIterator<Item = (S, T, P, R)>,
        S: AsRef<str>,
        T: AsRef<str>,
        P: IntoIterator<Item = (K, V)>,
        K: AsRef<str>,
        V: AsRef<str>,
        R: AsRef<str>,
    {
        for (source, target, props, rel_type) in edges {
            self.upsert_edge(source.as_ref(), target.as_ref(), props, rel_type.as_ref())?;
        }
        Ok(())
    }
}

/// Format a value for inclusion in a Cypher query.
fn format_value(v: &str) -> String {
    // Try to parse as number or boolean
    if v.parse::<i64>().is_ok() || v.parse::<f64>().is_ok() {
        v.to_string()
    } else if v == "true" || v == "false" {
        v.to_string()
    } else if v == "null" {
        "null".to_string()
    } else {
        format!("'{}'", escape_string(v))
    }
}

/// Create a new Graph instance (convenience function).
///
/// # Example
///
/// ```no_run
/// use graphqlite::graph;
///
/// let g = graph(":memory:")?;
/// # Ok::<(), graphqlite::Error>(())
/// ```
pub fn graph<P: AsRef<Path>>(path: P) -> Result<Graph> {
    Graph::open(path)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_escape_string() {
        assert_eq!(escape_string("hello"), "hello");
        assert_eq!(escape_string("it's"), "it\\'s");
        assert_eq!(escape_string("line\nbreak"), "line break");
    }

    #[test]
    fn test_sanitize_rel_type() {
        assert_eq!(sanitize_rel_type("KNOWS"), "KNOWS");
        assert_eq!(sanitize_rel_type("has-items"), "has_items");
        assert_eq!(sanitize_rel_type("CREATE"), "REL_CREATE");
        assert_eq!(sanitize_rel_type("123start"), "REL_123start");
    }

    #[test]
    fn test_format_value() {
        assert_eq!(format_value("42"), "42");
        assert_eq!(format_value("3.14"), "3.14");
        assert_eq!(format_value("true"), "true");
        assert_eq!(format_value("hello"), "'hello'");
        assert_eq!(format_value("it's"), "'it\\'s'");
    }
}
