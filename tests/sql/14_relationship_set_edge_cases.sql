-- Test: Relationship SET Operation Edge Cases
-- This test validates edge cases and error handling for SET operations
.load ../build/graphqlite.so

-- Test 1: SET on non-existent property (should create new property)
SELECT cypher('CREATE (a:Person)-[r1:KNOWS]->(b:Person)');
-- Expected: Query executed successfully

SELECT cypher('SET r1.new_property = "created"');
-- Expected: Updated 1 properties

SELECT cypher('MATCH (a)-[r1:KNOWS]->(b) RETURN r1');
-- Expected: {"identity": 1, "type": "KNOWS", "start": 1, "end": 2, "properties": {"new_property": "created"}}

-- Test 2: SET with type change (string to integer)
SELECT cypher('CREATE (c:Person)-[r2:RATED {score: "5"}]->(d:Product)');
-- Expected: Query executed successfully

SELECT cypher('SET r2.score = 10');
-- Expected: Updated 1 properties

SELECT cypher('MATCH (c)-[r2:RATED]->(d) RETURN r2');
-- Expected: {"identity": 2, "type": "RATED", "start": 3, "end": 4, "properties": {"score": "5", "score": 10}}
-- Note: This might result in both properties being stored in different tables

-- Test 3: SET with empty string
SELECT cypher('CREATE (e:Person)-[r3:MESSAGED {content: "Hello"}]->(f:Person)');
-- Expected: Query executed successfully

SELECT cypher('SET r3.content = ""');
-- Expected: Updated 1 properties

SELECT cypher('MATCH (e)-[r3:MESSAGED]->(f) RETURN r3');
-- Expected: {"identity": 3, "type": "MESSAGED", "start": 5, "end": 6, "properties": {"content": ""}}

-- Test 4: SET with very large integer
SELECT cypher('CREATE (g:Bank)-[r4:TRANSFERRED {amount: 100}]->(h:Bank)');
-- Expected: Query executed successfully

SELECT cypher('SET r4.amount = 9223372036854775807');
-- Expected: Updated 1 properties

SELECT cypher('MATCH (g)-[r4:TRANSFERRED]->(h) RETURN r4');
-- Expected: {"identity": 4, "type": "TRANSFERRED", "start": 7, "end": 8, "properties": {"amount": 9223372036854775807}}

-- Test 5: SET with scientific notation float
SELECT cypher('CREATE (i:Lab)-[r5:MEASURED {value: 0.0}]->(j:Sample)');
-- Expected: Query executed successfully

SELECT cypher('SET r5.value = 1.23e-10');
-- Expected: Updated 1 properties

SELECT cypher('MATCH (i)-[r5:MEASURED]->(j) RETURN r5');
-- Expected: {"identity": 5, "type": "MEASURED", "start": 9, "end": 10, "properties": {"value": 1.23e-10}}

-- Test 6: SET multiple times on same property
SELECT cypher('CREATE (k:Person)-[r6:SCORED {points: 0}]->(l:Game)');
-- Expected: Query executed successfully

SELECT cypher('SET r6.points = 100');
-- Expected: Updated 1 properties

SELECT cypher('SET r6.points = 200');
-- Expected: Updated 1 properties

SELECT cypher('SET r6.points = 300');
-- Expected: Updated 1 properties

SELECT cypher('MATCH (k)-[r6:SCORED]->(l) RETURN r6');
-- Expected: {"identity": 6, "type": "SCORED", "start": 11, "end": 12, "properties": {"points": 300}}