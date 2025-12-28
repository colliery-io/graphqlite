# Handle Special Characters

This guide explains how to handle special characters in property values to avoid query issues.

## The Problem

Property values containing certain characters can break Cypher query parsing:

```python
# This will cause issues
g.query("CREATE (n:Note {text: 'Line1\nLine2'})")
```

Characters that need special handling:
- Newlines (`\n`)
- Carriage returns (`\r`)
- Tabs (`\t`)
- Single quotes (`'`)
- Backslashes (`\`)

## Solution 1: Use Parameterized Queries (Recommended)

The safest approach is to use parameterized queries via the `Connection.cypher()` method:

```python
g.connection.cypher(
    "CREATE (n:Note {text: $text})",
    {"text": "Line1\nLine2"}
)
```

Parameters are properly escaped automatically.

## Solution 2: Use the Graph API

The high-level `Graph` API handles escaping for you:

```python
g.upsert_node("note1", {"text": "Line1\nLine2"}, label="Note")
```

## Solution 3: Manual Escaping

If you must build queries manually, escape problematic characters:

```python
def escape_for_cypher(value: str) -> str:
    """Escape a string for use in Cypher property values."""
    return (value
        .replace("\\", "\\\\")   # Backslashes first
        .replace("'", "\\'")      # Single quotes
        .replace("\n", " ")       # Newlines
        .replace("\r", " ")       # Carriage returns
        .replace("\t", " "))      # Tabs

text = escape_for_cypher("Line1\nLine2")
g.query(f"CREATE (n:Note {{text: '{text}'}})")
```

## Common Symptoms

### Nodes exist but MATCH returns nothing

**Symptom**: You insert nodes and can verify they exist with raw SQL (`SELECT * FROM nodes`), but `MATCH (n) RETURN n` returns empty results.

**Cause**: Newlines or other control characters in property values break the query.

**Solution**: Use parameterized queries or escape the values.

### Query syntax errors

**Symptom**: `SyntaxError` when creating nodes with text content.

**Cause**: Unescaped single quotes in the value.

**Solution**: Escape quotes or use parameters:

```python
# Wrong
g.query("CREATE (n:Quote {text: 'It's a test'})")

# Right - escape the quote
g.query("CREATE (n:Quote {text: 'It\\'s a test'})")

# Better - use parameters
g.connection.cypher("CREATE (n:Quote {text: $text})", {"text": "It's a test"})
```

## Best Practices

1. **Always use parameterized queries** for user-provided data
2. **Use the Graph API** for simple CRUD operations
3. **Validate input** before storing if using raw queries
4. **Consider replacing control characters** with spaces or removing them entirely if they're not meaningful
