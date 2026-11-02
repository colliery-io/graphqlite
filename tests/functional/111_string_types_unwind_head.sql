-- ========================================================================
-- Test 111: strings that look like JSON stay strings (GQLITE-T-0481)
-- ========================================================================
-- PURPOSE: text carries no SQLite subtype, so only a type tag says that
--          '7' or '[1]' is a string. UNWIND [s], head(), last() and
--          reduce() took the tag from _gql_typetag, which cannot tell, and
--          the reader turned the string into a number, a boolean or a list.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 111: string types ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:S111 {v: 3, l: [1, 2], s: [''7'', ''x'']})') as setup;

INSERT INTO _assert SELECT 'UNWIND of a string variable',
    json(cypher('WITH ''7'' AS s UNWIND [s, ''true''] AS x RETURN x'))
    = json('[{"x":"7"},{"x":"true"}]');
INSERT INTO _assert SELECT 'UNWIND of toString()',
    json(cypher('MATCH (n:S111) UNWIND [toString(n.v)] AS x RETURN x')) = json('[{"x":"3"}]');
INSERT INTO _assert SELECT 'head() and last() of strings',
    json(cypher('RETURN head([''[1]'']) AS a, last([''1'', ''{}'']) AS b'))
    = json('[{"a":"[1]","b":"{}"}]');
INSERT INTO _assert SELECT 'head() of a string property list',
    json(cypher('MATCH (n:S111) RETURN head(n.s) AS a')) = json('[{"a":"7"}]');
INSERT INTO _assert SELECT 'reduce() into a string, next to another column',
    json(cypher('MATCH (n:S111) RETURN n.v AS k, reduce(acc = '''', x IN n.l | acc + toString(x)) AS x'))
    = json('[{"k":3,"x":"12"}]');

SELECT 'Test 111 passed: ' || count(*) || ' assertions' AS result FROM _assert;
