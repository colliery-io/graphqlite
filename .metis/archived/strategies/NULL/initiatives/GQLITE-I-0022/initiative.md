---
id: multi-graph-support-via
level: initiative
title: "Multi-Graph Support via GraphManager and ATTACH"
short_code: "GQLITE-I-0022"
created_at: 2025-12-25T20:16:11.339561+00:00
updated_at: 2026-01-04T14:34:29.624102+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: true

tags:
  - "#initiative"
  - "#phase/completed"


exit_criteria_met: false
estimated_complexity: M
strategy_id: NULL
initiative_id: multi-graph-support-via
---

# Multi-Graph Support via GraphManager and ATTACH Initiative

*This template includes sections for various types of initiatives. Delete sections that don't apply to your specific use case.*

## Context

GraphQLite currently operates as a single implicit graph per SQLite database file. Users needing multiple isolated graphs (multi-tenancy, versioning, test/prod separation, domain isolation) must manage separate `.db` files manually.

Rather than implementing complex schema-prefix multi-graph within a single database, we can leverage:
1. **Separate files** for true graph isolation (already works)
2. **SQLite ATTACH** for cross-graph queries when needed
3. **GraphManager** abstraction in bindings for ergonomic multi-file management
4. **Cypher `FROM` clause** for cross-graph query syntax

Key insight: If you frequently query across "separate" graphs, they're arguably one graph with unmade connections. True multi-graph is for isolation (tenants, versions, environments) - not domain separation within a connected system.

## Goals & Non-Goals

**Goals:**
- Provide `GraphManager` class in Python and Rust bindings for managing multiple graph files
- Enable cross-graph SQL queries via ATTACH for power users
- Add `FROM graph_name` clause to Cypher for cross-graph queries
- Keep single-graph usage simple and unchanged (backward compatible)

**Non-Goals:**
- Schema-prefix multi-graph within a single file (rejected: adds complexity, cross-graph queries indicate modeling problem)
- Cross-graph edges (graphs remain isolated; correlation via property matching only)
- Automatic graph discovery/federation

## Requirements **[CONDITIONAL: Requirements-Heavy Initiative]**

{Delete if not a requirements-focused initiative}

### User Requirements
- **User Characteristics**: {Technical background, experience level, etc.}
- **System Functionality**: {What users expect the system to do}
- **User Interfaces**: {How users will interact with the system}

### System Requirements
- **Functional Requirements**: {What the system should do - use unique identifiers}
  - REQ-001: {Functional requirement 1}
  - REQ-002: {Functional requirement 2}
- **Non-Functional Requirements**: {How the system should behave}
  - NFR-001: {Performance requirement}
  - NFR-002: {Security requirement}

## Use Cases

### Use Case 1: Graph Versioning
- **Actor**: Data engineer
- **Scenario**: Maintain versioned snapshots during schema migration or data transformation
- **Example**: `gm.create("graph_v2")`, migrate data, validate, then `gm.drop("graph_v1")`
- **Outcome**: Safe rollback capability, ability to compare before/after states

### Use Case 2: Cross-Graph Analytics
- **Actor**: Analyst or application developer
- **Scenario**: Correlate entities across separate domain graphs by shared identifier
- **Example**: Join a knowledge graph with an activity graph via shared entity IDs
- **Outcome**: Ad-hoc cross-domain queries without merging conceptually separate graphs

### Use Case 3: Domain Isolation in Monolith Applications
- **Actor**: Application developer
- **Scenario**: Single application manages multiple independent graph datasets (e.g., a note-taking app with separate graphs per workspace/project)
- **Example**: `gm.open_or_create("project_alpha")`, `gm.open_or_create("project_beta")`
- **Outcome**: Clean separation of unrelated data within one application

## Architecture **[CONDITIONAL: Technically Complex Initiative]**

{Delete if not technically complex}

### Overview
{High-level architectural approach}

