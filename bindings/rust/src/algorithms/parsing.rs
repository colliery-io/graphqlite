//! Shared parsing helpers for algorithm results.

use crate::{Row, Value};

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
