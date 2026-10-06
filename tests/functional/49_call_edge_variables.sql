-- ========================================================================
-- Test 49: edge variables across CALL subqueries (GQLITE-T-0373)
-- ========================================================================
-- PURPOSE: An edge bound by the outer MATCH stays an edge after a CALL
--          subquery (property access and type() work), can be imported with
--          WITH l, and several imported variables build valid inner SQL.
-- COVERS:  GQLITE-T-0373.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 49: CALL edge variables ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A49 {id: "a"})-[:L49 {w: 3}]->(:F49 {id: "f"})') as setup;

-- Edge bound outside the CALL, read after it (values compared numerically;
-- exported aliases render as text today, see T-0373 second symptom).
INSERT INTO _assert SELECT 'T-0373 1.1 edge property after CALL',
    CAST(json_extract(cypher('MATCH (a:A49)-[l:L49]->(f:F49) CALL { WITH a RETURN a.id AS x } RETURN f.id AS fid, l.w AS lw'), '$[0].lw') AS INTEGER) = 3
    AND json_extract(cypher('MATCH (a:A49)-[l:L49]->(f:F49) CALL { WITH a RETURN a.id AS x } RETURN f.id AS fid, l.w AS lw'), '$[0].fid') = 'f';
INSERT INTO _assert SELECT 'T-0373 1.2 type(edge) after CALL',
    json_extract(cypher('MATCH (a:A49)-[l:L49]->(f:F49) CALL { WITH a RETURN a.id AS x } RETURN type(l) AS lt'), '$[0].lt') = 'L49';

-- Edge imported into the subquery.
INSERT INTO _assert SELECT 'T-0373 2.1 WITH l inside CALL',
    CAST(json_extract(cypher('MATCH (:A49)-[l:L49]->(:F49) CALL { WITH l RETURN l.w AS n, type(l) AS t } RETURN n, t'), '$[0].n') AS INTEGER) = 3
    AND json_extract(cypher('MATCH (:A49)-[l:L49]->(:F49) CALL { WITH l RETURN l.w AS n, type(l) AS t } RETURN n, t'), '$[0].t') = 'L49';

-- Two imported variables (node + edge): the inner SQL must be valid.
INSERT INTO _assert SELECT 'T-0373 2.2 WITH a, l inside CALL',
    json_extract(cypher('MATCH (a:A49)-[l:L49]->(:F49) CALL { WITH a, l RETURN a.id AS ai, l.w AS n } RETURN ai, n'), '$[0].ai') = 'a'
    AND CAST(json_extract(cypher('MATCH (a:A49)-[l:L49]->(:F49) CALL { WITH a, l RETURN a.id AS ai, l.w AS n } RETURN ai, n'), '$[0].n') AS INTEGER) = 3;

-- Inner MATCH binding an edge, read in the inner RETURN.
INSERT INTO _assert SELECT 'T-0373 2.3 inner MATCH edge variable',
    CAST(json_extract(cypher('MATCH (a:A49) CALL { WITH a MATCH (a)-[l2:L49]->(f2) RETURN l2.w AS w2, f2.id AS f2id } RETURN w2, f2id'), '$[0].w2') AS INTEGER) = 3;

SELECT name, ok FROM _assert;
SELECT '=== Test 49 Complete ===' as test_section;
