//! Shared parsing helpers for algorithm results.

use crate::{Row, Value};

/// Extract a wrapped array result from graph algorithms.
///
/// Graph algorithms return results in one of two formats:
/// 1. Old format: Multiple rows with fields directly accessible
/// 2. New format: Single row with a column containing an array of objects
///
/// This function detects the new format and extracts the array elements,
/// converting each object into a Row for consistent processing.
pub(crate) fn extract_algo_array(result: &[&Row]) -> Vec<Row> {
    // If multiple rows, assume old format - clone and return
    if result.len() != 1 {
        return result.iter().map(|r| (*r).clone()).collect();
    }

    // Single row - check if it has an array column
    let row = result[0];

    // Try common column names for wrapped array results
    for col_name in ["column_0", "wcc()", "scc()", "pagerank()", "degree_centrality()",
                     "betweenness_centrality()", "closeness_centrality()", "eigenvector_centrality()",
                     "labelPropagation()", "louvain()"] {
        if let Some(Value::Array(arr)) = row.get_value(col_name) {
            // Convert array of objects to Vec<Row>
            return arr.iter().filter_map(|v| {
                if let Value::Object(obj) = v {
                    // Convert HashMap<String, Value> to Row
                    Some(Row::from_map(obj.clone()))
                } else {
                    None
                }
            }).collect();
        }
    }

    // No array column found, return cloned original result
    result.iter().map(|r| (*r).clone()).collect()
}

/// Extract node_id from a result row.
pub(crate) fn extract_node_id(row: &Row) -> Option<String> {
    row.get_value("node_id").and_then(|v| match v {
        Value::Integer(i) => Some(i.to_string()),
        Value::String(s) => Some(s.clone()),
        _ => None,
    })
}

/// Extract user_id from a result row.
pub(crate) fn extract_user_id(row: &Row) -> Option<String> {
    row.get_value("user_id").and_then(|v| match v {
        Value::String(s) => Some(s.clone()),
        Value::Integer(i) => Some(i.to_string()),
        _ => None,
    })
}

/// Extract a float score from a result row.
pub(crate) fn extract_float(row: &Row, field: &str) -> f64 {
    row.get_value(field)
        .map(|v| match v {
            Value::Float(f) => *f,
            Value::Integer(i) => *i as f64,
            _ => 0.0,
        })
        .unwrap_or(0.0)
}

/// Extract an integer value from a result row.
pub(crate) fn extract_int(row: &Row, field: &str) -> i64 {
    row.get_value(field)
        .map(|v| match v {
            Value::Integer(i) => *i,
            Value::Float(f) => *f as i64,
            _ => 0,
        })
        .unwrap_or(0)
}

/// Extract a string value from a result row.
pub(crate) fn extract_string(row: &Row, field: &str) -> Option<String> {
    row.get_value(field).and_then(|v| match v {
        Value::String(s) => Some(s.clone()),
        Value::Integer(i) => Some(i.to_string()),
        _ => None,
    })
}
