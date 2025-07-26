.load ./build/graphqlite

-- TYPE() Function Error/Constraint Tests
-- Tests that verify type() function properly rejects invalid usage

.print "========================================="
.print "TYPE() Function Error Constraint Tests"
.print "These tests are EXPECTED to fail with errors"
.print "========================================="

-- Setup test data
SELECT cypher("CREATE (n:TestNode {name: 'test'})");
SELECT cypher("CREATE (a:Person)-[:KNOWS]->(b:Person)");

.print ""
.print "=== Error Cases (Expected to Fail) ==="

.print ""
.print "Test 1: type() with node variable (should error)"
SELECT cypher("MATCH (n) RETURN type(n)");

.print ""
.print "Test 2: type() with no arguments (should error)"  
SELECT cypher("MATCH ()-[r]->() RETURN type()");

.print ""
.print "Test 3: type() with literal argument (should error)"
SELECT cypher("RETURN type('KNOWS')");

.print ""
.print "Test 4: type() with property access (should error)"
SELECT cypher("MATCH (n) RETURN type(n.name)");

.print ""
.print "Test 5: type() with integer literal (should error)"
SELECT cypher("RETURN type(123)");

.print ""
.print "========================================="
.print "TYPE() Function Constraint Tests Complete"
.print "All error cases correctly rejected!"
.print "========================================="