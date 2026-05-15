//! Query operations for Graph.

use super::{Graph, GraphStats};
use crate::utils::rel_type_pattern;
use crate::{CypherResult, Result, Value};

impl Graph {
    /// Get the degree (number of connections) of a node.
    pub fn node_degree(&self, node_id: &str) -> Result<i64> {
        let result = self
            .connection()
            .cypher_builder("MATCH (n {id: $id})-[r]-() RETURN count(r) AS degree")
            .param("id", node_id)
            .run()?;
        if result.is_empty() {
            return Ok(0);
        }
        Ok(result[0].get("degree").unwrap_or(0))
    }

    /// Get all neighboring nodes (connected via any edge direction).
    pub fn get_neighbors(&self, node_id: &str) -> Result<Vec<Value>> {
        let result = self
            .connection()
            .cypher_builder("MATCH (n {id: $id})-[]-(m) RETURN DISTINCT m")
            .param("id", node_id)
            .run()?;
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
        let nodes_result = self
            .connection()
            .cypher("MATCH (n) RETURN count(n) AS cnt")?;
        let edges_result = self
            .connection()
            .cypher("MATCH ()-[r]->() RETURN count(r) AS cnt")?;

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

        Ok(GraphStats {
            node_count: nodes,
            edge_count: edges,
        })
    }

    /// Get all outgoing edges from a node.
    ///
    /// Returns a [`CypherResult`] with columns: `source`, `target`, `r`.
    pub fn get_edges_from(&self, node_id: &str) -> Result<CypherResult> {
        self.connection()
            .cypher_builder("MATCH (a {id: $id})-[r]->(b) RETURN a.id AS source, b.id AS target, r")
            .param("id", node_id)
            .run()
    }

    /// Get all incoming edges to a node.
    ///
    /// Returns a [`CypherResult`] with columns: `source`, `target`, `r`.
    pub fn get_edges_to(&self, node_id: &str) -> Result<CypherResult> {
        self.connection()
            .cypher_builder("MATCH (a)-[r]->(b {id: $id}) RETURN a.id AS source, b.id AS target, r")
            .param("id", node_id)
            .run()
    }

    /// Get outgoing edges of a specific type from a node.
    ///
    /// Returns a [`CypherResult`] with columns: `source`, `target`, `r`.
    pub fn get_edges_by_type(&self, node_id: &str, rel_type: &str) -> Result<CypherResult> {
        let rel = rel_type_pattern(Some(rel_type));
        let query = format!(
            "MATCH (a {{id: $id}})-[r{}]->(b) RETURN a.id AS source, b.id AS target, r",
            rel
        );
        self.connection()
            .cypher_builder(&query)
            .param("id", node_id)
            .run()
    }

    /// Get all edges (both directions) connected to a node.
    ///
    /// Returns a [`CypherResult`] with columns: `source`, `target`, `r`.
    pub fn get_node_edges(&self, node_id: &str) -> Result<CypherResult> {
        self.connection()
            .cypher_builder("MATCH (n {id: $id})-[r]-(m) RETURN n.id AS source, m.id AS target, r")
            .param("id", node_id)
            .run()
    }
}
