"""Regression tests for #134, #135 and #136 (0.10.0).

#134: writes after a rollback lost or renamed properties; a write query
that failed kept its earlier writes.
#135: FOREACH skipped MERGE/REMOVE/DELETE bodies, ignored an outer MATCH
and a $param list; path functions of variable-length paths; boolean vs
number equality.
#136: UNWIND write pipelines (unmatched nodes updated, UNWIND/WITH values
unbound in SET, values stored as the wrong type, null SET kept the old
value), bulk insert of None/lists/maps.
"""

import pytest

from graphqlite import Graph

from test_graph import get_extension_path


@pytest.fixture
def g():
    with Graph(":memory:", extension_path=get_extension_path()) as graph:
        yield graph


def props(g, node_id):
    node = g.get_node(node_id)
    return None if node is None else node["properties"]


# ---------------------------------------------------------------- #134


def test_rollback_then_write(g):
    db = g.connection.sqlite_connection
    g.upsert_node("safe", {}, "Item")
    db.execute("BEGIN")
    g.upsert_node("discard", {"flag": True}, "Item")
    db.rollback()
    g.upsert_node("b", {"other": 123}, "Item")
    g.upsert_node("a", {"flag": False}, "Item")
    assert props(g, "a") == {"id": "a", "flag": False}
    assert props(g, "b") == {"id": "b", "other": 123}


def test_failed_write_query_leaves_nothing(g):
    with pytest.raises(Exception):
        g.query("CREATE (n:Item {id: 'a'})-[:R]->(:B) WITH n DELETE n")
    assert g.stats()["node_count"] == 0
    assert g.connection.sqlite_connection.in_transaction is False


def test_failed_write_query_in_open_transaction(g):
    db = g.connection.sqlite_connection
    g.upsert_node("keep", {}, "Item")
    db.execute("BEGIN")
    with pytest.raises(Exception):
        g.query("CREATE (n:Item {id: 'a'})-[:R]->(:B) WITH n DELETE n")
    assert db.in_transaction is True
    db.commit()
    assert g.stats()["node_count"] == 1


def test_with_create_set(g):
    g.query("WITH 42 AS x CREATE (n:Item {id: 'a'}) SET n.value = x")
    assert props(g, "a") == {"id": "a", "value": 42}


# ---------------------------------------------------------------- #135


def test_foreach_merge(g):
    g.query("FOREACH (x IN [1, 2] | MERGE (:Item {value: x}))")
    assert g.stats()["node_count"] == 2


@pytest.mark.parametrize(
    "command, expected",
    [
        ("REMOVE n.value", {"id": "a"}),
        ("DELETE n", None),
        ("SET n.value = x", {"id": "a", "value": 1}),
    ],
)
def test_foreach_body_sees_outer_match(g, command, expected):
    g.upsert_node("a", {"value": 10}, "Item")
    g.query(f"MATCH (n:Item {{id: 'a'}}) FOREACH (x IN [1] | {command})")
    assert props(g, "a") == expected


def test_foreach_param_list(g):
    g.query("FOREACH (x IN $xs | CREATE (:Item {value: x}))", {"xs": [1, 2]})
    rows = g.query("MATCH (n:Item) RETURN n.value AS v ORDER BY v")
    assert [r["v"] for r in rows] == [1, 2]


PATH_SETUP = (
    "CREATE (a:Item {id: 'a', name: 'A'}), (b:Item {name: 'B'}), (c:Item {id: 'c', name: 'C'}),"
    " (a)-[:LINK {cost: 1.0}]->(b), (b)-[:LINK {cost: 2.0}]->(c)"
)


def test_variable_length_path_functions(g):
    g.query(PATH_SETUP)
    rows = g.query(
        "MATCH p = (a:Item {id: 'a'})-[:LINK*1..2]->(c:Item {id: 'c'}) "
        "RETURN length(p) AS hops, size(nodes(p)) AS nodes, [n IN nodes(p) | n.name] AS names, "
        "[r IN relationships(p) | r.cost] AS costs"
    )
    assert list(rows) == [{"hops": 2, "nodes": 3, "names": ["A", "B", "C"], "costs": [1.0, 2.0]}]


