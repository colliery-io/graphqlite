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
