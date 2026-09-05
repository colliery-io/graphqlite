//! Path finding algorithm implementations.

use super::parsing::{algo_object, algo_rows, extract_float, extract_string};
use super::{AStarResult, ApspResult, ShortestPathResult};
use crate::graph::Graph;
use crate::utils::{assert_identifier, escape_string};
use crate::{Result, Value};
use std::collections::HashMap;

fn path_field(data: &HashMap<String, Value>) -> Vec<String> {
    match data.get("path") {
        Some(Value::Array(arr)) => arr
            .iter()
            .filter_map(|v| match v {
                Value::String(s) => Some(s.clone()),
                _ => None,
            })
            .collect(),
        _ => Vec::new(),
    }
}

fn distance_field(data: &HashMap<String, Value>) -> Option<f64> {
    data.get("distance").and_then(Value::as_f64)
}

fn found_field(data: &HashMap<String, Value>) -> bool {
    data.get("found").and_then(Value::as_bool).unwrap_or(false)
}

impl Graph {
    /// Find the shortest path between two nodes using Dijkstra's algorithm.
    ///
    /// # Arguments
    ///
    /// * `source_id` - ID of the source node
    /// * `target_id` - ID of the target node
    /// * `weight_property` - Optional edge property to use as weight
    pub fn shortest_path(
        &self,
        source_id: &str,
        target_id: &str,
        weight_property: Option<&str>,
    ) -> Result<ShortestPathResult> {
        let esc_source = escape_string(source_id);
        let esc_target = escape_string(target_id);

        let query = match weight_property {
            Some(wp) => format!(
                "RETURN dijkstra(\"{}\", \"{}\", \"{}\")",
                esc_source,
                esc_target,
                escape_string(wp)
            ),
            None => format!("RETURN dijkstra(\"{}\", \"{}\")", esc_source, esc_target),
        };

        let result = self.connection().cypher(&query)?;

        Ok(match algo_object(&result) {
            Some(data) => ShortestPathResult {
                path: path_field(&data),
                distance: distance_field(&data),
                found: found_field(&data),
            },
            None => ShortestPathResult {
                path: Vec::new(),
                distance: None,
                found: false,
            },
        })
    }

    /// Find shortest path using A* algorithm with heuristic guidance.
    ///
    /// # Arguments
    ///
    /// * `source_id` - Starting node's id
    /// * `target_id` - Target node's id
    /// * `lat_prop` - Optional property name for latitude (must be a plain identifier)
    /// * `lon_prop` - Optional property name for longitude (must be a plain identifier)
    ///
    /// # Errors
    ///
    /// Returns [`crate::Error::InvalidIdentifier`] if a coordinate property
    /// name is not a valid identifier; nothing is executed in that case.
    pub fn astar(
        &self,
        source_id: &str,
        target_id: &str,
        lat_prop: Option<&str>,
        lon_prop: Option<&str>,
    ) -> Result<AStarResult> {
        let esc_source = escape_string(source_id);
        let esc_target = escape_string(target_id);

        let query = match (lat_prop, lon_prop) {
            (Some(lat), Some(lon)) => {
                assert_identifier(lat)?;
                assert_identifier(lon)?;
                format!(
                    "RETURN astar('{}', '{}', '{}', '{}')",
                    esc_source, esc_target, lat, lon
                )
            }
            _ => format!("RETURN astar('{}', '{}')", esc_source, esc_target),
        };

        let result = self.connection().cypher(&query)?;

        Ok(match algo_object(&result) {
            Some(data) => AStarResult {
                path: path_field(&data),
                distance: distance_field(&data),
                found: found_field(&data),
                nodes_explored: data
                    .get("nodes_explored")
                    .and_then(Value::as_i64)
                    .unwrap_or(0),
            },
            None => AStarResult {
                path: Vec::new(),
                distance: None,
                found: false,
                nodes_explored: 0,
            },
        })
    }

    /// Compute shortest paths between all pairs of nodes.
    ///
    /// Uses Floyd-Warshall algorithm with O(V³) time complexity.
    pub fn apsp(&self) -> Result<Vec<ApspResult>> {
        let result = self.connection().cypher("RETURN apsp()")?;
        let rows = algo_rows(&result);

        let mut paths = Vec::new();
        for row in rows.iter() {
            if let (Some(source), Some(target)) =
                (extract_string(row, "source"), extract_string(row, "target"))
            {
                paths.push(ApspResult {
                    source,
                    target,
                    distance: extract_float(row, "distance"),
                });
            }
        }
        Ok(paths)
    }
}