def test_fixed_path_comprehensions(g):
    g.query(PATH_SETUP)
    rows = g.query(
        "MATCH p = (a:Item {id: 'a'})-[:LINK]->(b:Item)-[:LINK]->(c:Item {id: 'c'}) "
        "RETURN [n IN nodes(p) | n.name] AS names, [r IN relationships(p) | r.cost] AS costs"
    )
    assert list(rows) == [{"names": ["A", "B", "C"], "costs": [1.0, 2.0]}]


def test_shortest_path_functions(g):
    g.query(PATH_SETUP)
    rows = g.query(
        "MATCH p = shortestPath((a:Item {id: 'a'})-[:LINK*]->(c:Item {id: 'c'})) "
        "RETURN length(p) AS hops, size(nodes(p)) AS nodes"
    )
    assert list(rows) == [{"hops": 2, "nodes": 3}]


def test_boolean_is_not_a_number(g):
    rows = g.query(
        "RETURN true = 1 AS eq, true IN [1] AS member, false = 0 AS eq0, "
        "false IN [0] AS member0, 1 IN [true] AS rev, true IN [true] AS same"
    )
    assert list(rows) == [
        {"eq": False, "member": False, "eq0": False, "member0": False, "rev": False, "same": True}
    ]


# ---------------------------------------------------------------- #136


def test_unwind_match_set_changes_only_matched(g):
    g.query("CREATE (:Item {id: 'a', value: 1}), (:Item {id: 'b', value: 2})")
    g.query(
        "UNWIND $rows AS row MATCH (n:Item {id: row.id}) SET n.value = $value",
        {"rows": [{"id": "a"}], "value": 7},
    )
    rows = g.query("MATCH (n:Item) RETURN n.id AS id, n.value AS value ORDER BY id")
    assert list(rows) == [{"id": "a", "value": 7}, {"id": "b", "value": 2}]


@pytest.mark.parametrize("value", [None, [1, 2], [False, True], {"k": 1}, True, 1.5])
def test_bulk_insert_keeps_types(g, value):
    g.insert_nodes_bulk([("a", {"value": value}, "Item")])
    assert props(g, "a").get("value") == value


def test_unwind_match_with_set_return(g):
    g.upsert_node("a", {"value": 10}, "Item")
    rows = g.query(
        "UNWIND $rows AS row MATCH (n:Item {id: row.id}) WITH n, row SET n.value = $value RETURN n.id AS id",
        {"rows": [{"id": "a"}], "value": 7},
    )
    assert list(rows) == [{"id": "a"}]
    assert props(g, "a")["value"] == 7


def test_match_unwind_with_where_set(g):
    g.upsert_node("a", {"value": 10}, "Item")
    g.query(
        "MATCH (n:Item) UNWIND $rows AS row WITH n, row WHERE n.id = row.id SET n.value = row.value",
        {"rows": [{"id": "a", "value": 7}]},
    )
    assert props(g, "a")["value"] == 7


@pytest.mark.parametrize(
    "query, params",
    [
        ("UNWIND $rows AS row MATCH (n:Item {id: row.id}) SET n.value = row.value",
         {"rows": [{"id": "a", "value": 1}]}),
        ("WITH $value AS value MATCH (n:Item {id: 'a'}) SET n.value = value", {"value": 1}),
        ("WITH $row AS row MATCH (n:Item {id: 'a'}) SET n.value = row.value", {"row": {"value": 1}}),
    ],
)
def test_set_reads_unwind_and_with_values(g, query, params):
    g.upsert_node("a", {}, "Item")
    g.query(query, params)
    assert props(g, "a") == {"id": "a", "value": 1}


