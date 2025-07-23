---
id: end-to-end-node-operations-create
level: initiative
title: "End-to-End Node Operations (CREATE/MATCH)"
created_at: 2025-07-22T01:39:35.969613+00:00
updated_at: 2025-07-23T12:27:28.393465+00:00
parent: opencypher-implementation-strategy
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/completed"


exit_criteria_met: false
estimated_complexity: M
---

# End-to-End Node Operations (CREATE/MATCH) Initiative

## Context

Build a complete end-to-end pipeline for basic graph operations to validate the entire architecture. This initiative started with the simplest possible graph operations (creating and matching nodes) but expanded to include full relationship/edge support.

**Original Core Goal**: Prove that we can parse, execute, and return results for basic OpenCypher queries without relationships or complex patterns.

**Expanded Achievement**: We successfully implemented not only node operations but also complete relationship/edge CREATE and MATCH operations with flexible pattern support.

**Implemented Query Support**:
```cypher
# Original target queries (all working)
CREATE (n:Person {name: "John"})
MATCH (n:Person) RETURN n
MATCH (n:Person {name: "John"}) RETURN n

# Additional implemented features
CREATE (a:Person)-[:KNOWS]->(b:Person)
CREATE (a:Person)-[r:KNOWS {since: 2020}]->(b:Person)
CREATE (a:Person)<-[:FOLLOWS]-(b:Person)
MATCH (a:Person)-[:KNOWS]->(b:Person) RETURN a, b
MATCH (a:Person)-[r:KNOWS]->(b:Person) RETURN r
MATCH (a)-[r]->(b) RETURN a, r, b  # Flexible patterns
```

This "wide before deep" approach validated all major components (parser, AST, executor, storage, results) and established a solid foundation for the graph database.

## Goals & Non-Goals

**Original Goals (All Achieved ✅):**
- Parse CREATE statements for nodes with single label and single property ✅
- Parse MATCH statements for nodes with label and property filtering ✅
- Parse basic RETURN statements for node projection ✅
- Store nodes in SQLite tables with proper schema ✅
- Retrieve nodes based on label and property criteria ✅
- Return query results in structured format ✅
- Establish memory management patterns for AST and results ✅
- Validate entire pipeline with CUnit integration tests ✅
- Demonstrate functional SQLite extension interface ✅

**Additional Achievements (Beyond Original Scope):**
- Parse CREATE statements for relationships/edges with types and properties ✅
- Parse MATCH statements for relationship patterns ✅
- Support flexible edge patterns (with/without types, with/without variables) ✅
- Support directional relationships (both -> and <-) ✅
- Store edges in SQLite tables with typed EAV schema ✅
- Retrieve edges based on type and property criteria ✅
- Return OpenCypher-style JSON entities with identity, labels, and properties ✅
- Multiple properties per node (via typed EAV tables) ✅
- RETURN projection with proper variable extraction from patterns ✅
- Comprehensive SQL test suite (7 test files) ✅
- Full unit test coverage (47 tests, 481 assertions) ✅

**What Remains as Non-Goals:**
- Multiple labels per node (still future iteration)
- Complex WHERE clause expressions beyond simple property equality
- Advanced RETURN projections (aliases, aggregations, ORDER BY, LIMIT)
- Query optimization or indexing
- Error recovery in parser (fails fast on syntax errors)
- Performance optimization
- Variable-length patterns or multi-hop traversals (e.g., (a)-[:KNOWS*1..3]->(b))
- MERGE operations
- DELETE operations
- SET operations for property updates

## Detailed Design

## Architecture Components

### 1. Parser Layer (BISON + FLEX)
- **Extended BISON Grammar**: Supports CREATE, MATCH, RETURN for both nodes and relationships
- **FLEX Lexer**: Tokenizes keywords, identifiers, strings, symbols, operators (ARROW_RIGHT, ARROW_LEFT)
- **AST Nodes**: Comprehensive node types including:
  - Statement nodes (CREATE, MATCH, RETURN, COMPOUND)
  - Pattern nodes (node_pattern, edge_pattern, relationship_pattern, path_pattern)
  - Expression nodes (identifiers, literals, property maps)
  - Support for flexible patterns (nodes/edges with/without labels, types, variables)

