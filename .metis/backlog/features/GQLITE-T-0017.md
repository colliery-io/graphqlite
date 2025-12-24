---
id: allow-backtick-quoting-for
level: task
title: "Allow backtick-quoting for reserved words as relationship types"
short_code: "GQLITE-T-0017"
created_at: 2025-12-24T15:24:25.202508+00:00
updated_at: 2025-12-24T18:38:18.531244+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#feature"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# Allow backtick-quoting for reserved words as relationship types

## Objective

Support backtick-quoted identifiers in Cypher, allowing reserved words and special characters to be used as relationship types, labels, and property names.

## Backlog Item Details

### Type
- [x] Feature - New functionality or enhancement  

### Priority
- [x] P3 - Low (when time permits)

### Business Justification
- **User Value**: Freedom to use any naming convention without worrying about reserved word conflicts. Better compatibility with data imported from other systems.
- **Business Value**: Standard Cypher feature. Removes artificial naming restrictions.
- **Effort Estimate**: S

## Problem Statement

Currently, relationship types that conflict with Cypher reserved words cause parse errors. The nanographrag demo works around this by prefixing problematic names:

```python
# Current workaround
CYPHER_RESERVED = {
    'CREATE', 'MATCH', 'RETURN', 'WHERE', 'DELETE', 'SET', 'REMOVE',
    'ORDER', 'BY', 'SKIP', 'LIMIT', 'WITH', 'UNWIND', 'AS', 'AND', 'OR',
    'NOT', 'IN', 'IS', 'NULL', 'TRUE', 'FALSE', 'MERGE', 'ON', 'CALL',
    ...
}

def upsert_edge(self, source_id, target_id, edge_data, rel_type):
    # Sanitize rel_type to be a valid Cypher identifier
    safe_rel_type = ''.join(c if c.isalnum() or c == '_' else '_' for c in rel_type)
    if not safe_rel_type or safe_rel_type[0].isdigit():
        safe_rel_type = "REL_" + safe_rel_type
    # Avoid reserved keywords
    if safe_rel_type.upper() in self.CYPHER_RESERVED:
        safe_rel_type = "REL_" + safe_rel_type
```

This means:
- Can't use natural names like `IN`, `ON`, `AS` for relationships
- Data must be transformed/sanitized before storage
- Names don't round-trip faithfully

## Desired API

```cypher
-- Use reserved word as relationship type
CREATE (a)-[r:`IN`]->(b)
MATCH (a)-[:`ORDER`]->(b) RETURN a, b

-- Use reserved word as label
CREATE (n:`Match` {name: 'test'})

-- Use reserved word as property name
CREATE (n {`return`: 'value', `order`: 1})

-- Use special characters in names
CREATE (a)-[:`has-part`]->(b)
CREATE (n:`My Label` {`full name`: 'John Doe'})
```

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] Backtick-quoted strings parsed as identifiers, not string literals
- [ ] Reserved words work as relationship types when backtick-quoted
- [ ] Reserved words work as labels when backtick-quoted
- [ ] Reserved words work as property names when backtick-quoted
- [ ] Spaces and special characters allowed inside backticks
- [ ] Backticks can be escaped with double-backtick (``` `` ```)
- [ ] Unquoted identifiers continue to work as before
- [ ] Unit tests cover all reserved words as relationship types

## Implementation Notes

### Technical Approach

1. Add backtick-quoted identifier token to lexer (`cypher_scanner.l`)
2. Token should capture content between backticks as identifier text
3. Grammar rules should accept this token wherever IDENTIFIER is valid
4. No changes needed to transform/executor - they just see identifier strings

### Lexer Addition

```c
/* In cypher_scanner.l */
`[^`]+`|``  {
    /* Backtick-quoted identifier */
    /* Strip the backticks and handle escapes */
    yylval->string = strndup(yytext+1, yyleng-2);
    return IDENTIFIER;
}
```

### Grammar Consideration

May need separate token if we want to distinguish quoted vs unquoted identifiers for error messages or warnings.

### Dependencies
- Lexer changes only
- No parser grammar changes if we reuse IDENTIFIER token

### Risk Considerations
- Must handle empty backtick identifiers (``` `` ```) - probably error
- Must handle unclosed backticks gracefully
- Performance: adds one more pattern to lexer matching