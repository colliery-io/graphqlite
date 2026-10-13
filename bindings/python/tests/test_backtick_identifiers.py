"""Backtick-quoted identifiers work in every slot.

The name inside the backticks is the name: it comes back as the result key
and is stored as the label, type or property key. #128 point 4 found
aliases with a space or a dash reaching SQL unquoted (GQLITE-T-0399).
"""

import pytest

from graphqlite import Graph

from test_graph import get_extension_path

# name as written between backticks -> the name it stands for
NAMES = {
    "plain": ("x", "x"),
    "space": ("node id", "node id"),
    "dash": ("node-id", "node-id"),
    "dot": ("a.b", "a.b"),
    "backtick": ("a``b", "a`b"),
    "double_quote": ('q"q', 'q"q'),
    "single_quote": ("it's", "it's"),
    "keyword": ("select", "select"),
    "cypher_keyword": ("order", "order"),
    "leading_digit": ("1abc", "1abc"),
    "unicode": ("ünï", "ünï"),
    "brackets": ("a[0]", "a[0]"),
    "semicolon": ("a;b", "a;b"),
}

SLOTS = {
    "return_alias": lambda g, q, n: (
        g.query(f"RETURN 1 AS {q}"), [{n: 1}]),
    "return_property_alias": lambda g, q, n: (
        g.query(f"CREATE (:L {{k: 'a'}})") and None or
        g.query(f"MATCH (m:L) RETURN m.k AS {q}"), [{n: "a"}]),
    "with_alias": lambda g, q, n: (
        g.query(f"WITH 1 AS {q} RETURN {q}"), [{n: 1}]),
    "with_alias_where": lambda g, q, n: (
        g.query(f"UNWIND [1, 2] AS i WITH i AS {q} WHERE {q} > 1 RETURN {q}"), [{n: 2}]),
    "with_alias_order": lambda g, q, n: (
        g.query(f"UNWIND [2, 1] AS i WITH i AS {q} RETURN {q} ORDER BY {q}"), [{n: 1}, {n: 2}]),
    "unwind_variable": lambda g, q, n: (
        g.query(f"UNWIND [1] AS {q} RETURN {q}"), [{n: 1}]),
    "aggregate_alias": lambda g, q, n: (
        g.query(f"UNWIND [1, 2] AS i RETURN count(i) AS {q}"), [{n: 2}]),
    "node_variable": lambda g, q, n: (
        g.query("CREATE (:L {k: 'a'})") and None or
        g.query(f"MATCH ({q}:L) RETURN {q}.k AS k"), [{"k": "a"}]),
    "label": lambda g, q, n: (
        g.query(f"CREATE (:{q})") and None or
        g.query(f"MATCH (m:{q}) RETURN labels(m) AS l"), [{"l": [n]}]),
    "relationship_type": lambda g, q, n: (
        g.query(f"CREATE (:A)-[:{q}]->(:B)") and None or
        g.query(f"MATCH ()-[r:{q}]->() RETURN type(r) AS t"), [{"t": n}]),
    "property_key": lambda g, q, n: (
        g.query(f"CREATE (:L {{{q}: 1}})") and None or
        g.query(f"MATCH (m:L) RETURN m.{q} AS v, keys(m) AS k"), [{"v": 1, "k": [n]}]),
    "set_property_key": lambda g, q, n: (
        g.query("CREATE (:L)") and None or
        g.query(f"MATCH (m:L) SET m.{q} = 1") and None or
        g.query(f"MATCH (m:L) RETURN m.{q} AS v"), [{"v": 1}]),
    "match_inline_key": lambda g, q, n: (
        g.query(f"CREATE (:L {{{q}: 1}}), (:L {{{q}: 2}})") and None or
        g.query(f"MATCH (m:L {{{q}: 1}}) RETURN count(m) AS c"), [{"c": 1}]),
    "unwind_property_key": lambda g, q, n: (
        g.query(f"CREATE (:L {{{q}: [1, 2]}})") and None or
        g.query(f"MATCH (m:L) UNWIND m.{q} AS x RETURN x"), [{"x": 1}, {"x": 2}]),
    "nested_map_key": lambda g, q, n: (
        g.query(f"CREATE (:L {{m: {{{q}: true}}}})") and None or
        g.query(f"MATCH (m:L) RETURN m.m.{q} AS v"), [{"v": True}]),
    "unwind_map_key": lambda g, q, n: (
        g.query(f"UNWIND [{{{q}: false}}] AS r RETURN r.{q} AS v"), [{"v": False}]),
    "aggregate_property_key": lambda g, q, n: (
        g.query(f"CREATE (:L {{{q}: 2}}), (:L {{{q}: 3}})") and None or
        g.query(f"MATCH (m:L) RETURN sum(m.{q}) AS v"), [{"v": 5}]),
    "iter_query_alias": lambda g, q, n: (
        list(g.iter_query(f"RETURN 1 AS {q}")), [{n: 1}]),
    "map_key": lambda g, q, n: (
        g.query(f"RETURN {{{q}: 1}} AS m"), [{"m": {n: 1}}]),
}


@pytest.mark.parametrize("name", list(NAMES))
@pytest.mark.parametrize("slot", list(SLOTS))
def test_backtick_identifier(slot, name):
    raw, plain = NAMES[name]
    g = Graph(":memory:", extension_path=get_extension_path())
    try:
        got, want = SLOTS[slot](g, f"`{raw}`", plain)
        assert got == want
    finally:
        g.close()
