-- Test: Relationship SET Operations
-- This test validates SET operations on relationship properties
.load ../build/graphqlite.so

-- Test 1: Basic SET operation on string property
SELECT cypher('CREATE (a:Person {name: "Alice"})-[r:KNOWS {since: "2020"}]->(b:Person {name: "Bob"})');
-- Expected: Query executed successfully

SELECT cypher('SET r.since = "2019"');
-- Expected: Updated 1 properties

SELECT cypher('MATCH (a)-[r:KNOWS]->(b) RETURN r');
-- Expected: {"identity": 1, "type": "KNOWS", "start": 1, "end": 2, "properties": {"since": "2019"}}

-- Test 2: SET operation on integer property
SELECT cypher('CREATE (c:Person)-[r2:WORKS_AT {years: 5}]->(d:Company)');
-- Expected: Query executed successfully

SELECT cypher('SET r2.years = 7');
-- Expected: Updated 1 properties

SELECT cypher('MATCH (c)-[r2:WORKS_AT]->(d) RETURN r2');
-- Expected: {"identity": 2, "type": "WORKS_AT", "start": 3, "end": 4, "properties": {"years": 7}}

-- Test 3: SET operation on float property
SELECT cypher('CREATE (e:Person)-[r3:INVESTED_IN {amount: 10000.50}]->(f:Startup)');
-- Expected: Query executed successfully

SELECT cypher('SET r3.amount = 15000.75');
-- Expected: Updated 1 properties

SELECT cypher('MATCH (e)-[r3:INVESTED_IN]->(f) RETURN r3');
-- Expected: {"identity": 3, "type": "INVESTED_IN", "start": 5, "end": 6, "properties": {"amount": 15000.75}}

-- Test 4: SET multiple properties (comma-separated)
SELECT cypher('CREATE (g:Person)-[r4:MENTOR_OF {level: "junior"}]->(h:Person)');
-- Expected: Query executed successfully

SELECT cypher('SET r4.level = "senior", r4.experience = 10');
-- Expected: Updated 2 properties

SELECT cypher('MATCH (g)-[r4:MENTOR_OF]->(h) RETURN r4');
-- Expected: {"identity": 4, "type": "MENTOR_OF", "start": 7, "end": 8, "properties": {"level": "senior", "experience": 10}}

-- Test 5: SET operation preserves other properties
SELECT cypher('CREATE (i:Person)-[r5:FRIEND_OF {since: "2015", trust: 9, active: true}]->(j:Person)');
-- Expected: Query executed successfully

SELECT cypher('SET r5.trust = 10');
-- Expected: Updated 1 properties

SELECT cypher('MATCH (i)-[r5:FRIEND_OF]->(j) RETURN r5');
-- Expected: {"identity": 5, "type": "FRIEND_OF", "start": 9, "end": 10, "properties": {"since": "2015", "trust": 10, "active": true}}

-- Test 6: SET with mixed property types
SELECT cypher('CREATE (k:Person)-[r6:MANAGES {department: "Engineering", team_size: 8, budget: 500000.00}]->(l:Team)');
-- Expected: Query executed successfully

SELECT cypher('SET r6.team_size = 12, r6.budget = 750000.00');
-- Expected: Updated 2 properties

SELECT cypher('MATCH (k)-[r6:MANAGES]->(l) RETURN r6');
-- Expected: {"identity": 6, "type": "MANAGES", "start": 11, "end": 12, "properties": {"department": "Engineering", "team_size": 12, "budget": 750000}}