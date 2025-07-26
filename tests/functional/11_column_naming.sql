-- Load the GraphQLite extension
.load ./build/graphqlite

-- Column Naming Tests
-- Verify that column names are semantic instead of generic column_0, column_1, etc.

SELECT 'Test 1: Property access should use property name as column' as test_name;
SELECT cypher('CREATE (p:ColumnTest {name: ''Alice'', age: 30})') as setup;
SELECT cypher('MATCH (p:ColumnTest) RETURN p.name, p.age') as property_access_columns;

SELECT 'Test 2: Variable access should use variable name as column' as test_name;
SELECT cypher('MATCH (p:ColumnTest) RETURN p') as variable_access_column;

SELECT 'Test 3: Explicit alias should override default naming' as test_name;
SELECT cypher('MATCH (p:ColumnTest) RETURN p.name AS person_name') as alias_column;

SELECT 'Test 4: Mixed property access and variables' as test_name;
SELECT cypher('MATCH (p:ColumnTest) RETURN p, p.name, p.age') as mixed_columns;

SELECT 'Test 5: Complex expression should fallback to generic column name' as test_name;
SELECT cypher('MATCH (p:ColumnTest) RETURN count(p)') as expression_column;

SELECT 'Test 6: Multiple aliases with different patterns' as test_name;
SELECT cypher('MATCH (p:ColumnTest) RETURN p.name AS first_name, p.age AS years_old, p AS person') as multiple_aliases;

SELECT 'Test 7: Property access with different data types' as test_name;
SELECT cypher('CREATE (t:TypeTest {str: ''hello'', num: 42, flag: true})') as type_setup;
SELECT cypher('MATCH (t:TypeTest) RETURN t.str, t.num, t.flag') as type_columns;

SELECT 'Test complete: Column naming working correctly!' as status;