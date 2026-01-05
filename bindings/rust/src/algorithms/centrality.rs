//! Centrality algorithm implementations.

use crate::graph::Graph;
use crate::Result;
use super::{
    PageRankResult, DegreeCentralityResult, BetweennessCentralityResult,
    ClosenessCentralityResult, EigenvectorCentralityResult,
    parsing::{extract_algo_array, extract_node_id, extract_user_id, extract_float, extract_int},
};

impl Graph {
    /// Run PageRank algorithm.
    ///
    /// # Arguments
    ///
    /// * `damping` - Damping factor, typically 0.85
    /// * `iterations` - Number of iterations, typically 20
    pub fn pagerank(&self, damping: f64, iterations: i32) -> Result<Vec<PageRankResult>> {
        let query = format!("RETURN pageRank({}, {})", damping, iterations);
        let result = self.connection().cypher(&query)?;
        let rows = extract_algo_array(result.iter().collect::<Vec<_>>().as_slice());

        let mut ranks = Vec::new();
        for row in rows.iter() {
            if let Some(node_id) = extract_node_id(row) {
                ranks.push(PageRankResult {
                    node_id,
                    user_id: extract_user_id(row),
                    score: extract_float(row, "score"),
                });
            }
        }
        Ok(ranks)
    }

    /// Calculate degree centrality for all nodes.
    pub fn degree_centrality(&self) -> Result<Vec<DegreeCentralityResult>> {
        let result = self.connection().cypher("RETURN degreeCentrality()")?;
        let rows = extract_algo_array(result.iter().collect::<Vec<_>>().as_slice());

        let mut degrees = Vec::new();
        for row in rows.iter() {
            if let Some(node_id) = extract_node_id(row) {
                degrees.push(DegreeCentralityResult {
                    node_id,
                    user_id: extract_user_id(row),
                    in_degree: extract_int(row, "in_degree"),
                    out_degree: extract_int(row, "out_degree"),
                    degree: extract_int(row, "degree"),
                });
            }
        }
        Ok(degrees)
    }

    /// Calculate betweenness centrality for all nodes.
    pub fn betweenness_centrality(&self) -> Result<Vec<BetweennessCentralityResult>> {
        let result = self.connection().cypher("RETURN betweennessCentrality()")?;
        let rows = extract_algo_array(result.iter().collect::<Vec<_>>().as_slice());

        let mut scores = Vec::new();
        for row in rows.iter() {
            if let Some(node_id) = extract_node_id(row) {
                scores.push(BetweennessCentralityResult {
                    node_id,
                    user_id: extract_user_id(row),
                    score: extract_float(row, "score"),
                });
            }
        }
        Ok(scores)
    }

    /// Calculate closeness centrality for all nodes.
    pub fn closeness_centrality(&self) -> Result<Vec<ClosenessCentralityResult>> {
        let result = self.connection().cypher("RETURN closenessCentrality()")?;
        let rows = extract_algo_array(result.iter().collect::<Vec<_>>().as_slice());

        let mut scores = Vec::new();
        for row in rows.iter() {
            if let Some(node_id) = extract_node_id(row) {
                scores.push(ClosenessCentralityResult {
                    node_id,
                    user_id: extract_user_id(row),
                    score: extract_float(row, "score"),
                });
            }
        }
        Ok(scores)
    }

    /// Calculate eigenvector centrality for all nodes.
    ///
    /// # Arguments
    ///
    /// * `iterations` - Maximum iterations for power iteration (default 100)
    pub fn eigenvector_centrality(&self, iterations: i32) -> Result<Vec<EigenvectorCentralityResult>> {
        let query = format!("RETURN eigenvectorCentrality({})", iterations);
        let result = self.connection().cypher(&query)?;
        let rows = extract_algo_array(result.iter().collect::<Vec<_>>().as_slice());

        let mut scores = Vec::new();
        for row in rows.iter() {
            if let Some(node_id) = extract_node_id(row) {
                scores.push(EigenvectorCentralityResult {
                    node_id,
                    user_id: extract_user_id(row),
                    score: extract_float(row, "score"),
                });
            }
        }
        Ok(scores)
    }
}
