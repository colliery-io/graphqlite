import sys, time, json, statistics
from harness import open_db, build_graph, cypher
N = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
c = open_db(); build_graph(c, N, N * 5)
def t(name, sql, params=(), reps=3):
    ts = []
    for _ in range(reps):
        t0 = time.perf_counter(); rows = c.execute(sql, params).fetchall(); ts.append((time.perf_counter() - t0) * 1000)
    print(f"{name:34s} {statistics.median(ts):9.2f} ms  rows={len(rows)}")
    return rows
def tc(name, q, p=None, reps=3):
    ts = []
    for _ in range(reps):
        t0 = time.perf_counter(); out = cypher(c, q, p); ts.append((time.perf_counter() - t0) * 1000)
    print(f"{name:34s} {statistics.median(ts):9.2f} ms  bytes={len(out)}")
def gen_sql(q, p=None):
    out = cypher(c, "EXPLAIN " + q, p)
    return out.split("SQL: ", 1)[1].rsplit("\"}]", 1)[0].replace('\\"', '"')

print("--- node materialization (RETURN n), N =", N)
tc("cypher: MATCH (n) RETURN n", "MATCH (n) RETURN n")
t("generated SQL only", gen_sql("MATCH (n) RETURN n"))
ALT_NODE = """SELECT json_object('id', n.id,
 'labels', COALESCE((SELECT json_group_array(label) FROM node_labels WHERE node_id = n.id), json('[]')),
 'properties', COALESCE((SELECT json_group_object(pk.key, v) FROM (
    SELECT key_id, value AS v FROM node_props_text WHERE node_id = n.id UNION ALL
    SELECT key_id, value FROM node_props_int WHERE node_id = n.id UNION ALL
    SELECT key_id, value FROM node_props_real WHERE node_id = n.id UNION ALL
    SELECT key_id, json(CASE WHEN value THEN 'true' ELSE 'false' END) FROM node_props_bool WHERE node_id = n.id UNION ALL
    SELECT key_id, json(value) FROM node_props_json WHERE node_id = n.id) p JOIN property_keys pk ON pk.id = p.key_id), json('{}')))
FROM nodes n"""
t("ALT: UNION ALL by node_id", ALT_NODE)

print("--- property projection (RETURN n.id, n.name, n.age)")
tc("cypher", "MATCH (n:Person) RETURN n.id, n.name, n.age")
t("generated SQL only", gen_sql("MATCH (n:Person) RETURN n.id, n.name, n.age"))
keys = dict(c.execute("SELECT key, id FROM property_keys"))
ALT_PROPS = f"""SELECT (SELECT value FROM node_props_text WHERE node_id = n.id AND key_id = {keys['id']}),
 (SELECT value FROM node_props_text WHERE node_id = n.id AND key_id = {keys['name']}),
 COALESCE((SELECT value FROM node_props_int WHERE node_id = n.id AND key_id = {keys['age']}),
          (SELECT value FROM node_props_text WHERE node_id = n.id AND key_id = {keys['age']}),
          (SELECT value FROM node_props_real WHERE node_id = n.id AND key_id = {keys['age']}))
FROM nodes n JOIN node_labels nl ON nl.node_id = n.id AND nl.label = 'Person'"""
t("ALT: key_id resolved at transform", ALT_PROPS)

print("--- parameterized id lookup")
tc("cypher literal", "MATCH (n {id: 'n42'}) RETURN n.name")
tc("cypher $param", "MATCH (n {id: $id}) RETURN n.name", {"id": "n42"})
ALT_PARAM = f"""SELECT (SELECT value FROM node_props_text WHERE node_id = n.id AND key_id = {keys['name']}) FROM nodes n
WHERE n.id IN (SELECT node_id FROM node_props_text WHERE key_id = {keys['id']} AND value = :id
  UNION ALL SELECT node_id FROM node_props_int WHERE key_id = {keys['id']} AND value = :id
  UNION ALL SELECT node_id FROM node_props_real WHERE key_id = {keys['id']} AND value = :id)"""
t("ALT: IN (typed index lookups)", ALT_PARAM, {"id": "n42"})

print("--- WHERE n.age > 85 (range filter)")
tc("cypher", "MATCH (n:Person) WHERE n.age > 85 RETURN n.name")
ALT_RANGE = f"""SELECT (SELECT value FROM node_props_text WHERE node_id = n.id AND key_id = {keys['name']})
FROM node_props_int p JOIN nodes n ON n.id = p.node_id JOIN node_labels nl ON nl.node_id = n.id AND nl.label = 'Person'
WHERE p.key_id = {keys['age']} AND p.value > 85"""
t("ALT: index range on (key_id,value)", ALT_RANGE)

print("--- varlen *1..3 from one node")
tc("cypher", "MATCH (a {id: 'n42'})-[:KNOWS*1..3]->(b) RETURN count(DISTINCT b)")
ALT_VARLEN = f"""WITH RECURSIVE p(end_id, depth, visited) AS (
  SELECT e.target_id, 1, ',' || e.id || ',' FROM edges e
   WHERE e.type = 'KNOWS' AND e.source_id = (SELECT node_id FROM node_props_text WHERE key_id = {keys['id']} AND value = 'n42')
  UNION ALL
  SELECT e.target_id, p.depth + 1, p.visited || e.id || ',' FROM p JOIN edges e ON e.source_id = p.end_id
   WHERE p.depth < 3 AND e.type = 'KNOWS' AND p.visited NOT LIKE '%,' || e.id || ',%')
SELECT count(DISTINCT end_id) FROM p"""
t("ALT: CTE anchored at start node", ALT_VARLEN)
