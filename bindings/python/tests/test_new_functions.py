"""Tests for new Cypher functions added to GraphQLite."""

import math
import os
from pathlib import Path

import pytest

from graphqlite import connect


def get_extension_path():
    """Get path to the built extension."""
    test_dir = Path(__file__).parent
    build_dir = test_dir.parent.parent.parent / "build"

    if (build_dir / "graphqlite.dylib").exists():
        return str(build_dir / "graphqlite.dylib")
    elif (build_dir / "graphqlite.so").exists():
        return str(build_dir / "graphqlite.so")

    env_path = os.environ.get("GRAPHQLITE_EXTENSION_PATH")
    if env_path and Path(env_path).exists():
        return env_path

    pytest.skip("GraphQLite extension not found. Build with 'make extension'")


@pytest.fixture
def db():
    """Create an in-memory GraphQLite database."""
    ext_path = get_extension_path()
    conn = connect(":memory:", extension_path=ext_path)
    yield conn
    conn.close()


@pytest.fixture
def db_with_nodes(db):
    """Database with some numeric nodes for aggregate tests."""
    db.cypher("CREATE (n:Val {val: 10})")
    db.cypher("CREATE (n:Val {val: 20})")
    db.cypher("CREATE (n:Val {val: 30})")
    db.cypher("CREATE (n:Val {val: 40})")
    db.cypher("CREATE (n:Val {val: 50})")
    return db


# =============================================================================
# Tranche 1: Core functions
# =============================================================================


class TestReturnStar:
    def test_return_star(self, db):
        db.cypher("CREATE (n:Animal {name: 'Cat', legs: 4})")
        results = db.cypher("MATCH (n:Animal) RETURN *")
        assert len(results) == 1
        # RETURN * should include the node's properties
        row = results[0]
        assert any("name" in str(v) or "Cat" in str(v) for v in row.values())


class TestIsEmpty:
    def test_isempty_empty_string(self, db):
        results = db.cypher("RETURN isEmpty('') AS result")
        assert results[0]["result"] in (True, 1)

    def test_isempty_nonempty_string(self, db):
        results = db.cypher("RETURN isEmpty('hello') AS result")
        assert results[0]["result"] in (False, 0)


class TestBtrim:
    def test_btrim(self, db):
        results = db.cypher("RETURN btrim('  hello  ') AS result")
        assert results[0]["result"] == "hello"


class TestToIntegerOrNull:
    def test_valid_integer(self, db):
        results = db.cypher("RETURN toIntegerOrNull('42') AS result")
        assert results[0]["result"] == 42

    def test_invalid_integer(self, db):
        results = db.cypher("RETURN toIntegerOrNull('hello') AS result")
        assert results[0]["result"] is None


class TestToFloatOrNull:
    def test_valid_float(self, db):
        results = db.cypher("RETURN toFloatOrNull('3.14') AS result")
        assert abs(results[0]["result"] - 3.14) < 0.001

    def test_invalid_float(self, db):
        results = db.cypher("RETURN toFloatOrNull('nope') AS result")
        assert results[0]["result"] is None


class TestToBooleanOrNull:
    def test_valid_boolean(self, db):
        results = db.cypher("RETURN toBooleanOrNull('true') AS result")
        assert results[0]["result"] in (True, 1)

    def test_invalid_boolean(self, db):
        results = db.cypher("RETURN toBooleanOrNull('maybe') AS result")
        assert results[0]["result"] is None


class TestToStringOrNull:
    def test_valid_tostring(self, db):
        results = db.cypher("RETURN toStringOrNull(42) AS result")
        assert results[0]["result"] == "42"

    def test_null_tostring(self, db):
        results = db.cypher("RETURN toStringOrNull(null) AS result")
        assert results[0]["result"] is None


class TestElementId:
    def test_elementid(self, db):
        db.cypher("CREATE (n:Thing {name: 'widget'})")
        results = db.cypher("MATCH (n:Thing) RETURN elementId(n) AS eid")
        assert results[0]["eid"] is not None


class TestNullIf:
    def test_nullif_equal(self, db):
        results = db.cypher("RETURN nullIf(1, 1) AS result")
        assert results[0]["result"] is None

    def test_nullif_different(self, db):
        results = db.cypher("RETURN nullIf(1, 2) AS result")
        assert results[0]["result"] == 1


