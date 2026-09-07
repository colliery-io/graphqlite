import sys, time, json
from harness import open_db, build_graph, cypher
N = int(sys.argv[1]) if len(sys.argv) > 1 else 10000
ops = int(sys.argv[2]) if len(sys.argv) > 2 else 1000
only = sys.argv[3] if len(sys.argv) > 3 else None
c = open_db(); build_graph(c, N, N * 5)
def run(name, fn, n):
    if only and only != name: return
    c.execute("BEGIN"); t0 = time.perf_counter()
    for i in range(n): fn(i)
    dt = time.perf_counter() - t0; c.execute("COMMIT")
    print(f"{name:28s} {n:6d} ops  {dt*1e6/n:9.1f} us/op   {n/dt:9.0f} ops/s")
run("create_literal", lambda i: cypher(c, f"CREATE (n:Person {{id: 'x{i}', name: 'Name {i}', age: 30, score: 1.5}})"), ops)
run("create_param", lambda i: cypher(c, "CREATE (n:Person {id: $id, name: $name, age: $age, score: $score})", {"id": f"y{i}", "name": f"Name {i}", "age": 30, "score": 1.5}), ops)
run("has_node_param", lambda i: cypher(c, "MATCH (n {id: $id}) RETURN count(n) AS cnt", {"id": f"n{i+1}"}), min(ops, 200))
run("has_node_literal", lambda i: cypher(c, f"MATCH (n {{id: 'n{i+1}'}}) RETURN count(n) AS cnt"), ops)
run("merge_node_literal", lambda i: cypher(c, f"MERGE (n:Person {{id: 'm{i}'}}) ON CREATE SET n.name = 'M'"), ops)
run("merge_node_existing", lambda i: cypher(c, f"MERGE (n:Person {{id: 'n{i+1}'}}) ON MATCH SET n.age = 40"), min(ops, 300))
run("upsert_edge_param", lambda i: cypher(c, "MATCH (a {id: $src}), (b {id: $tgt}) MERGE (a)-[r:KNOWS]->(b)", {"src": f"n{i+1}", "tgt": f"n{i+2}"}), min(ops, 100))
run("upsert_edge_literal", lambda i: cypher(c, f"MATCH (a {{id: 'n{i+1}'}}), (b {{id: 'n{i+3}'}}) MERGE (a)-[r:KNOWS]->(b)"), min(ops, 300))
run("set_literal", lambda i: cypher(c, f"MATCH (n {{id: 'n{i+1}'}}) SET n.age = 31"), min(ops, 300))
run("set_param", lambda i: cypher(c, "MATCH (n {id: $id}) SET n.age = $v RETURN n", {"id": f"n{i+1}", "v": 31}), min(ops, 100))
rows = [{"id": f"u{i}", "name": f"U {i}", "age": 20} for i in range(ops)]
run("unwind_create_batch", lambda i: cypher(c, "UNWIND $rows AS row CREATE (n:Person {id: row.id, name: row.name, age: row.age})", {"rows": rows}), 1)
def raw(i):
    cur = c.execute("INSERT INTO nodes DEFAULT VALUES"); nid = cur.lastrowid
    c.execute("INSERT INTO node_labels VALUES (?,?)", (nid, "Person"))
    c.execute("INSERT INTO node_props_text VALUES (?,1,?)", (nid, f"z{i}"))
    c.execute("INSERT INTO node_props_text VALUES (?,2,?)", (nid, f"Name {i}"))
    c.execute("INSERT INTO node_props_int VALUES (?,3,?)", (nid, 30))
    c.execute("INSERT INTO node_props_real VALUES (?,4,?)", (nid, 1.5))
run("raw_sql_insert", raw, ops)
