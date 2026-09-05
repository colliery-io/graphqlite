//! Shared parsing helpers for algorithm results.

use crate::{CypherResult, Row, Value};
use std::collections::HashMap;

/// The core returns every graph-algorithm result as a single row whose only
/// column is `column_0` (verified for all 18 algorithm functions).
const ALGO_COLUMN: &str = "column_0";

/// Unwrap a list-valued algorithm result into rows.
///
/// This and [`algo_object`] are the *only* places the `column_0` wrapper is
/// unwrapped; every algorithm method must go through one of them
/// (GitHub #104/#105/#106). If the result is a single row whose `column_0`
/// is an array, its object elements become rows; otherwise the rows are
/// returned unchanged.
pub(crate) fn algo_rows(result: &CypherResult) -> Vec<Row> {
    if result.len() == 1 {
        if let Some(Value::Array(arr)) = result[0].get_value(ALGO_COLUMN) {
            return arr
                .iter()
                .filter_map(|v| match v {
                    Value::Object(obj) => Some(Row::from_map(obj.clone())),
                    _ => None,
                })
                .collect();
        }
    }
    result.iter().cloned().collect()
}

/// Unwrap an object-valued algorithm result (dijkstra, astar) into a map.
///
/// Returns `None` when the result has no rows. If the single row's
/// `column_0` is an object, that object is returned; otherwise the row's own
/// columns are returned so callers can read fields directly.
pub(crate) fn algo_object(result: &CypherResult) -> Option<HashMap<String, Value>> {
    let row = result.get(0)?;
    if let Some(Value::Object(obj)) = row.get_value(ALGO_COLUMN) {
        return Some(obj.clone());
    }
    Some(
        row.columns()
            .iter()
            .filter_map(|c| row.get_value(c).map(|v| (c.clone(), v.clone())))
            .collect(),
    )
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
