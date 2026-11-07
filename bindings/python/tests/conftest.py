"""Shared test hooks.

GQLITE-T-0525: after every test, each GraphQLite connection the test opened
and did not close is checked with gql_check_storage(); a test that leaves an
inconsistent stored graph fails, even if its own assertions passed. Set
GRAPHQLITE_SKIP_STORAGE_CHECK=1 to turn the check off.
"""

import os
import sqlite3
import pytest

import graphqlite.connection as _gc

# Strong references until teardown: a test's connection is otherwise
# collected when the test function returns, before it can be checked.
_opened: "list[_gc.Connection]" = []
_orig_init = _gc.Connection.__init__


def _tracking_init(self, *args, **kwargs):
    _orig_init(self, *args, **kwargs)
    _opened.append(self)


_gc.Connection.__init__ = _tracking_init


def _problems(conn: "_gc.Connection"):
    try:
        raw = conn.sqlite_connection.execute("SELECT gql_check_storage()").fetchone()[0]
    except sqlite3.ProgrammingError:      # closed connection
        return None
    except sqlite3.OperationalError as e:  # an extension without the checker
        return None if "no such function" in str(e) else str(e)
    return raw if raw and raw != "[]" else None


@pytest.fixture(autouse=True)
def _storage_is_consistent():
    _opened.clear()
    yield
    if os.environ.get("GRAPHQLITE_SKIP_STORAGE_CHECK"):
        return
    bad = [p for p in (_problems(c) for c in list(_opened)) if p]
    _opened.clear()
    if bad:
        pytest.fail(f"stored graph inconsistent after the test: {bad}", pytrace=False)