@pytest.mark.parametrize("verb", ["CREATE", "MERGE"])
@pytest.mark.parametrize("suffix", ["", " RETURN r.value AS value"])
def test_relationship_set_reads_unwind_value(g, verb, suffix):
    g.query("CREATE (:A {id: 'a'}), (:B {id: 'b'})")
    g.query(
        f"UNWIND $rows AS row MATCH (a:A), (b:B) {verb} (a)-[r:LINK]->(b) SET r.value = row.value{suffix}",
        {"rows": [{"value": 1}]},
    )
    assert list(g.query("MATCH ()-[r:LINK]->() RETURN r.value AS v")) == [{"v": 1}]


@pytest.mark.parametrize("existing", [False, True])
def test_unwind_merge_set_without_return(g, existing):
    if existing:
        g.upsert_node("a", {"value": 5}, "Item")
    g.query(
        "UNWIND $rows AS row MERGE (n:Item {id: row.id}) SET n.value = row.value",
        {"rows": [{"id": "a", "value": 1}]},
    )
    assert props(g, "a") == {"id": "a", "value": 1}


TYPED_QUERIES = [
    "UNWIND $rows AS row CREATE (n:Item) SET n.id = row.id, n.value = row.value",
    "UNWIND $rows AS row MERGE (n:Item {id: row.id}) SET n.value = row.value RETURN n.value",
    "UNWIND $rows AS row MERGE (n:Item {id: row.id}) ON CREATE SET n.value = row.value",
    "UNWIND $rows AS row MERGE (n:Item {id: row.id}) SET n.value = row.value",
]


@pytest.mark.parametrize("query", TYPED_QUERIES)
@pytest.mark.parametrize("value", [False, True, [1, 2], [False, True], {"k": 1}])
def test_row_field_set_keeps_types(g, query, value):
    g.query(query, {"rows": [{"id": "a", "value": value}]})
    assert props(g, "a").get("value") == value


def test_on_match_set_keeps_types(g):
    g.upsert_node("a", {"value": 10}, "Item")
    g.query(
        "UNWIND $rows AS row MERGE (n:Item {id: row.id}) ON MATCH SET n.value = row.value",
        {"rows": [{"id": "a", "value": [True]}]},
    )
    assert props(g, "a")["value"] == [True]


@pytest.mark.parametrize(
    "query",
    [
        "CREATE (:Item {id: 'a', value: $value})",
        "WITH $value AS value CREATE (:Item {id: 'a', value: value})",
    ],
)
def test_map_property_stays_a_map(g, query):
    g.query(query, {"value": {"k": 1}})
    assert props(g, "a")["value"] == {"k": 1}


@pytest.mark.parametrize(
    "query",
    [
        "UNWIND $rows AS row MATCH (n:Item {id: 'a'}) SET n.value = $value",
        "UNWIND $rows AS row MERGE (n:Item {id: row.id}) SET n.value = row.value RETURN n.value",
        "UNWIND $rows AS row MERGE (n:Item {id: row.id}) ON MATCH SET n.value = row.value",
        "UNWIND $rows AS row MERGE (n:Item {id: row.id}) SET n.value = row.value",
        "MATCH (n:Item {id: 'a'}) SET n.value = $value",
    ],
)
def test_null_set_removes_property(g, query):
    g.upsert_node("a", {"value": 10}, "Item")
    g.query(query, {"value": None, "rows": [{"id": "a", "value": None}]})
    assert "value" not in props(g, "a")


@pytest.mark.parametrize(
    "query",
    [
        "UNWIND $rows AS row CREATE (n:Item {id: row.id, value: row.value}) RETURN n.value AS v",
        "UNWIND $rows AS row CREATE (n:Item) SET n.id = row.id, n.value = row.value RETURN n.value AS v",
    ],
)
def test_unwind_param_create_return(g, query):
    rows = g.query(query, {"rows": [{"id": "a", "value": 1}]})
    assert list(rows) == [{"v": 1}]
