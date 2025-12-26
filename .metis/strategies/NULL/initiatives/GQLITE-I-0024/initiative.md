---
id: unify-variable-tracking-systems
level: initiative
title: "Unify Variable Tracking Systems"
short_code: "GQLITE-I-0024"
created_at: 2025-12-26T04:50:28.594656+00:00
updated_at: 2025-12-26T20:16:59.373875+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/completed"


exit_criteria_met: false
estimated_complexity: M
strategy_id: NULL
initiative_id: unify-variable-tracking-systems
---

# Unify Variable Tracking Systems Initiative

## Context

The transform layer maintains **two parallel variable tracking systems** that evolved independently:

### System 1: Legacy `variables[]` Array
Located in `cypher_transform_context` (cypher_transform.h):
```c
struct {
    char *name;
    char *alias;
    var_type type;  // VAR_TYPE_NODE, VAR_TYPE_EDGE, VAR_TYPE_PATH
} variables[MAX_VARIABLES];
int variable_count;
```

Used by: `lookup_variable_alias()`, `add_variable()`, most of transform_return.c

### System 2: AGE-Style `entities[]` Array
Also in `cypher_transform_context`:
```c
struct {
    char *name;
    char *table_alias;
    entity_type type;  // ENTITY_VERTEX, ENTITY_EDGE
    bool is_current_clause;
} entities[MAX_ENTITIES];
int entity_count;
```

Used by: `lookup_entity()`, `register_entity()`, transform_match.c for OPTIONAL MATCH

### Problems

1. **Duplicate state**: Same variable registered in both systems
2. **Inconsistent lookups**: Some code uses `lookup_variable_alias()`, other uses `lookup_entity()`
3. **Different semantics**: `is_current_clause` only in entities, `var_type` richer in variables
4. **Maintenance burden**: Changes must update both systems
5. **Bug source**: OPTIONAL MATCH issues stemmed from inconsistent entity tracking

## Goals & Non-Goals

**Goals:**
- Consolidate into single unified variable tracking system
- Preserve all functionality from both systems
- Single lookup function with consistent semantics
- Cleaner separation between "declared" vs "in-scope" variables
- Enable proper scoping for WITH clause

**Non-Goals:**
- Full lexical scoping implementation (future work)
- Breaking changes to external API
- Parser modifications

## Architecture

### Unified Variable Structure

```c
typedef enum {
    VAR_NODE,
    VAR_EDGE,
    VAR_PATH,
    VAR_PROJECTED,  // Result of WITH/RETURN projection
    VAR_AGGREGATED  // Result of aggregation
} variable_kind;

typedef struct {
    char *name;              // Cypher variable name (e.g., "n", "r")
    char *table_alias;       // SQL alias (e.g., "n_0", "e_1")
    variable_kind kind;
    
    // Scope tracking
    int declared_in_clause;  // Which clause index declared this
    bool is_visible;         // Currently in scope?
    
    // For nodes/edges
    char *label;             // Primary label if known
    
    // For projected variables  
    char *source_expr;       // Original expression (for WITH aliasing)
} transform_variable;
```

### Unified Access API

```c
// Registration
int register_variable(cypher_transform_context *ctx, 
                      const char *name, 
                      variable_kind kind,
                      const char *table_alias);

// Lookup
transform_variable *lookup_variable(cypher_transform_context *ctx, 
                                    const char *name);

// Scope management
void enter_clause_scope(cypher_transform_context *ctx);
void exit_clause_scope(cypher_transform_context *ctx);
void project_variables(cypher_transform_context *ctx, 
                       const char **names, 
                       int count);  // For WITH clause
```

## Detailed Design

### Phase 1: Create New System (Additive)

1. Define `transform_variable` struct in new header `transform_variables.h`
2. Implement registration and lookup functions
3. Add to context alongside existing systems
4. No existing code changes yet

### Phase 2: Migrate Callers (Incremental)

For each file, update to use new system:

| File | Current Usage | Migration |
|------|---------------|-----------|
| `transform_match.c` | entities[] for nodes/edges | Use register_variable() |
| `transform_return.c` | variables[] for lookups | Use lookup_variable() |
| `transform_with.c` | Both systems | Use project_variables() |
| `transform_expr_predicate.c` | entities[] for EXISTS | Use lookup_variable() |
| `cypher_transform.c` | Both for setup | Single initialization |

### Phase 3: Remove Legacy Systems

1. Remove `variables[]` array and helpers
2. Remove `entities[]` array and helpers
3. Clean up unused code
4. Update all references

## Testing Strategy

### Critical Test Cases

1. **Basic MATCH**: Variables registered and accessible
2. **MATCH + RETURN**: Variable lookup works across clauses
3. **WITH projection**: Only projected variables visible after WITH
4. **OPTIONAL MATCH**: Variables from optional clause properly scoped
5. **Multiple MATCH**: Variables from all MATCH clauses accessible
6. **Shadowing**: WITH can redefine variable names
7. **Aggregation**: Aggregated results properly typed

### Regression Testing

- Full test suite must pass at each phase
- Python and Rust bindings unchanged
- All functional tests pass

## Alternatives Considered

1. **Keep both systems**: Status quo - rejected, maintenance burden too high
2. **Extend entities[] only**: Loses path/projected tracking from variables[]
3. **Extend variables[] only**: Loses is_current_clause semantics
4. **Full symbol table**: Overkill for current needs, future consideration

## Implementation Plan

### Task 1: Design & Header
- Create `transform_variables.h` with new structures
- Define API signatures
- No functional changes

### Task 2: Implement Core Functions
- Create `transform_variables.c`
- Implement register, lookup, scope functions
- Unit tests for new module

### Task 3: Migrate transform_match.c
- Switch to new registration system
- Keep legacy system populated for other files
- Run tests

### Task 4: Migrate transform_return.c
- Switch to new lookup system
- Run tests

### Task 5: Migrate Remaining Files
- transform_with.c
- transform_expr_predicate.c
- cypher_transform.c
- Run tests after each

### Task 6: Remove Legacy Systems
- Delete old arrays and functions
- Final cleanup
- Full test suite