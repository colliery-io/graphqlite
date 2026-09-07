import sqlite3, sys, time, json, os, random, statistics

EXT = os.environ.get("GQL_EXT", os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "build", "graphqlite.so"))

def rss_kb():
    with open("/proc/self/status") as f:
        for line in f:
            if line.startswith("VmRSS:"):
                return int(line.split()[1])

def hwm_kb():
    with open("/proc/self/status") as f:
        for line in f:
            if line.startswith("VmHWM:"):
                return int(line.split()[1])

def open_db(path=":memory:"):
    c = sqlite3.connect(path, isolation_level=None)
    c.enable_load_extension(True)
    c.load_extension(EXT)
    c.execute("PRAGMA journal_mode=OFF"); c.execute("PRAGMA synchronous=OFF")
    return c

def build_graph(c, n_nodes, n_edges, seed=1):
    random.seed(seed)
    c.execute("BEGIN")
    c.execute("INSERT OR IGNORE INTO property_keys(key) VALUES ('id'),('name'),('age'),('score'),('weight')")
    keys = {k: i for k, i in c.execute("SELECT key, id FROM property_keys")}
    c.executemany("INSERT INTO nodes(id) VALUES (?)", ((i,) for i in range(1, n_nodes + 1)))
    c.executemany("INSERT INTO node_labels(node_id,label) VALUES (?,?)",
                  ((i, "Person" if i % 10 else "Company") for i in range(1, n_nodes + 1)))
    c.executemany("INSERT INTO node_props_text(node_id,key_id,value) VALUES (?,?,?)",
                  ((i, keys["id"], f"n{i}") for i in range(1, n_nodes + 1)))
    c.executemany("INSERT INTO node_props_text(node_id,key_id,value) VALUES (?,?,?)",
                  ((i, keys["name"], f"Name {i}") for i in range(1, n_nodes + 1)))
    c.executemany("INSERT INTO node_props_int(node_id,key_id,value) VALUES (?,?,?)",
                  ((i, keys["age"], i % 90) for i in range(1, n_nodes + 1)))
    c.executemany("INSERT INTO node_props_real(node_id,key_id,value) VALUES (?,?,?)",
                  ((i, keys["score"], (i * 7 % 1000) / 10.0) for i in range(1, n_nodes + 1)))
    edges = [(random.randint(1, n_nodes), random.randint(1, n_nodes), "KNOWS" if e % 4 else "WORKS_AT")
             for e in range(n_edges)]
    c.executemany("INSERT INTO edges(id,source_id,target_id,type) VALUES (?,?,?,?)",
                  ((e + 1, s, t, ty) for e, (s, t, ty) in enumerate(edges)))
    c.executemany("INSERT INTO edge_props_real(edge_id,key_id,value) VALUES (?,?,?)",
                  ((e + 1, keys["weight"], (e % 100) / 10.0) for e in range(n_edges)))
    c.execute("COMMIT")
    c.execute("ANALYZE")

def cypher(c, q, params=None):
    if params is None:
        return c.execute("SELECT cypher(?)", (q,)).fetchone()[0]
    return c.execute("SELECT cypher(?, ?)", (q, json.dumps(params))).fetchone()[0]

def timeit(c, q, params=None, reps=3):
    ts = []; out = None
    for _ in range(reps):
        t0 = time.perf_counter(); out = cypher(c, q, params); ts.append((time.perf_counter() - t0) * 1000)
    return statistics.median(ts), out

if __name__ == "__main__":
    n_nodes, n_edges, qname = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
    reps = int(sys.argv[4]) if len(sys.argv) > 4 else 3
    c = open_db()
    build_graph(c, n_nodes, n_edges)
    base_rss, base_hwm = rss_kb(), hwm_kb()
    Q = {
      "return1": ("RETURN 1", None),
      "lookup": ("MATCH (n {id: $id}) RETURN n", {"id": "n42"}),
      "lookup_prop": ("MATCH (n:Person {id: $id}) RETURN n.name, n.age", {"id": "n42"}),
      "count": ("MATCH (n) RETURN count(n)", None),
      "match_all": ("MATCH (n) RETURN n", None),
      "match_all_props": ("MATCH (n:Person) RETURN n.id, n.name, n.age", None),
      "filter": ("MATCH (n:Person) WHERE n.age > 85 RETURN n.name", None),
      "filter_return_node": ("MATCH (n:Person) WHERE n.age > 85 RETURN n", None),
      "hop1": ("MATCH (a {id: $id})-[:KNOWS]->(b) RETURN b.id", {"id": "n42"}),
      "hop2": ("MATCH (a {id: $id})-[:KNOWS]->(b)-[:KNOWS]->(c) RETURN c.id", {"id": "n42"}),
      "hop3": ("MATCH (a {id: $id})-[:KNOWS]->(b)-[:KNOWS]->(c)-[:KNOWS]->(d) RETURN d.id", {"id": "n42"}),
      "hop2_nodes": ("MATCH (a {id: $id})-[:KNOWS]->(b)-[:KNOWS]->(c) RETURN c", {"id": "n42"}),
      "varlen": ("MATCH (a {id: $id})-[:KNOWS*1..3]->(b) RETURN count(DISTINCT b)", {"id": "n42"}),
      "edges_all": ("MATCH (a)-[r:KNOWS]->(b) RETURN a.id, r.weight, b.id", None),
      "agg": ("MATCH (n:Person) RETURN n.age, count(*) AS c ORDER BY c DESC LIMIT 5", None),
      "pagerank": ("RETURN pageRank()", None),
      "pagerank_top": ("RETURN topPageRank(10)", None),
      "wcc": ("RETURN wcc()", None),
      "louvain": ("RETURN louvain()", None),
      "dijkstra": ("RETURN dijkstra('n42', 'n99')", None),
      "betweenness": ("RETURN betweennessCentrality()", None),
      "nodesim": ("RETURN nodeSimilarity(0.5)", None),
      "set_one": ("MATCH (n {id: $id}) SET n.age = 31 RETURN n", {"id": "n42"}),
    }
    q, p = Q[qname]
    if qname.startswith("pagerank_cached"):
        c.execute("SELECT gql_load_graph()")
    ms, out = timeit(c, q, p, reps)
    rss, hwm = rss_kb(), hwm_kb()
    print(json.dumps({"q": qname, "nodes": n_nodes, "edges": n_edges, "ms": round(ms, 2),
                      "out_bytes": len(out) if out else 0, "base_rss_kb": base_rss,
                      "hwm_delta_kb": hwm - base_hwm, "rss_delta_kb": rss - base_rss}))
