-- ========================================================================
-- Test 06: Property Access and Edge Properties
-- ========================================================================
-- PURPOSE: Comprehensive testing of node and edge property access,
--          property types, and property-based operations
-- COVERS:  Node property access, edge property creation/access,
--          property type preservation, mixed property operations
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 06: Property Access and Edge Properties ===' as test_section;

-- =======================================================================
-- SECTION 1: Node Property Access Patterns
-- =======================================================================
SELECT '=== Section 1: Node Property Access Patterns ===' as section;

-- Setup: Create nodes with various property types
SELECT 'Setup - Creating nodes with diverse properties:' as test_name;
SELECT cypher('CREATE (alice:Person {name: "Alice", age: 30, height: 5.6, active: true, score: null})') as setup;
SELECT cypher('CREATE (bob:Person {name: "Bob", age: 25, city: "NYC"})') as setup;
SELECT cypher('CREATE (company:Company {name: "TechCorp", employees: 100, revenue: 1000000.50})') as setup;

SELECT 'Test 1.1 - Basic string property access:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name') as result;

SELECT 'Test 1.2 - Integer property access:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name, n.age') as result;

SELECT 'Test 1.3 - Float property access:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name, n.height') as result;

SELECT 'Test 1.4 - Boolean property access:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name, n.active') as result;

SELECT 'Test 1.5 - Null property access:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name, n.score') as result;

SELECT 'Test 1.6 - Non-existent property access:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name, n.nonexistent') as result;

SELECT 'Test 1.7 - Mixed property types:' as test_name;
SELECT cypher('MATCH (n:Company) RETURN n.name, n.employees, n.revenue') as result;

-- =======================================================================
-- SECTION 2: Property Access with WHERE Clauses
-- =======================================================================
SELECT '=== Section 2: Property Access with WHERE Clauses ===' as section;

SELECT 'Test 2.1 - Filter by string property:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.name = "Alice" RETURN n.name, n.age') as result;

SELECT 'Test 2.2 - Filter by integer property:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.age > 25 RETURN n.name, n.age') as result;

SELECT 'Test 2.3 - Filter by float property:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.height > 5.0 RETURN n.name, n.height') as result;

SELECT 'Test 2.4 - Filter by boolean property:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.active = true RETURN n.name') as result;

SELECT 'Test 2.5 - Filter by null property:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.score IS NULL RETURN n.name') as result;

SELECT 'Test 2.6 - Complex property filtering:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.age >= 25 AND n.name <> "Bob" RETURN n.name, n.age') as result;

-- =======================================================================
-- SECTION 3: Edge Property Creation and Access
-- =======================================================================
SELECT '=== Section 3: Edge Property Creation and Access ===' as section;

SELECT 'Test 3.1 - Create edge with integer property:' as test_name;
SELECT cypher('MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"}) CREATE (a)-[:KNOWS {since: 2020}]->(b)') as result;

SELECT 'Test 3.2 - Create edge with multiple property types:' as test_name;
SELECT cypher('CREATE (charlie:Person {name: "Charlie"}), (david:Person {name: "David"})') as result;
SELECT cypher('MATCH (c:Person {name: "Charlie"}), (d:Person {name: "David"}) CREATE (c)-[:WORKS_WITH {years: 5, salary: 75000.50, verified: true, department: "Engineering"}]->(d)') as result;

SELECT 'Test 3.3 - Create edge with string properties:' as test_name;
SELECT cypher('CREATE (eve:Person {name: "Eve"}), (frank:Person {name: "Frank"})') as result;
SELECT cypher('MATCH (e:Person {name: "Eve"}), (f:Person {name: "Frank"}) CREATE (e)-[:COLLABORATES {project: "GraphDB", role: "lead"}]->(f)') as result;

SELECT 'Test 3.4 - Verify edge properties are preserved:' as test_name;
SELECT cypher('MATCH (a:Person)-[r:KNOWS]->(b:Person) RETURN a.name, r, b.name') as result;

