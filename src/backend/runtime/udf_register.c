/*
 * runtime/udf_register.c
 *    Registers the cache-less helper UDFs (defined in udf_helpers.c) on
 *    a sqlite3 connection. Called from sqlite3_graphqlite_init for the
 *    loadable extension, and from cypher_executor_create so direct C-API
 *    consumers (e.g. the unit-test harness) also get the UDFs that
 *    transformed Cypher queries emit.
 *
 *    Moved from src/extension.c (I-0040 M7). Replaces the
 *    extern int graphqlite_register_helper_udfs(sqlite3 *db);
 *    forward-decl pattern with a proper header (runtime/udf_register.h).
 */

#include "graphqlite_sqlite.h"
#include "runtime/udf_register.h"
#include "runtime/udf_helpers.h"


#ifdef _WIN32
__declspec(dllexport)
#endif
/*
 * Register the cache-less helper UDFs that the transform/executor layer
 * emits into SQL (regexp, _gql_bool, _gql_normalize_date, _gql_in,
 * _gql_dyn_add, etc.). This is called both by sqlite3_graphqlite_init for
 * the loadable extension, and from cypher_executor_create so that any C
 * API consumer (notably the unit-test harness) automatically has the UDFs
 * a transformed Cypher query needs.
 *
 * Cache-bound functions (cypher, gql_load_graph, etc.) stay in
 * sqlite3_graphqlite_init because they need the per-connection cache and
 * are SQL-only entrypoints (irrelevant to direct C-API users).
 */
