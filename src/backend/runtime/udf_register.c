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

#include <stdbool.h>
#include <stdlib.h>
#include "graphqlite_sqlite.h"
#include "runtime/udf_register.h"
#include "runtime/udf_helpers.h"
#include "executor/graph_algorithms.h"

/* _gql_label_propagation(iterations): the C label propagation as JSON
 * text, [{node_id, user_id, community}, ...]. The SQL forms of
 * labelPropagation / communityOf / communityMembers / communityCount read
 * it, so every form gives the same communities (GQLITE-T-0383: the
 * unrolled SQL version updated labels synchronously and oscillated). */
static void gql_label_propagation_func(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  int iterations = argc > 0 ? sqlite3_value_int(argv[0]) : 10;
  if (iterations < 1) iterations = 1;
  graph_algo_result *r = execute_label_propagation(sqlite3_context_db_handle(context), NULL, iterations);
  if (!r) { sqlite3_result_error_nomem(context); return; }
  if (!r->success) {
    sqlite3_result_error(context, r->error_message ? r->error_message : "labelPropagation failed", -1);
  } else {
    sqlite3_result_text(context, r->json_result ? r->json_result : "[]", -1, SQLITE_TRANSIENT);
  }
  graph_algo_result_free(r);
}


/* _gql_personalized_pagerank(seeds_json, damping, iterations): personalized
 * PageRank as JSON text, [{node_id, score}, ...] by score, highest first.
 * The same steps as the SQL form it replaces: seeds start at 1/|seeds|;
 * each step a node gets (1 - d)/|seeds| if it is a seed, plus d times the
 * sum of score(m)/outdeg(m) over its in-edges; scores are normalized to sum
 * to 1. The SQL form was one CTE per step, and SQLite 3.54 counts that
 * chain against SQLITE_LIMIT_EXPR_DEPTH ("VIEWs and/or subqueries nested
 * too deep" at 100 steps). */
static int ppr_index_of(const sqlite3_int64 *ids, int n, sqlite3_int64 id)
{
  int lo = 0, hi = n - 1;
  while (lo <= hi) {
    int mid = lo + (hi - lo) / 2;
    if (ids[mid] == id) return mid;
    if (ids[mid] < id) lo = mid + 1; else hi = mid - 1;
  }
  return -1;
}

typedef struct { sqlite3_int64 id; double score; } ppr_out;

static int ppr_out_cmp(const void *a, const void *b)
{
  const ppr_out *x = (const ppr_out *)a, *y = (const ppr_out *)b;
  if (x->score > y->score) return -1;
  if (x->score < y->score) return 1;
  return x->id < y->id ? -1 : (x->id > y->id ? 1 : 0);
}

