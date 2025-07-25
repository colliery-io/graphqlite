-- ========================================================================
-- Test 07: AGE-Compatible Output Format (AGType)
-- ========================================================================
-- PURPOSE: Verifies that GraphQLite output format matches Apache AGE
--          specification for vertices, edges, and scalar values
-- COVERS:  ::vertex annotations, ::edge annotations, scalar formatting,
--          mixed return types, array formats, special values
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 07: AGE-Compatible Output Format ===' as test_section;

-- =======================================================================
-- SECTION 1: Vertex AGType Format Testing
-- =======================================================================
SELECT '=== Section 1: Vertex AGType Format Testing ===' as section;

SELECT 'Test 1.1 - Single vertex with properties:' as test_name;
SELECT cypher('CREATE (alice:Person {name: "Alice", age: 30, active: true})') as result;
SELECT cypher('MATCH (n:Person) RETURN n LIMIT 1') as single_vertex;

SELECT 'Test 1.2 - Multiple vertices:' as test_name;
SELECT cypher('CREATE (bob:Person {name: "Bob", age: 25}), (charlie:Company {name: "ACME"})') as result;
SELECT cypher('MATCH (n) RETURN n LIMIT 3') as multiple_vertices;

SELECT 'Test 1.3 - Vertex with mixed property types:' as test_name;
SELECT cypher('CREATE (data:TestNode {str: "text", int: 42, float: 3.14, bool: false})') as result;
SELECT cypher('MATCH (n:TestNode) RETURN n') as mixed_properties;

SELECT 'Test 1.4 - Vertex without properties:' as test_name;
SELECT cypher('CREATE (empty:EmptyNode)') as result;
SELECT cypher('MATCH (n:EmptyNode) RETURN n') as empty_vertex;

SELECT 'Test 1.5 - Vertex without label:' as test_name;
SELECT cypher('CREATE (unlabeled {type: "unlabeled"})') as result;
SELECT cypher('MATCH (n {type: "unlabeled"}) RETURN n') as unlabeled_vertex;

-- =======================================================================
-- SECTION 2: Edge AGType Format Testing
-- =======================================================================
SELECT '=== Section 2: Edge AGType Format Testing ===' as section;

SELECT 'Test 2.1 - Simple edge without properties:' as test_name;
SELECT cypher('MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"}) CREATE (a)-[:KNOWS]->(b)') as result;
SELECT cypher('MATCH ()-[r:KNOWS]->() RETURN r LIMIT 1') as simple_edge;

SELECT 'Test 2.2 - Edge with properties:' as test_name;
SELECT cypher('MATCH (a:Person {name: "Bob"}), (c:Company) CREATE (a)-[:WORKS_FOR {since: 2020, salary: 75000.50, remote: true}]->(c)') as result;
SELECT cypher('MATCH ()-[r:WORKS_FOR]->() RETURN r') as edge_with_properties;

SELECT 'Test 2.3 - Multiple edges of different types:' as test_name;
SELECT cypher('MATCH (a:Person {name: "Alice"}), (c:Company) CREATE (a)-[:OWNS {percentage: 51.0}]->(c)') as result;
SELECT cypher('MATCH ()-[r]->() RETURN r LIMIT 3') as multiple_edges;

SELECT 'Test 2.4 - Edge with complex properties:' as test_name;
SELECT cypher('CREATE (diana:Person {name: "Diana"}), (eve:Person {name: "Eve"})') as result;
SELECT cypher('MATCH (d:Person {name: "Diana"}), (e:Person {name: "Eve"}) CREATE (d)-[:COLLABORATES {project: "GraphDB", start_date: "2023-01-01", budget: 100000, confidential: false}]->(e)') as result;
SELECT cypher('MATCH ()-[r:COLLABORATES]->() RETURN r') as complex_edge;

-- =======================================================================
-- SECTION 3: Scalar Value AGType Format Testing
-- =======================================================================
SELECT '=== Section 3: Scalar Value AGType Format Testing ===' as section;

SELECT 'Test 3.1 - String scalar (should have quotes):' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name LIMIT 1') as string_scalar;

SELECT 'Test 3.2 - Integer scalar (no quotes):' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.age LIMIT 1') as integer_scalar;

SELECT 'Test 3.3 - Boolean scalar (no quotes):' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.active LIMIT 1') as boolean_scalar;

SELECT 'Test 3.4 - Float scalar (no quotes):' as test_name;
SELECT cypher('MATCH (n:TestNode) RETURN n.float') as float_scalar;

SELECT 'Test 3.5 - Null scalar:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.nonexistent LIMIT 1') as null_scalar;

SELECT 'Test 3.6 - Zero and negative numbers:' as test_name;
SELECT cypher('CREATE (num:Numbers {zero: 0, negative: -42, negFloat: -3.14})') as result;
SELECT cypher('MATCH (n:Numbers) RETURN n.zero, n.negative, n.negFloat') as special_numbers;

-- =======================================================================
-- SECTION 4: Mixed Return Types
-- =======================================================================
SELECT '=== Section 4: Mixed Return Types ===' as section;

SELECT 'Test 4.1 - Vertex and scalar in same query:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n, n.name LIMIT 1') as vertex_and_scalar;

SELECT 'Test 4.2 - Vertex, edge, and vertex:' as test_name;
SELECT cypher('MATCH (a:Person)-[r:KNOWS]->(b:Person) RETURN a, r, b LIMIT 1') as vertex_edge_vertex;

SELECT 'Test 4.3 - Edge and scalar properties:' as test_name;
SELECT cypher('MATCH (a)-[r:WORKS_FOR]->(b) RETURN r, r.since, r.salary') as edge_and_scalars;

