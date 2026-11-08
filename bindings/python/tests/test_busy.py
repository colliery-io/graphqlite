"""A write that loses the write lock to another connection is a busy error
the caller can retry (GQLITE-T-0533)."""

import pytest

import graphqlite
from graphqlite import CypherError


def test_busy_write_is_retryable(tmp_path):
    path = str(tmp_path / "busy.db")
    a = graphqlite.connect(path)
    b = graphqlite.connect(path)
    sa = a.sqlite_connection
    sa.isolation_level = None
    b.sqlite_connection.isolation_level = None
    sa.execute("PRAGMA journal_mode = WAL")
    a.cypher("CREATE (:X {v: 1})")
    sa.execute("BEGIN")
    a.cypher("MATCH (n:X) RETURN count(*) AS c")      # a read snapshot
    b.cypher("CREATE (:X {v: 2})")                    # another writer commits
    with pytest.raises(CypherError) as ei:
        a.cypher("CREATE (:X {v: 3})")                # a cannot upgrade
    assert ei.value.code == "BUSY"
    assert ei.value.retryable
    sa.execute("ROLLBACK")
    a.cypher("CREATE (:X {v: 3})")                    # the retry works
    assert a.cypher("MATCH (n:X) RETURN count(*) AS c")[0]["c"] == 3
    a.close()
    b.close()


def test_other_errors_are_not_retryable(tmp_path):
    db = graphqlite.connect(":memory:")
    with pytest.raises(CypherError) as ei:
        db.cypher("RETURN 9223372036854775807 + 1 AS x")
    assert not ei.value.retryable
    db.close()
