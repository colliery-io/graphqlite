-- Test 06: RETURN Clause Features (DISTINCT, ORDER BY, LIMIT, SKIP)

-- Load GraphQLite extension
.load ./build/graphqlite

.print "=== Test 06: RETURN Clause Features ==="

-- Setup: Create test data with varied names and ages
.print "=== Setup - Creating test data ==="
SELECT cypher('CREATE (a:Person {name: "Alice", age: 30})') as setup1;
SELECT cypher('CREATE (b:Person {name: "Bob", age: 25})') as setup2;
SELECT cypher('CREATE (c:Person {name: "Charlie", age: 35})') as setup3;
SELECT cypher('CREATE (d:Person {name: "Alice", age: 28})') as setup4; -- Duplicate name
SELECT cypher('CREATE (e:Person {name: "David", age: 40})') as setup5;

-- Test 1: DISTINCT
.print "=== Test 1: DISTINCT ===";

.print "Test 1.1 - Return names (with duplicates):";
SELECT cypher('MATCH (n:Person) RETURN n.name') as result;

.print "Test 1.2 - Return distinct names:";
SELECT cypher('MATCH (n:Person) RETURN DISTINCT n.name') as result;

.print "Test 1.3 - Return distinct ages:";
SELECT cypher('MATCH (n:Person) RETURN DISTINCT n.age') as result;

-- Test 2: ORDER BY
.print "=== Test 2: ORDER BY ===";

.print "Test 2.1 - Order by name:";
SELECT cypher('MATCH (n:Person) RETURN n.name ORDER BY n.name') as result;

.print "Test 2.2 - Order by age:";
SELECT cypher('MATCH (n:Person) RETURN n.name, n.age ORDER BY n.age') as result;

.print "Test 2.3 - Order by multiple columns:";
SELECT cypher('MATCH (n:Person) RETURN n.name, n.age ORDER BY n.name, n.age') as result;

-- Test 3: LIMIT
.print "=== Test 3: LIMIT ===";

.print "Test 3.1 - Limit to 2 results:";
SELECT cypher('MATCH (n:Person) RETURN n.name LIMIT 2') as result;

.print "Test 3.2 - Limit to 1 result:";
SELECT cypher('MATCH (n:Person) RETURN n.name LIMIT 1') as result;

.print "Test 3.3 - Limit with ORDER BY:";
SELECT cypher('MATCH (n:Person) RETURN n.name ORDER BY n.name LIMIT 3') as result;

-- Test 4: SKIP
.print "=== Test 4: SKIP ===";

.print "Test 4.1 - Skip first 2 results:";
SELECT cypher('MATCH (n:Person) RETURN n.name ORDER BY n.name SKIP 2') as result;

.print "Test 4.2 - Skip first result:";
SELECT cypher('MATCH (n:Person) RETURN n.name ORDER BY n.name SKIP 1') as result;

-- Test 5: Combined clauses
.print "=== Test 5: Combined Clauses ===";

.print "Test 5.1 - DISTINCT + ORDER BY:";
SELECT cypher('MATCH (n:Person) RETURN DISTINCT n.name ORDER BY n.name') as result;

.print "Test 5.2 - ORDER BY + LIMIT:";
SELECT cypher('MATCH (n:Person) RETURN n.name ORDER BY n.age LIMIT 2') as result;

.print "Test 5.3 - ORDER BY + SKIP + LIMIT:";
SELECT cypher('MATCH (n:Person) RETURN n.name ORDER BY n.age SKIP 1 LIMIT 2') as result;

.print "Test 5.4 - All features combined:";
SELECT cypher('MATCH (n:Person) RETURN DISTINCT n.name ORDER BY n.name SKIP 0 LIMIT 10') as result;

-- Verification
.print "=== Verification ===";
.print "Total Person nodes created:";
SELECT cypher('MATCH (n:Person) RETURN n') as total_count;