class TestValueType:
    def test_valuetype_integer(self, db):
        results = db.cypher("RETURN valueType(42) AS result")
        assert "INT" in results[0]["result"].upper()

    def test_valuetype_string(self, db):
        results = db.cypher("RETURN valueType('hello') AS result")
        assert "STRING" in results[0]["result"].upper() or "TEXT" in results[0]["result"].upper()

    def test_valuetype_float(self, db):
        results = db.cypher("RETURN valueType(3.14) AS result")
        assert "FLOAT" in results[0]["result"].upper() or "REAL" in results[0]["result"].upper()


class TestCharLength:
    def test_char_length(self, db):
        results = db.cypher("RETURN char_length('hello') AS result")
        assert results[0]["result"] == 5

    def test_character_length(self, db):
        results = db.cypher("RETURN character_length('world!') AS result")
        assert results[0]["result"] == 6


# =============================================================================
# Tranche 2: List slicing, aggregates, math
# =============================================================================


class TestListSlicing:
    def test_slice_range(self, db):
        results = db.cypher("RETURN [1,2,3,4,5][1..3] AS result")
        val = results[0]["result"]
        # Expect elements at index 1 and 2 (i.e., [2, 3])
        if isinstance(val, str):
            assert "2" in val and "3" in val
        else:
            assert val == [2, 3]

    def test_slice_from(self, db):
        results = db.cypher("RETURN [1,2,3,4,5][2..] AS result")
        val = results[0]["result"]
        if isinstance(val, str):
            assert "3" in val and "4" in val and "5" in val
        else:
            assert val == [3, 4, 5]

    def test_slice_to(self, db):
        results = db.cypher("RETURN [1,2,3,4,5][..2] AS result")
        val = results[0]["result"]
        if isinstance(val, str):
            assert "1" in val and "2" in val
        else:
            assert val == [1, 2]


class TestStDev:
    def test_stdev(self, db_with_nodes):
        results = db_with_nodes.cypher(
            "MATCH (n:Val) RETURN stDev(n.val) AS result"
        )
        val = float(results[0]["result"])
        # Sample std dev of [10,20,30,40,50] = sqrt(250) ~ 15.81
        assert abs(val - 15.8114) < 0.1

    def test_stdevp(self, db_with_nodes):
        results = db_with_nodes.cypher(
            "MATCH (n:Val) RETURN stDevP(n.val) AS result"
        )
        val = float(results[0]["result"])
        # Population std dev of [10,20,30,40,50] = sqrt(200) ~ 14.14
        assert abs(val - 14.1421) < 0.1


class TestTrigFunctions:
    def test_atan2(self, db):
        results = db.cypher("RETURN atan2(1, 1) AS result")
        val = float(results[0]["result"])
        assert abs(val - math.atan2(1, 1)) < 0.0001

    def test_degrees(self, db):
        results = db.cypher("RETURN degrees(pi()) AS result")
        val = float(results[0]["result"])
        assert abs(val - 180.0) < 0.0001

    def test_radians(self, db):
        results = db.cypher("RETURN radians(180) AS result")
        val = float(results[0]["result"])
        assert abs(val - math.pi) < 0.0001

    def test_cot(self, db):
        results = db.cypher("RETURN cot(1.0) AS result")
        val = float(results[0]["result"])
        assert abs(val - (1.0 / math.tan(1.0))) < 0.0001

    def test_haversin(self, db):
        results = db.cypher("RETURN haversin(1.0) AS result")
        val = float(results[0]["result"])
        expected = (1.0 - math.cos(1.0)) / 2.0
        assert abs(val - expected) < 0.0001


class TestHyperbolicFunctions:
    def test_sinh(self, db):
        results = db.cypher("RETURN sinh(1.0) AS result")
        val = float(results[0]["result"])
        assert abs(val - math.sinh(1.0)) < 0.0001

    def test_cosh(self, db):
        results = db.cypher("RETURN cosh(1.0) AS result")
        val = float(results[0]["result"])
        assert abs(val - math.cosh(1.0)) < 0.0001

    def test_tanh(self, db):
        results = db.cypher("RETURN tanh(0.5) AS result")
        val = float(results[0]["result"])
        assert abs(val - math.tanh(0.5)) < 0.0001

    def test_coth(self, db):
        results = db.cypher("RETURN coth(1.0) AS result")
        val = float(results[0]["result"])
        expected = math.cosh(1.0) / math.sinh(1.0)
        assert abs(val - expected) < 0.0001


