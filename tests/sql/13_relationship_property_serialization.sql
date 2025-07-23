-- Test: Relationship Property Serialization
-- This test validates that all property types are correctly serialized in relationship results
.load ../build/graphqlite.so

-- Test 1: Text property serialization
SELECT cypher('CREATE (a:Person)-[r1:KNOWS {status: "best_friend"}]->(b:Person)');
-- Expected: Query executed successfully

SELECT cypher('MATCH (a)-[r1:KNOWS]->(b) RETURN r1');
-- Expected: {"identity": 1, "type": "KNOWS", "start": 1, "end": 2, "properties": {"status": "best_friend"}}

-- Test 2: Integer property serialization
SELECT cypher('CREATE (c:Person)-[r2:RATED {score: 5}]->(d:Product)');
-- Expected: Query executed successfully

SELECT cypher('MATCH (c)-[r2:RATED]->(d) RETURN r2');
-- Expected: {"identity": 2, "type": "RATED", "start": 3, "end": 4, "properties": {"score": 5}}

-- Test 3: Float/Real property serialization
SELECT cypher('CREATE (e:Person)-[r3:TRANSFERRED {amount: 1234.56}]->(f:Account)');
-- Expected: Query executed successfully

SELECT cypher('MATCH (e)-[r3:TRANSFERRED]->(f) RETURN r3');
-- Expected: {"identity": 3, "type": "TRANSFERRED", "start": 5, "end": 6, "properties": {"amount": 1234.56}}

-- Test 4: Boolean property serialization
SELECT cypher('CREATE (g:Person)-[r4:FOLLOWS {notifications: true}]->(h:Person)');
-- Expected: Query executed successfully

SELECT cypher('MATCH (g)-[r4:FOLLOWS]->(h) RETURN r4');
-- Expected: {"identity": 4, "type": "FOLLOWS", "start": 7, "end": 8, "properties": {"notifications": true}}

-- Test 5: Mixed property types serialization
SELECT cypher('CREATE (i:Company)-[r5:EMPLOYS {role: "Engineer", salary: 120000.50, years: 3, remote: false}]->(j:Person)');
-- Expected: Query executed successfully

SELECT cypher('MATCH (i)-[r5:EMPLOYS]->(j) RETURN r5');
-- Expected: {"identity": 5, "type": "EMPLOYS", "start": 9, "end": 10, "properties": {"role": "Engineer", "salary": 120000.5, "years": 3, "remote": false}}

-- Test 6: Empty properties serialization
SELECT cypher('CREATE (k:Person)-[r6:CONTACTED]->(l:Person)');
-- Expected: Query executed successfully

SELECT cypher('MATCH (k)-[r6:CONTACTED]->(l) RETURN r6');
-- Expected: {"identity": 6, "type": "CONTACTED", "start": 11, "end": 12, "properties": {}}