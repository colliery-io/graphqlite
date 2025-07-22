-- Test 03: Property type handling
.load ./build/graphqlite.so

-- Test all property types
SELECT cypher('CREATE (n:TestNode {
    stringProp: "Hello World",
    intProp: 42,
    floatProp: 3.14159,
    boolTrue: true,
    boolFalse: false
})');

-- Test negative numbers
SELECT cypher('CREATE (n:Numbers {negative: -100, zero: 0, positive: 100})');

-- Test edge cases
SELECT cypher('CREATE (n:EdgeCases {
    emptyString: "",
    largeInt: 2147483647,
    smallFloat: 0.0001,
    scientificNotation: 1.23e-5
})');

-- Verify property storage
SELECT '=== Property Keys ===';
SELECT * FROM property_keys ORDER BY id;

SELECT '=== Integer Properties ===';
SELECT n.id, pk.key, npi.value 
FROM node_props_int npi 
JOIN property_keys pk ON npi.key_id = pk.id 
JOIN nodes n ON npi.node_id = n.id
ORDER BY n.id, pk.key;

SELECT '=== Text Properties ===';
SELECT n.id, pk.key, npt.value 
FROM node_props_text npt 
JOIN property_keys pk ON npt.key_id = pk.id 
JOIN nodes n ON npt.node_id = n.id
ORDER BY n.id, pk.key;

SELECT '=== Real Properties ===';
SELECT n.id, pk.key, npr.value 
FROM node_props_real npr 
JOIN property_keys pk ON npr.key_id = pk.id 
JOIN nodes n ON npr.node_id = n.id
ORDER BY n.id, pk.key;

SELECT '=== Boolean Properties ===';
SELECT n.id, pk.key, npb.value 
FROM node_props_bool npb 
JOIN property_keys pk ON npb.key_id = pk.id 
JOIN nodes n ON npb.node_id = n.id
ORDER BY n.id, pk.key;