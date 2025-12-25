//! Community detection algorithm implementations.

use crate::graph::Graph;
use crate::Result;
use super::{
    CommunityResult,
    parsing::{extract_node_id, extract_user_id, extract_int},
};

impl Graph {
    /// Run community detection using label propagation.
    ///
    /// # Arguments
    ///
    /// * `iterations` - Number of iterations, typically 10
    pub fn community_detection(&self, iterations: i32) -> Result<Vec<CommunityResult>> {
        let query = format!("RETURN labelPropagation({})", iterations);
        let result = self.connection().cypher(&query)?;

        let mut communities = Vec::new();
        for row in result.iter() {
            if let Some(node_id) = extract_node_id(row) {
                communities.push(CommunityResult {
                    node_id,
                    user_id: extract_user_id(row),
                    community: extract_int(row, "community"),
                });
            }
        }
        Ok(communities)
    }

    /// Run Louvain community detection algorithm.
    ///
    /// # Arguments
    ///
    /// * `resolution` - Resolution parameter (default 1.0). Higher = more communities.
    pub fn louvain(&self, resolution: f64) -> Result<Vec<CommunityResult>> {
        let query = format!("RETURN louvain({})", resolution);
        let result = self.connection().cypher(&query)?;

        let mut communities = Vec::new();
        for row in result.iter() {
            if let Some(node_id) = extract_node_id(row) {
                communities.push(CommunityResult {
                    node_id,
                    user_id: extract_user_id(row),
                    community: extract_int(row, "community"),
                });
            }
        }
        Ok(communities)
    }
}
