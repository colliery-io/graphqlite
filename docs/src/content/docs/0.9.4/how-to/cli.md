---
title: Using the gqlite CLI
slug: 0.9.4/how-to/cli
---

`gqlite` is an interactive Cypher shell for querying GraphQLite databases from the command line. It supports interactive mode, multi-line statements, dot commands, and script execution.

## Building

Build the CLI from source:

```bash
make graphqlite
```

For a release build with optimizations:

```bash
make graphqlite RELEASE=1
```

The binary is placed at `build/gqlite`.

See [Building from Source](../building-from-source/) for prerequisites.

## Command Line Options

```
Usage: build/gqlite [OPTIONS] [DATABASE_FILE]
```

| Option | Description |
|--------|-------------|
| `-h`, `--help` | Show help message and exit |
| `-v`, `--verbose` | Enable verbose debug output (shows query execution details) |
| `-i`, `--init` | Initialize a fresh database (overwrites existing) |
| `DATABASE_FILE` | Path to the SQLite database file (default: `graphqlite.db`) |

### Examples

```bash
# Open the default database (graphqlite.db in the current directory)
./build/gqlite

# Open a specific file
./build/gqlite social.db

# Initialize a fresh database, discarding any existing content
./build/gqlite -i social.db

# Verbose mode — shows row counts and execution details
./build/gqlite -v social.db
```

## Interactive Mode

Start `gqlite` without piping stdin to enter interactive mode:

```
GraphQLite Interactive Shell
Type .help for help, .quit to exit
Queries must end with semicolon (;)

graphqlite>
```

### Statement Termination

All Cypher statements must end with a semicolon (`;`). `gqlite` buffers input across multiple lines until it sees a `;`.

```
graphqlite> CREATE (a:Person {name: "Alice", age: 30});
Query executed successfully
  Nodes created: 1
  Properties set: 2

graphqlite> MATCH (n:Person) RETURN n.name, n.age;
n.name    n.age
----------
Alice     30
```

### Multi-Line Statements

Press Enter to continue a statement on the next line. A `...>` prompt indicates continuation:

```
graphqlite> MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"})
       ...>   CREATE (a)-[:KNOWS {since: 2021}]->(b);
Query executed successfully
  Relationships created: 1
```

### Dot Commands

Dot commands control the shell itself. They do not end with a semicolon.

| Command | Description |
|---------|-------------|
| `.help` | Show all available commands |
| `.schema` | Display the full database schema |
| `.tables` | List all tables in the database |
| `.stats` | Show graph statistics (node count, edge count, labels, types) |
| `.quit` | Exit the shell |
| `.exit` | Alias for `.quit` |

#### Example: .stats

```
graphqlite> .stats

Database Statistics:
===================
  Nodes           : 3
  Edges           : 2
  Node Labels     : 1
  Property Keys   : 2
  Edge Types      : KNOWS
```

#### Example: .schema

```
graphqlite> .schema
CREATE TABLE nodes (rowid INTEGER PRIMARY KEY, user_id TEXT UNIQUE, label TEXT);
CREATE TABLE edges (rowid INTEGER PRIMARY KEY, source_id INTEGER, target_id INTEGER, ...);
...
```

## Script Mode

Pipe a file or inline text to `gqlite` to run a script non-interactively:

```bash
# Execute a script file
./build/gqlite social.db < setup.cypher

# Inline heredoc
./build/gqlite social.db <<'EOF'
CREATE (alice:Person {name: "Alice", age: 30});
CREATE (bob:Person {name: "Bob", age: 25});
MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"})
  CREATE (a)-[:KNOWS {since: 2020}]->(b);
MATCH (a:Person)-[:KNOWS]->(b:Person) RETURN a.name, b.name;
EOF

# Inline echo
echo 'MATCH (n) RETURN n.name;' | ./build/gqlite social.db
```

### Script Format

Scripts use the same semicolon-terminated syntax as the interactive shell. Use `--` for comments:

```cypher
-- setup.cypher
-- Create people
CREATE (alice:Person {name: "Alice", age: 30, city: "London"});
CREATE (bob:Person {name: "Bob", age: 25, city: "Paris"});
CREATE (carol:Person {name: "Carol", age: 35, city: "Berlin"});

-- Create relationships
MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"})
  CREATE (a)-[:KNOWS {since: 2018}]->(b);

MATCH (b:Person {name: "Bob"}), (c:Person {name: "Carol"})
  CREATE (b)-[:KNOWS {since: 2021}]->(c);

-- Query friend-of-friend
MATCH (a:Person {name: "Alice"})-[:KNOWS]->()-[:KNOWS]->(fof)
RETURN fof.name AS friend_of_friend;
```

## Worked Example: Build and Query a Graph

The following session creates a small knowledge graph, queries it, and inspects statistics.

### Step 1: Initialize the Database

```bash
./build/gqlite -i company.db
```

```
GraphQLite Interactive Shell
Type .help for help, .quit to exit
Queries must end with semicolon (;)

graphqlite>
```

### Step 2: Add People

```
graphqlite> CREATE (alice:Person {name: "Alice", title: "Engineer"});
Query executed successfully
  Nodes created: 1
  Properties set: 2

graphqlite> CREATE (bob:Person {name: "Bob", title: "Manager"});
Query executed successfully
  Nodes created: 1
  Properties set: 2

graphqlite> CREATE (carol:Person {name: "Carol", title: "Engineer"});
Query executed successfully
  Nodes created: 1
  Properties set: 2
```

### Step 3: Add a Project Node

```
graphqlite> CREATE (proj:Project {name: "GraphQLite", status: "active"});
Query executed successfully
  Nodes created: 1
  Properties set: 2
```

### Step 4: Create Relationships

```
graphqlite> MATCH (alice:Person {name: "Alice"}), (proj:Project {name: "GraphQLite"})
       ...>   CREATE (alice)-[:WORKS_ON {since: 2023}]->(proj);
Query executed successfully
  Relationships created: 1

graphqlite> MATCH (carol:Person {name: "Carol"}), (proj:Project {name: "GraphQLite"})
       ...>   CREATE (carol)-[:WORKS_ON {since: 2024}]->(proj);
Query executed successfully
  Relationships created: 1

graphqlite> MATCH (bob:Person {name: "Bob"}), (alice:Person {name: "Alice"})
       ...>   CREATE (bob)-[:MANAGES]->(alice);
Query executed successfully
  Relationships created: 1
```

### Step 5: Query the Graph

```
graphqlite> MATCH (mgr:Person)-[:MANAGES]->(eng:Person)-[:WORKS_ON]->(p:Project)
       ...>   RETURN mgr.name AS manager, eng.name AS engineer, p.name AS project;
manager    engineer    project
-----------------------------------
Bob        Alice       GraphQLite
```

### Step 6: Check Statistics

```
graphqlite> .stats

Database Statistics:
===================
  Nodes           : 4
  Edges           : 3
  Node Labels     : 2
  Property Keys   : 6
  Edge Types      : MANAGES, WORKS_ON

graphqlite> .quit
Goodbye!
```

## Tips

* Use `.tables` to see all underlying SQLite tables, which is useful for debugging the graph schema.
* In verbose mode (`-v`), each query prints timing and row count information.
* The CLI loads the bundled extension automatically; no separate `GRAPHQLITE_EXTENSION_PATH` configuration is needed.
* For large imports, use [Bulk Import](../bulk-import/) from Python rather than piping thousands of `CREATE` statements through the CLI.
