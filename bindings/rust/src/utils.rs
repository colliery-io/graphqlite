//! Utility functions for Cypher query construction.

use std::collections::HashSet;

/// Cypher reserved keywords that can't be used as relationship types.
pub static CYPHER_RESERVED: &[&str] = &[
    // Clauses
    "CREATE",
    "MATCH",
    "RETURN",
    "WHERE",
    "DELETE",
    "SET",
    "REMOVE",
    "ORDER",
    "BY",
    "SKIP",
    "LIMIT",
    "WITH",
    "UNWIND",
    "AS",
    "AND",
    "OR",
    "NOT",
    "IN",
    "IS",
    "NULL",
    "TRUE",
    "FALSE",
    "MERGE",
    "ON",
    "CALL",
    "YIELD",
    "DETACH",
    "OPTIONAL",
    "UNION",
    "ALL",
    "CASE",
    "WHEN",
    "THEN",
    "ELSE",
    "END",
    "EXISTS",
    "FOREACH",
    // Aggregate functions
    "COUNT",
    "SUM",
    "AVG",
    "MIN",
    "MAX",
    "COLLECT",
    // List functions
    "REDUCE",
    "FILTER",
    "EXTRACT",
    "ANY",
    "NONE",
    "SINGLE",
    // Other reserved
    "STARTS",
    "ENDS",
    "CONTAINS",
    "XOR",
    "DISTINCT",
    "LOAD",
    "CSV",
    "USING",
    "PERIODIC",
    "COMMIT",
    "CONSTRAINT",
    "INDEX",
    "DROP",
    "ASSERT",
];

/// Escape a string for use in Cypher queries.
///
/// Handles backslashes, quotes, and whitespace characters.
pub fn escape_string(s: &str) -> String {
    s.replace('\\', "\\\\")
        .replace('\'', "\\'")
        .replace('"', "\\\"")
        .replace(['\n', '\r', '\t'], " ")
}

/// Sanitize a relationship type for use in Cypher.
///
/// Ensures the type is a valid Cypher identifier and not a reserved word.
pub fn sanitize_rel_type(rel_type: &str) -> String {
    let safe: String = rel_type
        .chars()
        .map(|c| {
            if c.is_alphanumeric() || c == '_' {
                c
            } else {
                '_'
            }
        })
        .collect();

    let safe = if safe.is_empty() || safe.chars().next().is_some_and(|c| c.is_numeric()) {
        format!("REL_{}", safe)
    } else {
        safe
    };

    let reserved: HashSet<&str> = CYPHER_RESERVED.iter().copied().collect();
    if reserved.contains(safe.to_uppercase().as_str()) {
        format!("REL_{}", safe)
    } else {
        safe
    }
}

/// Build a Cypher relationship type pattern fragment.
///
/// Returns `":TYPE"` when a type is given, or an empty string for wildcard matching.
pub fn rel_type_pattern(rel_type: Option<&str>) -> String {
    match rel_type {
        Some(rt) => format!(":{}", sanitize_rel_type(rt)),
        None => String::new(),
    }
}

/// Check if a string has a leading zero that would be lost in numeric parsing.
///
/// Returns `true` for strings like "02134", "007", "-0123" that would lose
/// their leading zeros if parsed as numbers. Returns `false` for "0", "0.5",
/// "-0.5", "42", etc.
fn has_leading_zero(s: &str) -> bool {
    let digits = s.strip_prefix('-').unwrap_or(s);
    digits.len() > 1 && digits.starts_with('0') && !digits.starts_with("0.")
}

/// Format a value for inclusion in a Cypher query.
pub fn format_value(v: &str) -> String {
    // Preserve leading-zero strings as text (e.g., zip codes "02134")
    if has_leading_zero(v) {
        return format!("'{}'", escape_string(v));
    }
    // Numbers, booleans, and null pass through as-is
    if v.parse::<i64>().is_ok()
        || v.parse::<f64>().is_ok()
        || v == "true"
        || v == "false"
        || v == "null"
    {
        v.to_string()
    } else {
        format!("'{}'", escape_string(v))
    }
}

