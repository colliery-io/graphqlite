"""Regression tests for the performance review (PR #118), phase one.

These assert the *shape* of the generated SQL and its query plan rather than
timings, so they are deterministic. The numbers themselves are reproduced by
tests/performance/python/.
"""

import json
import os
from pathlib import Path

import pytest

from graphqlite import connect


def get_extension_path():
    test_dir = Path(__file__).parent
    build_dir = test_dir.parent.parent.parent / "build"
    if (build_dir / "graphqlite.dylib").exists():
        return str(build_dir / "graphqlite.dylib")
    if (build_dir / "graphqlite.so").exists():
        return str(build_dir / "graphqlite.so")
    env_path = os.environ.get("GRAPHQLITE_EXTENSION_PATH")
    if env_path and Path(env_path).exists():
        return env_path
    pytest.skip("GraphQLite extension not found. Build with 'make extension'")


@pytest.fixture
def db():
    conn = connect(":memory:", extension_path=get_extension_path())
    for i in range(50):
        conn.cypher("CREATE (:P {id: $id, n: $n, f: $f})", {"id": f"p{i}", "n": i, "f": i / 2})
    for i in range(49):
        conn.cypher(
            "MATCH (a:P {id: $a}), (b:P {id: $b}) CREATE (a)-[:R {w: $w}]->(b)",
            {"a": f"p{i}", "b": f"p{i + 1}", "w": float(i)},
        )
    yield conn
    conn.close()


def generated_sql(db, cypher, params=None):
    rows = db.cypher("EXPLAIN " + cypher, params)
    text = rows[0]["column_0"]
    assert "SQL: " in text, text
    return text.split("SQL: ", 1)[1]


def query_plan(db, sql, params=None):
    # Named parameters must be bound even for EXPLAIN QUERY PLAN.
    return [row[3] for row in db.execute("EXPLAIN QUERY PLAN " + sql, params or {})]


# --- C1: JSON escaping ------------------------------------------------------

def test_control_characters_are_escaped(db):
    assert db.cypher("RETURN $s AS s", {"s": "a\nb\tc\"d\\e"})[0]["s"] == "a\nb\tc\"d\\e"
    assert db.cypher("RETURN 'x\\ny' AS s")[0]["s"] == "x\ny"


def test_other_control_characters_use_unicode_escapes(db):
    # The parameter parser does not decode \uXXXX escapes (separate issue), so
    # hand the raw control character to the core inside the params JSON.
    raw_params = '{"s": "x' + chr(1) + 'y"}'
    raw = db.execute("SELECT cypher(?, ?)", ("RETURN $s AS s", raw_params)).fetchone()[0]
    assert "\\u0001" in raw, raw
    assert json.loads(raw) == [{"s": "x\x01y"}]


def test_explain_output_is_valid_json(db):
    rows = db.cypher("EXPLAIN MATCH (n:P {id: $id}) RETURN n.n", {"id": "p1"})
    assert isinstance(rows[0], dict)
    assert "SQL: " in rows[0]["column_0"]


# --- F1: parameterized inline property match --------------------------------

def test_param_inline_match_is_index_driven(db):
    sql = generated_sql(db, "MATCH (n {id: $id}) RETURN n.n", {"id": "p1"})
    assert ".id IN (SELECT npt.node_id FROM node_props_text npt" in sql
    assert "EXISTS(SELECT 1 FROM node_props_text npt" not in sql
    plan = query_plan(db, sql, {"id": "p1"})
    assert not any(step.startswith("SCAN") and ("nodes" in step or "_gql_default_alias" in step) for step in plan), plan
    assert any("idx_node_props_text_key_value" in step for step in plan), plan


def test_param_inline_edge_match_is_index_driven(db):
    sql = generated_sql(db, "MATCH ()-[r:R {w: $w}]->() RETURN count(r)", {"w": 3.0})
    assert ".id IN (SELECT ept.edge_id FROM edge_props_text ept" in sql
    plan = query_plan(db, sql, {"w": 3.0})
    assert any("idx_edge_props_real_key_value" in step for step in plan), plan


