# Use the gqlite CLI

The `gqlite` command-line tool provides an interactive shell for executing Cypher queries against a SQLite database.

## Building

```bash
angreal build app
# or
make graphqlite
```

This creates `build/gqlite`.

## Usage

```bash
# Interactive mode with default database
./build/gqlite

# Specify a database file
./build/gqlite mydata.db

# Initialize a fresh database
./build/gqlite -i mydata.db

# Verbose mode (shows query execution details)
./build/gqlite -v mydata.db
```

## Interactive Shell

When you start `gqlite`, you'll see an interactive prompt:

```
GraphQLite Interactive Shell
Type .help for help, .quit to exit
Queries must end with semicolon (;)

graphqlite>
```

### Statement Termination

All Cypher queries must end with a semicolon (`;`). Multi-line statements are supported:

```cypher
graphqlite> CREATE (a:Person {name: "Alice"});
Query executed successfully
  Nodes created: 1
  Properties set: 1

graphqlite> MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"})
       ...>     CREATE (a)-[:KNOWS]->(b);
Query executed successfully
  Relationships created: 1
```

The `...>` prompt indicates you're continuing a multi-line statement.

### Dot Commands

| Command | Description |
|---------|-------------|
| `.help` | Show help information |
| `.schema` | Display database schema |
| `.tables` | List all tables |
| `.stats` | Show database statistics |
| `.quit` | Exit the shell |

## Script Execution

You can pipe Cypher scripts to `gqlite`:

```bash
# Execute a script file
./build/gqlite mydata.db < script.cypher

# Inline script
echo 'CREATE (n:Test {value: 42});
MATCH (n:Test) RETURN n.value;' | ./build/gqlite mydata.db
```

### Script Format

Scripts should have one statement per line or use multi-line statements ending with semicolons:

```cypher
-- setup.cypher
CREATE (a:Person {name: "Alice", age: 30});
CREATE (b:Person {name: "Bob", age: 25});
CREATE (c:Person {name: "Charlie", age: 35});

MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"})
    CREATE (a)-[:KNOWS]->(b);

MATCH (b:Person {name: "Bob"}), (c:Person {name: "Charlie"})
    CREATE (b)-[:KNOWS]->(c);

-- Query friend-of-friend
MATCH (a:Person {name: "Alice"})-[:KNOWS]->()-[:KNOWS]->(fof)
    RETURN fof.name;
```

## Examples

### Create and Query a Social Network

```bash
$ ./build/gqlite social.db
graphqlite> CREATE (alice:Person {name: "Alice"});
Query executed successfully
  Nodes created: 1
  Properties set: 1

graphqlite> CREATE (bob:Person {name: "Bob"});
Query executed successfully
  Nodes created: 1
  Properties set: 1

graphqlite> MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"})
       ...>     CREATE (a)-[:FRIENDS_WITH]->(b);
Query executed successfully
  Relationships created: 1

graphqlite> MATCH (p:Person)-[:FRIENDS_WITH]->(friend)
       ...>     RETURN p.name, friend.name;
p.name         friend.name
---------------
Alice          Bob

graphqlite> .quit
Goodbye!
```

### Check Database Statistics

```
graphqlite> .stats

Database Statistics:
===================
  Nodes           : 2
  Edges           : 1
  Node Labels     : 2
  Property Keys   : 1
  Edge Types      : FRIENDS_WITH
```

## Command Line Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Show help message |
| `-v, --verbose` | Enable verbose debug output |
| `-i, --init` | Initialize new database (overwrites existing) |
