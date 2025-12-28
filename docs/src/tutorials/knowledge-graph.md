# Building a Knowledge Graph

This tutorial shows how to build a knowledge graph for storing and querying interconnected information.

## What You'll Build

A knowledge graph of companies, people, and their relationships—similar to what you might find in a business intelligence system.

## What You'll Learn

- Model complex domains with multiple node types
- Create various relationship types
- Write sophisticated Cypher queries
- Use aggregation and path queries

## Step 1: Design the Schema

Our knowledge graph will have:

**Node Types (Labels)**:
- `Company` - Organizations
- `Person` - Individuals
- `Technology` - Products and technologies

**Relationship Types**:
- `WORKS_AT` - Person works at Company
- `FOUNDED` - Person founded Company
- `USES` - Company uses Technology
- `KNOWS` - Person knows Person

## Step 2: Create the Graph

```python
from graphqlite import Graph

g = Graph("knowledge.db")  # Persistent database

# Companies
g.upsert_node("acme", {"name": "Acme Corp", "founded": 2010, "industry": "Software"}, label="Company")
g.upsert_node("globex", {"name": "Globex Inc", "founded": 2015, "industry": "AI"}, label="Company")

# People
g.upsert_node("alice", {"name": "Alice Chen", "role": "CEO"}, label="Person")
g.upsert_node("bob", {"name": "Bob Smith", "role": "CTO"}, label="Person")
g.upsert_node("carol", {"name": "Carol Jones", "role": "Engineer"}, label="Person")

# Technologies
g.upsert_node("python", {"name": "Python", "type": "Language"}, label="Technology")
g.upsert_node("graphql", {"name": "GraphQL", "type": "API"}, label="Technology")
```

## Step 3: Add Relationships

```python
# Employment
g.upsert_edge("alice", "acme", {"since": 2010, "title": "CEO"}, rel_type="WORKS_AT")
g.upsert_edge("bob", "acme", {"since": 2012, "title": "CTO"}, rel_type="WORKS_AT")
g.upsert_edge("carol", "globex", {"since": 2020, "title": "Senior Engineer"}, rel_type="WORKS_AT")

# Founding
g.upsert_edge("alice", "acme", {"year": 2010}, rel_type="FOUNDED")

# Technology usage
g.upsert_edge("acme", "python", {"primary": True}, rel_type="USES")
g.upsert_edge("acme", "graphql", {"primary": False}, rel_type="USES")
g.upsert_edge("globex", "python", {"primary": True}, rel_type="USES")

# Personal connections
g.upsert_edge("alice", "bob", {"since": 2010}, rel_type="KNOWS")
g.upsert_edge("bob", "carol", {"since": 2019}, rel_type="KNOWS")
```

## Step 4: Query the Knowledge Graph

### Find all employees of a company

```python
results = g.query("""
    MATCH (p:Person)-[r:WORKS_AT]->(c:Company {name: 'Acme Corp'})
    RETURN p.name AS employee, r.title AS title, r.since AS since
    ORDER BY r.since
""")
```

### Find companies using a technology

```python
results = g.query("""
    MATCH (c:Company)-[:USES]->(t:Technology {name: 'Python'})
    RETURN c.name AS company, c.industry AS industry
""")
```

### Find connections between people

```python
results = g.query("""
    MATCH path = (a:Person {name: 'Alice Chen'})-[:KNOWS*1..3]->(b:Person)
    RETURN b.name AS connected_person, length(path) AS distance
""")
```

### Aggregate: Count employees per company

```python
results = g.query("""
    MATCH (p:Person)-[:WORKS_AT]->(c:Company)
    RETURN c.name AS company, count(p) AS employee_count
    ORDER BY employee_count DESC
""")
```

### Find founders who still work at their company

```python
results = g.query("""
    MATCH (p:Person)-[:FOUNDED]->(c:Company),
          (p)-[:WORKS_AT]->(c)
    RETURN p.name AS founder, c.name AS company
""")
```

## Step 5: Update the Graph

Add new information as it becomes available:

```python
# Carol moves to Acme
g.query("""
    MATCH (p:Person {name: 'Carol Jones'})-[r:WORKS_AT]->(:Company)
    DELETE r
""")
g.upsert_edge("carol", "acme", {"since": 2024, "title": "Staff Engineer"}, rel_type="WORKS_AT")

# Add a new technology
g.upsert_node("rust", {"name": "Rust", "type": "Language"}, label="Technology")
g.upsert_edge("globex", "rust", {"primary": False}, rel_type="USES")
```

## Next Steps

- [Graph Analytics](./graph-analytics.md) - Run algorithms on your knowledge graph
- [Graph Algorithms Reference](../reference/algorithms.md) - Available algorithms
