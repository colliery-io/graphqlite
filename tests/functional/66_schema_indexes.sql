-- ========================================================================
-- Test 66: CREATE / DROP / SHOW INDEX and CONSTRAINT (GQLITE-T-0008)
-- ========================================================================
-- PURPOSE: indexes are records (every property is indexed already);
--          UNIQUE and NOT NULL constraints hold after each write query: a
--          violation fails the query and leaves no write. `index` stays a
--          name.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 66: indexes and constraints ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE INDEX FOR (p:P66) ON (p.name)') as i1;
SELECT cypher('CREATE INDEX ON :C66(name)') as i2;
SELECT cypher('CREATE INDEX i66 IF NOT EXISTS FOR (p:P66) ON (p.name)') as i3;
INSERT INTO _assert SELECT 'SHOW INDEXES',
    json(cypher('SHOW INDEXES')) = json('[{"name":"index_C66_name","type":"RANGE","entityType":"NODE","labelsOrTypes":["C66"],"properties":["name"]},{"name":"index_P66_name","type":"RANGE","entityType":"NODE","labelsOrTypes":["P66"],"properties":["name"]}]');
SELECT cypher('DROP INDEX index_C66_name') as d1;
INSERT INTO _assert SELECT 'DROP INDEX',
    json_array_length(cypher('SHOW INDEXES')) = 1;

SELECT cypher('CREATE CONSTRAINT p66_id FOR (p:P66) REQUIRE p.id IS UNIQUE') as c1;
SELECT cypher('CREATE CONSTRAINT ON (p:P66) ASSERT EXISTS (p.name)') as c2;
INSERT INTO _assert SELECT 'SHOW CONSTRAINTS',
    json_array_length(cypher('SHOW CONSTRAINTS')) = 2;
SELECT cypher('CREATE (:P66 {id: 1, name: "Robin"})') as w1;
SELECT cypher('CREATE (:P66 {id: "1", name: "Sam"})') as w2;
INSERT INTO _assert SELECT 'a string is not the number',
    json_extract(cypher('MATCH (p:P66) RETURN count(p) AS c'), '$[0].c') = 2;
INSERT INTO _assert SELECT 'validate accepts schema statements',
    json_extract(cypher_validate('CREATE CONSTRAINT FOR (p:P66) REQUIRE p.x IS NOT NULL'), '$.valid') = 1;
SELECT cypher('DROP CONSTRAINT p66_id') as d2;
SELECT cypher('CREATE (:P66 {id: 1, name: "Jamie"})') as w3;
INSERT INTO _assert SELECT 'after DROP CONSTRAINT the duplicate is allowed',
    json_extract(cypher('MATCH (p:P66 {id: 1}) RETURN count(p) AS c'), '$[0].c') = 2;
INSERT INTO _assert SELECT 'index stays a name',
    json(cypher('CREATE (index:T66 {index: 1}) RETURN index.index AS i')) = json('[{"i":1}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 66 Complete ===' as test_section;