### 2. Database Schema (Typed EAV Design)
```sql
-- Core entity tables
CREATE TABLE nodes (id INTEGER PRIMARY KEY AUTOINCREMENT);
CREATE TABLE edges (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    source_id INTEGER NOT NULL,
    target_id INTEGER NOT NULL,
    type TEXT NOT NULL,
    FOREIGN KEY (source_id) REFERENCES nodes(id),
    FOREIGN KEY (target_id) REFERENCES nodes(id)
);

-- Property system
CREATE TABLE property_keys (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    key TEXT NOT NULL UNIQUE
);

-- Typed property storage
CREATE TABLE node_props_int (node_id, key_id, value INTEGER);
CREATE TABLE node_props_real (node_id, key_id, value REAL);
CREATE TABLE node_props_text (node_id, key_id, value TEXT);
CREATE TABLE node_props_bool (node_id, key_id, value BOOLEAN);
CREATE TABLE edge_props_int/real/text/bool (same pattern);

-- Labels
CREATE TABLE node_labels (
    node_id INTEGER,
    label TEXT NOT NULL
);
```

### 3. Execution Pipeline
- **AST Walker**: Interprets parsed statements and dispatches to handlers
- **CREATE Handlers**: 
  - `execute_create_node()`: Creates nodes with labels and properties
  - `execute_create_relationship()`: Creates edges between nodes
- **MATCH Handlers**:
  - `execute_match_node()`: Queries nodes by label/properties
  - `execute_match_relationship()`: JOINs nodes and edges for pattern matching
- **RETURN Handler**: Projects selected variables with OpenCypher JSON serialization
- **Entity Serializers**: 
  - `serialize_node_entity()`: Formats nodes as `{"identity": id, "labels": [...], "properties": {...}}`
  - `serialize_relationship_entity()`: Similar format for relationships

### 4. Memory Management
- **AST Cleanup**: Recursive tree freeing with `ast_free()`
- **Result Management**: `graphqlite_result_free()` handles all allocations
- **Property Key Interning**: Cached key lookups reduce allocations
- **Error Handling**: Proper cleanup paths on all failures

### 5. API Interface
```c
// SQLite extension entry point
int sqlite3_graphqlite_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi);

// Main query function (registered as SQL function)
void graphqlite_cypher_func(sqlite3_context *context, int argc, sqlite3_value **argv);

// Returns JSON entities for MATCH, "Query executed successfully" for CREATE
```

## Data Flow
1. **Parse**: BISON/FLEX converts query string → AST
2. **Execute**: AST walker performs database operations
3. **Store/Retrieve**: SQLite table operations for nodes
4. **Format**: Convert internal data → result structure
5. **Cleanup**: Free all allocated memory

## Alternatives Considered

**1. Multiple Properties per Node from Start**
- **Rejected**: Adds complexity to schema design and property handling
- **Rationale**: Single property validates the pattern, multiple can be added incrementally

**2. JSON Property Storage**
- **Considered**: Store properties as JSON blob in single column
- **Rejected**: Harder to query and filter, less SQL-friendly
- **Rationale**: Explicit columns make debugging and SQL queries easier

**3. Starting with Relationships**
- **Rejected**: Relationships require source/target node management complexity
- **Rationale**: Nodes are simpler foundation, relationships build on top

**4. Hand-written Parser Instead of BISON**
- **Considered**: Custom recursive descent parser
- **Rejected**: More maintenance burden, less standard
- **Rationale**: BISON provides proven parser generation with good error handling

**5. Complete OpenCypher Grammar from Start**
- **Rejected**: Too much complexity for initial validation
- **Rationale**: Minimal grammar proves architecture, incremental expansion is safer