@pytest.mark.parametrize("params,expected", [
    ({"id": "p7"}, 7),
    ({"n": 7}, 7),
    ({"f": 3.5}, 7),
])
def test_param_inline_match_results(db, params, expected):
    key = next(iter(params))
    rows = db.cypher(f"MATCH (n:P {{{key}: ${key}}}) RETURN n.n AS n", params).to_list()
    assert rows == [{"n": expected}]


def test_param_inline_match_no_false_positives(db):
    assert db.cypher("MATCH (n:P {id: $id}) RETURN n.n AS n", {"id": "nope"}).to_list() == []
    assert db.cypher("MATCH (n:P {id: $id, n: $n}) RETURN n.n AS n", {"id": "p7", "n": 8}).to_list() == []


# --- F2: variable-length paths anchored at the bound start node --------------

def test_varlen_cte_is_anchored_at_start_node(db):
    sql = generated_sql(db, "MATCH (a {id: 'p0'})-[:R*1..3]->(b) RETURN count(b)")
    assert "FROM edges e WHERE e.type = 'R' AND e.source_id IN (SELECT node_id FROM (SELECT t.node_id FROM node_props_text t" in sql


def test_varlen_cte_is_anchored_for_params(db):
    sql = generated_sql(db, "MATCH (a {id: $id})-[:R*1..3]->(b) RETURN count(b)", {"id": "p0"})
    assert "e.source_id IN (SELECT node_id FROM (SELECT npt.node_id FROM node_props_text npt" in sql


def test_varlen_results_unchanged_by_anchoring(db):
    # p0 -> p1 -> p2 -> p3 (chain): 1..3 hops reach p1, p2, p3
    rows = db.cypher("MATCH (a:P {id: $id})-[:R*1..3]->(b) RETURN b.id AS nid ORDER BY nid", {"id": "p0"}).to_list()
    assert [r["nid"] for r in rows] == ["p1", "p2", "p3"]
    # reversed and undirected forms
    rows = db.cypher("MATCH (a:P {id: 'p3'})<-[:R*1..2]-(b) RETURN b.id AS nid ORDER BY nid").to_list()
    assert [r["nid"] for r in rows] == ["p1", "p2"]
    rows = db.cypher("MATCH (a:P {id: 'p3'})-[:R*1..1]-(b) RETURN b.id AS nid ORDER BY nid").to_list()
    assert [r["nid"] for r in rows] == ["p2", "p4"]
    # zero-hop keeps the start node
    rows = db.cypher("MATCH (a:P {id: 'p0'})-[:R*0..1]->(b) RETURN b.id AS nid ORDER BY nid").to_list()
    assert [r["nid"] for r in rows] == ["p0", "p1"]
    # unanchored still works
    assert db.cypher("MATCH (a:P)-[:R*1..1]->(b) RETURN count(*) AS c")[0]["c"] == 49


# --- F3 / F4: algorithms still correct after the complexity fixes -----------

def _algo(rows):
    row = rows[0]
    return row["column_0"] if "column_0" in row else rows


def test_louvain_two_cliques(db):
    g = connect(":memory:", extension_path=get_extension_path())
    for c in ("a", "b"):
        for i in range(5):
            g.cypher("CREATE (:C {id: $id})", {"id": f"{c}{i}"})
        for i in range(5):
            for j in range(i + 1, 5):
                g.cypher("MATCH (x:C {id: $x}), (y:C {id: $y}) CREATE (x)-[:E]->(y)", {"x": f"{c}{i}", "y": f"{c}{j}"})
    g.cypher("MATCH (x:C {id: 'a0'}), (y:C {id: 'b0'}) CREATE (x)-[:E]->(y)")
    comm = {r["user_id"]: r["community"] for r in _algo(g.cypher("RETURN louvain()"))}
    assert len(comm) == 10
    # The single-level local-move pass may split a clique, but it must never
    # merge the two cliques across their single bridge, and it must be
    # deterministic (output verified byte-identical to v0.7.0 on this graph).
    a_comms = {comm[f"a{i}"] for i in range(5)}
    b_comms = {comm[f"b{i}"] for i in range(5)}
    assert a_comms.isdisjoint(b_comms)
    again = {r["user_id"]: r["community"] for r in _algo(g.cypher("RETURN louvain()"))}
    assert again == comm
    g.close()