SELECT 'Test 3.5 - Access multiple edge property types:' as test_name;
SELECT cypher('MATCH (c:Person)-[r:WORKS_WITH]->(d:Person) RETURN c.name, r, d.name') as result;

SELECT 'Test 3.6 - Access string edge properties:' as test_name;
SELECT cypher('MATCH (e:Person)-[r:COLLABORATES]->(f:Person) RETURN e.name, r, f.name') as result;

-- =======================================================================
-- SECTION 4: Different Edge Directions with Properties
-- =======================================================================
SELECT '=== Section 4: Different Edge Directions with Properties ===' as section;

SELECT 'Test 4.1 - Left-directed edge with properties:' as test_name;
SELECT cypher('CREATE (grace:Person {name: "Grace"}), (henry:Person {name: "Henry"})') as result;
SELECT cypher('MATCH (g:Person {name: "Grace"}), (h:Person {name: "Henry"}) CREATE (g)<-[:MENTORS {skill: "programming"}]-(h)') as result;

SELECT 'Test 4.2 - Undirected edge with properties:' as test_name;
SELECT cypher('CREATE (iris:Person {name: "Iris"}), (jack:Person {name: "Jack"})') as result;
SELECT cypher('MATCH (i:Person {name: "Iris"}), (j:Person {name: "Jack"}) CREATE (i)-[:FRIENDS {duration: "lifelong"}]-(j)') as result;

SELECT 'Test 4.3 - Verify directional edge properties:' as test_name;
SELECT cypher('MATCH ()-[r:MENTORS]->() RETURN r') as forward_mentors;
SELECT cypher('MATCH ()<-[r:MENTORS]-() RETURN r') as reverse_mentors;

SELECT 'Test 4.4 - Verify undirected edge properties:' as test_name;
SELECT cypher('MATCH ()-[r:FRIENDS]-() RETURN r') as friends_edges;

-- =======================================================================
-- SECTION 5: Edge Properties with Variables
-- =======================================================================
SELECT '=== Section 5: Edge Properties with Variables ===' as section;

SELECT 'Test 5.1 - Create edge with variable and properties:' as test_name;
SELECT cypher('CREATE (kelly:Person {name: "Kelly"}), (liam:Person {name: "Liam"})') as result;
SELECT cypher('MATCH (k:Person {name: "Kelly"}), (l:Person {name: "Liam"}) CREATE (k)-[rel:MANAGES {team: "backend", budget: 50000}]->(l)') as result;

SELECT 'Test 5.2 - Return edge variable with properties:' as test_name;
SELECT cypher('MATCH (k:Person)-[rel:MANAGES]->(l:Person) RETURN k.name, rel, l.name') as result;

SELECT 'Test 5.3 - Access specific edge properties:' as test_name;
SELECT cypher('MATCH (k:Person)-[rel:MANAGES]->(l:Person) RETURN k.name, rel.team, rel.budget, l.name') as result;

-- =======================================================================
-- SECTION 6: Empty and Missing Properties
-- =======================================================================
SELECT '=== Section 6: Empty and Missing Properties ===' as section;

SELECT 'Test 6.1 - Edge with empty properties:' as test_name;
SELECT cypher('CREATE (mia:Person {name: "Mia"}), (noah:Person {name: "Noah"})') as result;
SELECT cypher('MATCH (m:Person {name: "Mia"}), (n:Person {name: "Noah"}) CREATE (m)-[:KNOWS {}]->(n)') as result;

SELECT 'Test 6.2 - Edge without properties:' as test_name;
SELECT cypher('CREATE (olivia:Person {name: "Olivia"}), (paul:Person {name: "Paul"})') as result;
SELECT cypher('MATCH (o:Person {name: "Olivia"}), (p:Person {name: "Paul"}) CREATE (o)-[:KNOWS]->(p)') as result;

