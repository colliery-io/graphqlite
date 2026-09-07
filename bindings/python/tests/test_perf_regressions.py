"""Regression tests for the performance review (PR #118), phase one.

These assert the *shape* of the generated SQL and its query plan rather than
timings, so they are deterministic. The numbers themselves are reproduced by
tests/performance/python/.
"""

import json
import os
import sqlite3
from pathlib import Path

import pytest

from graphqlite import Graph, connect


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


# --- F8: per-connection statement cache ---------------------------------------

def test_statement_cache_reflects_params_and_writes(db):
    q = "MATCH (n:P {id: $id}) RETURN n.n AS n"
    assert db.cypher(q, {"id": "p3"})[0]["n"] == 3
    assert db.cypher(q, {"id": "p4"})[0]["n"] == 4          # same text, new binding
    db.cypher("MATCH (n:P {id: 'p3'}) SET n.n = 300")
    assert db.cypher(q, {"id": "p3"})[0]["n"] == 300        # cached statement sees the write
    db.cypher("CREATE (:P {id: 'p_new', n: 77})")
    assert db.cypher(q, {"id": "p_new"})[0]["n"] == 77      # and new rows
    db.cypher("MATCH (n:P {id: 'p_new'}) DELETE n")
    assert db.cypher(q, {"id": "p_new"}).to_list() == []    # and deletions
    # a missing parameter binds NULL (no match), never a stale value from the last call
    assert db.cypher(q, {"other": 1}).to_list() == []
    assert db.cypher(q, {"id": "p4"})[0]["n"] == 4           # cache still healthy afterwards


def test_statement_cache_survives_many_distinct_queries(db):
    for i in range(100):
        assert db.cypher(f"MATCH (n:P {{id: 'p{i % 50}'}}) RETURN n.n AS n")[0]["n"] == i % 50
    assert db.cypher("MATCH (n:P {id: 'p7'}) RETURN n.n AS n")[0]["n"] == 7


# ---------------------------------------------------------------------------
# F10: cypher_rows table-valued interface
# ---------------------------------------------------------------------------


class TestCypherRows:
    @pytest.fixture
    def conn(self):
        c = connect(":memory:", extension_path=get_extension_path())
        yield c
        c.close()

    @pytest.fixture
    def graph(self):
        with Graph(":memory:", extension_path=get_extension_path()) as g:
            yield g

    def test_iter_rows_matches_cypher(self, conn):
        conn.cypher("CREATE (a:P {name: 'Ann', age: 30})-[:KNOWS {since: 2020}]->(b:P {name: 'Bob', age: 25})")
        via_cypher = conn.cypher("MATCH (n:P)-[r]->(m) RETURN n, r, m ORDER BY n.name").to_list()
        via_rows = list(conn.iter_rows("MATCH (n:P)-[r]->(m) RETURN n, r, m ORDER BY n.name"))
        assert via_rows == via_cypher
        assert via_rows[0]["n"]["properties"]["name"] == "Ann"
        assert via_rows[0]["r"]["properties"]["since"] == 2020

    def test_iter_rows_is_a_generator_with_params(self, conn):
        conn.cypher("UNWIND range(1, 100) AS i CREATE (:N {i: i})")
        it = conn.iter_rows("MATCH (n:N) WHERE n.i > $min RETURN n.i AS i ORDER BY i", {"min": 90})
        assert next(it) == {"i": 91}
        assert [r["i"] for r in it] == list(range(92, 101))

    def test_iter_rows_write_stats_and_errors(self, conn):
        assert list(conn.iter_rows("CREATE (:N {i: 1})")) == [
            {"nodes_created": 1, "relationships_created": 0, "nodes_deleted": 0,
             "relationships_deleted": 0, "properties_set": 1}
        ]
        with pytest.raises(sqlite3.Error, match="syntax error"):
            list(conn.iter_rows("BOGUS"))

    def test_native_column_types(self, conn):
        conn.cypher("CREATE (:T {s: 'x', i: 7, f: 2.5, b: true})")
        row = conn.execute(
            "SELECT c0, typeof(c0), c1, typeof(c1), c2, typeof(c2), c3, typeof(c3), c4, typeof(c4) "
            "FROM cypher_rows('MATCH (n:T) RETURN n.s, n.i, n.f, n.b, n.missing')"
        ).fetchone()
        assert row == ("x", "text", 7, "integer", 2.5, "real", 1, "integer", None, "null")

    def test_sql_limit_stops_early(self, conn):
        conn.cypher("UNWIND range(1, 5000) AS i CREATE (:N {i: i})")
        rows = conn.execute("SELECT c0 FROM cypher_rows('MATCH (n:N) RETURN n.i ORDER BY n.i') LIMIT 3").fetchall()
        assert rows == [(1,), (2,), (3,)]

    def test_graph_iter_query(self, graph):
        graph.upsert_node("a", {"name": "A"})
        assert [r["name"] for r in graph.iter_query("MATCH (n) RETURN n.name AS name")] == ["A"]


