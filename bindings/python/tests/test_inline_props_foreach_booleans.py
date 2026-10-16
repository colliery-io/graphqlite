"""Regression tests for GQLITE-T-0421 to GQLITE-T-0424.

0421: an inline property with an expression value (`{id: i}`) was dropped,
so the pattern matched every node and a write after it changed every node.
0422: a stored boolean compared equal to 1 / 0.
0423: FOREACH over nodes(p), relationships(p) or a collected list could not
write the entities; FOREACH after a leading CREATE did not see it.
0424: CREATE ... WITH ... UNWIND ... CREATE ran only the first CREATE.
"""

import pytest

from graphqlite import Graph

from test_graph import get_extension_path


@pytest.fixture
def g():
    with Graph(":memory:", extension_path=get_extension_path()) as graph:
        yield graph


def ids(rows):
    return [r["id"] for r in rows]


# ---------------------------------------------------------------- 0421


def test_unwind_variable_inline_property(g):
    g.query("CREATE (:Item {id: 'a'}), (:Item {id: 'b'}), (:Item {id: 'c'})")
    rows = g.query("UNWIND $ids AS i MATCH (n:Item {id: i}) RETURN n.id AS id ORDER BY id", {"ids": ["a", "b"]})
    assert ids(rows) == ["a", "b"]


def test_unwind_variable_inline_property_write(g):
    g.query("CREATE (:Item {id: 'a', v: 1}), (:Item {id: 'b', v: 2})")
    g.query("UNWIND $ids AS i MATCH (n:Item {id: i}) SET n.v = 9", {"ids": ["a"]})
    rows = g.query("MATCH (n:Item) RETURN n.id AS id, n.v AS v ORDER BY id")
    assert list(rows) == [{"id": "a", "v": 9}, {"id": "b", "v": 2}]


@pytest.mark.parametrize(
    "query, expected",
    [
        ("WITH 1 AS x MATCH (n:Item {v: x + 1}) RETURN n.id AS id", ["b"]),
        ("WITH 'A' AS s MATCH (n:Item {id: toLower(s)}) RETURN n.id AS id", ["a"]),
        ("UNWIND [true] AS t MATCH (n:Item {f: t}) RETURN n.id AS id", ["a"]),
        ("UNWIND [1] AS i MATCH (n:Item {id: i}) RETURN n.id AS id", []),
        ("WITH null AS x MATCH (n:Item {v: x}) RETURN n.id AS id", []),
    ],
)
def test_inline_property_expressions(g, query, expected):
    g.query("CREATE (:Item {id: 'a', v: 1, f: true}), (:Item {id: 'b', v: 2})")
    assert ids(g.query(query)) == expected


def test_relationship_inline_property_from_row(g):
    g.query("CREATE (a:Item {id: 'a'}), (b:Item {id: 'b'}), (c:Item {id: 'c'}), "
            "(a)-[:R {w: 1}]->(b), (a)-[:R {w: 2}]->(c)")
    rows = g.query("UNWIND $rows AS row MATCH (:Item {id: row.a})-[:R {w: row.w}]->(t) RETURN t.id AS id",
                   {"rows": [{"a": "a", "w": 2}]})
    assert ids(rows) == ["c"]


# ---------------------------------------------------------------- 0422


def test_stored_boolean_is_not_a_number(g):
    g.query("CREATE (:Item {flag: true, v: 1})")
    rows = g.query(
        "MATCH (n:Item) RETURN n.flag = 1 AS a, n.flag IN [1] AS b, n.v = true AS c, "
        "1 IN [true] AS d, n.flag = true AS e, n.flag IN [true] AS f, n.v = 1 AS h"
    )
    assert list(rows) == [{"a": False, "b": False, "c": False, "d": False, "e": True, "f": True, "h": True}]
    assert list(g.query("MATCH (n:Item) WHERE n.flag = 1 RETURN count(*) AS c")) == [{"c": 0}]


# ---------------------------------------------------------------- 0423

CHAIN = "CREATE (:Item {name: 'A'})-[:LINK]->(:Item {name: 'B'})-[:LINK]->(:Item {name: 'C'})"


def test_foreach_over_path_nodes(g):
    g.query(CHAIN)
    g.query("MATCH p = (:Item {name: 'A'})-[:LINK*]->(:Item {name: 'C'}) FOREACH (n IN nodes(p) | SET n.seen = true)")
    assert list(g.query("MATCH (n:Item) WHERE n.seen RETURN count(n) AS c")) == [{"c": 3}]


def test_foreach_over_path_relationships(g):
    g.query(CHAIN)
    g.query("MATCH p = (:Item {name: 'A'})-[:LINK*]->(:Item {name: 'C'}) "
            "FOREACH (r IN relationships(p) | SET r.w = 5)")
    assert list(g.query("MATCH ()-[r:LINK]->() RETURN sum(r.w) AS s")) == [{"s": 10}]


def test_foreach_over_collected_nodes(g):
    g.query(CHAIN)
    g.query("MATCH (a:Item) WITH collect(a) AS ns FOREACH (n IN ns | SET n.k = 1)")
    assert list(g.query("MATCH (n:Item) RETURN sum(n.k) AS s")) == [{"s": 3}]


def test_foreach_delete_listed_entity(g):
    g.query(CHAIN)
    g.query("MATCH (c:Item {name: 'C'}) FOREACH (n IN [c] | DETACH DELETE n)")
    assert g.stats()["node_count"] == 2


def test_foreach_after_create(g):
    g.query("CREATE (x:X) FOREACH (i IN [1, 2] | CREATE (x)-[:R]->(:Y {i: i}))")
    rows = g.query("MATCH (:X)-[:R]->(y:Y) RETURN y.i AS i ORDER BY i")
    assert [r["i"] for r in rows] == [1, 2]


# ---------------------------------------------------------------- 0424


def test_create_with_unwind_create(g):
    g.query("CREATE (n:Item {id: 'a'}) WITH n UNWIND [1] AS x CREATE (m:Item {id: 'b'}) SET m.v = x")
    rows = g.query("MATCH (n:Item) RETURN n.id AS id, n.v AS v ORDER BY id")
    assert list(rows) == [{"id": "a", "v": None}, {"id": "b", "v": 1}]


def test_unwind_two_creates(g):
    g.query("UNWIND [1, 2] AS x CREATE (:A {v: x}) CREATE (:B {v: x})")
    assert g.stats()["node_count"] == 4
