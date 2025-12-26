//! Similarity algorithm implementations.

use crate::graph::Graph;
use crate::utils::escape_string;
use crate::Result;
use super::{NodeSimilarityResult, KnnResult, TriangleCountResult};
use super::parsing::{extract_node_id, extract_user_id, extract_float, extract_int, extract_string};

impl Graph {
    /// Compute node similarity using Jaccard coefficient.
    ///
    /// # Arguments
    ///
    /// * `node1_id` - First node's id (optional)
    /// * `node2_id` - Second node's id (optional)
    /// * `threshold` - Minimum similarity to include (default 0.0)
    /// * `top_k` - Maximum pairs to return (0 = unlimited)
    pub fn node_similarity(
        &self,
        node1_id: Option<&str>,
        node2_id: Option<&str>,
        threshold: f64,
        top_k: i32,
    ) -> Result<Vec<NodeSimilarityResult>> {
        let query = match (node1_id, node2_id) {
            (Some(n1), Some(n2)) => {
                format!(
                    "RETURN nodeSimilarity('{}', '{}')",
                    escape_string(n1),
                    escape_string(n2)
                )
            }
            _ if threshold > 0.0 && top_k > 0 => {
                format!("RETURN nodeSimilarity({}, {})", threshold, top_k)
            }
            _ if threshold > 0.0 => {
                format!("RETURN nodeSimilarity({})", threshold)
            }
            _ => "RETURN nodeSimilarity()".to_string(),
        };

        let result = self.connection().cypher(&query)?;

        let mut pairs = Vec::new();
        for row in result.iter() {
            let node1 = extract_string(row, "node1");
            let node2 = extract_string(row, "node2");

            if let (Some(n1), Some(n2)) = (node1, node2) {
                pairs.push(NodeSimilarityResult {
                    node1: n1,
                    node2: n2,
                    similarity: extract_float(row, "similarity"),
                });
            }
        }
        Ok(pairs)
    }

    /// Find K-nearest neighbors using Jaccard similarity.
    ///
    /// # Arguments
    ///
    /// * `node_id` - The node's id
    /// * `k` - Number of neighbors to return
    pub fn knn(&self, node_id: &str, k: i32) -> Result<Vec<KnnResult>> {
        let query = format!("RETURN knn('{}', {})", escape_string(node_id), k);
        let result = self.connection().cypher(&query)?;

        let mut neighbors = Vec::new();
        for row in result.iter() {
            if let Some(neighbor) = extract_string(row, "neighbor") {
                neighbors.push(KnnResult {
                    neighbor,
                    similarity: extract_float(row, "similarity"),
                    rank: extract_int(row, "rank"),
                });
            }
        }
        Ok(neighbors)
    }

    /// Count triangles each node participates in.
    pub fn triangle_count(&self) -> Result<Vec<TriangleCountResult>> {
        let result = self.connection().cypher("RETURN triangleCount()")?;

        let mut triangles = Vec::new();
        for row in result.iter() {
            if let Some(node_id) = extract_node_id(row) {
                triangles.push(TriangleCountResult {
                    node_id,
                    user_id: extract_user_id(row),
                    triangles: extract_int(row, "triangles"),
                    clustering_coefficient: extract_float(row, "clustering_coefficient"),
                });
            }
        }
        Ok(triangles)
    }
}
