//! Utility functions for Cypher query construction.

use std::collections::HashSet;

/// Cypher reserved keywords that can't be used as relationship types.
pub static CYPHER_RESERVED: &[&str] = &[
    // Clauses
    "CREATE", "MATCH", "RETURN", "WHERE", "DELETE", "SET", "REMOVE",
    "ORDER", "BY", "SKIP", "LIMIT", "WITH", "UNWIND", "AS", "AND", "OR",
    "NOT", "IN", "IS", "NULL", "TRUE", "FALSE", "MERGE", "ON", "CALL",
    "YIELD", "DETACH", "OPTIONAL", "UNION", "ALL", "CASE", "WHEN", "THEN",
    "ELSE", "END", "EXISTS", "FOREACH",
    // Aggregate functions
    "COUNT", "SUM", "AVG", "MIN", "MAX", "COLLECT",
    // List functions
    "REDUCE", "FILTER", "EXTRACT", "ANY", "NONE", "SINGLE",
    // Other reserved
    "STARTS", "ENDS", "CONTAINS", "XOR", "DISTINCT", "LOAD", "CSV",
    "USING", "PERIODIC", "COMMIT", "CONSTRAINT", "INDEX", "DROP", "ASSERT",
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
        .map(|c| if c.is_alphanumeric() || c == '_' { c } else { '_' })
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

/// Format a value for inclusion in a Cypher query.
pub fn format_value(v: &str) -> String {
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
