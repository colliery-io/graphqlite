//! Graph traversal algorithm implementations.

use crate::graph::Graph;
use crate::utils::escape_string;
use crate::Result;
use super::TraversalResult;
use super::parsing::{extract_int, extract_string};

impl Graph {
    /// Perform breadth-first search traversal from a starting node.
    ///
    /// # Arguments
    ///
    /// * `start_id` - Starting node's id
    /// * `max_depth` - Maximum depth to traverse (None for unlimited)
    pub fn bfs(&self, start_id: &str, max_depth: Option<i32>) -> Result<Vec<TraversalResult>> {
        let esc_start = escape_string(start_id);

        let query = match max_depth {
            Some(depth) => format!("RETURN bfs('{}', {})", esc_start, depth),
            None => format!("RETURN bfs('{}')", esc_start),
        };

        let result = self.connection().cypher(&query)?;

        let mut nodes = Vec::new();
        for row in result.iter() {
            if let Some(user_id) = extract_string(row, "user_id") {
                nodes.push(TraversalResult {
                    user_id,
                    depth: extract_int(row, "depth"),
                    order: extract_int(row, "order"),
                });
            }
        }
        Ok(nodes)
    }

    /// Perform depth-first search traversal from a starting node.
    ///
    /// # Arguments
    ///
    /// * `start_id` - Starting node's id
    /// * `max_depth` - Maximum depth to traverse (None for unlimited)
    pub fn dfs(&self, start_id: &str, max_depth: Option<i32>) -> Result<Vec<TraversalResult>> {
        let esc_start = escape_string(start_id);

        let query = match max_depth {
            Some(depth) => format!("RETURN dfs('{}', {})", esc_start, depth),
            None => format!("RETURN dfs('{}')", esc_start),
        };

        let result = self.connection().cypher(&query)?;

        let mut nodes = Vec::new();
        for row in result.iter() {
            if let Some(user_id) = extract_string(row, "user_id") {
                nodes.push(TraversalResult {
                    user_id,
                    depth: extract_int(row, "depth"),
                    order: extract_int(row, "order"),
                });
            }
        }
        Ok(nodes)
    }
}
