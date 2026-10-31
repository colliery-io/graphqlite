"""LOAD CSV errors that were silent (GQLITE-T-0443)."""

import sqlite3

import pytest

import graphqlite
from graphqlite import connect

from test_connection import get_extension_path


@pytest.fixture
def conn():
    c = connect(":memory:", extension_path=get_extension_path())
    yield c
    c.close()


def load(conn, path, extra=""):
    return conn.cypher(f"LOAD CSV FROM '{path}' AS row {extra} RETURN row")


def test_unclosed_quote(conn, tmp_path):
    p = tmp_path / "q.csv"
    p.write_bytes(b'a,b\n1,"x\n3,4\n')
    with pytest.raises(graphqlite.CypherError) as ei:
        load(conn, p)
    assert "not closed" in str(ei.value)


def test_nul_byte(conn, tmp_path):
    p = tmp_path / "n.csv"
    p.write_bytes(b"a,b\n1,x\x00y\n3,4\n")
    with pytest.raises(graphqlite.CypherError) as ei:
        load(conn, p)
    assert "NUL" in str(ei.value)


def test_long_field_terminator(conn, tmp_path):
    p = tmp_path / "t.csv"
    p.write_bytes(b"a,b\n1,2\n")
    with pytest.raises(graphqlite.CypherError) as ei:
        load(conn, p, "FIELDTERMINATOR 'ab'")
    assert "one character" in str(ei.value)


@pytest.mark.skipif(not hasattr(sqlite3.Connection, "setlimit"),
                    reason="Connection.setlimit needs Python 3.11")
def test_too_large_is_an_error(tmp_path):
    p = tmp_path / "big.csv"
    p.write_bytes(b"a\n" + b"x" * 4000 + b"\n")
    c = connect(":memory:", extension_path=get_extension_path())
    try:
        c._conn.setlimit(sqlite3.SQLITE_LIMIT_LENGTH, 2000)
        with pytest.raises(graphqlite.CypherError) as ei:
            load(c, p)
        assert "too large" in str(ei.value) or "bind" in str(ei.value)
    finally:
        c.close()