static void gql_personalized_pagerank_func(sqlite3_context *context, int argc, sqlite3_value **argv)
{
  sqlite3 *db = sqlite3_context_db_handle(context);
  const char *seeds = argc > 0 ? (const char *)sqlite3_value_text(argv[0]) : NULL;
  double damping = argc > 1 ? sqlite3_value_double(argv[1]) : 0.85;
  int iterations = argc > 2 ? sqlite3_value_int(argv[2]) : 20;
  if (iterations < 1) iterations = 1;
  /* The SQL form wrote both factors with 4 decimals. */
  char buf[64];
  sqlite3_snprintf(sizeof(buf), buf, "%.4f", damping);
  double d = strtod(buf, NULL);
  sqlite3_snprintf(sizeof(buf), buf, "%.4f", 1.0 - damping);
  double teleport_total = strtod(buf, NULL);

  sqlite3_stmt *st = NULL;
  sqlite3_int64 *ids = NULL; int n = 0, cap = 0;
  if (sqlite3_prepare_v2(db, "SELECT id FROM nodes ORDER BY id", -1, &st, NULL) != SQLITE_OK) goto fail;
  while (sqlite3_step(st) == SQLITE_ROW) {
    if (n == cap) {
      cap = cap ? cap * 2 : 64;
      sqlite3_int64 *g = realloc(ids, (size_t)cap * sizeof(*ids));
      if (!g) goto nomem;
      ids = g;
    }
    ids[n++] = sqlite3_column_int64(st, 0);
  }
  sqlite3_finalize(st); st = NULL;

  bool *is_seed = calloc((size_t)(n > 0 ? n : 1), sizeof(bool));
  double *outdeg = calloc((size_t)(n > 0 ? n : 1), sizeof(double));
  double *pr = calloc((size_t)(n > 0 ? n : 1), sizeof(double));
  double *nx = calloc((size_t)(n > 0 ? n : 1), sizeof(double));
  int *esrc = NULL, *etgt = NULL; int ne = 0, ecap = 0;
  double seed_count = 0;
  if (!is_seed || !outdeg || !pr || !nx) goto nomem2;

  if (seeds) {
    if (sqlite3_prepare_v2(db, "SELECT value FROM json_each(?1)", -1, &st, NULL) != SQLITE_OK) goto fail2;
    sqlite3_bind_text(st, 1, seeds, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW) {
      seed_count += 1;
      if (sqlite3_column_type(st, 0) == SQLITE_INTEGER) {
        int k = ppr_index_of(ids, n, sqlite3_column_int64(st, 0));
        if (k >= 0) is_seed[k] = true;
      }
    }
    sqlite3_finalize(st); st = NULL;
  }

  if (sqlite3_prepare_v2(db, "SELECT source_id, target_id FROM edges", -1, &st, NULL) != SQLITE_OK) goto fail2;
  while (sqlite3_step(st) == SQLITE_ROW) {
    int a = ppr_index_of(ids, n, sqlite3_column_int64(st, 0));
    int b = ppr_index_of(ids, n, sqlite3_column_int64(st, 1));
    if (a < 0 || b < 0) continue;
    if (ne == ecap) {
      ecap = ecap ? ecap * 2 : 128;
      int *gs = realloc(esrc, (size_t)ecap * sizeof(int));
      if (!gs) goto nomem2;
      esrc = gs;
      int *gt = realloc(etgt, (size_t)ecap * sizeof(int));
      if (!gt) goto nomem2;
      etgt = gt;
    }
    esrc[ne] = a; etgt[ne] = b; ne++;
    outdeg[a] += 1;
  }
  sqlite3_finalize(st); st = NULL;

  for (int i = 0; i < n; i++) pr[i] = (is_seed[i] && seed_count > 0) ? 1.0 / seed_count : 0.0;
  for (int it = 0; it < iterations; it++) {
    for (int i = 0; i < n; i++)
      nx[i] = (is_seed[i] && seed_count > 0) ? teleport_total / seed_count : 0.0;
    for (int e = 0; e < ne; e++) nx[etgt[e]] += d * (pr[esrc[e]] / outdeg[esrc[e]]);
    double *t = pr; pr = nx; nx = t;
  }

  double sum = 0;
  for (int i = 0; i < n; i++) sum += pr[i];
  ppr_out *out = calloc((size_t)(n > 0 ? n : 1), sizeof(ppr_out));
  if (!out) goto nomem2;
  for (int i = 0; i < n; i++) { out[i].id = ids[i]; out[i].score = pr[i]; }
  qsort(out, (size_t)n, sizeof(ppr_out), ppr_out_cmp);

  sqlite3_str *js = sqlite3_str_new(db);
  sqlite3_str_appendchar(js, 1, '[');
  for (int i = 0; i < n; i++) {
    if (i) sqlite3_str_appendchar(js, 1, ',');
    if (sum != 0)
      sqlite3_str_appendf(js, "{\"node_id\":%lld,\"score\":%!.15g}", out[i].id, out[i].score / sum);
    else
      sqlite3_str_appendf(js, "{\"node_id\":%lld,\"score\":null}", out[i].id);
  }
  sqlite3_str_appendchar(js, 1, ']');
  free(out);
  char *text = sqlite3_str_finish(js);
  if (!text) goto nomem2;
  sqlite3_result_text(context, text, -1, sqlite3_free);
  free(ids); free(is_seed); free(outdeg); free(pr); free(nx); free(esrc); free(etgt);
  return;

nomem2:
  free(is_seed); free(outdeg); free(pr); free(nx); free(esrc); free(etgt);
nomem:
  if (st) sqlite3_finalize(st);
  free(ids);
  sqlite3_result_error_nomem(context);
  return;
fail2:
  free(is_seed); free(outdeg); free(pr); free(nx); free(esrc); free(etgt);
fail:
  if (st) sqlite3_finalize(st);
  free(ids);
  sqlite3_result_error(context, sqlite3_errmsg(db), -1);
}

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

  rc = sqlite3_create_function(db, "_gql_label_propagation", 1, SQLITE_UTF8, 0,
                         gql_label_propagation_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_personalized_pagerank", 3, SQLITE_UTF8, 0,
                         gql_personalized_pagerank_func, 0, 0);
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

  /* _gql_typetag reads subtypes (SQLITE_SUBTYPE); _gql_retag sets them
   * (GQLITE-T-0406, GQLITE-A-0007). */