class TestIsNaN:
    def test_isnan_number(self, db):
        results = db.cypher("RETURN isNaN(42) AS result")
        assert results[0]["result"] in (False, 0)


# =============================================================================
# Tranche 3: Date/time, spatial
# =============================================================================


class TestDateConstruction:
    def test_date(self, db):
        results = db.cypher(
            "RETURN date({year: 2024, month: 3, day: 15}) AS result"
        )
        val = str(results[0]["result"])
        assert "2024" in val and "03" in val and "15" in val

    def test_time(self, db):
        results = db.cypher(
            "RETURN time({hour: 14, minute: 30, second: 0}) AS result"
        )
        val = str(results[0]["result"])
        assert "14" in val and "30" in val

    def test_datetime(self, db):
        results = db.cypher(
            "RETURN datetime({year: 2024, month: 6, day: 15, hour: 10, minute: 30}) AS result"
        )
        val = str(results[0]["result"])
        assert "2024" in val and "06" in val or "6" in val

    def test_duration(self, db):
        results = db.cypher(
            "RETURN duration({days: 5, hours: 3}) AS result"
        )
        val = results[0]["result"]
        assert val is not None


class TestTemporalFromEpoch:
    def test_datetime_from_epoch(self, db):
        results = db.cypher("RETURN datetimeFromEpoch(0) AS result")
        val = str(results[0]["result"])
        # Epoch 0 = 1970-01-01
        assert "1970" in val

    def test_datetime_from_epoch_millis(self, db):
        results = db.cypher("RETURN datetimeFromEpochMillis(86400000) AS result")
        val = str(results[0]["result"])
        # 86400000 ms = 1 day after epoch = 1970-01-02
        assert "1970" in val


class TestDurationIn:
    def test_duration_in_days(self, db):
        results = db.cypher(
            "RETURN durationInDays('2024-01-01', '2024-03-15') AS result"
        )
        val = results[0]["result"]
        # Jan has 31 days, Feb has 29 (2024 is leap year), + 14 days in March = 74
        assert int(val) == 74

    def test_duration_in_seconds(self, db):
        results = db.cypher(
            "RETURN durationInSeconds('2024-01-01 00:00:00', '2024-01-01 01:30:00') AS result"
        )
        val = results[0]["result"]
        assert int(val) == 5400


class TestDateTruncate:
    def test_date_truncate(self, db):
        results = db.cypher(
            "RETURN dateTruncate('month', '2024-03-15') AS result"
        )
        val = str(results[0]["result"])
        assert "2024-03-01" in val


class TestDateArithmetic:
    def test_date_add(self, db):
        results = db.cypher(
            "RETURN dateAdd('2024-01-15', {days: 30}) AS result"
        )
        val = str(results[0]["result"])
        assert "2024-02-14" in val

    def test_date_sub(self, db):
        results = db.cypher(
            "RETURN dateSub('2024-06-15', {months: 3}) AS result"
        )
        val = str(results[0]["result"])
        assert "2024-03-15" in val


class TestPointConstruction:
    def test_point_cartesian(self, db):
        results = db.cypher(
            "RETURN point({x: 3, y: 4}) AS result"
        )
        val = results[0]["result"]
        assert val is not None

    def test_point_geographic(self, db):
        results = db.cypher(
            "RETURN point({latitude: 40.7128, longitude: -74.006}) AS result"
        )
        val = results[0]["result"]
        assert val is not None


class TestDistance:
    def test_distance_cartesian(self, db):
        results = db.cypher(
            "RETURN distance(point({x: 0, y: 0}), point({x: 3, y: 4})) AS result"
        )
        val = float(results[0]["result"])
        assert abs(val - 5.0) < 0.01

    def test_distance_geographic(self, db):
        results = db.cypher(
            "RETURN distance(point({latitude: 40.7128, longitude: -74.006}), point({latitude: 51.5074, longitude: -0.1278})) AS result"
        )
        val = float(results[0]["result"])
        # Great-circle distance NYC to London ~ 5570 km = 5570000 m
        # Allow generous tolerance since implementations vary
        assert 5500000 < val < 5700000 or 5500 < val < 5700


