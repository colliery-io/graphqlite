//! Utility functions for Cypher query construction.

use crate::{Error, Result};
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
    // Control characters are escaped, not replaced: a newline or tab was
    // stored as a space (GQLITE-T-0417).
    let mut out = String::with_capacity(s.len() + 8);
    for c in s.chars() {
        match c {
            '\\' => out.push_str("\\\\"),
            '\'' => out.push_str("\\'"),
            '"' => out.push_str("\\\""),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            c if (c as u32) < 0x20 => out.push_str(&format!("\\u{:04x}", c as u32)),
            c => out.push(c),
        }
    }
    out
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

/// Returns `true` if `name` matches `^[A-Za-z_][A-Za-z0-9_]*$`.
///
/// Labels, property keys, coordinate property names, and graph names are
/// interpolated directly into Cypher/SQL (they cannot be bound as `$params`),
/// so they must be plain identifiers.
pub fn is_identifier(name: &str) -> bool {
    let mut chars = name.chars();
    match chars.next() {
        Some(c) if c.is_ascii_alphabetic() || c == '_' => {}
        _ => return false,
    }
    chars.all(|c| c.is_ascii_alphanumeric() || c == '_')
}

/// Validate that `name` is a plain identifier, returning
/// [`Error::InvalidIdentifier`] otherwise (GitHub #110).
pub fn assert_identifier(name: &str) -> Result<()> {
    if is_identifier(name) {
        Ok(())
    } else {
        Err(Error::InvalidIdentifier(name.to_string()))
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

/// Format a string as a Cypher string literal: quoted and escaped.
///
/// A string is always a string literal: `"123"` gives `'123'` and `"true"`
/// gives `'true'` (GQLITE-T-0415; before 0.10.0 they gave `123` and `true`).
/// For a typed value use [`PropertyValue::to_cypher`].
///
/// ```
/// use graphqlite::format_value;
/// assert_eq!(format_value("123"), "'123'");
/// assert_eq!(format_value("it's"), "'it\\'s'");
/// ```
pub fn format_value(v: &str) -> String {
    format!("'{}'", escape_string(v))
}

/// A typed property value for graph nodes and edges.
///
/// A string (`&str`, `String`) is always [`PropertyValue::Text`]: `"123"`
/// and `"true"` stay strings. Pass an `i64`, `f64` or `bool`, or a typed
/// variant, for other types. (Before 0.10.0 a string was typed by its text.)
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
/// // A string is text, also when it looks like a number
/// g.upsert_node("n2", [("name", "Alice"), ("code", "123")], "Person")?;
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
    /// A string is text: `"123"` stays the string `"123"` (#128). Before
    /// 0.10.0 it was typed by its text.
    fn from(s: &str) -> Self {
        PropertyValue::Text(s.to_string())
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