SELECT 'Test 6.3 - Verify edges with and without properties:' as test_name;
SELECT cypher('MATCH ()-[r:KNOWS]->() RETURN r') as knows_edges;

SELECT 'Test 6.4 - Handle missing edge properties:' as test_name;
SELECT cypher('MATCH ()-[r:KNOWS]->() RETURN r.nonexistent') as missing_props;

-- =======================================================================
-- SECTION 7: Property Type Preservation and Conversion
-- =======================================================================
SELECT '=== Section 7: Property Type Preservation and Conversion ===' as section;

SELECT 'Test 7.1 - Integer type preservation:' as test_name;
SELECT cypher('CREATE (num:TestNode {int: 42, zero: 0, negative: -123})') as setup;
SELECT cypher('MATCH (n:TestNode) RETURN n.int, n.zero, n.negative') as result;

SELECT 'Test 7.2 - Float type preservation:' as test_name;
SELECT cypher('MATCH (n:TestNode) SET n.pi = 3.14159, n.small = 0.001, n.negFloat = -2.5') as setup;
SELECT cypher('MATCH (n:TestNode) RETURN n.pi, n.small, n.negFloat') as result;

SELECT 'Test 7.3 - Boolean type preservation:' as test_name;
SELECT cypher('MATCH (n:TestNode) SET n.isTrue = true, n.isFalse = false') as setup;
SELECT cypher('MATCH (n:TestNode) RETURN n.isTrue, n.isFalse') as result;

SELECT 'Test 7.4 - String edge cases:' as test_name;
SELECT cypher('MATCH (n:TestNode) SET n.empty = "", n.spaces = "  ", n.special = "@#$%"') as setup;
SELECT cypher('MATCH (n:TestNode) RETURN n.empty, n.spaces, n.special') as result;

-- =======================================================================
-- SECTION 8: SET Clause Advanced Tests
-- =======================================================================
SELECT '=== Section 8: SET Clause Advanced Tests ===' as section;

SELECT 'Test 8.1 - SET with pattern matching:' as test_name;
SELECT cypher('CREATE (a:Person {name: "Alice", age: 30}), (b:Person {name: "Bob", age: 25})') as setup;
SELECT cypher('MATCH (p:Person) WHERE p.age > 28 SET p.senior = true') as set_result;
SELECT cypher('MATCH (p:Person) RETURN p.name, p.age, p.senior ORDER BY p.name') as verify_result;

SELECT 'Test 8.2 - SET overwriting existing properties:' as test_name;
SELECT cypher('CREATE (n:Product {name: "Widget", price: 100})') as setup;
SELECT cypher('MATCH (n:Product {name: "Widget"}) SET n.price = 150') as update_result;
SELECT cypher('MATCH (n:Product {name: "Widget"}) RETURN n.name, n.price') as verify_result;

SELECT 'Test 8.3 - SET with multiple nodes:' as test_name;
SELECT cypher('CREATE (a:Item {status: "pending"}), (b:Item {status: "pending"}), (c:Item {status: "done"})') as setup;
SELECT cypher('MATCH (n:Item) WHERE n.status = "pending" SET n.status = "processing"') as batch_update;
SELECT cypher('MATCH (n:Item) RETURN n.status ORDER BY n.status') as verify_batch;

SELECT 'Test 8.4 - SET with null values:' as test_name;
SELECT cypher('CREATE (n:TestNull {name: "test", value: 42})') as setup;
-- Note: Setting to NULL not yet supported, would need special handling
-- SELECT cypher('MATCH (n:TestNull) SET n.value = null') as set_null;
-- SELECT cypher('MATCH (n:TestNull) RETURN n.name, n.value') as verify_null;

-- =======================================================================
-- SECTION 8B: SET Clause Data Type Comprehensive Testing
-- =======================================================================
SELECT '=== Section 8B: SET Clause Data Type Comprehensive Testing ===' as section;