/// A typed property value for graph nodes and edges.
///
/// Use this instead of raw strings when you need explicit type control
/// (e.g., to preserve a string like `"02134"` that would otherwise be
/// auto-detected as an integer).
///
/// # Example
///
/// ```no_run
/// use graphqlite::{Graph, PropertyValue};
///
/// let g = Graph::open_in_memory()?;
///
/// // Typed values — no ambiguity
/// g.upsert_node("n1", [
///     ("score", PropertyValue::Float(0.87)),
///     ("count", PropertyValue::Integer(42)),
///     ("active", PropertyValue::Bool(true)),
///     ("zipcode", PropertyValue::Text("02134".into())),
/// ], "Thing")?;
///
/// // String pairs still work — auto-detected like before
/// g.upsert_node("n2", [("name", "Alice")], "Person")?;
/// # Ok::<(), graphqlite::Error>(())
/// ```
#[derive(Debug, Clone, PartialEq)]
pub enum PropertyValue {
    /// A text/string value.
    Text(String),
    /// A 64-bit integer value.
    Integer(i64),
    /// A 64-bit floating-point value.
    Float(f64),
    /// A boolean value.
    Bool(bool),
}

impl PropertyValue {
    /// Format this value for embedding in a Cypher query string.
    pub fn to_cypher(&self) -> String {
        match self {
            PropertyValue::Text(s) => format!("'{}'", escape_string(s)),
            PropertyValue::Integer(v) => v.to_string(),
            PropertyValue::Float(v) => {
                let s = v.to_string();
                // Ensure floats always have a decimal point for Cypher
                if s.contains('.') {
                    s
                } else {
                    format!("{}.0", s)
                }
            }
            PropertyValue::Bool(v) => v.to_string(),
        }
    }
}

impl From<&str> for PropertyValue {
    /// Auto-detect type from string (backward-compatible behavior).
    ///
    /// Parses as integer, then float, then boolean, falling back to text.
    /// Leading-zero strings like `"02134"` are preserved as text.
    fn from(s: &str) -> Self {
        // Preserve leading-zero strings (zip codes, padded IDs, etc.)
        if has_leading_zero(s) {
            return PropertyValue::Text(s.to_string());
        }
        if let Ok(v) = s.parse::<i64>() {
            return PropertyValue::Integer(v);
        }
        if let Ok(v) = s.parse::<f64>() {
            return PropertyValue::Float(v);
        }
        match s {
            "true" => PropertyValue::Bool(true),
            "false" => PropertyValue::Bool(false),
            _ => PropertyValue::Text(s.to_string()),
        }
    }
}

impl From<String> for PropertyValue {
    fn from(s: String) -> Self {
        PropertyValue::from(s.as_str())
    }
}

impl From<&String> for PropertyValue {
    fn from(s: &String) -> Self {
        PropertyValue::from(s.as_str())
    }
}

impl From<i64> for PropertyValue {
    fn from(v: i64) -> Self {
        PropertyValue::Integer(v)
    }
}

impl From<i32> for PropertyValue {
    fn from(v: i32) -> Self {
        PropertyValue::Integer(v as i64)
    }
}

impl From<f64> for PropertyValue {
    fn from(v: f64) -> Self {
        PropertyValue::Float(v)
    }
}

impl From<f32> for PropertyValue {
    fn from(v: f32) -> Self {
        PropertyValue::Float(v as f64)
    }
}

impl From<bool> for PropertyValue {
    fn from(v: bool) -> Self {
        PropertyValue::Bool(v)
    }
}

impl From<usize> for PropertyValue {
    fn from(v: usize) -> Self {
        PropertyValue::Integer(v as i64)
    }
}