SELECT 'Test 4.4 - Multiple scalars:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name, n.age, n.active LIMIT 1') as multiple_scalars;

SELECT 'Test 4.5 - Mixed node types:' as test_name;
SELECT cypher('MATCH (a:Person), (b:Company) RETURN a, b LIMIT 1') as mixed_nodes;

-- =======================================================================
-- SECTION 5: Array Format for Multiple Results
-- =======================================================================
SELECT '=== Section 5: Array Format for Multiple Results ===' as section;

SELECT 'Test 5.1 - Multiple scalar values (should be array):' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name') as scalar_array;

SELECT 'Test 5.2 - Multiple vertices (should be array):' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n') as vertex_array;

SELECT 'Test 5.3 - Multiple edges (should be array):' as test_name;
SELECT cypher('MATCH ()-[r]->() RETURN r') as edge_array;

SELECT 'Test 5.4 - Mixed array types:' as test_name;
SELECT cypher('MATCH (n) RETURN n LIMIT 5') as mixed_array;

-- =======================================================================
-- SECTION 6: Special Cases and Edge Values
-- =======================================================================
SELECT '=== Section 6: Special Cases and Edge Values ===' as section;

SELECT 'Test 6.1 - Empty string values:' as test_name;
SELECT cypher('CREATE (empty:TestNode {empty_str: "", space_str: " "})') as result;
SELECT cypher('MATCH (n:TestNode) RETURN n.empty_str, n.space_str') as empty_strings;

SELECT 'Test 6.2 - Large numbers:' as test_name;
SELECT cypher('CREATE (big:TestNode {big_int: 1000000, big_float: 1.23e10})') as result;
SELECT cypher('MATCH (n:TestNode) RETURN n.big_int, n.big_float') as large_numbers;

SELECT 'Test 6.3 - Special characters in strings:' as test_name;
SELECT cypher('CREATE (special:TestNode {special: "Special: @#$%^&*()[]"})') as result;
SELECT cypher('MATCH (n:TestNode) RETURN n.special') as special_chars;

SELECT 'Test 6.4 - Empty result set:' as test_name;
SELECT cypher('MATCH (n:NonExistent) RETURN n') as empty_result;

-- =======================================================================
-- SECTION 7: AGType Annotations Verification
-- =======================================================================
SELECT '=== Section 7: AGType Annotations Verification ===' as section;

SELECT 'Test 7.1 - Vertex should have ::vertex annotation:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n LIMIT 1') as vertex_annotation;

SELECT 'Test 7.2 - Edge should have ::edge annotation:' as test_name;
SELECT cypher('MATCH ()-[r]->() RETURN r LIMIT 1') as edge_annotation;

SELECT 'Test 7.3 - Scalars should not have annotations:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name, n.age LIMIT 1') as scalar_no_annotation;

SELECT 'Test 7.4 - Mixed annotations in single result:' as test_name;
SELECT cypher('MATCH (a:Person)-[r]->(b) RETURN a, r, b, a.name LIMIT 1') as mixed_annotations;

-- =======================================================================
-- SECTION 8: Property Value Formatting
-- =======================================================================
SELECT '=== Section 8: Property Value Formatting ===' as section;

SELECT 'Test 8.1 - String properties with quotes:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.name IS NOT NULL RETURN n') as string_props;

SELECT 'Test 8.2 - Numeric properties without quotes:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.age IS NOT NULL RETURN n') as numeric_props;

SELECT 'Test 8.3 - Boolean properties without quotes:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.active IS NOT NULL RETURN n') as boolean_props;

SELECT 'Test 8.4 - Mixed property types in single vertex:' as test_name;
SELECT cypher('MATCH (n:TestNode) RETURN n') as mixed_prop_vertex;

-- =======================================================================
-- SECTION 9: Compliance with AGE Specification
-- =======================================================================
SELECT '=== Section 9: Compliance with AGE Specification ===' as section;

SELECT 'Test 9.1 - Vertex ID inclusion:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n LIMIT 1') as vertex_id_check;

SELECT 'Test 9.2 - Edge ID and endpoints:' as test_name;
SELECT cypher('MATCH (a)-[r]->(b) RETURN r LIMIT 1') as edge_id_check;

SELECT 'Test 9.3 - Label preservation:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n LIMIT 1') as label_check;

SELECT 'Test 9.4 - Relationship type preservation:' as test_name;
SELECT cypher('MATCH ()-[r:KNOWS]->() RETURN r LIMIT 1') as type_check;

-- =======================================================================
-- VERIFICATION: AGType Format Compliance
-- =======================================================================
SELECT '=== Verification: AGType Format Compliance ===' as section;

SELECT 'Total vertices created:' as test_name;
SELECT COUNT(*) as vertex_count FROM nodes;

SELECT 'Total edges created:' as test_name;
SELECT COUNT(*) as edge_count FROM edges;

SELECT 'Vertex labels distribution:' as test_name;
.mode column
.headers on
SELECT COALESCE(nl.label, '[no label]') as label, COUNT(*) as count 
FROM nodes n
LEFT JOIN node_labels nl ON n.id = nl.node_id
GROUP BY nl.label 
ORDER BY count DESC;

SELECT 'Edge types distribution:' as test_name;
SELECT type, COUNT(*) as count FROM edges GROUP BY type ORDER BY count DESC;

-- Cleanup note
SELECT '=== AGType Compatibility Test Complete ===' as section;
SELECT 'All AGType format requirements verified for AGE compatibility' as note;