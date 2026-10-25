-- ========================================================================
-- Test 68: entity functions on variables carried through WITH / UNWIND
-- ========================================================================
-- PURPOSE: labels(), keys(), properties() and aggregates over a node
--          property emitted `<alias>.id` for a node carried through WITH or
--          UNWIND, where the alias already is the id column:
--          "no such column: _with_0.a.id" (GQLITE-T-0450). keys(null) gave
--          [] instead of null. Pattern predicates such as WHERE (a)-->()
--          after UNWIND had the same fault. Found by the metamorphic harness.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 68: entity functions after WITH / UNWIND ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:I68 {v: 1, s: ''x''})-[:R68 {w: 2}]->(:J68)') as setup;

INSERT INTO _assert SELECT 'keys() after WITH',
    json(cypher('MATCH (a:I68) WITH a RETURN keys(a) AS k')) = json('[{"k":["v","s"]}]');
INSERT INTO _assert SELECT 'labels() after WITH',
    json(cypher('MATCH (a:I68) WITH a RETURN labels(a) AS l')) = json('[{"l":["I68"]}]');
INSERT INTO _assert SELECT 'properties() after WITH',
    json(cypher('MATCH (a:I68) WITH a RETURN properties(a) AS p')) = json('[{"p":{"v":1,"s":"x"}}]');
INSERT INTO _assert SELECT 'labels(), keys() after UNWIND',
    json(cypher('MATCH (a:I68) UNWIND [1] AS i RETURN labels(a) AS l, keys(a) AS k')) = json('[{"l":["I68"],"k":["v","s"]}]');
INSERT INTO _assert SELECT 'relationship functions after WITH',
    json(cypher('MATCH (:I68)-[r]->() WITH r RETURN type(r) AS t, keys(r) AS k, properties(r) AS p')) = json('[{"t":"R68","k":["w"],"p":{"w":2}}]');
INSERT INTO _assert SELECT 'aggregate over a property after WITH',
    json(cypher('MATCH (a:I68) WITH a RETURN size(labels(a)) AS n, collect(a.s) AS c')) = json('[{"n":1,"c":["x"]}]');
INSERT INTO _assert SELECT 'aggregate over an OPTIONAL variable after UNWIND',
    json(cypher('MATCH (a:I68) OPTIONAL MATCH (a)-->(d) UNWIND [1] AS i RETURN collect(labels(d)) AS l')) = json('[{"l":[["J68"]]}]');
INSERT INTO _assert SELECT 'keys(null) is null',
    json(cypher('MATCH (a:I68) OPTIONAL MATCH (a)<--(z) RETURN keys(z) AS k')) = json('[{"k":null}]');
INSERT INTO _assert SELECT 'keys(null) is null after WITH',
    json(cypher('MATCH (a:I68) OPTIONAL MATCH (a)<--(z) WITH z RETURN keys(z) AS k, labels(z) AS l')) = json('[{"k":null,"l":null}]');

INSERT INTO _assert SELECT 'pattern predicate after UNWIND',
    json(cypher('MATCH (a:I68) UNWIND [1] AS i WITH * WHERE (a)-->() RETURN a.v AS v')) = json('[{"v":1}]');
INSERT INTO _assert SELECT 'negated pattern predicate after UNWIND',
    json(cypher('MATCH (a:I68) UNWIND [1] AS i WITH * WHERE NOT (a)-->() RETURN a.v AS v')) = json('[]');
INSERT INTO _assert SELECT 'pattern predicate with a label after an aggregating WITH',
    json(cypher('MATCH (a:I68) WITH a, count(*) AS c WITH * WHERE (a)-->(:J68) RETURN a.v AS v')) = json('[{"v":1}]');
INSERT INTO _assert SELECT 'pattern predicate after two WITH *',
    json(cypher('MATCH (a:I68)-->(b) WITH * WITH * WHERE (b)<--(:I68) RETURN labels(b) AS l')) = json('[{"l":["J68"]}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 68 Complete ===' as test_section;