### Component Diagrams
{Describe or link to component diagrams}

### Class Diagrams
{Describe or link to class diagrams - for OOP systems}

### Sequence Diagrams
{Describe or link to sequence diagrams - for interaction flows}

### Deployment Diagrams
{Describe or link to deployment diagrams - for infrastructure}

## Detailed Design

### GraphManager API (Python)
```python
from graphqlite import graphs

with graphs("./data") as gm:
    gm.list()                    # ['social', 'products']
    gm.create("analytics")       # creates ./data/analytics.db
    gm.open("social")            # returns Graph instance
    gm.open_or_create("cache")   # create if missing
    gm.drop("old_graph")         # deletes file
    
    # Cross-graph SQL (via ATTACH)
    gm.query_sql("""
        SELECT s.value, p.value
        FROM social.node_props_text s
        JOIN products.node_props_text p ON s.value = p.value
    """, graphs=["social", "products"])
```

### GraphManager API (Rust)
```rust
let mut gm = graphs("./data")?;
gm.list()?;                      // vec!["social", "products"]
gm.create("analytics")?;
gm.open("social")?;
gm.open_or_create("cache")?;
gm.drop("old_graph")?;
gm.query_sql("...", &["social", "products"])?;
```

### Cypher FROM Clause
```cypher
-- Explicit graph selection
MATCH (u:User) FROM social RETURN u

-- Cross-graph correlation
MATCH (u:User) FROM social
MATCH (p:Product) FROM products
WHERE u.user_id = p.buyer_id
RETURN u.name, p.title
```

### Implementation Layers

| Layer | Component | Work Required |
|-------|-----------|---------------|
| Python bindings | `GraphManager` class | New file, ~150 lines |
| Rust bindings | `GraphManager` struct | New file, ~200 lines |
| Parser | `FROM graph_name` clause | Grammar extension |
| Transform | Prefix table references | Handle `match->from_graph` |
| Executor | Coordinator connection | In-memory + ATTACH |

### SQL Generation for Cross-Graph
```sql
-- ATTACH databases to coordinator
ATTACH DATABASE 'social.db' AS social;
ATTACH DATABASE 'products.db' AS products;

-- Query uses prefixed table names
SELECT ... FROM social.nodes JOIN products.nodes ...
```

## UI/UX Design **[CONDITIONAL: Frontend Initiative]**

{Delete if no UI components}

### User Interface Mockups
{Describe or link to UI mockups}

### User Flows
{Describe key user interaction flows}

### Design System Integration
{How this fits with existing design patterns}

## Testing Strategy **[CONDITIONAL: Separate Testing Initiative]**

{Delete if covered by separate testing initiative}

### Unit Testing
- **Strategy**: {Approach to unit testing}
- **Coverage Target**: {Expected coverage percentage}
- **Tools**: {Testing frameworks and tools}

### Integration Testing
- **Strategy**: {Approach to integration testing}
- **Test Environment**: {Where integration tests run}
- **Data Management**: {Test data strategy}

### System Testing
- **Strategy**: {End-to-end testing approach}
- **User Acceptance**: {How UAT will be conducted}
- **Performance Testing**: {Load and stress testing}

### Test Selection
{Criteria for determining what to test}

### Bug Tracking
{How defects will be managed and prioritized}

## Alternatives Considered

### 1. Schema Prefix Within Single Database
Add `graph_name` prefix to all tables: `social_nodes`, `products_nodes`, etc.

**Rejected because:**
- Adds parsing complexity to every query
- Cross-graph queries still need special handling
- Schema migrations multiply by graph count
- If querying across graphs frequently, they should be one graph

### 2. graph_name Column on All Tables
Add `graph TEXT` column to nodes, edges, properties tables.

**Rejected because:**
- Every query needs `WHERE graph = ?` filter
- Easy to forget, causing data leaks
- Indexes must include graph column
- Harder to drop a graph (DELETE vs DROP TABLE)
- Performance overhead on every query