#ifndef SQLITE_SUBTYPE
#define SQLITE_SUBTYPE 0x000100000
#endif
  rc = sqlite3_create_function(db, "_gql_typetag", 1,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC | SQLITE_SUBTYPE, 0,
                         gql_typetag_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_json_elem", 1,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC | SQLITE_SUBTYPE | SQLITE_RESULT_SUBTYPE, 0,
                         gql_json_elem_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_retag", 2,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC | SQLITE_RESULT_SUBTYPE, 0,
                         gql_retag_func, 0, 0);
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

  /* Cypher orderability type-rank (0..9) — the primary ORDER BY key so mixed
   * types sort map<node<rel<list<path<string<bool<number<NaN<null. The
   * secondary key (_gql_order_key) then sorts within a (homogeneous) rank. */
  rc = sqlite3_create_function(db, "_gql_order_rank", 1,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0,
                         gql_order_rank_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  /* Normalize a numeric tz offset (drop a zero seconds suffix). */
  rc = sqlite3_create_function(db, "_gql_fmt_offset", 1,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0,
                         gql_fmt_offset_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  /* labels()/type() over a statically-Any argument: return the labels/type for
   * a node/relationship value, null for null, else raise a TypeError. */
  rc = sqlite3_create_function(db, "_gql_labels", 1,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0,
                         gql_labels_func, 0, 0);
  if (rc != SQLITE_OK) return rc;
  rc = sqlite3_create_function(db, "_gql_type", 1,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0,
                         gql_type_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  /* _gql_in reads the boolean subtype of its left side (#135). */
  rc = sqlite3_create_function(db, "_gql_in", 2,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC | SQLITE_SUBTYPE, 0,
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
                         SQLITE_UTF8 | SQLITE_RESULT_SUBTYPE, 0, gql_dyn_prop_func, 0, 0);
  if (rc != SQLITE_OK) return rc;
  /* Same lookup, returning the value's type tag (GQLITE-A-0007). */
  rc = sqlite3_create_function(db, "_gql_dyn_prop_tag", 2,
                         SQLITE_UTF8, (void *)1, gql_dyn_prop_func, 0, 0);
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

  /* GQLITE-T-0414: a map key read with json_each (any key text). */
  rc = sqlite3_create_function(db, "_gql_map_get", 3,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC | SQLITE_RESULT_SUBTYPE, 0,
                         gql_map_get_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_subscript_tag", 2,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0,
                         gql_subscript_tag_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_subscript", 2,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC | SQLITE_RESULT_SUBTYPE, 0,
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
  /* With the type tags of the two operands (Comparison2 [3]). */
  rc = sqlite3_create_function(db, "_gql_order_cmp", 5,
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

  rc = sqlite3_create_function(db, "_gql_duration_field", 2, SQLITE_UTF8, 0,
                         gql_duration_field_func, 0, 0);
  if (rc != SQLITE_OK) return rc;
  rc = sqlite3_create_function(db, "_gql_temporal_field", 2, SQLITE_UTF8, 0,
                         gql_temporal_field_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_tz_offset_for", 2, SQLITE_UTF8, 0,
                         gql_tz_offset_for_func, 0, 0);
  if (rc != SQLITE_OK) return rc;
  /* With hour, minute, second of the wall-clock time. */
  rc = sqlite3_create_function(db, "_gql_tz_offset_for", 5, SQLITE_UTF8, 0,
                         gql_tz_offset_for_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "_gql_dyn_add", 2,
                         SQLITE_UTF8 | SQLITE_SUBTYPE | SQLITE_RESULT_SUBTYPE, 0,
                         gql_dyn_add_func, 0, 0);
  if (rc != SQLITE_OK) return rc;
  rc = sqlite3_create_function(db, "_gql_dyn_sub", 2, SQLITE_UTF8, 0,
                         gql_dyn_sub_func, 0, 0);
  if (rc != SQLITE_OK) return rc;
  rc = sqlite3_create_function(db, "_gql_dyn_mul", 2, SQLITE_UTF8, 0,
                         gql_dyn_mul_func, 0, 0);
  if (rc != SQLITE_OK) return rc;
  rc = sqlite3_create_function(db, "_gql_dyn_div", 2, SQLITE_UTF8, 0,
                         gql_dyn_div_func, 0, 0);
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
  rc = sqlite3_create_function(db, "_gql_time_compose", 13, SQLITE_UTF8, 0,
                         gql_time_compose_func, 0, 0);
  if (rc != SQLITE_OK) return rc;
  /* With the composed date (datetime with a named zone). */
  rc = sqlite3_create_function(db, "_gql_time_compose", 14, SQLITE_UTF8, 0,
                         gql_time_compose_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  rc = sqlite3_create_function(db, "cypher_validate", 1, SQLITE_UTF8, 0,
                         cypher_validate_func, 0, 0);
  if (rc != SQLITE_OK) return rc;

  /* GQLITE-T-0100: capability metadata for clients (GitHub #17). */
  rc = sqlite3_create_function(db, "cypher_capabilities", 0,
                         SQLITE_UTF8 | SQLITE_DETERMINISTIC, 0,
                         cypher_capabilities_func, 0, 0);
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

