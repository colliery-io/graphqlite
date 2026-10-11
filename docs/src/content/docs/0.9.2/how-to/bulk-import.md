---
title: Bulk Importing Data
slug: 0.9.2/how-to/bulk-import
---

For loading large datasets into a GraphQLite graph, the bulk import API is 100–500x faster than issuing individual Cypher `CREATE` statements. This guide covers when to use it, how it works, and a complete worked example.

## When to Use Bulk Import

| Scenario | Recommended approach |
|----------|----------------------|
| Interactive graph building, &lt;1 000 nodes | `upsert_node` / `upsert_edge` or Cypher `CREATE` |
| Importing CSV / JSON datasets, >1 000 nodes | `insert_graph_bulk` or `insert_nodes_bulk` + `insert_edges_bulk` |
| Incremental updates to an existing graph | `upsert_nodes_batch` / `upsert_edges_batch` |
| Connecting new edges to existing nodes | `insert_edges_bulk` with `resolve_node_ids` |

The bulk API bypasses the Cypher parser and writes directly to the graph tables in a single transaction, eliminating per-query overhead.

## Python Bulk API

### insert_nodes_bulk

Insert a list of nodes in a single transaction. Returns an `id_map` dictionary mapping each external node ID to its internal SQLite rowid.

```python
from graphqlite import Graph

g = Graph("company.db")

nodes = [
    ("emp_1",  {"name": "Alice", "role": "engineer", "age": 30}, "Employee"),
    ("emp_2",  {"name": "Bob",   "role": "manager",  "age": 40}, "Employee"),
    ("emp_3",  {"name": "Carol", "role": "engineer", "age": 28}, "Employee"),
    ("dept_1", {"name": "Engineering", "budget": 500000},         "Department"),
    ("dept_2", {"name": "Product",     "budget": 300000},         "Department"),
]

id_map = g.insert_nodes_bulk(nodes)
# id_map = {"emp_1": 1, "emp_2": 2, "emp_3": 3, "dept_1": 4, "dept_2": 5}
print(f"Inserted {len(id_map)} nodes")
```

Each entry in the `nodes` list is a tuple of `(external_id, properties, label)`:

| Position | Type | Description |
|----------|------|-------------|
| 0 | `str` | External identifier (any string) |
| 1 | `dict` | Dictionary of property key-value pairs |
| 2 | `str` | Node label |

### insert_edges_bulk

Insert edges after nodes have been loaded. Uses the `id_map` returned by `insert_nodes_bulk` to resolve external IDs to internal rowids:

```python
edges = [
    ("emp_1", "dept_1", {"since": 2020}, "WORKS_IN"),
    ("emp_2", "dept_1", {"since": 2018}, "WORKS_IN"),
    ("emp_3", "dept_2", {"since": 2022}, "WORKS_IN"),
    ("emp_2", "emp_1",  {},              "MANAGES"),
    ("emp_2", "emp_3",  {},              "MANAGES"),
]

g.insert_edges_bulk(edges, id_map)
print(f"Inserted {len(edges)} edges")
```

Each entry in the `edges` list is a tuple of `(source_id, target_id, properties, rel_type)`:

| Position | Type | Description |
|----------|------|-------------|
| 0 | `str` | External ID of the source node |
| 1 | `str` | External ID of the target node |
| 2 | `dict` | Dictionary of property key-value pairs (may be empty `{}`) |
| 3 | `str` | Relationship type |

### insert_graph_bulk

Insert nodes and edges together in a single call. Internally calls `insert_nodes_bulk` then `insert_edges_bulk`:

```python
nodes = [
    ("a", {"name": "Alice", "age": 30}, "Person"),
    ("b", {"name": "Bob",   "age": 25}, "Person"),
    ("c", {"name": "Carol", "age": 35}, "Person"),
]

edges = [
    ("a", "b", {"since": 2019}, "KNOWS"),
    ("b", "c", {"since": 2021}, "KNOWS"),
]

result = g.insert_graph_bulk(nodes, edges)
print(f"Inserted {result.nodes_inserted} nodes, {result.edges_inserted} edges")
print(result.id_map)  # {"a": 1, "b": 2, "c": 3}
```

