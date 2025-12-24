---
id: add-regex-matching-operator
level: task
title: "Add regex matching operator (=~)"
short_code: "GQLITE-T-0005"
created_at: 2025-12-24T01:49:48.904017+00:00
updated_at: 2025-12-24T02:54:59.590287+00:00
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

# Add regex matching operator (=~)

## Objective

Add the regex matching operator (`=~`) for pattern matching on string properties using regular expressions.

## Priority
- [x] P2 - Medium (useful for text search)

## Business Justification
- **User Value**: Powerful text pattern matching without application-level filtering
- **Business Value**: OpenCypher compliance, enables complex text queries
- **Effort Estimate**: S (Small - SQLite has REGEXP support)

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `MATCH (n) WHERE n.name =~ "A.*" RETURN n` matches names starting with A
- [ ] `MATCH (n) WHERE n.email =~ ".*@gmail.com" RETURN n` matches Gmail addresses
- [ ] Case-sensitive matching by default
- [ ] `(?i)` flag for case-insensitive: `n.name =~ "(?i)alice"`
- [ ] Proper error for invalid regex patterns

## Implementation Notes

### Technical Approach
1. **Scanner**: Add `=~` as `REGEX_MATCH` token in `cypher_scanner.l`
2. **Grammar**: Add `expr '=~' expr` production as comparison operator
3. **Transform**: Use SQLite's `REGEXP` function (requires extension) or `LIKE` fallback

### SQLite REGEXP Support
SQLite doesn't have built-in REGEXP, but we can:
- Option A: Load SQLite's `regexp` extension
- Option B: Implement `regexp()` as a user-defined function using PCRE/regex.h
- Option C: For simple patterns, convert to LIKE/GLOB

### Example Transformation
```cypher
MATCH (n) WHERE n.name =~ "^A.*$" RETURN n
```
Transforms to:
```sql
SELECT ... FROM nodes n WHERE regexp('^A.*$', n.name)
```

### Key Files
- `src/backend/parser/cypher_scanner.l` - Token definition
- `src/backend/parser/cypher_gram.y` - Grammar rule
- `src/backend/transform/transform_return.c` - Binary op handling
- `src/executor/cypher_executor.c` - Register regexp function

## Status Updates

*To be added during implementation*