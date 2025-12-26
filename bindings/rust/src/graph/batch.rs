//! Batch operations for Graph.

use crate::Result;
use super::Graph;

impl Graph {
    /// Batch upsert multiple nodes.
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