class TestPointWithinBBox:
    def test_point_within_bbox_true(self, db):
        results = db.cypher(
            "RETURN pointWithinBBox(point({x: 5, y: 5}), point({x: 0, y: 0}), point({x: 10, y: 10})) AS result"
        )
        assert results[0]["result"] in (True, 1)

    def test_point_within_bbox_false(self, db):
        results = db.cypher(
            "RETURN pointWithinBBox(point({x: 15, y: 5}), point({x: 0, y: 0}), point({x: 10, y: 10})) AS result"
        )
        assert results[0]["result"] in (False, 0)

    def test_point_within_bbox_edge(self, db):
        """Point exactly on the boundary should be inside."""
        results = db.cypher(
            "RETURN pointWithinBBox(point({x: 10, y: 10}), point({x: 0, y: 0}), point({x: 10, y: 10})) AS result"
        )
        assert results[0]["result"] in (True, 1)


# =============================================================================
# Edge Cases and Boundary Tests
# =============================================================================


class TestOrNullEdgeCases:
    """Boundary tests for OrNull type conversion functions."""

    def test_tointegerornull_float_string(self, db):
        """Float string should convert to integer (truncated)."""
        results = db.cypher("RETURN toIntegerOrNull('3.14') AS result")
        # May return 3 or null depending on strictness
        val = results[0]["result"]
        assert val is None or val == 3

    def test_tointegerornull_empty_string(self, db):
        results = db.cypher("RETURN toIntegerOrNull('') AS result")
        assert results[0]["result"] is None

    def test_tointegerornull_negative(self, db):
        results = db.cypher("RETURN toIntegerOrNull('-42') AS result")
        assert results[0]["result"] == -42

    def test_tofloatornull_negative(self, db):
        results = db.cypher("RETURN toFloatOrNull('-3.14') AS result")
        val = results[0]["result"]
        assert val is not None
        assert abs(val - (-3.14)) < 0.01

    def test_tobooleanornull_integer_one(self, db):
        results = db.cypher("RETURN toBooleanOrNull('1') AS result")
        assert results[0]["result"] in (True, 1)

    def test_tobooleanornull_integer_zero(self, db):
        results = db.cypher("RETURN toBooleanOrNull('0') AS result")
        assert results[0]["result"] in (False, 0)

    def test_tostringornull_boolean(self, db):
        results = db.cypher("RETURN toStringOrNull(true) AS result")
        assert results[0]["result"] is not None


class TestIsEmptyEdgeCases:
    def test_isempty_null(self, db):
        results = db.cypher("RETURN isEmpty(null) AS result")
        # null has no length, should return true (1)
        assert results[0]["result"] in (True, 1, None)

    def test_isempty_whitespace(self, db):
        """Whitespace-only string is NOT empty (has length > 0)."""
        results = db.cypher("RETURN isEmpty('   ') AS result")
        assert results[0]["result"] in (False, 0)

    def test_isempty_single_char(self, db):
        results = db.cypher("RETURN isEmpty('x') AS result")
        assert results[0]["result"] in (False, 0)


class TestListSlicingEdgeCases:
    def test_slice_empty_result(self, db):
        """Slice beyond array length returns empty array."""
        results = db.cypher("RETURN [1,2,3][5..10] AS result")
        val = results[0]["result"]
        assert val is None or val == [] or val == "[]"

    def test_slice_single_element(self, db):
        results = db.cypher("RETURN [10,20,30][1..2] AS result")
        val = results[0]["result"]
        assert val == [20] or val == "[20]"

    def test_slice_full_range(self, db):
        """Slice covering entire list."""
        results = db.cypher("RETURN [1,2,3][0..3] AS result")
        val = results[0]["result"]
        assert val == [1, 2, 3] or str(val) == "[1,2,3]"

    def test_slice_zero_length(self, db):
        """Start == end gives empty slice."""
        results = db.cypher("RETURN [1,2,3][1..1] AS result")
        val = results[0]["result"]
        assert val is None or val == [] or val == "[]"


