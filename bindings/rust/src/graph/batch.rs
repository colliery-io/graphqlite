//! Batch operations for Graph.
//!
//! These methods provide convenient batch upsert operations using Cypher MERGE semantics.
//! For high-performance atomic batch inserts, use the bulk insert methods instead.

use crate::Result;
use super::Graph;

impl Graph {
    /// Batch upsert multiple nodes.
    ///
    /// Convenience method that calls `upsert_node` for each item.
    /// Uses Cypher MERGE semantics (update if exists, create if not).
    ///
    /// # Note
    ///
    /// This method does NOT provide atomicity - if an operation fails partway
    /// through, earlier operations will have already completed. For atomic
    /// batch inserts, use [`insert_nodes_bulk`](Self::insert_nodes_bulk) instead.
    ///
    /// # Example
    ///
    /// ```no_run
    /// # use graphqlite::Graph;
    /// let g = Graph::open_in_memory()?;
    /// g.upsert_nodes_batch([
    ///     ("n1", [("name", "Node1")], "Type"),
    ///     ("n2", [("name", "Node2")], "Type"),
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
    /// Convenience method that calls `upsert_edge` for each item.
    /// Uses Cypher MERGE semantics (update if exists, create if not).
    ///
    /// # Note
    ///
    /// This method does NOT provide atomicity - if an operation fails partway
    /// through, earlier operations will have already completed. For atomic
    /// batch inserts, use [`insert_edges_bulk`](Self::insert_edges_bulk) instead.
    ///
    /// # Example
    ///
    /// ```no_run
    /// # use graphqlite::Graph;
    /// let g = Graph::open_in_memory()?;
    /// g.upsert_nodes_batch([
    ///     ("n1", [("name", "Node1")], "Type"),
    ///     ("n2", [("name", "Node2")], "Type"),
    /// ])?;
    /// g.upsert_edges_batch([
    ///     ("n1", "n2", [("weight", "1.0")], "CONNECTS"),
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