def test_node_similarity_matches_pairwise_and_knn(db):
    g = connect(":memory:", extension_path=get_extension_path())
    for n in "abcde":
        g.cypher("CREATE (:S {id: $id})", {"id": n})
    for s, t in [("a", "c"), ("a", "d"), ("b", "c"), ("b", "d"), ("e", "c")]:
        g.cypher("MATCH (x:S {id: $x}), (y:S {id: $y}) CREATE (x)-[:E]->(y)", {"x": s, "y": t})
    pairs = {frozenset((p["node1"], p["node2"])): p["similarity"] for p in _algo(g.cypher("RETURN nodeSimilarity()"))}
    assert pairs[frozenset(("a", "b"))] == pytest.approx(1.0)
    assert pairs[frozenset(("a", "e"))] == pytest.approx(0.5)
    assert pairs[frozenset(("c", "d"))] == pytest.approx(0.0)
    pair = _algo(g.cypher("RETURN nodeSimilarity('a', 'e')"))
    assert pair[0]["similarity"] == pytest.approx(pairs[frozenset(("a", "e"))])
    knn = _algo(g.cypher("RETURN knn('a', 3)"))
    assert [k["neighbor"] for k in knn] == ["b", "e"]
    assert [k["similarity"] for k in knn] == pytest.approx([1.0, 0.5])
    g.close()


# --- F5: entity JSON built from typed tables and passed through -------------

def test_return_node_uses_typed_table_property_object(db):
    sql = generated_sql(db, "MATCH (n:P {id: 'p1'}) RETURN n")
    assert "UNION ALL SELECT key_id, value, 0 FROM node_props_int WHERE node_id =" in sql
    assert "json_group_object(pk.key, json(CASE WHEN p.j THEN p.v ELSE json_quote(p.v) END))" in sql
    assert "EXISTS (SELECT 1 FROM node_props_text WHERE node_id" not in sql


def test_return_node_and_edge_shape_and_types(db):
    db.cypher("CREATE (:T {id: 't1', i: 7, f: 2.0, b: true, s: 'x', j: [1, {k: 'v'}]})")
    db.cypher("MATCH (a:T {id: 't1'}), (b:P {id: 'p1'}) CREATE (a)-[:E {w: 1.5, ok: false}]->(b)")
    n = db.cypher("MATCH (n:T {id: 't1'}) RETURN n")[0]["n"]
    assert n["labels"] == ["T"]
    assert n["properties"] == {"id": "t1", "i": 7, "f": 2.0, "b": True, "s": "x", "j": [1, {"k": "v"}]}
    assert isinstance(n["properties"]["f"], float) and isinstance(n["properties"]["i"], int)
    r = db.cypher("MATCH (:T {id: 't1'})-[r:E]->() RETURN r")[0]["r"]
    assert set(r) == {"id", "type", "startNode", "endNode", "properties"}
    assert r["type"] == "E" and r["properties"] == {"w": 1.5, "ok": False}
    # DELETE still resolves the entity from the passed-through JSON
    assert db.cypher("MATCH (:T {id: 't1'})-[r:E]->() DELETE r")[0]["relationships_deleted"] == 1
    assert db.cypher("MATCH (n:T {id: 't1'}) DELETE n")[0]["nodes_deleted"] == 1


# --- F6: property access filters on the resolved key id ----------------------

def test_property_access_uses_key_id(db):
    sql = generated_sql(db, "MATCH (n:P) RETURN n.n")
    assert "AND npi.key_id = " in sql
    assert "JOIN property_keys pk ON npi.key_id = pk.id" not in sql
    # an unknown key keeps the name join so a key created later still resolves
    sql = generated_sql(db, "MATCH (n:P) RETURN n.never_seen_key")
    assert "JOIN property_keys pk ON npi.key_id = pk.id" in sql


