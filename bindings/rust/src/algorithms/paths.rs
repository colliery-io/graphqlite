//! Path finding algorithm implementations.

use crate::graph::Graph;
use crate::utils::escape_string;
use crate::{Result, Value};
use super::{ShortestPathResult, AStarResult, ApspResult};
use super::parsing::extract_float;

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
                esc_source, esc_target, escape_string(wp)
            ),
            None => format!("RETURN dijkstra(\"{}\", \"{}\")", esc_source, esc_target),
        };

        let result = self.connection().cypher(&query)?;

        if result.is_empty() {
            return Ok(ShortestPathResult {
                path: Vec::new(),
                distance: None,
                found: false,
            });
        }

        let row = &result[0];

        // Handle nested column_0 structure
        if let Some(Value::Object(data)) = row.get_value("column_0") {
            let path = data.get("path")
                .and_then(|v| match v {
                    Value::Array(arr) => Some(
                        arr.iter()
                            .filter_map(|v| match v {
                                Value::String(s) => Some(s.clone()),
                                _ => None,
                            })
                            .collect()
                    ),
                    _ => None,
                })
                .unwrap_or_default();

            let distance = data.get("distance").and_then(|v| match v {
                Value::Float(f) => Some(*f),
                Value::Integer(i) => Some(*i as f64),
                _ => None,
            });

            let found = data.get("found")
                .and_then(|v| match v {
                    Value::Bool(b) => Some(*b),
                    _ => None,
                })
                .unwrap_or(false);

            return Ok(ShortestPathResult { path, distance, found });
        }

        // Direct access
        let path = row.get_value("path")
            .and_then(|v| match v {
                Value::Array(arr) => Some(
                    arr.iter()
                        .filter_map(|v| match v {
                            Value::String(s) => Some(s.clone()),
                            _ => None,
                        })
                        .collect()
                ),
                _ => None,
            })
            .unwrap_or_default();

        let distance = row.get_value("distance").and_then(|v| match v {
            Value::Float(f) => Some(*f),
            Value::Integer(i) => Some(*i as f64),
            _ => None,
        });

        let found = row.get_value("found")
            .and_then(|v| match v {
                Value::Bool(b) => Some(*b),
                _ => None,
            })
            .unwrap_or(false);

        Ok(ShortestPathResult { path, distance, found })
    }

    /// Find shortest path using A* algorithm with heuristic guidance.
    ///
    /// # Arguments
    ///
    /// * `source_id` - Starting node's id
    /// * `target_id` - Target node's id
    /// * `lat_prop` - Optional property name for latitude
    /// * `lon_prop` - Optional property name for longitude
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
            (Some(lat), Some(lon)) => format!(
                "RETURN astar('{}', '{}', '{}', '{}')",
                esc_source, esc_target, lat, lon
            ),
            _ => format!("RETURN astar('{}', '{}')", esc_source, esc_target),
        };

        let result = self.connection().cypher(&query)?;

        if result.is_empty() {
            return Ok(AStarResult {
                path: Vec::new(),
                distance: None,
                found: false,
                nodes_explored: 0,
            });
        }

        let row = &result[0];

        let path = row.get_value("path")
            .and_then(|v| match v {
                Value::Array(arr) => Some(
                    arr.iter()
                        .filter_map(|v| match v {
                            Value::String(s) => Some(s.clone()),
                            _ => None,
                        })
                        .collect()
                ),
                _ => None,
            })
            .unwrap_or_default();

        let distance = row.get_value("distance").and_then(|v| match v {
            Value::Float(f) => Some(*f),
            Value::Integer(i) => Some(*i as f64),
            _ => None,
        });

        let found = row.get_value("found")
            .and_then(|v| match v {
                Value::Bool(b) => Some(*b),
                _ => None,
            })
            .unwrap_or(false);

        let nodes_explored = row.get_value("nodes_explored")
            .map(|v| match v {
                Value::Integer(i) => *i,
                _ => 0,
            })
            .unwrap_or(0);

        Ok(AStarResult { path, distance, found, nodes_explored })
    }

    /// Compute shortest paths between all pairs of nodes.
    ///
    /// Uses Floyd-Warshall algorithm with O(V³) time complexity.
    pub fn apsp(&self) -> Result<Vec<ApspResult>> {
        let result = self.connection().cypher("RETURN apsp()")?;

        let mut paths = Vec::new();
        for row in result.iter() {
            let source = row.get_value("source").and_then(|v| match v {
                Value::String(s) => Some(s.clone()),
                _ => None,
            });
            let target = row.get_value("target").and_then(|v| match v {
                Value::String(s) => Some(s.clone()),
                _ => None,
            });

            if let (Some(source), Some(target)) = (source, target) {
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
