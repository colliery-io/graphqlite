/*
 * runtime/udf_helpers.h
 *    Public symbols for the cache-less helper UDFs that the transform /
 *    executor layer emits into generated SQL. Implementations live in
 *    src/backend/runtime/udf_helpers.c; registration in udf_register.c.
 */

#ifndef RUNTIME_UDF_HELPERS_H
#define RUNTIME_UDF_HELPERS_H

#include "graphqlite_sqlite.h"

/* Equality, containment, subscript, type strictness, bool coercion */
void gql_eq_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_subscript_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_in_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_to_bool_strict_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_to_integer_strict_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_to_float_strict_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_to_string_strict_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_bool_str_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_bool_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);

/* T-0308: cross-type ordering comparison (returns null on type mismatch). */
void gql_order_cmp_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);

/* Order key + namespace/timezone extractors */
void gql_order_key_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_extract_ns_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_strip_tz_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_extract_tz_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);

/* Duration arithmetic + temporal +/- */
void gql_duration_from_total_ns_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_duration_in_days_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_duration_in_months_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_duration_in_seconds_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_duration_calendar_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_tz_offset_for_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_dyn_add_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_dyn_sub_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);

/* Temporal value construction + parsing + reflection */
void gql_date_compose_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_time_compose_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_duration_compose_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_duration_parse_iso_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_temporal_field_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_normalize_date_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_normalize_time_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_normalize_datetime_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_temporal_diff_ns_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);

/* Regex + Cypher reflection */
void regexp_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void cypher_validate_func(sqlite3_context *context, int argc, sqlite3_value **argv);

/* Percentile aggregates (M15) — register as aggregates with these
 * step+final pairs. percentileCont = linear interp; percentileDisc =
 * nearest-rank. Second argument is the percentile in [0,1]. */
void gql_percentile_cont_step(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_percentile_cont_final(sqlite3_context *ctx);
void gql_percentile_disc_step(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_percentile_disc_final(sqlite3_context *ctx);

/* JSON constructors that honor GQL_SUBTYPE_BOOLEAN (T-0304). */
void gql_list_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);
void gql_map_func(sqlite3_context *ctx, int argc, sqlite3_value **argv);

#endif /* RUNTIME_UDF_HELPERS_H */
