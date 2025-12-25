"""Utility functions for GraphQLite."""

from typing import Any


# Cypher reserved keywords that can't be used as relationship types
CYPHER_RESERVED = {
    # Clauses
    'CREATE', 'MATCH', 'RETURN', 'WHERE', 'DELETE', 'SET', 'REMOVE',
    'ORDER', 'BY', 'SKIP', 'LIMIT', 'WITH', 'UNWIND', 'AS', 'AND', 'OR',
    'NOT', 'IN', 'IS', 'NULL', 'TRUE', 'FALSE', 'MERGE', 'ON', 'CALL',
    'YIELD', 'DETACH', 'OPTIONAL', 'UNION', 'ALL', 'CASE', 'WHEN', 'THEN',
    'ELSE', 'END', 'EXISTS', 'FOREACH',
    # Aggregate functions
    'COUNT', 'SUM', 'AVG', 'MIN', 'MAX', 'COLLECT',
    # List functions and expressions
    'REDUCE', 'FILTER', 'EXTRACT', 'ANY', 'NONE', 'SINGLE',
    # Other reserved words
    'STARTS', 'ENDS', 'CONTAINS', 'XOR', 'DISTINCT', 'LOAD', 'CSV',
    'USING', 'PERIODIC', 'COMMIT', 'CONSTRAINT', 'INDEX', 'DROP', 'ASSERT',
}


def escape_string(s: str) -> str:
    """
    Escape a string for use in Cypher queries.

    Handles backslashes, quotes, and whitespace characters.

    Args:
        s: String to escape

    Returns:
        Escaped string safe for Cypher queries
    """
    return (s.replace("\\", "\\\\")
             .replace("'", "\\'")
             .replace('"', '\\"')
             .replace("\n", " ")
             .replace("\r", " ")
             .replace("\t", " "))


def sanitize_rel_type(rel_type: str) -> str:
    """
    Sanitize a relationship type for use in Cypher.

    Ensures the type is a valid Cypher identifier and not a reserved word.

    Args:
        rel_type: Relationship type name

    Returns:
        Safe relationship type name
    """
    safe = ''.join(c if c.isalnum() or c == '_' else '_' for c in rel_type)
    if not safe or safe[0].isdigit():
        safe = "REL_" + safe
    if safe.upper() in CYPHER_RESERVED:
        safe = "REL_" + safe
    return safe


def format_props(props: dict[str, Any], escape_fn=escape_string) -> str:
    """
    Format a properties dict as a Cypher property string.

    Args:
        props: Dictionary of property key-value pairs
        escape_fn: Function to escape strings (default: escape_string)

    Returns:
        String like "key1: 'value1', key2: 123"
    """
    parts = []
    for k, v in props.items():
        if isinstance(v, str):
            parts.append(f"{k}: '{escape_fn(v)}'")
        elif isinstance(v, bool):
            parts.append(f"{k}: {str(v).lower()}")
        elif v is None:
            parts.append(f"{k}: null")
        else:
            parts.append(f"{k}: {v}")
    return ", ".join(parts)