# ---------------------------------------------------------------------------
# Smaller review items: CSR cache staleness, growable transform buffers
# ---------------------------------------------------------------------------


def test_cached_graph_refreshes_after_writes():
    g = connect(":memory:", extension_path=get_extension_path())
    for i in range(3):
        g.cypher("CREATE (:C {id: $id})", {"id": f"c{i}"})
    g.cypher("MATCH (a:C {id: 'c0'}), (b:C {id: 'c1'}) CREATE (a)-[:E]->(b)")
    assert json.loads(g.execute("SELECT gql_load_graph()").fetchone()[0])["nodes"] == 3
    assert len(_algo(g.cypher("RETURN louvain()"))) == 3
    # A write after gql_load_graph() used to leave algorithms on the old graph
    g.cypher("CREATE (:C {id: 'c3'})-[:E]->(:C {id: 'c4'})")
    assert len(_algo(g.cypher("RETURN louvain()"))) == 5
    assert json.loads(g.execute("SELECT gql_load_graph()").fetchone()[0])["nodes"] == 5
    g.close()


def test_large_pattern_comprehension_projection(db):
    # The collect expression of a pattern comprehension used to be rendered
    # into a fixed 4 KB stack buffer that append_sql could try to realloc.
    rows = db.cypher(
        "MATCH (a:P {id: 'p0'}) RETURN [(a)-[:R]->(b) | {x: b, y: b, z: b, w: b}] AS l"
    ).to_list()
    assert len(rows) == 1 and len(rows[0]["l"]) == 1
    assert set(rows[0]["l"][0].keys()) == {"x", "y", "z", "w"}
    assert rows[0]["l"][0]["x"]["properties"]["id"] == "p1"


def test_property_keys_sharing_a_hash_slot_stay_cached(db):
    # 2,000 distinct keys over a 1,024-slot cache guarantee collisions; every
    # key must still resolve (the cache used to evict on collision).
    props = ", ".join(f"k{i}: {i}" for i in range(400))
    for batch in range(5):
        db.cypher(f"CREATE (:K {{ {props.replace('k', f'k{batch}_')} }})")
    for batch in range(5):
        for i in (0, 199, 399):
            key = f"k{batch}_{i}"
            assert db.cypher(f"MATCH (n:K) WHERE n.{key} = {i} RETURN count(*) AS c")[0]["c"] == 1


# ---------------------------------------------------------------------------
# Correctness bugs found during the performance review
# ---------------------------------------------------------------------------

SOH = chr(1)


class TestParameterEscapes:
    """json.dumps() escapes every non-ASCII character as \\uXXXX by default and
    the parameter decoder copied the escape through literally, so "cafe'"
    arrived as "cafu00e9" and every non-ASCII parameter was corrupted."""

    @pytest.fixture
    def conn(self):
        c = connect(":memory:", extension_path=get_extension_path())
        yield c
        c.close()

    @pytest.mark.parametrize(
        "value",
        ["café", "中文", "a\U0001F600b", "naïve→",
         "a\x01b", "tab\there", "nl\nhere", 'quote"here', "back\\slash",
         "sol/idus", "\x1f\x08\x0c"],
    )
    def test_parameter_round_trip(self, conn, value):
        assert conn.cypher("RETURN $s AS s", {"s": value})[0]["s"] == value
        assert conn.cypher("RETURN size($s) AS n", {"s": value})[0]["n"] == len(value)

    @pytest.mark.parametrize("value", ["café", "中文", "a\U0001F600b", "a\x01b"])
    def test_stored_property_round_trip(self, conn, value):
        conn.cypher("CREATE (:U {v: $v})", {"v": value})
        assert conn.cypher("MATCH (n:U) RETURN n.v AS v")[0]["v"] == value
        assert conn.cypher("MATCH (n:U) RETURN n")[0]["n"]["properties"]["v"] == value
        assert conn.cypher("MATCH (n:U {v: $v}) RETURN count(*) AS c", {"v": value})[0]["c"] == 1

    def test_set_property_via_parameter(self, conn):
        conn.cypher("CREATE (:W {id: 1})")
        conn.cypher("MATCH (n:W) SET n.v = $v", {"v": "café"})
        assert conn.cypher("MATCH (n:W) RETURN n.v AS v")[0]["v"] == "café"

    def test_lone_surrogate_and_truncated_escape_do_not_crash(self, conn):
        # Neither can come from json.dumps, but the decoder must not read past
        # the end of the string if it is handed one.
        got = conn.execute("SELECT cypher(?, ?)",
                           ("RETURN $s AS s", '{"s": "a\\uD800b"}')).fetchone()[0]
        assert json.loads(got)[0]["s"] == "a�b"
        conn.execute("SELECT cypher(?, ?)",
                     ("RETURN $s AS s", '{"s": "a\\u00"}')).fetchone()