## The id_map Pattern

The `id_map` bridges the gap between your external IDs (strings like `"emp_1"`) and the internal SQLite rowids that GraphQLite uses for edge resolution.

- `insert_nodes_bulk` returns `{"emp_1": 1, "emp_2": 2, ...}`.
- `insert_edges_bulk` uses this map to look up source and target rowids before writing.
- The external IDs are also stored as the `user_id` column on each node, so Cypher queries can still reference them by string: `MATCH (n {id: 'emp_1'})`.

If an external ID in an edge list is not present in `id_map`, `insert_edges_bulk` raises a `KeyError`. Validate your edge list against your node list before calling bulk insert.

## Connecting New Edges to Existing Nodes

When importing edges that reference nodes already in the database (from a previous import), use `resolve_node_ids` to build the id_map without re-inserting the nodes:

```python
existing_ids = ["emp_1", "emp_2", "emp_3"]
id_map = g.resolve_node_ids(existing_ids)
# id_map = {"emp_1": 1, "emp_2": 2, "emp_3": 3}

# Now add new edges referencing those existing nodes
new_edges = [
    ("emp_1", "emp_3", {"project": "Phoenix"}, "COLLABORATES"),
]
g.insert_edges_bulk(new_edges, id_map)
```

This is the correct pattern when you import nodes in one batch and edges in a separate batch, or when you are enriching an existing graph with new relationships.

## Batch Upsert

For incremental updates — adding or updating nodes and edges that may already exist — use the batch upsert methods. These use Cypher MERGE semantics (update if exists, create if not) and are slower than bulk insert but handle conflicts gracefully.

> **Non-atomicity warning:** Batch upsert methods call `upsert_node`/`upsert_edge` in a loop. If an operation fails partway through, earlier operations will have already completed. For atomic batch inserts, use the bulk insert methods instead, or wrap the call in an explicit transaction via `g.connection.sqlite_connection`.

```python
# Upsert multiple nodes
nodes_to_update = [
    ("emp_1", {"name": "Alice", "role": "senior engineer", "age": 31}, "Employee"),
    ("emp_4", {"name": "Dave",  "role": "analyst",         "age": 27}, "Employee"),
]
g.upsert_nodes_batch(nodes_to_update)

# Upsert multiple edges
edges_to_update = [
    ("emp_4", "dept_1", {"since": 2023}, "WORKS_IN"),
]
g.upsert_edges_batch(edges_to_update)
```

Each tuple in `upsert_nodes_batch` is `(node_id, properties, label)`. Each tuple in `upsert_edges_batch` is `(source_id, target_id, properties, rel_type)`.

## Performance Comparison

Approximate timings for inserting 100 000 nodes + 200 000 edges on a modern laptop:

| Method | Time (approx.) |
|--------|---------------|
| Cypher `CREATE` (one per statement) | 90–180 seconds |
| `upsert_node` / `upsert_edge` in a loop | 30–60 seconds |
| `upsert_nodes_batch` / `upsert_edges_batch` | 10–20 seconds |
| `insert_nodes_bulk` + `insert_edges_bulk` | 0.5–2 seconds |

Bulk insert achieves its speed by:

1. Writing all rows in a single SQLite transaction.
2. Bypassing the Cypher parser entirely.
3. Preparing INSERT statements once and reusing them.

## Complete Example: Importing a CSV

This example imports a CSV of employees and a CSV of manager relationships:

