-- ========================================================================
-- Test 45: engine behaviours behind the Rust binding tests (GQLITE-T-0301)
-- ========================================================================
-- PURPOSE: Pin the five engine behaviours whose Rust integration tests were
--          #[ignore]d as "engine regression (T-0301)": COUNT over an
--          OPTIONAL MATCH miss, an edge variable carried through WITH,
--          function calls inside a CREATE property map, CALL { } exporting
--          its inner RETURN aliases, and CALL { } running its body for
--          every inner MATCH row.
-- COVERS:  bindings/rust/tests/integration.rs
--          test_count_skips_nulls_from_optional_match,
--          test_edge_variable_through_with,
--          test_function_call_in_create_property_map,
--          test_call_subquery_exports_inner_return,
--          test_call_subquery_processes_all_inner_match_rows.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 45: Rust binding engine regressions (T-0301) ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

-- ------------------------------------------------------------------------
-- 1. COUNT(r) over an OPTIONAL MATCH miss counts only non-NULL bindings
-- ------------------------------------------------------------------------
SELECT cypher('CREATE (a:Cnt45 {id: "x"})') as setup;
SELECT cypher('CREATE (p:Pet45 {id: "p"})') as setup;
SELECT cypher('MATCH (a:Cnt45), (p:Pet45) CREATE (a)-[:OWNS45]->(p)') as setup;

INSERT INTO _assert SELECT 'T-0301 1.1 COUNT(node) over OPTIONAL MATCH miss is 0',
    json(cypher('MATCH (a:Cnt45) OPTIONAL MATCH (a)-->(r:Ghost45) RETURN a.id AS aid, COUNT(r) AS cnt'))
    = json('[{"aid":"x","cnt":0}]');
INSERT INTO _assert SELECT 'T-0301 1.2 COUNT(edge) over OPTIONAL MATCH miss is 0',
    json(cypher('MATCH (a:Cnt45) OPTIONAL MATCH (a)-[e:NOPE45]->() RETURN COUNT(e) AS cnt'))
    = json('[{"cnt":0}]');
INSERT INTO _assert SELECT 'T-0301 1.3 COUNT(node) over OPTIONAL MATCH hit is 1',
    json(cypher('MATCH (a:Cnt45) OPTIONAL MATCH (a)-->(p:Pet45) RETURN COUNT(p) AS cnt'))
    = json('[{"cnt":1}]');
INSERT INTO _assert SELECT 'T-0301 1.4 COUNT(*) still counts the OPTIONAL MATCH row',
    json(cypher('MATCH (a:Cnt45) OPTIONAL MATCH (a)-->(r:Ghost45) RETURN COUNT(*) AS cnt'))
    = json('[{"cnt":1}]');
INSERT INTO _assert SELECT 'T-0301 1.5 COUNT(n.prop) skips a missing property',
    json(cypher('MATCH (a:Cnt45) RETURN COUNT(a.missing) AS miss, COUNT(a.id) AS hit'))
    = json('[{"miss":0,"hit":1}]');

-- ------------------------------------------------------------------------
-- 2. Edge variable projected through WITH keeps type() and property access
-- ------------------------------------------------------------------------
SELECT cypher('CREATE (:EwA45 {id: "a"})-[:EREL45 {weight: 7}]->(:EwB45 {id: "b"})') as setup;

INSERT INTO _assert SELECT 'T-0301 2.1 WITH a, b, r then type(r), r.weight',
    json(cypher('MATCH (a:EwA45)-[r:EREL45]->(b:EwB45) WITH a, b, r RETURN type(r) AS t, r.weight AS w'))
    = json('[{"t":"EREL45","w":7}]');
INSERT INTO _assert SELECT 'T-0301 2.2 WITH r alone then r.weight',
    json(cypher('MATCH (:EwA45)-[r:EREL45]->(:EwB45) WITH r RETURN r.weight AS w'))
    = json('[{"w":7}]');
INSERT INTO _assert SELECT 'T-0301 2.3 WITH r then WHERE on r.weight',
    json(cypher('MATCH (:EwA45)-[r:EREL45]->(:EwB45) WITH r WHERE r.weight > 5 RETURN type(r) AS t'))
    = json('[{"t":"EREL45"}]');

-- ------------------------------------------------------------------------
-- 3. Function calls inside a CREATE property map are evaluated
-- ------------------------------------------------------------------------
SELECT cypher('CREATE (n:FnCreate45 {upper: toUpper("hello"), lower: toLower("WORLD"), len: size("abc")})') as setup;

