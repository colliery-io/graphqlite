"""gql_check_storage() finds inconsistent storage (GQLITE-T-0525), and the
delete paths leave none when PRAGMA foreign_keys is off (SQLite's default)."""

import json

import pytest

import graphqlite


@pytest.fixture
def db():
    conn = graphqlite.connect(":memory:")
    yield conn
    conn.close()


def check(db):
    return json.loads(db.sqlite_connection.execute("SELECT gql_check_storage()").fetchone()[0])


def test_empty_database_is_consistent():
    conn = graphqlite.connect(":memory:")
    assert check(conn) == []
    conn.close()


def test_corruption_is_reported(db):
    db.cypher("CREATE (:A {x: 1})-[:R {w: 2}]->(:B)")
    c = db.sqlite_connection
    c.commit()
    c.execute("PRAGMA foreign_keys = OFF")
    c.execute("DELETE FROM nodes WHERE id = 2")                        # dangling edge, orphan label
    c.execute("INSERT INTO node_props_text VALUES (1, 1, 'dup')")      # x in two tables
    c.execute("INSERT INTO edge_props_int VALUES (77, 999, 3)")        # orphan, unknown key
    c.commit()
    found = {v["check"] for v in check(db)}
    assert {"edge_target_missing", "orphan_node_labels", "node_property_in_two_tables",
            "orphan_edge_props_int", "unknown_key_edge_props_int"} <= found


@pytest.mark.parametrize("delete", [
    "MATCH (n) DETACH DELETE n",
    "MATCH ()-[r]->() DELETE r",
    "MATCH (a)-[r]->(b) DELETE r, a, b",
    "UNWIND [1] AS i MATCH (n) DETACH DELETE n",
])
def test_delete_leaves_no_orphans_without_foreign_keys(db, delete):
    c = db.sqlite_connection
    c.execute("PRAGMA foreign_keys = OFF")
    db.cypher("CREATE (:A {x: 1, l: [1, 2]})-[:T {w: 1, s: 'a', b: true, f: 1.5, m: {k: 1}}]->(:B {y: 2})")
    db.cypher(delete)
    assert check(db) == []