```python
import csv
from graphqlite import Graph

g = Graph("hr.db")

# --- Load employees.csv ---
# Columns: id, name, department, age, salary
nodes = []
dept_set = set()
with open("employees.csv") as f:
    for row in csv.DictReader(f):
        nodes.append((
            row["id"],
            {
                "name":       row["name"],
                "department": row["department"],
                "age":        int(row["age"]),
                "salary":     float(row["salary"]),
            },
            "Employee",
        ))
        dept_set.add(row["department"])

print(f"Loading {len(nodes)} employees ...")
id_map = g.insert_nodes_bulk(nodes)

# --- Load department nodes (deduplicated from employee data) ---
dept_nodes = [
    (f"dept_{d}", {"name": d}, "Department")
    for d in dept_set
]
dept_id_map = g.insert_nodes_bulk(dept_nodes)

# Combine id_maps for edge resolution
full_id_map = {**id_map, **dept_id_map}

# Add WORKS_IN edges from employees to departments
dept_edges = [
    (ext_id, f"dept_{props['department']}", {}, "WORKS_IN")
    for ext_id, props, _label in nodes
]
g.insert_edges_bulk(dept_edges, full_id_map)

# --- Load managers.csv ---
# Columns: employee_id, manager_id
edges = []
with open("managers.csv") as f:
    for row in csv.DictReader(f):
        edges.append((
            row["manager_id"],
            row["employee_id"],
            {},
            "MANAGES",
        ))

print(f"Loading {len(edges)} manager relationships ...")
g.insert_edges_bulk(edges, id_map)

print(g.stats())

# Query to verify
results = g.query("""
    MATCH (mgr:Employee)-[:MANAGES]->(emp:Employee)
    RETURN mgr.name AS manager, emp.name AS report
    ORDER BY mgr.name, emp.name
    LIMIT 10
""")
for row in results:
    print(f"  {row['manager']} manages {row['report']}")
```

## Rust Bulk Import

The Rust `Graph` API exposes equivalent batch methods:

```rust
use graphqlite::Graph;

fn main() -> graphqlite::Result<()> {
    let g = Graph::open("company.db")?;

    // Bulk insert nodes
    let nodes = vec![
        ("emp_1", vec![("name", "Alice"), ("role", "engineer"), ("age", "30")], "Employee"),
        ("emp_2", vec![("name", "Bob"),   ("role", "manager"),  ("age", "40")], "Employee"),
        ("emp_3", vec![("name", "Carol"), ("role", "engineer"), ("age", "28")], "Employee"),
    ];
    g.upsert_nodes_batch(nodes)?;

    // Bulk insert edges
    let edges = vec![
        ("emp_2", "emp_1", vec![("since", "2020")], "MANAGES"),
        ("emp_2", "emp_3", vec![("since", "2021")], "MANAGES"),
    ];
    g.upsert_edges_batch(edges)?;

    let stats = g.stats()?;
    println!("Nodes: {}, Edges: {}", stats.nodes, stats.edges);

    // Verify
    let results = g.query("MATCH (m:Employee)-[:MANAGES]->(e:Employee) RETURN m.name, e.name")?;
    for row in &results {
        println!(
            "{} manages {}",
            row.get::<String>("m.name")?,
            row.get::<String>("e.name")?
        );
    }

    Ok(())
}
```

> The Rust API currently exposes `upsert_nodes_batch` and `upsert_edges_batch` (which use INSERT OR REPLACE). For maximum throughput on very large imports, call the Python bulk API via the Python bindings, or build the graph in Python and use it from Rust.

## Tips

- **Wrap bulk inserts in a transaction** if you call `insert_nodes_bulk` and `insert_edges_bulk` separately, to ensure atomicity:

  ```python
  with g.connection.sqlite_connection:
      id_map = g.insert_nodes_bulk(nodes)
      g.insert_edges_bulk(edges, id_map)
  ```

- **Validate before inserting.** Check that all edge source/target IDs exist in your node list before calling `insert_edges_bulk`. Missing IDs raise a `KeyError` mid-insert, which can leave the database in a partial state.

- **Reload the graph cache** after a bulk import if you plan to run algorithms immediately:

  ```python
  g.insert_graph_bulk(nodes, edges)
  g.reload_graph()
  results = g.pagerank()
  ```
