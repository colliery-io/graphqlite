"""CALL algorithm() YIELD: the outputs are declared per algorithm
(GQLITE-T-0442)."""

import pytest

import graphqlite
from graphqlite import connect

from test_connection import get_extension_path


@pytest.fixture
def conn():
    c = connect(":memory:", extension_path=get_extension_path())
    yield c
    c.close()


def test_unknown_yield_on_an_empty_graph(conn):
    with pytest.raises(graphqlite.CypherError) as ei:
        conn.cypher("CALL pageRank() YIELD nope")
    assert "no output `nope`" in str(ei.value)


def test_unknown_yield_with_rows(conn):
    conn.cypher("CREATE (:N {id: 'a'})-[:R]->(:N {id: 'b'})")
    with pytest.raises(graphqlite.CypherError):
        conn.cypher("CALL pageRank() YIELD nope")


def test_yield_star_then_count_on_an_empty_graph(conn):
    assert list(conn.cypher("CALL pageRank() YIELD * RETURN count(*) AS n")) == [{"n": 0}]


@pytest.mark.parametrize("call,outputs", [
    ("pageRank()", ["node_id", "user_id", "score"]),
    ("wcc()", ["node_id", "user_id", "component"]),
    ("degreeCentrality()", ["node_id", "user_id", "in_degree", "out_degree", "degree"]),
    ("apsp()", ["source", "target", "distance"]),
])
def test_declared_outputs_can_be_yielded_on_an_empty_graph(conn, call, outputs):
    items = ", ".join(outputs)
    assert list(conn.cypher(f"CALL {call} YIELD {items} RETURN count(*) AS n")) == [{"n": 0}]
