---
title: Getting Started with SQL
slug: 0.12.0/tutorials/sql-getting-started
---

This tutorial shows how to use GraphQLite directly from the SQLite command-line interface. No Python or Rust required — just `sqlite3` and the compiled extension.

## What You Will Learn

* Build the extension from source
* Load it into the SQLite CLI
* Create nodes and relationships with Cypher
* Query patterns with MATCH and WHERE
* Use parameterized queries
* Run a graph algorithm and read the results

## Prerequisites

* SQLite 3.x CLI (`sqlite3 --version`)
* A C compiler, Bison, and Flex (for building from source), **or** a pre-built binary from the Python wheel

## Step 1: Get the Extension

### Option A — Build from source

```bash
# Clone the repository
git clone https://github.com/colliery-io/graphqlite.git
cd graphqlite

# macOS
brew install bison flex sqlite
export PATH="$(brew --prefix bison)/bin:$PATH"
make extension RELEASE=1

# Linux (Debian/Ubuntu)
sudo apt-get install build-essential bison flex libsqlite3-dev
make extension RELEASE=1
```

The compiled extension lands in:

* `build/graphqlite.dylib` (macOS)
* `build/graphqlite.so` (Linux)
* `build/graphqlite.dll` (Windows)

### Option B — Extract from the Python package

```bash
pip install graphqlite
python -c "import graphqlite; print(graphqlite.loadable_path())"
# /path/to/site-packages/graphqlite/graphqlite.dylib
```

Use that path anywhere this tutorial says `build/graphqlite`.

## Step 2: Open SQLite and Load the Extension

```bash
sqlite3 social.db
```

Inside the SQLite prompt:

```sql
-- Load the extension (adjust extension for your platform)
.load build/graphqlite

-- Confirm it loaded
SELECT cypher('RETURN 1 + 1 AS result');
-- [{"result":2}]
```

Enable column headers for readable output:

```sql
.mode column
.headers on
```

## Step 3: Create Nodes

Create a small social network. Each `CREATE` call returns an empty JSON array `[]` — that is normal for write operations.

```sql
SELECT cypher('CREATE (a:Person {name: "Alice", age: 30, city: "London"})');
SELECT cypher('CREATE (b:Person {name: "Bob",   age: 25, city: "Paris"})');
SELECT cypher('CREATE (c:Person {name: "Carol", age: 35, city: "London"})');
SELECT cypher('CREATE (d:Person {name: "Dave",  age: 28, city: "Berlin"})');
```

Verify the nodes exist:

```sql
SELECT cypher('MATCH (p:Person) RETURN p.name, p.age, p.city ORDER BY p.name');
```

Output:

```
[{"p.name":"Alice","p.age":30,"p.city":"London"},
 {"p.name":"Bob","p.age":25,"p.city":"Paris"},
 {"p.name":"Carol","p.age":35,"p.city":"London"},
 {"p.name":"Dave","p.age":28,"p.city":"Berlin"}]
```

Use `json_each()` to get one row per result:

```sql
SELECT
    json_extract(value, '$.p.name') AS name,
    json_extract(value, '$.p.age')  AS age,
    json_extract(value, '$.p.city') AS city
FROM json_each(cypher('MATCH (p:Person) RETURN p.name, p.age, p.city ORDER BY p.name'));
```

Output:

```
name   age  city
-----  ---  ------
Alice  30   London
Bob    25   Paris
Carol  35   London
Dave   28   Berlin
```

## Step 4: Create Relationships

```sql
-- Alice knows Bob
SELECT cypher('
    MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"})
    CREATE (a)-[:KNOWS {since: 2020}]->(b)
');

-- Alice knows Carol
SELECT cypher('
    MATCH (a:Person {name: "Alice"}), (c:Person {name: "Carol"})
    CREATE (a)-[:KNOWS {since: 2018}]->(c)
');

-- Bob knows Dave
SELECT cypher('
    MATCH (b:Person {name: "Bob"}), (d:Person {name: "Dave"})
    CREATE (b)-[:KNOWS {since: 2022}]->(d)
');

-- Carol knows Dave
SELECT cypher('
    MATCH (c:Person {name: "Carol"}), (d:Person {name: "Dave"})
    CREATE (c)-[:KNOWS {since: 2021}]->(d)
');
```

## Step 5: Query Patterns with MATCH and WHERE

### Who does Alice know?

```sql
SELECT
    json_extract(value, '$.friend.name') AS friend,
    json_extract(value, '$.r.since')     AS since
FROM json_each(cypher('
    MATCH (a:Person {name: "Alice"})-[r:KNOWS]->(friend)
    RETURN friend, r
'));
```

Output:

```
friend  since
------  -----
Bob     2020
Carol   2018
```

### People in a specific city

```sql
SELECT
    json_extract(value, '$.name') AS name
FROM json_each(cypher('
    MATCH (p:Person)
    WHERE p.city = "London"
    RETURN p.name AS name
'));
```

Output:

```
name
-----
Alice
Carol
```

### Friends of friends (two hops)