### 3. Separate SQLite Files (Chosen)
Each graph is a separate `.db` file; use ATTACH for cross-graph.

**Chosen because:**
- Zero implementation cost for isolation (already works)
- Native SQLite features (backup, permissions, copy)
- Clean mental model
- Cross-graph via ATTACH when truly needed
- Performance isolation (locks per-file)

## Implementation Plan

### Phase 1: C Extension - FROM Clause Support

**Parser Changes (`cypher_gram.y`, `cypher_scanner.l`):**
- Add `FROM` token handling in MATCH context (distinguish from LOAD CSV FROM)
- Grammar rule: `match_clause : MATCH pattern_list FROM identifier ...`
- Store graph name in `cypher_match` AST node

**AST Changes (`cypher_ast.h`, `cypher_ast.c`):**
- Add `from_graph` field to `cypher_match` struct
- Update AST construction/destruction functions
- Add accessor: `cypher_match_get_from_graph()`

**Transform Layer (`transform_match.c`):**
- When `from_graph` is set, prefix all table references with graph name
- Example: `nodes` → `{graph_name}.nodes`, `edges` → `{graph_name}.edges`
- No connection management - just string prefixing in generated SQL

**Testing:**
- Unit tests for parser accepting/rejecting FROM syntax
- Transform tests verifying correct table prefixing
- Ensure backward compatibility (queries without FROM unchanged)

### Phase 2: Binding Layer - GraphManager

**Python (`graphqlite/manager.py`):**
```python
class GraphManager:
    def __init__(self, base_path: str)
    def list(self) -> list[str]
    def create(self, name: str) -> Graph
    def open(self, name: str) -> Graph
    def open_or_create(self, name: str) -> Graph
    def drop(self, name: str) -> None
    def query_cross(self, cypher: str, graphs: list[str]) -> Result
```
- `query_cross` handles ATTACH to coordinator connection
- Connection pooling for open graphs
- Context manager support (`with graphs(...) as gm`)

**Rust (`src/manager.rs`):**
```rust
pub struct GraphManager { ... }
impl GraphManager {
    pub fn new(base_path: impl AsRef<Path>) -> Result<Self>
    pub fn list(&self) -> Result<Vec<String>>
    pub fn create(&mut self, name: &str) -> Result<Graph>
    pub fn open(&mut self, name: &str) -> Result<Graph>
    pub fn open_or_create(&mut self, name: &str) -> Result<Graph>
    pub fn drop(&mut self, name: &str) -> Result<()>
    pub fn query_cross(&self, cypher: &str, graphs: &[&str]) -> Result<QueryResult>
}
```

**ATTACH Coordination:**
- Maintain a "coordinator" in-memory connection for cross-graph queries
- Dynamically ATTACH requested graphs before executing
- DETACH after query completes (or pool attached state)

**Testing:**
- File creation/deletion
- Connection lifecycle
- Cross-graph queries via ATTACH
- Error handling (missing graphs, permission errors)

### Phase 3: Documentation & Examples

- Update README with multi-graph section
- Add `examples/multi_graph/` with:
  - Graph versioning workflow
  - Cross-graph analytics example
- Document when to use multi-graph vs single graph with better modeling
- API reference for GraphManager in both Python and Rust docs

### Design Decisions

**FROM on MATCH clause (not query-level)**
- Per-MATCH is explicit: `MATCH (u:User) FROM social` clearly sources that pattern
- Query-level `FROM social, products MATCH ...` would be ambiguous about which pattern goes where
- Aligns with Neo4j/GQL direction

**Fail noisy on missing graphs**
- Clear error: `Graph 'social' not found. Available: ['products', 'analytics']`
- No silent empty results
- No fallback to default graph

**Auto-close on drop**
- `gm.drop("name")` closes any open connection before deleting file
- Prevents dangling connections to deleted databases