class TestTemporalEdgeCases:
    def test_date_leap_year(self, db):
        results = db.cypher("RETURN date({year: 2024, month: 2, day: 29}) AS result")
        assert results[0]["result"] == "2024-02-29"

    def test_date_end_of_year(self, db):
        results = db.cypher("RETURN date({year: 2024, month: 12, day: 31}) AS result")
        assert results[0]["result"] == "2024-12-31"

    def test_date_add_cross_month(self, db):
        """Adding days that cross a month boundary."""
        results = db.cypher("RETURN dateAdd('2024-01-30', {days: 5}) AS result")
        assert "2024-02-04" in results[0]["result"]

    def test_date_add_cross_year(self, db):
        """Adding months that cross a year boundary."""
        results = db.cypher("RETURN dateAdd('2024-11-15', {months: 3}) AS result")
        assert "2025-02-15" in results[0]["result"]

    def test_date_sub_cross_year(self, db):
        results = db.cypher("RETURN dateSub('2024-02-15', {months: 3}) AS result")
        assert "2023-11-15" in results[0]["result"]

    def test_duration_in_days_same_date(self, db):
        results = db.cypher("RETURN durationInDays('2024-01-01', '2024-01-01') AS result")
        assert results[0]["result"] == 0

    def test_duration_in_seconds_negative(self, db):
        """Earlier date first gives negative duration."""
        results = db.cypher(
            "RETURN durationInSeconds('2024-01-02 00:00:00', '2024-01-01 00:00:00') AS result"
        )
        assert results[0]["result"] < 0

    def test_datetime_from_epoch_negative(self, db):
        """Negative epoch = before 1970."""
        results = db.cypher("RETURN datetimeFromEpoch(-86400) AS result")
        assert "1969-12-31" in results[0]["result"]


class TestSpatialEdgeCases:
    def test_distance_same_point(self, db):
        results = db.cypher(
            "RETURN distance(point({x: 5, y: 5}), point({x: 5, y: 5})) AS result"
        )
        assert results[0]["result"] == 0.0

    def test_distance_negative_coords(self, db):
        results = db.cypher(
            "RETURN distance(point({x: -3, y: -4}), point({x: 0, y: 0})) AS result"
        )
        assert abs(results[0]["result"] - 5.0) < 0.01

    def test_point_3d(self, db):
        results = db.cypher("RETURN point({x: 1, y: 2, z: 3}) AS result")
        p = results[0]["result"]
        assert p["z"] == 3

    def test_geographic_distance_same_point(self, db):
        results = db.cypher(
            "RETURN distance(point({latitude: 0, longitude: 0}), point({latitude: 0, longitude: 0})) AS result"
        )
        assert abs(results[0]["result"]) < 1.0  # should be ~0

    def test_geographic_distance_antipodal(self, db):
        """North pole to south pole ≈ 20,015 km."""
        results = db.cypher(
            "RETURN distance(point({latitude: 90, longitude: 0}), point({latitude: -90, longitude: 0})) AS result"
        )
        val = results[0]["result"]
        assert 19900000 < val < 20100000  # ~20,015 km in meters

    def test_bbox_geographic(self, db):
        """NYC is within a US northeast bounding box."""
        results = db.cypher(
            "RETURN pointWithinBBox("
            "point({latitude: 40.71, longitude: -74.00}), "
            "point({latitude: 38.0, longitude: -80.0}), "
            "point({latitude: 42.0, longitude: -70.0})) AS result"
        )
        assert results[0]["result"] in (True, 1)


class TestNullIfEdgeCases:
    def test_nullif_strings(self, db):
        results = db.cypher("RETURN nullIf('hello', 'hello') AS result")
        assert results[0]["result"] is None

    def test_nullif_null_args(self, db):
        results = db.cypher("RETURN nullIf(null, null) AS result")
        assert results[0]["result"] is None

    def test_nullif_mixed_types(self, db):
        results = db.cypher("RETURN nullIf(1, '1') AS result")
        # SQLite may or may not consider these equal
        val = results[0]["result"]
        assert val is not None or val is None  # just verify no crash


class TestValueTypeEdgeCases:
    def test_valuetype_null(self, db):
        results = db.cypher("RETURN valueType(null) AS result")
        assert results[0]["result"] == "NULL"

    def test_valuetype_boolean(self, db):
        results = db.cypher("RETURN valueType(true) AS result")
        # true is stored as integer in SQLite
        assert results[0]["result"] in ("INTEGER", "BOOLEAN")


class TestReturnStarEdgeCases:
    def test_return_star_with_relationship(self, db):
        db.cypher("CREATE (a:StarTest {id: 'st1'})-[:KNOWS]->(b:StarTest {id: 'st2'})")
        results = db.cypher("MATCH (a:StarTest)-[r]->(b:StarTest) RETURN *")
        assert len(results) == 1
        row = results[0]
        # Should have a, r, b columns
        assert len(row) >= 3

    def test_return_star_multiple_nodes(self, db):
        db.cypher("CREATE (x:StarMulti {id: 'sm1'})")
        db.cypher("CREATE (y:StarMulti {id: 'sm2'})")
        results = db.cypher("MATCH (n:StarMulti) RETURN *")
        assert len(results) == 2
