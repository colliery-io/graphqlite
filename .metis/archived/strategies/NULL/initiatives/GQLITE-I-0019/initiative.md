---
id: implement-proper-error-location
level: initiative
title: "Implement Proper Error Location Reporting"
short_code: "GQLITE-I-0019"
created_at: 2025-12-20T02:01:23.051752+00:00
updated_at: 2025-12-20T02:22:17.511875+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: true

tags:
  - "#initiative"
  - "#phase/completed"


exit_criteria_met: false
estimated_complexity: S
strategy_id: NULL
initiative_id: implement-proper-error-location
---

# Implement Proper Error Location Reporting Initiative

## Context

The parser currently reports incorrect column numbers in parse errors, making it difficult for users to identify and fix syntax issues. The location tracking between the scanner and parser is not properly synchronized, leading to misleading error messages that hinder developer productivity.

## Goals & Non-Goals

**Goals:**
- Fix column number calculation in error messages to point to exact error location
- Synchronize location tracking between scanner (cypher_scanner.l) and parser
- Provide accurate line and column information for all parse errors
- Improve error messages with contextual information about the error

**Non-Goals:**
- Adding semantic error reporting (type checking, etc.)
- Implementing error recovery mechanisms
- Creating IDE integration for error highlighting

## Detailed Design

The fix involves properly tracking token positions throughout the parsing pipeline:

1. **Scanner Enhancement** (cypher_scanner.l):
   - Track column position for each token
   - Update yylloc with accurate start/end positions
   - Handle multi-byte characters correctly

2. **Parser Integration** (cypher_gram.y):
   - Use %locations directive for automatic location tracking
   - Propagate location info through AST nodes
   - Access location in error reporting functions

3. **Error Reporting Improvements**:
   - Enhance yyerror to use location information
   - Show the problematic line with error indicator
   - Add helpful hints based on error context

4. **Location Structure**:
   ```c
   typedef struct {
       int first_line, first_column;
       int last_line, last_column;
   } YYLTYPE;
   ```

## Alternatives Considered

1. **Manual column tracking**: Track positions manually in each scanner rule - rejected due to error-prone nature
2. **Post-process error messages**: Fix locations after parsing - rejected as it doesn't address root cause
3. **Custom error system**: Build entirely new error reporting - rejected as overly complex
4. **Line-only tracking**: Report only line numbers - rejected as insufficient for complex queries

## Implementation Plan

1. **Scanner Updates**: Add column tracking variables, update token rules to set yylloc
2. **Parser Integration**: Add %locations directive, update error handling
3. **Error Message Enhancement**: Implement formatted error output with position markers
4. **Integration Testing**: Validate all error types show correct positions

## Testing Strategy

**Error Position Accuracy:**
- Test errors at start, middle, and end of queries
- Validate column numbers are exact (character-level precision)
- Test with multi-line queries

**Error Type Coverage:**
- Syntax errors: Missing parentheses, invalid operators
- Token errors: Invalid characters, unterminated strings
- Grammar errors: Invalid clause ordering, missing keywords

**Edge Cases:**
- Unicode characters and multi-byte sequences
- Tab characters and mixed whitespace
- Very long lines and deeply nested expressions