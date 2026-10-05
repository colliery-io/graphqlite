-- ========================================================================
-- Test 43: structured diagnostics + non-executing validation (GitHub #16)
-- ========================================================================
-- PURPOSE: cypher_validate() reports a stable "code" and, for syntax
--          errors, the 1-based "line" and "column" of the offending token;
--          it also runs the compile-time semantic pass so a query the
--          grammar accepts but openCypher rejects (RETURN NOT 1, UNION
--          column mismatch) is reported as VALIDATION_ERROR; and it never
--          executes the query.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 43: validate diagnostics (GH-16) ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

-- =======================================================================
-- SECTION 1: valid query
-- =======================================================================
SELECT 'Test 1.1 - valid query:' as test_name;
SELECT cypher_validate('MATCH (n:Person) RETURN n.name') as result;
INSERT INTO _assert SELECT 'GH-16 1.1 valid flag',
    json_extract(cypher_validate('MATCH (n:Person) RETURN n.name'), '$.valid') = 1;
INSERT INTO _assert SELECT 'GH-16 1.1 no error key',
    json_type(cypher_validate('MATCH (n:Person) RETURN n.name'), '$.error') IS NULL;

-- =======================================================================
-- SECTION 2: grammar-stage syntax errors carry code + line + column
-- =======================================================================
SELECT 'Test 2.1 - missing closing paren:' as test_name;
SELECT cypher_validate('MATCH (n:Person RETURN n.name') as result;
INSERT INTO _assert SELECT 'GH-16 2.1 invalid',
    json_extract(cypher_validate('MATCH (n:Person RETURN n.name'), '$.valid') = 0;
INSERT INTO _assert SELECT 'GH-16 2.1 code',
    json_extract(cypher_validate('MATCH (n:Person RETURN n.name'), '$.code') = 'PARSE_ERROR';
INSERT INTO _assert SELECT 'GH-16 2.1 line',
    json_extract(cypher_validate('MATCH (n:Person RETURN n.name'), '$.line') = 1;
INSERT INTO _assert SELECT 'GH-16 2.1 column',
    json_extract(cypher_validate('MATCH (n:Person RETURN n.name'), '$.column') = 17;
INSERT INTO _assert SELECT 'GH-16 2.1 expected-token hint',
    instr(json_extract(cypher_validate('MATCH (n:Person RETURN n.name'), '$.error'), 'expecting '')''') > 0;

SELECT 'Test 2.2 - error on the third line:' as test_name;
SELECT cypher_validate('MATCH (n)' || char(10) || 'WHERE n.x = ' || char(10) || 'RETURN n') as result;
INSERT INTO _assert SELECT 'GH-16 2.2 line 3',
    json_extract(cypher_validate('MATCH (n)' || char(10) || 'WHERE n.x = ' || char(10) || 'RETURN n'), '$.line') = 3;
INSERT INTO _assert SELECT 'GH-16 2.2 column 1',
    json_extract(cypher_validate('MATCH (n)' || char(10) || 'WHERE n.x = ' || char(10) || 'RETURN n'), '$.column') = 1;

-- =======================================================================
-- SECTION 3: scanner-stage errors carry the token column (was 0)
-- =======================================================================
SELECT 'Test 3.1 - integer overflow literal:' as test_name;
SELECT cypher_validate('RETURN 99999999999999999999999') as result;
INSERT INTO _assert SELECT 'GH-16 3.1 code',
    json_extract(cypher_validate('RETURN 99999999999999999999999'), '$.code') = 'PARSE_ERROR';
INSERT INTO _assert SELECT 'GH-16 3.1 column 8',
    json_extract(cypher_validate('RETURN 99999999999999999999999'), '$.column') = 8;

SELECT 'Test 3.2 - unterminated string (message contains a double quote; JSON stays valid):' as test_name;
SELECT cypher_validate('RETURN "unterminated') as result;
INSERT INTO _assert SELECT 'GH-16 3.2 json valid',
    json_valid(cypher_validate('RETURN "unterminated')) = 1;
INSERT INTO _assert SELECT 'GH-16 3.2 column 8',
    json_extract(cypher_validate('RETURN "unterminated'), '$.column') = 8;
INSERT INTO _assert SELECT 'GH-16 3.2 quote preserved in message',
    instr(json_extract(cypher_validate('RETURN "unterminated'), '$.error'), '''"''') > 0;

SELECT 'Test 3.3 - bad character on line 2:' as test_name;
INSERT INTO _assert SELECT 'GH-16 3.3 line 2 col 10',
    json_extract(cypher_validate('MATCH (n) RETURN n.name' || char(10) || 'ORDER BY ~x'), '$.line') = 2
    AND json_extract(cypher_validate('MATCH (n) RETURN n.name' || char(10) || 'ORDER BY ~x'), '$.column') = 10;

-- =======================================================================
-- SECTION 4: static semantic errors are VALIDATION_ERROR without location
-- =======================================================================
SELECT 'Test 4.1 - RETURN NOT 1:' as test_name;
SELECT cypher_validate('RETURN NOT 1') as result;
INSERT INTO _assert SELECT 'GH-16 4.1 invalid',
    json_extract(cypher_validate('RETURN NOT 1'), '$.valid') = 0;
INSERT INTO _assert SELECT 'GH-16 4.1 code',
    json_extract(cypher_validate('RETURN NOT 1'), '$.code') = 'VALIDATION_ERROR';
INSERT INTO _assert SELECT 'GH-16 4.1 no line key',
    json_type(cypher_validate('RETURN NOT 1'), '$.line') IS NULL;

SELECT 'Test 4.2 - UNION column mismatch:' as test_name;
SELECT cypher_validate('RETURN 1 UNION RETURN 1, 2') as result;
INSERT INTO _assert SELECT 'GH-16 4.2 code',
    json_extract(cypher_validate('RETURN 1 UNION RETURN 1, 2'), '$.code') = 'VALIDATION_ERROR';

-- =======================================================================
-- SECTION 5: validation never executes
-- =======================================================================
SELECT 'Test 5.1 - validating a CREATE creates nothing:' as test_name;
SELECT cypher_validate('CREATE (:G16Probe {k: 1})') as result;
INSERT INTO _assert SELECT 'GH-16 5.1 create validates',
    json_extract(cypher_validate('CREATE (:G16Probe {k: 1})'), '$.valid') = 1;
INSERT INTO _assert SELECT 'GH-16 5.1 nothing created',
    json_extract(cypher('MATCH (n:G16Probe) RETURN count(n) AS c'), '$[0].c') = 0;

-- =======================================================================
-- VERIFICATION SUMMARY
-- =======================================================================
SELECT '=== Assertions run (all must show ok=1) ===' as section;
SELECT name, ok FROM _assert;

SELECT '=== Test 43 Complete ===' as test_section;