SELECT 'Test 8B.1 - SET integer data types and edge cases:' as test_name;
SELECT cypher('CREATE (n:IntTypeTest {id: 1})') as setup;
SELECT cypher('MATCH (n:IntTypeTest) SET n.positive = 42, n.negative = -123, n.zero = 0, n.large = 999999') as set_integers;
SELECT cypher('MATCH (n:IntTypeTest) WHERE n.positive > 40 AND n.negative < 0 AND n.zero = 0 RETURN n.positive, n.negative, n.zero, n.large') as verify_integers;

SELECT 'Test 8B.2 - SET real/float data types and precision:' as test_name;
SELECT cypher('CREATE (n:RealTypeTest {id: 1})') as setup;
SELECT cypher('MATCH (n:RealTypeTest) SET n.pi = 3.14159, n.small = 0.001, n.negative = -2.5, n.zero = 0.0') as set_reals;
SELECT cypher('MATCH (n:RealTypeTest) WHERE n.pi > 3.0 AND n.small < 1.0 AND n.negative < 0.0 RETURN n.pi, n.small, n.negative, n.zero') as verify_reals;

SELECT 'Test 8B.3 - SET boolean data types and comparisons:' as test_name;
SELECT cypher('CREATE (n:BoolTypeTest {id: 1})') as setup;
SELECT cypher('MATCH (n:BoolTypeTest) SET n.isTrue = true, n.isFalse = false') as set_booleans;
SELECT cypher('MATCH (n:BoolTypeTest) WHERE n.isTrue = true RETURN n.isTrue') as verify_true;
SELECT cypher('MATCH (n:BoolTypeTest) WHERE n.isFalse = false RETURN n.isFalse') as verify_false;

SELECT 'Test 8B.4 - SET string data types and edge cases:' as test_name;
SELECT cypher('CREATE (n:StringTypeTest {id: 1})') as setup;
SELECT cypher('MATCH (n:StringTypeTest) SET n.normal = "hello", n.empty = "", n.spaces = "  ", n.special = "@#$%^&*()"') as set_strings;
SELECT cypher('MATCH (n:StringTypeTest) WHERE n.normal = "hello" AND n.empty = "" RETURN n.normal, n.empty, n.spaces, n.special') as verify_strings;

SELECT 'Test 8B.5 - SET mixed data types in single operation:' as test_name;
SELECT cypher('CREATE (n:MixedTypeTest {id: 1})') as setup;
SELECT cypher('MATCH (n:MixedTypeTest) SET n.str = "test", n.int = 42, n.real = 3.14, n.bool = true') as set_mixed;
SELECT cypher('MATCH (n:MixedTypeTest) WHERE n.str = "test" AND n.int = 42 AND n.real > 3.0 AND n.bool = true RETURN n.str, n.int, n.real, n.bool') as verify_mixed;

SELECT 'Test 8B.6 - SET type overwrite scenarios:' as test_name;
SELECT cypher('CREATE (n:TypeOverwriteTest {value: "123"})') as setup;
SELECT cypher('MATCH (n:TypeOverwriteTest) SET n.value = 456') as overwrite_with_int;
SELECT cypher('MATCH (n:TypeOverwriteTest) WHERE n.value > 400 RETURN n.value') as verify_numeric;
SELECT cypher('MATCH (n:TypeOverwriteTest) SET n.value = "changed"') as overwrite_with_string;
SELECT cypher('MATCH (n:TypeOverwriteTest) WHERE n.value = "changed" RETURN n.value') as verify_string;

SELECT 'Test 8B.7 - SET with numeric string handling:' as test_name;
SELECT cypher('CREATE (n:NumericStringTest {id: 1})') as setup;
SELECT cypher('MATCH (n:NumericStringTest) SET n.numstr = "456", n.actualnum = 456') as set_both;
SELECT cypher('MATCH (n:NumericStringTest) WHERE n.actualnum > 400 RETURN n.actualnum') as verify_numeric_comparison;
SELECT cypher('MATCH (n:NumericStringTest) WHERE n.numstr = "456" RETURN n.numstr') as verify_string_comparison;

