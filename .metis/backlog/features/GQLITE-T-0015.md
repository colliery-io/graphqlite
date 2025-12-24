---
id: normalize-result-format-to-always
level: task
title: "Normalize result format to always return dicts not JSON strings"
short_code: "GQLITE-T-0015"
created_at: 2025-12-24T15:24:25.005754+00:00
updated_at: 2025-12-24T18:19:00.241314+00:00
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

# Normalize result format to always return dicts not JSON strings

## Objective

Ensure all Cypher query results return consistent, parsed data structures (dicts/objects) rather than sometimes returning raw JSON strings that require additional parsing.

## Backlog Item Details

### Type
- [x] Bug - Production issue that needs fixing

### Priority
- [x] P2 - Medium (nice to have)

### Impact Assessment
- **Affected Users**: All users querying nodes with `RETURN n`
- **Reproduction Steps**: 
  1. Create a node: `CREATE (n:Person {name: 'Alice'})`
  2. Query: `MATCH (n) RETURN n`
  3. Observe result format varies between parsed dict and JSON string
- **Expected vs Actual**: Should always get `{"n": {"id": 1, "labels": ["Person"], "properties": {...}}}`, but sometimes get `{"result": "[{\"n\": ...}]"}` as a string

## Problem Statement

Query results are inconsistent in format. Sometimes results come back as parsed Python dicts, other times as JSON strings requiring additional parsing:

```python
# Current workaround (from nanographrag demo)
def get_all_nodes(self, label: Optional[str] = None) -> list[dict]:
    result = self._conn.cypher("MATCH (n) RETURN n")
    
    nodes = []
    for row in result:
        # Handle case where result is a JSON string of nodes
        if "result" in row and isinstance(row["result"], str):
            try:
                parsed = json.loads(row["result"])
                for item in parsed:
                    if "n" in item:
                        nodes.append(item["n"])
            except json.JSONDecodeError:
                pass
        elif "n" in row and row["n"]:
            nodes.append(row["n"])
    return nodes
```

This inconsistency forces every consumer to handle multiple formats.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `RETURN n` always returns node as a parsed dict, never a JSON string
- [ ] `RETURN r` always returns relationship as a parsed dict
- [ ] `RETURN n.property` returns the property value directly (string, int, etc.)
- [ ] `RETURN count(n)` returns an integer, not a string
- [ ] Aggregate functions return appropriate native types
- [ ] Python bindings always return `list[dict]` from `cypher()`
- [ ] No consumer code needs `json.loads()` on result values
- [ ] Results are consistent regardless of query complexity

## Debug Plan (Dec 24)

**Focus**: `test_return_whole_node` - one test at a time

**Symptom**: 
- Actual: `{"id": 0, "labels": [], "properties": {}}`
- Expected: `{"id": 1, "labels": ["WholeNode"], "properties": {"name": "Test", "value": 42}}`

**Key observations**:
1. `id: 0` is wrong - AUTOINCREMENT starts at 1
2. `labels: []` is empty - should have "WholeNode"  
3. `properties: {}` is empty - should have name/value

**Debug Steps**:

1. **Verify data storage** - After CREATE, query raw tables (nodes, node_labels, node_props_*) to confirm data is stored correctly

2. **Check generated SQL** - Use EXPLAIN to see what SQL is produced for `MATCH (n:WholeNode) RETURN n`

3. **Run generated SQL manually** - Execute the SQL directly in SQLite to see what it returns

4. **Identify mismatch** - Compare: Is the subquery referencing correct tables? Is alias correct?

5. **Fix and verify** - Make targeted fix, run single test

**Relevant code**: `transform_return.c:542-560` - node serialization SQL generation

## Debug Progress

**Step 1**: Data storage ✓ PASSED - node_id=1, labels, props all stored correctly

**Step 2**: Generated SQL ✓ CORRECT - SQL references correct tables and aliases

**Step 3**: Raw SQL execution ✓ WORKS - Returns `{"id":1,"labels":["WholeNode"],"properties":{"name":"Test","value":42}}`

**Conclusion**: SQL generation is correct. Bug is in **result processing** after SQL execution.

**Next**: Investigate `cypher_executor.c` result handling and `test_output_format.c` execute_and_format()

**Step 4**: Found root cause - executor expected node ID but got full JSON object. `atoll("{...}")` returns 0.

**Step 5**: Fixed by:
1. Added `agtype_value_from_vertex_json()` and `agtype_value_from_edge_json()` to parse JSON back to agtype
2. Updated executor to detect JSON objects (starts with `{`) and use new parse functions

**Result**: All 3588 unit tests pass. Pre-existing functional test failure unrelated to this fix.

## Implementation Notes

### Technical Approach

1. Audit all code paths that produce results in `cypher_executor.c`
2. Identify where JSON strings are being returned instead of structured data
3. Ensure the C result structure always contains parsed values
4. Update Python bindings to properly convert C results to Python dicts
5. Add tests that verify result types, not just values

### Affected Components
- `src/backend/executor/cypher_executor.c` - result building
- `bindings/python/src/graphqlite/` - Python result conversion
- `bindings/rust/src/` - Rust result conversion

### Risk Considerations
- Breaking change for any code that relies on current JSON string format
- Need to ensure all edge cases (NULL, empty results, nested structures) are handled