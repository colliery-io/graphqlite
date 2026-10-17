"""CREATE / DROP / SHOW INDEX and CONSTRAINT (GQLITE-T-0008).

A constraint holds after each write query: a query that breaks it fails
and leaves no write.
"""

import pytest

import graphqlite
from graphqlite import connect

from test_connection import get_extension_path


@pytest.fixture
def conn():
    c = connect(":memory:", extension_path=get_extension_path())
    yield c
    c.close()


def count(conn, label):
    return conn.cypher(f"MATCH (n:{label}) RETURN count(n) AS c")[0]["c"]


def test_unique_violation_rolls_back(conn):
    conn.cypher("CREATE CONSTRAINT FOR (p:Person) REQUIRE p.id IS UNIQUE")
    conn.cypher("CREATE (:Person {id: 1})")
    with pytest.raises(graphqlite.CypherError) as ei:
        conn.cypher("CREATE (:Other), (:Person {id: 1.0})")
    assert "ConstraintValidationFailed" in str(ei.value)
    assert count(conn, "Person") == 1
    assert count(conn, "Other") == 0


def test_unique_violation_by_set(conn):
    conn.cypher("CREATE CONSTRAINT FOR (p:Person) REQUIRE p.id IS UNIQUE")
    conn.cypher("CREATE (:Person {id: 1}), (:Person {id: 2})")
    with pytest.raises(graphqlite.CypherError):
        conn.cypher("MATCH (p:Person {id: 2}) SET p.id = 1")
    ids = conn.cypher("MATCH (p:Person) RETURN p.id AS id ORDER BY id")
    assert [r["id"] for r in ids] == [1, 2]


def test_not_null(conn):
    conn.cypher("CREATE CONSTRAINT FOR (p:Person) REQUIRE p.name IS NOT NULL")
    conn.cypher("CREATE (:Person {name: 'Robin'})")
    with pytest.raises(graphqlite.CypherError):
        conn.cypher("CREATE (:Person)")
    with pytest.raises(graphqlite.CypherError):
        conn.cypher("MATCH (p:Person) REMOVE p.name")
    assert count(conn, "Person") == 1


def test_constraint_refused_on_existing_violation(conn):
    conn.cypher("CREATE (:Person {id: 1}), (:Person {id: 1})")
    with pytest.raises(graphqlite.CypherError):
        conn.cypher("CREATE CONSTRAINT FOR (p:Person) REQUIRE p.id IS UNIQUE")
    assert len(conn.cypher("SHOW CONSTRAINTS")) == 0


def test_equivalent_and_if_exists(conn):
    conn.cypher("CREATE INDEX FOR (p:Person) ON (p.name)")
    with pytest.raises(graphqlite.CypherError) as ei:
        conn.cypher("CREATE INDEX FOR (p:Person) ON (p.name)")
    assert "EquivalentSchemaRuleAlreadyExists" in str(ei.value)
    conn.cypher("CREATE INDEX IF NOT EXISTS FOR (p:Person) ON (p.name)")
    with pytest.raises(graphqlite.CypherError):
        conn.cypher("DROP INDEX missing")
    conn.cypher("DROP INDEX missing IF EXISTS")


def test_relationship_index_unsupported(conn):
    with pytest.raises(graphqlite.CypherError) as ei:
        conn.cypher("CREATE INDEX FOR ()-[r:KNOWS]-() ON (r.since)")
    assert "UnsupportedFeature" in str(ei.value)
