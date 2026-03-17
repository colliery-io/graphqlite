//! Query operations for Graph.

use crate::utils::{escape_string, rel_type_pattern};
use crate::{CypherResult, Result, Value};
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

        Ok(GraphStats { node_count: nodes, edge_count: edges })
    }

    /// Get all outgoing edges from a node.
    ///
    /// Returns a [`CypherResult`] with columns: `source`, `target`, `r`.
    pub fn get_edges_from(&self, node_id: &str) -> Result<CypherResult> {
        let query = format!(
            "MATCH (a {{id: '{}'}})-[r]->(b) RETURN a.id AS source, b.id AS target, r",
            escape_string(node_id)
        );
        self.connection().cypher(&query)
    }

    /// Get all incoming edges to a node.
    ///
    /// Returns a [`CypherResult`] with columns: `source`, `target`, `r`.
    pub fn get_edges_to(&self, node_id: &str) -> Result<CypherResult> {
        let query = format!(
            "MATCH (a)-[r]->(b {{id: '{}'}}) RETURN a.id AS source, b.id AS target, r",
            escape_string(node_id)
        );
        self.connection().cypher(&query)
    }

    /// Get outgoing edges of a specific type from a node.
    ///
    /// Returns a [`CypherResult`] with columns: `source`, `target`, `r`.
    pub fn get_edges_by_type(&self, node_id: &str, rel_type: &str) -> Result<CypherResult> {
        let rel = rel_type_pattern(Some(rel_type));
        let query = format!(
            "MATCH (a {{id: '{}'}})-[r{}]->(b) RETURN a.id AS source, b.id AS target, r",
            escape_string(node_id),
            rel
        );
        self.connection().cypher(&query)
    }

    /// Get all edges (both directions) connected to a node.
    ///
    /// Returns a [`CypherResult`] with columns: `source`, `target`, `r`.
    pub fn get_node_edges(&self, node_id: &str) -> Result<CypherResult> {
        let query = format!(
            "MATCH (n {{id: '{}'}})-[r]-(m) RETURN n.id AS source, m.id AS target, r",
            escape_string(node_id)
        );
        self.connection().cypher(&query)
    }
}