INSERT INTO _assert SELECT 'T-0301 3.1 toUpper/toLower/size stored in CREATE map',
    json(cypher('MATCH (n:FnCreate45) RETURN n.upper, n.lower, n.len'))
    = json('[{"n.upper":"HELLO","n.lower":"world","n.len":3}]');

SELECT cypher('CREATE (:FnSrc45 {id: "s"})-[:FnRel45 {name: toUpper("rel")}]->(:FnDst45 {id: toLower("D")})') as setup;
INSERT INTO _assert SELECT 'T-0301 3.2 functions in edge and pattern-node CREATE maps',
    json(cypher('MATCH (:FnSrc45)-[r:FnRel45]->(d:FnDst45) RETURN r.name AS rn, d.id AS did'))
    = json('[{"rn":"REL","did":"d"}]');

-- ------------------------------------------------------------------------
-- 4. CALL { } exports its inner RETURN aliases to the outer scope
-- ------------------------------------------------------------------------
SELECT cypher('CREATE (:CallExp45 {id: "ce1"})') as setup;

INSERT INTO _assert SELECT 'T-0301 4.1 inner RETURN alias visible after CALL',
    json(cypher('MATCH (a:CallExp45) CALL { WITH a RETURN a.id AS inner_id } RETURN a.id AS outer_id, inner_id'))
    = json('[{"outer_id":"ce1","inner_id":"ce1"}]');
INSERT INTO _assert SELECT 'T-0301 4.2 exported alias usable in an expression',
    json(cypher('MATCH (a:CallExp45) CALL { WITH a RETURN a.id AS inner_id } RETURN inner_id + "!" AS tagged'))
    = json('[{"tagged":"ce1!"}]');

SELECT cypher('MATCH (a:CallExp45) CREATE (a)-[:CallL45 {w: 3}]->(:CallF45 {id: "cf1"})') as setup;
-- NOTE: edge property access after CALL (`l.w`) is a separate, pre-existing
-- gap tracked as its own backlog bug; only node properties are asserted here.
INSERT INTO _assert SELECT 'T-0301 4.3 post-CALL expressions with several outer variables',
    json(cypher('MATCH (a:CallExp45)-[l:CallL45]->(f:CallF45) CALL { WITH a RETURN a.id AS inner_id } RETURN inner_id, f.id AS fid, f.id + "!" AS fid2'))
    = json('[{"inner_id":"ce1","fid":"cf1","fid2":"cf1!"}]');
INSERT INTO _assert SELECT 'T-0301 4.4 numeric exported alias stays numeric in arithmetic',
    json(cypher('MATCH (f:CallF45) CALL { WITH f RETURN size(f.id) AS n } RETURN n, n + 1 AS n1'))
    = json('[{"n":3,"n1":4}]');

-- ------------------------------------------------------------------------
-- 5. CALL { } runs its body once per inner MATCH row
-- ------------------------------------------------------------------------
SELECT cypher('CREATE (:CallCo45 {id: "co"})') as setup;
SELECT cypher('CREATE (:CallDep45 {id: "d1"})') as setup;
SELECT cypher('CREATE (:CallDep45 {id: "d2"})') as setup;
SELECT cypher('CREATE (:CallDep45 {id: "d3"})') as setup;

INSERT INTO _assert SELECT 'T-0301 5.1 CALL MERGE reports 3 relationships created',
    json_extract(cypher('MATCH (c:CallCo45) CALL { WITH c MATCH (d:CallDep45) MERGE (c)-[:CHAS45]->(d) }'),
                 '$.relationships_created') = 3;
INSERT INTO _assert SELECT 'T-0301 5.2 all three inner rows linked',
    json(cypher('MATCH (:CallCo45)-[:CHAS45]->(d:CallDep45) RETURN d.id ORDER BY d.id'))
    = json('[{"d.id":"d1"},{"d.id":"d2"},{"d.id":"d3"}]');
INSERT INTO _assert SELECT 'T-0301 5.3 re-running the CALL MERGE creates nothing new',
    json_extract(cypher('MATCH (c:CallCo45) CALL { WITH c MATCH (d:CallDep45) MERGE (c)-[:CHAS45]->(d) }'),
                 '$.relationships_created') = 0;

SELECT name, ok FROM _assert;
SELECT '=== Test 45 Complete ===' as test_section;
