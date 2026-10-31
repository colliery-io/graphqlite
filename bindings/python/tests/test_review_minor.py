"""0.12.0 review items (GQLITE-T-0447)."""

import pytest

import graphqlite
from graphqlite import connect

from test_connection import get_extension_path


@pytest.fixture
def conn():
    c = connect(":memory:", extension_path=get_extension_path())
    yield c
    c.close()


def test_show_indexes_takes_nothing_after_it(conn):
    conn.cypher("CREATE INDEX i1 FOR (n:P) ON (n.x)")
    assert [r["name"] for r in conn.cypher("SHOW INDEXES")] == ["i1"]
    with pytest.raises(graphqlite.CypherError) as ei:
        conn.cypher("SHOW INDEXES blah blah")
    assert "nothing after it" in str(ei.value)


def test_procedure_call_after_other_clauses(conn):
    with pytest.raises(graphqlite.CypherError) as ei:
        conn.cypher("MATCH (n) CALL pageRank() YIELD score RETURN score")
    assert "must be the first clause" in str(ei.value)


def test_algorithm_with_string_parameters_runs_repeatedly(conn):
    # A*, BFS, DFS, similarity and KNN leaked their string parameters
    conn.cypher("CREATE (a:N {id: 'a', lat: 1.0, lon: 1.0})-[:R]->(b:N {id: 'b', lat: 2.0, lon: 2.0})")
    for _ in range(3):
        assert list(conn.cypher("CALL bfs('a') YIELD node_id RETURN count(*) AS n"))[0]["n"] >= 1
        assert list(conn.cypher("CALL astar('a', 'b', 'lat', 'lon') YIELD found RETURN found"))[0]["found"] is True
