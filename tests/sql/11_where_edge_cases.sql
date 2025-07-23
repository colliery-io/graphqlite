-- Test 11: WHERE clause edge cases and error handling
.load ../build/graphqlite.so

-- Create test data for edge case testing
SELECT '=== Creating Edge Case Test Data ===';
SELECT cypher('CREATE (n:EdgeTest {name: "Zero", value: 0, rating: 0.0, active: false})');
SELECT cypher('CREATE (n:EdgeTest {name: "Negative", value: -100, rating: -2.5, active: true})');
SELECT cypher('CREATE (n:EdgeTest {name: "Large", value: 2147483647, rating: 999999.99, active: true})');
SELECT cypher('CREATE (n:EdgeTest {name: "Small", value: -2147483648, rating: 0.0001, active: false})');
SELECT cypher('CREATE (n:EdgeTest {name: "Scientific", value: 123, rating: 1.23e-5, active: true})');
SELECT cypher('CREATE (n:EdgeTest {name: "Empty", description: "", active: false})');
SELECT cypher('CREATE (n:EdgeTest {name: "Special", description: "Hello@#$%^&*()", active: true})');
SELECT cypher('CREATE (n:EdgeTest {name: "Unicode", description: "café naïve résumé", active: false})');

-- ============================================================================
-- Boundary Value Testing
-- ============================================================================

SELECT '=== BOUNDARY VALUE TESTS ===';

-- Zero values
SELECT 'Test: value = 0 (zero integer)';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.value = 0 RETURN n');

SELECT 'Test: rating = 0.0 (zero float)';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.rating = 0.0 RETURN n');

SELECT 'Test: active = false (false boolean)';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.active = false RETURN n');

-- Negative values
SELECT 'Test: value < 0 (negative integers)';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.value < 0 RETURN n');

SELECT 'Test: rating < 0.0 (negative floats)';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.rating < 0.0 RETURN n');

-- Large values
SELECT 'Test: value > 1000000 (large integers)';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.value > 1000000 RETURN n');

SELECT 'Test: rating > 1000.0 (large floats)';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.rating > 1000.0 RETURN n');

-- Very small values
SELECT 'Test: rating < 0.001 (very small floats)';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.rating < 0.001 RETURN n');

-- ============================================================================
-- String Edge Cases
-- ============================================================================

SELECT '=== STRING EDGE CASES ===';

-- Empty strings
SELECT 'Test: description = "" (empty string)';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.description = "" RETURN n');

-- Special characters
SELECT 'Test: description with special characters';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.description = "Hello@#$%^&*()" RETURN n');

-- Unicode characters
SELECT 'Test: description with unicode characters';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.description = "café naïve résumé" RETURN n');

-- Case sensitivity
SELECT 'Test: case sensitive string matching';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.name = "zero" RETURN n');  -- Should find nothing
SELECT cypher('MATCH (n:EdgeTest) WHERE n.name = "Zero" RETURN n');  -- Should find Zero

-- String comparisons
SELECT 'Test: string comparison with >';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.name > "M" RETURN n');

SELECT 'Test: string comparison with <';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.name < "M" RETURN n');

-- ============================================================================
-- Type Mismatch Error Cases
-- ============================================================================

SELECT '=== TYPE MISMATCH TESTS ===';

-- Integer property with string value
SELECT 'Test: integer property with string value (should find nothing)';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.value = "123" RETURN n');

-- String property with integer value
SELECT 'Test: string property with integer value (should find nothing)';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.name = 123 RETURN n');

-- Boolean property with string value
SELECT 'Test: boolean property with string value (should find nothing)';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.active = "true" RETURN n');

-- Float property with integer value (this should work due to type coercion)
SELECT 'Test: float property with integer comparison';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.rating = 0 RETURN n');

-- ============================================================================
-- Nonexistent Property Cases
-- ============================================================================

SELECT '=== NONEXISTENT PROPERTY TESTS ===';

-- Properties that don't exist
SELECT 'Test: nonexistent property height';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.height > 180 RETURN n');

SELECT 'Test: nonexistent property weight';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.weight = 75.5 RETURN n');

SELECT 'Test: nonexistent boolean property';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.verified = true RETURN n');

-- Properties that exist on some nodes but not others
SELECT 'Test: property that exists on some nodes only';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.description = "test" RETURN n');

-- ============================================================================
-- Extreme Value Testing
-- ============================================================================

SELECT '=== EXTREME VALUE TESTS ===';

-- Test with maximum integer values
SELECT 'Test: maximum 32-bit integer';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.value = 2147483647 RETURN n');

SELECT 'Test: minimum 32-bit integer';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.value = -2147483648 RETURN n');

-- Test with very small float values
SELECT 'Test: very small float value';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.rating < 0.00001 RETURN n');

-- Test with scientific notation
SELECT 'Test: scientific notation comparison';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.rating = 0.0000123 RETURN n');

-- ============================================================================
-- Query Validation and Sanity Checks
-- ============================================================================

SELECT '=== QUERY VALIDATION ===';

-- Verify our test data was created correctly
SELECT 'Verify EdgeTest nodes count:';
SELECT COUNT(*) FROM nodes n JOIN node_labels nl ON n.id = nl.node_id WHERE nl.label = 'EdgeTest';

-- Check property distribution
SELECT 'Integer properties count:';
SELECT COUNT(*) FROM node_props_int npi 
JOIN nodes n ON npi.node_id = n.id 
JOIN node_labels nl ON n.id = nl.node_id 
WHERE nl.label = 'EdgeTest';

SELECT 'Text properties count:';
SELECT COUNT(*) FROM node_props_text npt 
JOIN nodes n ON npt.node_id = n.id 
JOIN node_labels nl ON n.id = nl.node_id 
WHERE nl.label = 'EdgeTest';

SELECT 'Float properties count:';
SELECT COUNT(*) FROM node_props_real npr 
JOIN nodes n ON npr.node_id = n.id 
JOIN node_labels nl ON n.id = nl.node_id 
WHERE nl.label = 'EdgeTest';

SELECT 'Boolean properties count:';
SELECT COUNT(*) FROM node_props_bool npb 
JOIN nodes n ON npb.node_id = n.id 
JOIN node_labels nl ON n.id = nl.node_id 
WHERE nl.label = 'EdgeTest';

-- Verify specific edge case nodes exist
SELECT 'Test data verification - find specific nodes:';
SELECT 'Zero node:', cypher('MATCH (n:EdgeTest) WHERE n.name = "Zero" RETURN n');
SELECT 'Negative node:', cypher('MATCH (n:EdgeTest) WHERE n.name = "Negative" RETURN n');
SELECT 'Large node:', cypher('MATCH (n:EdgeTest) WHERE n.name = "Large" RETURN n');

-- ============================================================================
-- Performance and Stress Testing
-- ============================================================================

SELECT '=== PERFORMANCE TESTS ===';

-- Test with ranges that should return multiple results
SELECT 'Test: broad range query (performance test)';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.value >= -1000000000 RETURN n');

-- Test with ranges that should return no results
SELECT 'Test: impossible range query';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.value > 3000000000 RETURN n');

-- Complex comparison chains
SELECT 'Test: multiple comparison conditions';
SELECT cypher('MATCH (n:EdgeTest) WHERE n.value >= 0 RETURN n');
SELECT cypher('MATCH (n:EdgeTest) WHERE n.rating <= 1000.0 RETURN n');
SELECT cypher('MATCH (n:EdgeTest) WHERE n.active = true RETURN n');

SELECT '=== WHERE EDGE CASES TESTS COMPLETED ===';