Who can Alice reach through two KNOWS relationships?

```sql
SELECT
    json_extract(value, '$.fof') AS friend_of_friend
FROM json_each(cypher('
    MATCH (a:Person {name: "Alice"})-[:KNOWS]->()-[:KNOWS]->(fof)
    RETURN DISTINCT fof.name AS fof
'));
```

Output:

```
friend_of_friend
----------------
Dave
```

### Filter with WHERE and aggregation

People who know more than one person:

```sql
SELECT
    json_extract(value, '$.name')  AS person,
    json_extract(value, '$.count') AS knows_count
FROM json_each(cypher('
    MATCH (a:Person)-[:KNOWS]->(b)
    WITH a.name AS name, count(b) AS count
    WHERE count > 1
    RETURN name, count
    ORDER BY count DESC
'));
```

Output:

```
person  knows_count
------  -----------
Alice   2
```

## Step 6: Use Parameterized Queries

For any value that comes from outside the query — user input, a variable, application data — use the `$param` syntax with a JSON parameters string as the second argument to `cypher()`:

```sql
-- Find a person by name using a parameter
SELECT cypher(
    'MATCH (p:Person {name: $name}) RETURN p.name, p.age, p.city',
    '{"name": "Carol"}'
);
-- [{"p.name":"Carol","p.age":35,"p.city":"London"}]
```

Multiple parameters:

```sql
SELECT cypher(
    'MATCH (p:Person) WHERE p.age >= $min_age AND p.city = $city RETURN p.name, p.age',
    '{"min_age": 28, "city": "London"}'
);
-- [{"p.name":"Alice","p.age":30},{"p.name":"Carol","p.age":35}]
```

Parameters protect against injection and correctly handle special characters in string values.

## Step 7: Run PageRank

GraphQLite includes 15+ graph algorithms. Load the graph cache first, then call the algorithm function inside a `RETURN` clause.

```sql
-- Load the graph into the algorithm cache
SELECT cypher('RETURN gql_load_graph()');
```

Run PageRank and display the results as a table:

```sql
SELECT
    json_extract(value, '$.user_id')                         AS person,
    printf('%.4f', json_extract(value, '$.score'))           AS pagerank
FROM json_each(cypher('RETURN pageRank(0.85, 20)'), '$[0].column_0')
ORDER BY json_extract(value, '$.score') DESC;
```

Output:

```
person  pagerank
------  --------
carol   0.2000
dave    0.1800
bob     0.1600
alice   0.1200
```

Dave and Carol score highest because they have multiple incoming paths from well-connected nodes.

## Complete Script

Save the following as `social.sql` and run it with `sqlite3 < social.sql`:

```sql
.load build/graphqlite
.mode column
.headers on

-- Nodes
SELECT cypher('CREATE (a:Person {name: "Alice", age: 30, city: "London"})');
SELECT cypher('CREATE (b:Person {name: "Bob",   age: 25, city: "Paris"})');
SELECT cypher('CREATE (c:Person {name: "Carol", age: 35, city: "London"})');
SELECT cypher('CREATE (d:Person {name: "Dave",  age: 28, city: "Berlin"})');

-- Relationships
SELECT cypher('MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"})   CREATE (a)-[:KNOWS {since: 2020}]->(b)');
SELECT cypher('MATCH (a:Person {name: "Alice"}), (c:Person {name: "Carol"}) CREATE (a)-[:KNOWS {since: 2018}]->(c)');
SELECT cypher('MATCH (b:Person {name: "Bob"}),   (d:Person {name: "Dave"})  CREATE (b)-[:KNOWS {since: 2022}]->(d)');
SELECT cypher('MATCH (c:Person {name: "Carol"}), (d:Person {name: "Dave"})  CREATE (c)-[:KNOWS {since: 2021}]->(d)');

SELECT '--- All people ---';
SELECT json_extract(value, '$.p.name') AS name,
       json_extract(value, '$.p.age')  AS age
FROM json_each(cypher('MATCH (p:Person) RETURN p.name, p.age ORDER BY p.name'));

SELECT '--- Who Alice knows ---';
SELECT json_extract(value, '$.friend') AS friend
FROM json_each(cypher('MATCH (:Person {name: "Alice"})-[:KNOWS]->(f) RETURN f.name AS friend'));

SELECT '--- PageRank ---';
SELECT cypher('RETURN gql_load_graph()');
SELECT json_extract(value, '$.user_id')               AS person,
       printf('%.4f', json_extract(value, '$.score')) AS score
FROM json_each(cypher('RETURN pageRank(0.85, 20)'), '$[0].column_0')
ORDER BY json_extract(value, '$.score') DESC;
```

## Next Steps

* [Query Patterns (SQL)](../sql-patterns/) — Variable-length paths, OPTIONAL MATCH, WITH, UNWIND, CASE, UNION
* [Graph Algorithms (SQL)](../sql-algorithms/) — All 15+ algorithms with SQL extraction patterns
* [SQL Interface Reference](../../reference/sql-interface/) — Complete `cypher()` function documentation and schema tables
