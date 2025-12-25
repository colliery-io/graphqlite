//! Query operations for Graph.

use crate::utils::escape_string;
use crate::{Result, Value};
use super::{Graph, GraphStats};

impl Graph {
    /// Get the degree (number of connections) of a node.
    pub fn node_degree(&self, node_id: &str) -> Result<i64> {
        let query = format!(
            "MATCH (n {{id: '{}'}})-[r]-() RETURN count(r) AS degree",
            escape_string(node_id)
        );
        let result = self.connection().cypher(&query)?;
        if result.is_empty() {
            return Ok(0);
        }
        Ok(result[0].get("degree").unwrap_or(0))
    }

    /// Get all neighboring nodes (connected via any edge direction).
    pub fn get_neighbors(&self, node_id: &str) -> Result<Vec<Value>> {
        let query = format!(
            "MATCH (n {{id: '{}'}})-[]-(m) RETURN DISTINCT m",
            escape_string(node_id)
        );
        let result = self.connection().cypher(&query)?;
        let mut neighbors = Vec::new();
        for row in result.iter() {
            if let Some(m) = row.get_value("m") {
                neighbors.push(m.clone());
            }
        }
        Ok(neighbors)
    }

    /// Get graph statistics (node and edge counts).
    pub fn stats(&self) -> Result<GraphStats> {
        let nodes_result = self.connection().cypher("MATCH (n) RETURN count(n) AS cnt")?;
        let edges_result = self.connection().cypher("MATCH ()-[r]->() RETURN count(r) AS cnt")?;

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
}