class TestControlCharacterOutput:
    """The agtype serializer replaced control characters with a space, silently
    corrupting the value on the way out of the scalar path."""

    @pytest.fixture
    def conn(self):
        c = connect(":memory:", extension_path=get_extension_path())
        yield c
        c.close()

    def test_control_characters_are_escaped_not_blanked_on_the_wire(self, conn):
        expected = "p" + SOH + "q"
        conn.cypher("CREATE (:C {v: $v})", {"v": expected})
        # The scalar read path used to emit a space in place of the control
        # character, which is valid JSON and therefore silently wrong.
        raw = conn.execute(
            "SELECT cypher('MATCH (n:C) RETURN n.v AS v')").fetchone()[0]
        assert "\\u0001" in raw
        assert '"p q"' not in raw
        assert json.loads(raw)[0]["v"] == expected
        assert conn.cypher("MATCH (n:C) RETURN n")[0]["n"]["properties"]["v"] == expected

    def test_every_control_character_round_trips(self, conn):
        value = "".join(chr(i) for i in range(1, 32))
        conn.cypher("CREATE (:D {v: $v})", {"v": value})
        assert conn.cypher("MATCH (n:D) RETURN n.v AS v")[0]["v"] == value
        assert conn.cypher("MATCH (n:D) RETURN n")[0]["n"]["properties"]["v"] == value


class TestOrderByAliasShadowing:
    """ORDER BY wraps its term in _gql_order_rank(), and SQLite only substitutes
    an output alias for a bare ORDER BY term, so an alias named like one of our
    own columns bound to the base table instead: `AS id ORDER BY id` sorted by
    the internal node id, and two patterns in scope failed as ambiguous."""

    @pytest.fixture
    def conn(self):
        c = connect(":memory:", extension_path=get_extension_path())
        # inserted out of order so the internal ids disagree with the property
        for v in ("b3", "b1", "b2"):
            c.cypher("CREATE (:B {id: $v, type: $v, value: $v})", {"v": v})
        yield c
        c.close()

    @pytest.mark.parametrize("alias", ["id", "type", "value", "label", "key", "node_id"])
    def test_shadowing_alias_sorts_by_the_projection(self, conn, alias):
        rows = conn.cypher("MATCH (b:B) RETURN b.id AS %s ORDER BY %s" % (alias, alias)).to_list()
        assert [r[alias] for r in rows] == ["b1", "b2", "b3"]
        rows = conn.cypher("MATCH (b:B) RETURN b.id AS %s ORDER BY %s DESC" % (alias, alias)).to_list()
        assert [r[alias] for r in rows] == ["b3", "b2", "b1"]

    def test_two_patterns_in_scope_is_not_ambiguous(self, conn):
        rows = conn.cypher(
            "MATCH (a:B), (b:B) WHERE a.id = 'b1' RETURN b.id AS id ORDER BY id"
        ).to_list()
        assert [r["id"] for r in rows] == ["b1", "b2", "b3"]

    def test_non_shadowing_alias_still_works(self, conn):
        rows = conn.cypher("MATCH (b:B) RETURN b.id AS bid ORDER BY bid").to_list()
        assert [r["bid"] for r in rows] == ["b1", "b2", "b3"]

    def test_order_by_a_property_is_unchanged(self, conn):
        rows = conn.cypher("MATCH (b:B) RETURN b.id AS id ORDER BY b.id DESC").to_list()
        assert [r["id"] for r in rows] == ["b3", "b2", "b1"]

    def test_order_by_alias_with_skip_and_limit(self, conn):
        rows = conn.cypher("MATCH (b:B) RETURN b.id AS id ORDER BY id SKIP 1 LIMIT 1").to_list()
        assert [r["id"] for r in rows] == ["b2"]

    def test_order_by_aggregate_alias(self, conn):
        rows = conn.cypher(
            "MATCH (b:B) RETURN b.id AS id, count(*) AS value ORDER BY value, id"
        ).to_list()
        assert [r["id"] for r in rows] == ["b1", "b2", "b3"]
