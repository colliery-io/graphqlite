# GraphQLite Parser Migration Plan

## Current Objective
Migrate from our basic 370-line OpenCypher parser to Apache AGE's comprehensive 3,398-line production parser to achieve full OpenCypher compatibility.

## What We're Trying To Do
Replace GraphQLite's limited parser with AGE's proven OpenCypher implementation while maintaining our SQLite-based architecture and existing query execution engine.

### Core Goal
Extract AGE's grammar expertise while discarding PostgreSQL dependencies, creating a clean SQLite-native parser that generates our custom AST.

## Current State Analysis

### ✅ Completed Work
1. **Modular Refactoring**: Successfully extracted 5 modules from monolithic codebase (54% size reduction)
2. **AGE Code Analysis**: Copied and analyzed AGE parser components  
3. **Clean Architecture**: Established backend/include directory structure
4. **SQLite AST Design**: Created lightweight AST types (`ast_node`, `ast_list`) without PostgreSQL dependencies
5. **Grammar Extraction**: Extracted core OpenCypher rules from AGE's `cypher_gram.y`
6. **Keyword System**: Copied AGE's keyword recognition approach

### 🔄 Current Issues
1. **Build System**: Makefile directory creation patterns need fixing
2. **Header Dependencies**: Circular includes between parser.h and parser.tab.h
3. **Type Definitions**: YYSTYPE/YYLTYPE conflicts between manual and generated definitions
4. **Scanner Integration**: Need to properly integrate AGE's keyword lookup with our scanner

### 🎯 Core Architecture Decision Made
**Option A**: Extract grammar rules and rewrite actions (CHOSEN)
- ✅ Clean separation of concerns
- ✅ No PostgreSQL compatibility baggage  
- ✅ Direct integration with existing SQLite execution engine

**Option B**: Create PostgreSQL compatibility layer (REJECTED)
- ❌ Would introduce unnecessary complexity
- ❌ Maintenance burden of PostgreSQL API mapping

## Implementation Plan

### Phase 1: Build System Stabilization (HIGH PRIORITY)
**Goal**: Get a clean, working build with pre-generated parser files

**Tasks**:
1. Fix Makefile directory creation for `build/backend/parser/` pattern
2. Resolve header dependency chain:
   - Move type definitions to proper location using `%code requires`
   - Eliminate circular includes
   - Ensure parser.tab.h includes necessary types
3. Treat generated parser files as checked-in source (like AGE does)
4. Create working `make clean` that only removes build artifacts

**Success Criteria**: `make clean && make` succeeds without errors

### Phase 2: Parser Integration (HIGH PRIORITY) 
**Goal**: Complete scanner/parser integration with keyword recognition

**Tasks**:
1. Finalize scanner.l with AGE's keyword lookup function
2. Integrate cypher_keywords.c with proper token definitions
3. Test basic parsing: `MATCH (n) RETURN n`
4. Fix any remaining grammar warnings (path_list, expr_list issues)

**Success Criteria**: Can parse simple OpenCypher queries into our AST

### Phase 3: AST Translation Layer (MEDIUM PRIORITY)
**Goal**: Bridge between parser AST and existing query execution

**Tasks**:
1. Create translation functions: `ast_node` → existing execution functions
2. Map OpenCypher patterns to our EAV schema operations
3. Integrate with existing `create_node_with_properties()`, etc.
4. Handle complex queries (joins, WHERE clauses, aggregations)

**Success Criteria**: End-to-end query execution from OpenCypher to SQLite results

### Phase 4: Testing & Validation (MEDIUM PRIORITY)
**Goal**: Comprehensive test coverage and OpenCypher compliance

**Tasks**:
1. Create CUnit test suite for parser
2. Test against OpenCypher TCK (Technology Compatibility Kit)
3. Performance benchmarking vs old parser
4. Memory leak detection and optimization

**Success Criteria**: Passes OpenCypher compliance tests, no regressions

## Key Technical Insights

### AGE's Architecture Worth Preserving
- **Keyword System**: Binary search lookup with case-insensitive matching
- **Grammar Structure**: Hierarchical query parsing (reading_clause → updating_clause → return)
- **AST Design**: Clean separation of patterns, expressions, and clauses

### Our SQLite Advantages
- **Lightweight AST**: Simple C structs vs PostgreSQL's ExtensibleNode
- **Direct Memory Management**: malloc/free vs palloc/pfree
- **Schema Integration**: Existing typed EAV tables work well
- **Extension API**: Clean SQLite extension interface

## Next Session Priorities

1. **Fix Build System** (30 minutes)
   - Resolve directory creation in Makefile
   - Fix header dependencies with %code requires
   
2. **Complete Parser Integration** (60 minutes)  
   - Get keyword lookup working
   - Test basic query parsing
   
3. **Start AST Translation** (30 minutes)
   - Create first translation function (simple MATCH)
   - Test end-to-end execution

## Success Metrics
- **Parser Coverage**: Support 80%+ of common OpenCypher constructs
- **Performance**: No significant regression vs current parser
- **Code Quality**: Clean, maintainable codebase without PostgreSQL dependencies
- **Test Coverage**: Comprehensive unit and integration tests

---

*This migration represents a significant step toward making GraphQLite a production-ready graph database with full OpenCypher support.*