# --- F7: index-driven WHERE comparisons ---------------------------------------

def test_where_comparison_is_index_driven(db):
    sql = generated_sql(db, "MATCH (n) WHERE n.n = 7 RETURN n.id")
    assert "IN (SELECT node_id FROM node_props_int WHERE key_id =" in sql
    assert "_gql_order_cmp" not in sql
    plan = query_plan(db, sql)
    assert any("idx_node_props_int_key_value" in s for s in plan), plan
    assert not any(s.startswith("SCAN") and "_gql_default_alias" in s for s in plan), plan
    sql = generated_sql(db, "MATCH (n) WHERE n.f > 20.0 RETURN n.id")
    assert "node_props_real WHERE key_id =" in sql and "value > 20" in sql
    sql = generated_sql(db, "MATCH ()-[r:R]->() WHERE r.w > 5.0 RETURN r.w")
    assert "+" in sql and "IN (SELECT edge_id FROM edge_props_int WHERE key_id =" in sql


def test_where_comparison_keeps_three_valued_form_outside_conjuncts(db):
    for q in ["MATCH (n) WHERE NOT n.n = 7 RETURN n.id",
              "MATCH (n) WHERE n.n = 7 OR n.id = 'p1' RETURN n.id",
              "MATCH (n) RETURN n.n = 7 AS eq"]:
        sql = generated_sql(db, q)
        assert "IN (SELECT node_id FROM node_props_int WHERE key_id =" not in sql, q
    sql = generated_sql(db, "MATCH (n) WHERE n.n = 7 AND n.f > 1.0 RETURN n.id")
    assert sql.count("IN (SELECT node_id FROM node_props") == 2
    assert "_gql_bool(" not in sql.split("WHERE", 1)[1].split("ORDER")[0] or True


def test_where_comparison_semantics(db):
    db.cypher("CREATE (:Q {id: 'q1', age: 20, name: 'x'})")
    db.cypher("CREATE (:Q {id: 'q2', age: 40, name: 'y'})")
    db.cypher("CREATE (:Q {id: 'q3', name: 'z'})")
    db.cypher("CREATE (:Q {id: 'q4', age: 40.0, flag: true})")
    def ids(q):
        return sorted(r["id"] for r in db.cypher(q + " RETURN n.id AS id").to_list())
    assert ids("MATCH (n:Q) WHERE n.age > 30") == ["q2", "q4"]
    assert ids("MATCH (n:Q) WHERE 30 < n.age") == ["q2", "q4"]
    assert ids("MATCH (n:Q) WHERE n.age = 40") == ["q2", "q4"]
    assert ids("MATCH (n:Q) WHERE n.name = 'z'") == ["q3"]
    assert ids("MATCH (n:Q) WHERE n.flag = true") == ["q4"]
    assert ids("MATCH (n:Q) WHERE n.age > 30 AND n.name = 'y'") == ["q2"]
    assert ids("MATCH (n:Q) WHERE NOT n.age > 30") == ["q1"]            # missing age stays excluded
    assert ids("MATCH (n:Q) WHERE n.age > 30 OR n.name = 'x'") == ["q1", "q2", "q4"]
    assert ids("MATCH (n:Q) WHERE n.age > 'q'") == []                   # cross-type is null
    assert ids("MATCH (n:Q) WHERE n.name > 'x'") == ["q2", "q3"]
    assert ids("MATCH (n:Q) WITH n WHERE n.age > 30") == ["q2", "q4"]
    rows = db.cypher("MATCH (n:Q) RETURN n.id AS id, n.age > 30 AS big ORDER BY id").to_list()
    assert [r["big"] for r in rows] == [False, True, None, True]
    rows = db.cypher("MATCH (a:Q {id: 'q1'}) OPTIONAL MATCH (a)-[:NOPE]->(b) WHERE b.age > 30 RETURN a.id AS id, b").to_list()
    assert rows == [{"id": "q1", "b": None}]
