//! Connected components algorithm implementations.

use crate::graph::Graph;
use crate::Result;
use super::{
    ComponentResult,
    parsing::{extract_node_id, extract_user_id, extract_int},
};

impl Graph {
    /// Find weakly connected components in the graph.
    ///
    /// Treats the graph as undirected and finds connected components.
    pub fn wcc(&self) -> Result<Vec<ComponentResult>> {
        let result = self.connection().cypher("RETURN wcc()")?;

        let mut components = Vec::new();
        for row in result.iter() {
            if let Some(node_id) = extract_node_id(row) {
                components.push(ComponentResult {
                    node_id,
                    user_id: extract_user_id(row),
                    component: extract_int(row, "component"),
                });
            }
        }
        Ok(components)
    }

    /// Find strongly connected components in the graph.
    ///
    /// Finds maximal subgraphs where every node is reachable from every
    /// other node following edge directions. Uses Tarjan's algorithm.
    pub fn scc(&self) -> Result<Vec<ComponentResult>> {
        let result = self.connection().cypher("RETURN scc()")?;

        let mut components = Vec::new();
        for row in result.iter() {
            if let Some(node_id) = extract_node_id(row) {
                components.push(ComponentResult {
                    node_id,
                    user_id: extract_user_id(row),
                    component: extract_int(row, "component"),
                });
            }
        }
        Ok(components)
    }
}