int graphqlite_register_helper_udfs(sqlite3 *db)
{
  int rc;

  rc = sqlite3_create_function(db, "regexp", 2, SQLITE_UTF8, 0,
                         regexp_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_extract_ns", 1, SQLITE_UTF8, 0,
                         gql_extract_ns_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_bool", 1,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0,
                         gql_bool_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  /* _gql_bool_str tags its result with the boolean subtype (I-0040
   * M13), which requires the SQLITE_RESULT_SUBTYPE function property. */
  rc = sqlite3_create_function(db, "_gql_bool_str", 1,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC | SQLITE_RESULT_SUBTYPE, 0,
                         gql_bool_str_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_to_bool_strict", 1,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0,
                         gql_to_bool_strict_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_to_int_strict", 1,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0,
                         gql_to_integer_strict_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_to_float_strict", 1,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0,
                         gql_to_float_strict_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_to_string_strict", 1,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0,
                         gql_to_string_strict_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_order_key", 1,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0,
                         gql_order_key_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_in", 2,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0,
                         gql_in_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  /* Dynamic property lookup by runtime key (Graph7 n['na'+'me']). */
  rc = sqlite3_create_function(db, "_gql_node_prop", 2,
                         SQLITE_UTF8, 0, gql_node_prop_func, 0, 0);
  if (rc != SQLITE_OK) return rc;
  rc = sqlite3_create_function(db, "_gql_edge_prop", 2,
                         SQLITE_UTF8, 0, gql_edge_prop_func, 0, 0);
  if (rc != SQLITE_OK) return rc;
  rc = sqlite3_create_function(db, "_gql_dyn_prop", 2,
                         SQLITE_UTF8, 0, gql_dyn_prop_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  /* Cypher-orderability min()/max() aggregates (step + final, no scalar fn). */
  rc = sqlite3_create_function(db, "_gql_min", 1,
                         SQLITE_UTF8, 0,
                         0, gql_min_step, gql_min_final);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_max", 1,
                         SQLITE_UTF8, 0,
                         0, gql_max_step, gql_max_final);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_subscript", 2,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0,
                         gql_subscript_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_eq", 2,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0,
                         gql_eq_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  /* T-0308: ordering comparison with Cypher type-class rules. */
  rc = sqlite3_create_function(db, "_gql_order_cmp", 3,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0,
                         gql_order_cmp_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_extract_tz", 1, SQLITE_UTF8, 0,
                         gql_extract_tz_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_strip_tz", 1, SQLITE_UTF8, 0,
                         gql_strip_tz_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_normalize_date", 1, SQLITE_UTF8, 0,
                         gql_normalize_date_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_normalize_time", 1, SQLITE_UTF8, 0,
                         gql_normalize_time_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_normalize_datetime", 1, SQLITE_UTF8, 0,
                         gql_normalize_datetime_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_duration_compose", 10, SQLITE_UTF8, 0,
                         gql_duration_compose_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_duration_parse_iso", 1, SQLITE_UTF8, 0,
                         gql_duration_parse_iso_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_temporal_field", 2, SQLITE_UTF8, 0,
                         gql_temporal_field_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_tz_offset_for", 2, SQLITE_UTF8, 0,
                         gql_tz_offset_for_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_dyn_add", 2, SQLITE_UTF8, 0,
                         gql_dyn_add_func, 0, 0);
  if (rc != SQLITE_OK) return rc;
  rc = sqlite3_create_function(db, "_gql_dyn_sub", 2, SQLITE_UTF8, 0,
                         gql_dyn_sub_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_duration_from_total_ns", 1, SQLITE_UTF8, 0,
                         gql_duration_from_total_ns_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_temporal_diff_ns", 2, SQLITE_UTF8, 0,
                         gql_temporal_diff_ns_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_duration_calendar", 2, SQLITE_UTF8, 0,
                         gql_duration_calendar_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_duration_in_days", 2, SQLITE_UTF8, 0,
                         gql_duration_in_days_func, 0, 0);
  if (rc != SQLITE_OK) return rc;
  rc = sqlite3_create_function(db, "_gql_duration_in_months", 2, SQLITE_UTF8, 0,
                         gql_duration_in_months_func, 0, 0);
  if (rc != SQLITE_OK) return rc;
  rc = sqlite3_create_function(db, "_gql_duration_in_seconds", 2, SQLITE_UTF8, 0,
                         gql_duration_in_seconds_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_date_compose", 10, SQLITE_UTF8, 0,
                         gql_date_compose_func, 0, 0);
  if (rc != SQLITE_OK) return rc;
  rc = sqlite3_create_function(db, "_gql_time_compose", 12, SQLITE_UTF8, 0,
                         gql_time_compose_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "cypher_validate", 1, SQLITE_UTF8, 0,
                         cypher_validate_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  /* Percentile aggregates (M15). Registered under both the Cypher
   * names (percentileCont/percentileDisc) and an internal SQL alias
   * (_gql_percentile_cont/_disc) so the transform layer can choose. */
  rc = sqlite3_create_function(db, "percentileCont", 2, SQLITE_UTF8, 0,
                         0, gql_percentile_cont_step, gql_percentile_cont_final);
  if (rc != SQLITE_OK) return rc;
  rc = sqlite3_create_function(db, "percentileDisc", 2, SQLITE_UTF8, 0,
                         0, gql_percentile_disc_step, gql_percentile_disc_final);
  if (rc != SQLITE_OK) return rc;
  rc = sqlite3_create_function(db, "_gql_percentile_cont", 2, SQLITE_UTF8, 0,
                         0, gql_percentile_cont_step, gql_percentile_cont_final);
  if (rc != SQLITE_OK) return rc;
  rc = sqlite3_create_function(db, "_gql_percentile_disc", 2, SQLITE_UTF8, 0,
                         0, gql_percentile_disc_step, gql_percentile_disc_final);
  if (rc != SQLITE_OK) return rc;

  /* T-0304: JSON array/object constructors that honor GQL_SUBTYPE_BOOLEAN
   * (and tag their own output with the JSON subtype for proper nesting).
   * Variadic — nargs = -1. */
  rc = sqlite3_create_function(db, "_gql_list", -1,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC | SQLITE_RESULT_SUBTYPE, 0,
                         gql_list_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_map", -1,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC | SQLITE_RESULT_SUBTYPE, 0,
                         gql_map_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  return SQLITE_OK;
}

