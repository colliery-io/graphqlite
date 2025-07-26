-- DELETE Constraint Test
-- This test is expected to fail with a constraint error
-- Run separately to verify constraint enforcement

.load ./build/graphqlite

-- Create an author with books
SELECT cypher("CREATE (a:Author {name: 'Tolkien'})-[:WROTE]->(b:Book {title: 'LOTR'})");
SELECT cypher("CREATE (a:Author {name: 'Tolkien'})-[:WROTE]->(c:Book {title: 'Hobbit'})");

.print "Attempting to delete Author node that has relationships..."
.print "This SHOULD fail with: 'Cannot delete node - it still has relationships'"
.print ""

-- This will cause an error and exit
SELECT cypher("MATCH (a:Author {name: 'Tolkien'}) DELETE a");

-- This line should not be reached
.print "ERROR: Delete should have failed!"