-- Minimal Variable-Length Pattern Test
.load ../build/graphqlite.so

-- Simple test: just try to parse a variable-length pattern
SELECT cypher('MATCH (a)-[*]->(b) RETURN b');