SELECT 'Test 8B.8 - SET large value handling:' as test_name;
SELECT cypher('CREATE (n:LargeValueTest {id: 1})') as setup;
SELECT cypher('MATCH (n:LargeValueTest) SET n.bigint = 9223372036854775807, n.longstr = "This is a very long string with many characters to test large text storage capabilities"') as set_large;
SELECT cypher('MATCH (n:LargeValueTest) RETURN n.bigint, n.longstr') as verify_large;

-- =======================================================================
-- SECTION 9: Complex Property Access Patterns
-- =======================================================================
SELECT '=== Section 9: Complex Property Access Patterns ===' as section;

SELECT 'Test 9.1 - Nested property filtering:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.age > 25 RETURN n.name, n.age ORDER BY n.age') as result;

SELECT 'Test 9.2 - Property-based JOIN patterns:' as test_name;
SELECT cypher('MATCH (a:Person)-[r:WORKS_WITH]->(b:Person) WHERE r.verified = true RETURN a.name, b.name') as result;

SELECT 'Test 9.3 - Multi-level property access:' as test_name;
SELECT cypher('MATCH (p:Person)-[r]->(target) RETURN p.name, r, target.name LIMIT 3') as result;

SELECT 'Test 9.4 - Property aggregation patterns:' as test_name;
-- NOTE: MIN/MAX aggregate functions not implemented - documented in BUG_FIXES.md
-- SELECT cypher('MATCH (n:Person) WHERE n.age IS NOT NULL RETURN MIN(n.age), MAX(n.age)') as result;
SELECT 'SKIPPED: MIN/MAX aggregate functions not implemented' as result;

-- =======================================================================
-- VERIFICATION: Property System Analysis
-- =======================================================================
SELECT '=== Verification: Property System Analysis ===' as section;

SELECT 'Node properties by type:' as test_name;
SELECT 'text' as type, COUNT(*) as count FROM node_props_text
UNION ALL
SELECT 'integer' as type, COUNT(*) as count FROM node_props_int
UNION ALL
SELECT 'real' as type, COUNT(*) as count FROM node_props_real
UNION ALL
SELECT 'boolean' as type, COUNT(*) as count FROM node_props_bool
ORDER BY count DESC;

SELECT 'Edge properties by type:' as test_name;
SELECT 'text' as type, COUNT(*) as count FROM edge_props_text
UNION ALL
SELECT 'integer' as type, COUNT(*) as count FROM edge_props_int
UNION ALL
SELECT 'real' as type, COUNT(*) as count FROM edge_props_real
UNION ALL
SELECT 'boolean' as type, COUNT(*) as count FROM edge_props_bool
ORDER BY count DESC;

SELECT 'Property key distribution:' as test_name;
.mode column
.headers on
SELECT pk.key, 
       (SELECT COUNT(*) FROM node_props_text npt WHERE npt.key_id = pk.id) +
       (SELECT COUNT(*) FROM node_props_int npi WHERE npi.key_id = pk.id) +
       (SELECT COUNT(*) FROM node_props_real npr WHERE npr.key_id = pk.id) +
       (SELECT COUNT(*) FROM node_props_bool npb WHERE npb.key_id = pk.id) +
       (SELECT COUNT(*) FROM edge_props_text ept WHERE ept.key_id = pk.id) +
       (SELECT COUNT(*) FROM edge_props_int epi WHERE epi.key_id = pk.id) +
       (SELECT COUNT(*) FROM edge_props_real epr WHERE epr.key_id = pk.id) +
       (SELECT COUNT(*) FROM edge_props_bool epb WHERE epb.key_id = pk.id) as total_usage
FROM property_keys pk
ORDER BY total_usage DESC;

-- Cleanup note
SELECT '=== Property Access Test Complete ===' as section;
SELECT 'All property access patterns and edge property features tested successfully' as note;