## Implementation Plan

## Phase 1: Foundation (Week 1)
**Tasks:**
- Set up minimal BISON grammar file for CREATE/MATCH/RETURN
- Create FLEX lexer for basic tokens
- Define core AST node structures
- Implement AST memory management functions
- Update Makefile for BISON/FLEX compilation

**Exit Criteria:** Parser compiles and generates AST for target queries

## Phase 2: Database Integration (Week 1-2)
**Tasks:** 
- Create SQLite extension entry points
- Implement database schema creation
- Build node insertion functions
- Build node query functions
- Add basic error handling

**Exit Criteria:** Can store and retrieve nodes via direct C API calls

## Phase 3: Query Execution (Week 2)
**Tasks:**
- Implement AST walker/interpreter
- Connect parser output to database operations
- Build result formatting system
- Add memory cleanup for full pipeline
- Create main query execution function

**Exit Criteria:** End-to-end query execution working for target queries

## Phase 4: Testing & Validation (Week 2-3)
**Tasks:**
- Create CUnit test suite for each component
- Add integration tests for target queries
- Test memory management (no leaks)
- Validate SQLite extension loading
- Performance baseline measurements

**Exit Criteria:** All tests passing, ready for next iteration

## Testing Strategy

## Unit Testing (CUnit Framework)
**Parser Tests:**
- Valid query parsing produces correct AST
- Invalid queries produce appropriate errors
- Memory cleanup after parse failures

**Database Tests:**
- Node insertion with label and property
- Node retrieval by label only
- Node retrieval by label and property
- Schema creation and table validation

**Execution Tests:**
- CREATE statement execution
- MATCH statement execution  
- RETURN statement formatting
- End-to-end pipeline memory management

## Integration Testing
**Target Query Validation:**
```cypher
-- Test 1: Create and verify storage
CREATE (n:Person {name: "John"})

-- Test 2: Match by label
MATCH (n:Person) RETURN n

-- Test 3: Match by label and property
MATCH (n:Person {name: "John"}) RETURN n
```

**Success Criteria per Test:**
- Query parses without errors
- Database operations complete successfully
- Results contain expected data
- No memory leaks detected
- SQLite extension loads properly

## Performance Baseline
- Parse time: <1ms for target queries
- Execution time: <10ms for single node operations
- Memory usage: <1MB for test dataset
- Zero memory leaks under valgrind

## Validation Environment

- CUnit test runner integrated with make test ✅
- Memory testing with appropriate tools ✅
- SQLite extension loading verification ✅
- Cross-platform testing (macOS confirmed) ✅

## Completion Summary

**Initiative Status**: COMPLETE (Exceeded Original Scope)

**Test Results**:
- Unit Tests: 47/47 passing (481 assertions)
- SQL Tests: 7/7 passing
- No memory leaks reported
- All target queries working plus relationship support

**Key Deliverables**:
1. **Parser**: Full BISON/FLEX implementation supporting nodes and relationships
2. **Storage**: Typed EAV schema implementation for flexible property storage
3. **Execution**: Complete CREATE/MATCH/RETURN pipeline with projection
4. **Testing**: Comprehensive test suite with 100% pass rate
5. **Extension**: Working SQLite extension with cypher() function

**Unexpected Achievements**:
- Complete relationship/edge support (was planned for future iteration)
- Flexible pattern matching (nodes/edges with optional components)
- OpenCypher-style JSON entity serialization
- Multiple property support via typed tables
- Bidirectional relationship support

**Technical Decisions Made**:
- Chose typed EAV over single property column (better extensibility)
- Implemented property key interning (performance optimization)
- Added RETURN projection with variable extraction
- Used JSON format inspired by graph database conventions

**Ready for Next Phase**: The foundation is solid and tested. All major architectural components are proven. The implementation exceeds the original scope and provides a strong base for adding more OpenCypher features.