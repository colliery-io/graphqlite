/*
 * RETURN clause transformation
 * Converts RETURN items into SQL SELECT projections
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "transform/cypher_transform.h"
#include "entity_json_sql.h"
#include "transform/transform_func_string.h"
#include "transform/transform_func_math.h"
#include "transform/transform_func_entity.h"
#include "transform/transform_func_path.h"
#include "transform/transform_func_list.h"
#include "transform/transform_func_aggregate.h"
#include "transform/transform_func_graph.h"
#include "transform/transform_func_temporal.h"
#include "transform/transform_func_geo.h"
#include "transform/transform_func_json.h"
#include "transform/transform_func_typeconv.h"
#include "transform/transform_expr_predicate.h"
#include "transform/transform_helpers.h"
#include "transform/sql_builder.h"
#include "parser/cypher_debug.h"

/* Defined in the executor (executor_helpers.c). */
char* serialize_ast_to_json(ast_node *expr);

/* Nesting depth of a list/map literal made only of constants (integers,
 * strings, booleans, null), or -1. Decimals are excluded: the serializer
 * prints them with %g. */
static int const_literal_depth(ast_node *expr)
{
    if (!expr) return -1;
    if (expr->type == AST_NODE_LITERAL) {
        return ((cypher_literal*)expr)->literal_type == LITERAL_DECIMAL ? -1 : 0;
    }
    int deepest = 0;
    if (expr->type == AST_NODE_LIST) {
        cypher_list *l = (cypher_list*)expr;
        for (int i = 0; l->items && i < l->items->count; i++) {
            int d = const_literal_depth(l->items->items[i]);
            if (d < 0) return -1;
            if (d > deepest) deepest = d;
        }
        return deepest + 1;
    }
    if (expr->type == AST_NODE_MAP) {
        cypher_map *m = (cypher_map*)expr;
        for (int i = 0; m->pairs && i < m->pairs->count; i++) {
            cypher_map_pair *pair = (cypher_map_pair*)m->pairs->items[i];
            int d = pair ? const_literal_depth(pair->value) : -1;
            if (d < 0) return -1;
            if (d > deepest) deepest = d;
        }
        return deepest + 1;
    }
    return -1;
}

/* A deeply nested constant list/map literal as one json('...') constant.
 * As nested _gql_list/_gql_map calls it overflowed the fixed parser stack
 * of SQLite before 3.46 ("parser stack overflow", Literals7 [12],
 * Literals8 [15]/[16] on Linux). Returns 1 if emitted. */
#define CONST_LITERAL_FOLD_DEPTH 8
static int emit_folded_literal(cypher_transform_context *ctx, ast_node *expr)
{
    if (const_literal_depth(expr) < CONST_LITERAL_FOLD_DEPTH) return 0;
    char *json = serialize_ast_to_json(expr);
    if (!json) return 0;
    append_sql(ctx, "json(");
    append_string_literal(ctx, json);
    append_sql(ctx, ")");
    free(json);
    return 1;
}

/*
 * Pending property JOINs buffer for aggregation optimization.
 * These are accumulated during RETURN item processing and injected
 * into the FROM clause before it's appended back.
 *
 * Uses a dynamically growing buffer to handle arbitrary query complexity.
 */
/* Whether an AST node is a boolean-producing expression. Mirrors the
 * predicate at the top-of-RETURN-item wrap site below. Used when
 * emitting list/map values so booleans inside literals get tagged
 * with GQL_SUBTYPE_BOOLEAN (T-0304). */
bool ast_yields_boolean(ast_node *expr)
{
    if (!expr) return false;
    if (expr->type == AST_NODE_NOT_EXPR ||
        expr->type == AST_NODE_NULL_CHECK ||
        expr->type == AST_NODE_LIST_PREDICATE ||
        expr->type == AST_NODE_LABEL_EXPR ||
        expr->type == AST_NODE_EXISTS_EXPR) return true;
    if (expr->type == AST_NODE_LITERAL &&
        ((cypher_literal*)expr)->literal_type == LITERAL_BOOLEAN) return true;
    if (expr->type == AST_NODE_BINARY_OP) {
        switch (((cypher_binary_op*)expr)->op_type) {
            case BINARY_OP_AND: case BINARY_OP_OR: case BINARY_OP_XOR:
            case BINARY_OP_EQ:  case BINARY_OP_NEQ:
            case BINARY_OP_LT:  case BINARY_OP_GT:
            case BINARY_OP_LTE: case BINARY_OP_GTE:
            case BINARY_OP_IN:
            case BINARY_OP_STARTS_WITH:
            case BINARY_OP_ENDS_WITH:
            case BINARY_OP_CONTAINS:
            case BINARY_OP_REGEX_MATCH:
                return true;
            default: return false;
        }
    }
    return false;
}

/* Emit an expression that participates in a JSON list/map. For
 * boolean-producing AST nodes, wrap in _gql_bool_str(...) so the
 * carried GQL_SUBTYPE_BOOLEAN survives through _gql_list/_gql_map.
 * For boolean LITERALs we emit the static text directly. Otherwise
 * behaves like a normal transform_expression call. */
static int transform_list_item_value(cypher_transform_context *ctx, ast_node *expr)
{
    if (expr && expr->type == AST_NODE_LITERAL &&
        ((cypher_literal*)expr)->literal_type == LITERAL_BOOLEAN) {
        cypher_literal *lit = (cypher_literal*)expr;
        append_sql(ctx, "_gql_bool_str(%d)", lit->value.boolean ? 1 : 0);
        return 0;
    }
    if (ast_yields_boolean(expr)) {
        append_sql(ctx, "_gql_bool_str(CASE WHEN (");
        if (transform_expression(ctx, expr) < 0) return -1;
        append_sql(ctx, ") IS NULL THEN NULL WHEN (");
        if (transform_expression(ctx, expr) < 0) return -1;
        append_sql(ctx, ") THEN 1 ELSE 0 END)");
        return 0;
    }
    return transform_expression(ctx, expr);
}

/*
 * ORDER BY on a RETURN alias that shadows one of our own SQL column names.
 *
 * `RETURN b.id AS id ORDER BY id` emits `ORDER BY _gql_order_rank(id)`, and
 * SQLite only substitutes an output-column alias for an ORDER BY term that is
 * a *bare* identifier. Inside the ordering function the name binds to the
 * base table instead, so the query silently sorted by the internal node id,
 * or failed with "ambiguous column name" once two patterns were in scope.
 *
 * These are every column name our generated SQL can bring into scope (the
 * schema in cypher_schema.c). Any other alias is unambiguous and keeps the
 * cheap bare-alias reference.
 */
static char *transform_expression_to_string(cypher_transform_context *ctx, ast_node *expr);

/* Capture the SQL of `expr` as a boolean condition (transform_condition). */
static char *condition_to_string(cypher_transform_context *ctx, ast_node *expr)
{
    char *saved_buffer = ctx->sql_buffer;
    size_t saved_size = ctx->sql_size;
    size_t saved_capacity = ctx->sql_capacity;
    char *temp = malloc(4096);
    if (!temp) return NULL;
    temp[0] = '\0';
    ctx->sql_buffer = temp;
    ctx->sql_size = 0;
    ctx->sql_capacity = 4096;
    int rc = transform_condition(ctx, expr);
    char *out = (rc == 0 && ctx->sql_size > 0) ? strdup(ctx->sql_buffer) : NULL;
    free(ctx->sql_buffer);
    ctx->sql_buffer = saved_buffer;
    ctx->sql_size = saved_size;
    ctx->sql_capacity = saved_capacity;
    return out;
}

static char *str_printf(const char *fmt, const char *a, const char *b)
{
    size_t n = strlen(fmt) + (a ? strlen(a) : 0) + (b ? strlen(b) : 0) + 1;
    char *out = malloc(n);
    if (out) snprintf(out, n, fmt, a ? a : "", b ? b : "");
    return out;
}

/* GQLITE-A-0007: SQL of the type tag of an expression ('bool', 'json',
 * 'text', ...), or NULL when none is needed or known (a node, edge or path;
 * an int or real keeps its SQLite type; text of unknown origin keeps the old
 * handling). `value_sql` is the expression's SQL, for the _gql_typetag
 * fallback (NULL to transform it here). Caller frees. */
char *cypher_expr_tag_sql(cypher_transform_context *ctx, ast_node *expr, const char *value_sql)
{
    if (!ctx || !expr) return NULL;
    switch (expr->type) {
        case AST_NODE_IDENTIFIER: {
            const char *name = ((cypher_identifier *)expr)->name;
            if (!transform_var_is_projected(ctx->var_ctx, name)) return NULL;
            const char *t = transform_var_get_tag(ctx->var_ctx, name);
            return t ? strdup(t) : NULL;
        }
        case AST_NODE_PARAMETER: {
            const char *name = ((cypher_parameter *)expr)->name;
            return name ? str_printf(":%s__gqlt", name, NULL) : NULL;
        }
        case AST_NODE_PROPERTY: {
            char *t = transform_property_tag_sql(ctx, (cypher_property *)expr);
            if (t) return t;
            break;
        }
        case AST_NODE_LITERAL:
            switch (((cypher_literal *)expr)->literal_type) {
                case LITERAL_BOOLEAN: return strdup("'bool'");
                case LITERAL_STRING:  return strdup("'text'");
                default:              return NULL;
            }
        case AST_NODE_LIST:
        case AST_NODE_MAP:
            return strdup("'json'");
        case AST_NODE_SUBSCRIPT: {
            cypher_subscript *sub = (cypher_subscript *)expr;
            if (sub->is_slice || !sub->index) return strdup("'json'");
            char *l = transform_expression_to_string(ctx, sub->expr);
            char *ix = transform_expression_to_string(ctx, sub->index);
            size_t n = (l && ix) ? 2 * (strlen(l) + strlen(ix)) + 96 : 0;
            char *out = n ? malloc(n) : NULL;
            if (out) snprintf(out, n, "COALESCE(_gql_subscript_tag(%s, %s), _gql_typetag(_gql_subscript(%s, %s)))", l, ix, l, ix);
            free(l);
            free(ix);
            return out;
        }
        case AST_NODE_CASE_EXPR: {
            /* The tag of the branch the CASE takes. */
            cypher_case_expr *ce = (cypher_case_expr *)expr;
            if (!ce->when_clauses) break;
            dynamic_buffer b;
            dbuf_init(&b);
            dbuf_append(&b, "(CASE");
            if (ce->operand) {
                char *op = transform_expression_to_string(ctx, ce->operand);
                if (!op) { dbuf_free(&b); return NULL; }
                dbuf_appendf(&b, " %s", op);
                free(op);
            }
            bool ok = true;
            for (int i = 0; i < ce->when_clauses->count && ok; i++) {
                cypher_when_clause *wc = (cypher_when_clause *)ce->when_clauses->items[i];
                char *cond = ce->operand ? transform_expression_to_string(ctx, wc->condition)
                                         : condition_to_string(ctx, wc->condition);
                char *t = cypher_expr_tag_sql(ctx, wc->result, NULL);
                if (!cond) ok = false;
                else dbuf_appendf(&b, " WHEN %s THEN %s", cond, t ? t : "NULL");
                free(cond);
                free(t);
            }
            if (ce->else_expr) {
                char *t = cypher_expr_tag_sql(ctx, ce->else_expr, NULL);
                dbuf_appendf(&b, " ELSE %s", t ? t : "NULL");
                free(t);
            }
            dbuf_append(&b, " END)");
            if (!ok) { dbuf_free(&b); return NULL; }
            return dbuf_finish(&b);
        }
        case AST_NODE_FUNCTION_CALL: {
            cypher_function_call *fc = (cypher_function_call *)expr;
            /* Aggregates: collect() is a list; the numeric ones are native
             * numbers. No second evaluation of the aggregate. */
            if (fc->function_name) {
                const char *n = fc->function_name;
                /* GQLITE-T-0413: functions that return a string; their text
                 * is a string even when it looks like a number or JSON. */
                static const char *const string_fns[] = {
                    "toString", "toStringOrNull", "substring", "left", "right",
                    "trim", "ltrim", "rtrim", "btrim", "toUpper", "toLower",
                    "upper", "lower", "replace", "type", "valueType", NULL
                };
                for (int k = 0; string_fns[k]; k++) {
                    if (!strcasecmp(n, string_fns[k])) {
                        char *v = value_sql ? NULL : transform_expression_to_string(ctx, expr);
                        const char *vs = value_sql ? value_sql : v;
                        char *out = vs ? str_printf("(CASE WHEN (%s) IS NOT NULL THEN 'text' END)", vs, NULL) : NULL;
                        free(v);
                        return out;
                    }
                }
                if (!strcasecmp(n, "collect")) return strdup("'json'");
                if (!strcasecmp(n, "count") || !strcasecmp(n, "sum") || !strcasecmp(n, "avg") ||
                    !strncasecmp(n, "stDev", 5) || !strncasecmp(n, "percentile", 10)) return NULL;
            }
            /* coalesce(): the tag of the first non-null argument. */
            if (!fc->function_name || strcasecmp(fc->function_name, "coalesce") != 0 ||
                !fc->args || fc->args->count == 0) break;
            dynamic_buffer b;
            dbuf_init(&b);
            dbuf_append(&b, "(CASE");
            for (int i = 0; i < fc->args->count; i++) {
                ast_node *a = fc->args->items[i];
                char *v = transform_expression_to_string(ctx, a);
                if (!v) { dbuf_free(&b); return NULL; }
                char *t = cypher_expr_tag_sql(ctx, a, v);
                dbuf_appendf(&b, " WHEN (%s) IS NOT NULL THEN %s", v, t ? t : "NULL");
                free(v);
                free(t);
            }
            dbuf_append(&b, " END)");
            return dbuf_finish(&b);
        }
        default:
            break;
    }
    if (ast_is_boolean_expr(expr)) return strdup("'bool'");
    if (expr->type == AST_NODE_BINARY_OP && ((cypher_binary_op *)expr)->op_type == BINARY_OP_ADD) {
        /* GQLITE-T-0413: string + string is a string (a list from + carries
         * the JSON subtype; a duration is JSON text, so text starting with
         * [ or { is left untagged). */
        char *own2 = NULL;
        const char *vs = value_sql ? value_sql : (own2 = transform_expression_to_string(ctx, expr));
        char *out = NULL;
        if (vs) {
            size_t n = 4 * strlen(vs) + 200;
            out = malloc(n);
            if (out) snprintf(out, n,
                "COALESCE(_gql_typetag(%s), CASE WHEN typeof(%s) = 'text' AND "
                "substr(%s, 1, 1) NOT IN ('[', '{') THEN 'text' END)", vs, vs, vs);
        }
        free(own2);
        return out;
    }
    char *own = NULL;
    if (!value_sql) value_sql = own = transform_expression_to_string(ctx, expr);
    char *out = (value_sql && value_sql[0]) ? str_printf("_gql_typetag(%s)", value_sql, NULL) : NULL;
    free(own);
    return out;
}

/* GQLITE-A-0007: the tag of a RETURN item goes in a hidden column
 * __gqlt_<i> after the visible columns, as plain text that survives the
 * sorter of ORDER BY / DISTINCT / GROUP BY, which drops subtypes. The
 * result reader applies it. */
static bool expr_has_aggregate(const ast_node *expr)
{
    if (!expr) return false;
    if (expr->type == AST_NODE_FUNCTION_CALL) {
        const cypher_function_call *fc = (const cypher_function_call *)expr;
        const char *n = fc->function_name;
        if (n && (!strcasecmp(n, "count") || !strcasecmp(n, "sum") || !strcasecmp(n, "avg") ||
                  !strcasecmp(n, "min") || !strcasecmp(n, "max") || !strcasecmp(n, "collect") ||
                  !strncasecmp(n, "stDev", 5) || !strncasecmp(n, "percentile", 10)))
            return true;
        for (int i = 0; fc->args && i < fc->args->count; i++)
            if (expr_has_aggregate(fc->args->items[i])) return true;
        return false;
    }
    if (expr->type == AST_NODE_BINARY_OP) {
        const cypher_binary_op *b = (const cypher_binary_op *)expr;
        return expr_has_aggregate(b->left) || expr_has_aggregate(b->right);
    }
    return false;
}

/* Does the RETURN pass its rows through SQLite's sorter (ORDER BY,
 * DISTINCT, grouping, UNION), which drops subtypes? */
static bool return_uses_sorter(cypher_transform_context *ctx, cypher_return *ret)
{
    if (ret->distinct || ctx->in_union) return true;
    if (ret->order_by && ret->order_by->count > 0) return true;
    for (int i = 0; ret->items && i < ret->items->count; i++) {
        cypher_return_item *it = (cypher_return_item *)ret->items->items[i];
        if (it && expr_has_aggregate(it->expr)) return true;
    }
    return false;
}

static char *return_item_tag_sql(cypher_transform_context *ctx, cypher_return *ret,
                                 ast_node *expr, const char *expr_sql)
{
    /* A property read keeps its subtype to the reader when no sorter is in
     * the way, and the reader takes its TEXT as a string: the tag (two key
     * probes per row) is needed only with a sorter. */
    if (expr && expr->type == AST_NODE_PROPERTY && !return_uses_sorter(ctx, ret)) return NULL;
    return cypher_expr_tag_sql(ctx, expr, expr_sql);
}

/* Append the hidden tag columns __gqlt_0 .. __gqlt_<n-1> (NULL when an item
 * has none, so every arm of a UNION has the same shape). */
static void return_append_tags(cypher_transform_context *ctx, char **tags, int n)
{
    for (int i = 0; i < n; i++) {
        char nm[32];
        snprintf(nm, sizeof(nm), "__gqlt_%d", i);
        sql_select(ctx->unified_builder, tags && tags[i] ? tags[i] : "NULL", nm);
        if (tags) free(tags[i]);
    }
    free(tags);
}

static bool alias_shadows_sql_column(const char *name)
{
    static const char *const shadowed[] = {
        "id", "source_id", "target_id", "type", "node_id",
        "edge_id", "key_id", "key", "label", "value"
    };
    if (!name) return false;
    for (size_t i = 0; i < sizeof(shadowed) / sizeof(shadowed[0]); i++) {
        if (strcmp(name, shadowed[i]) == 0) return true;
    }
    return false;
}

/*
 * Render one ORDER BY term. When it is a bare identifier naming a RETURN alias
 * that would be captured by a base-table column, render the aliased expression
 * itself (exactly what `ORDER BY b.id` produces, which has always been
 * correct). Caller frees the result.
 */
static char *transform_order_by_term(cypher_transform_context *ctx,
                                     cypher_return *ret,
                                     ast_node *order_expr)
{
    if (order_expr && order_expr->type == AST_NODE_IDENTIFIER && ret && ret->items) {
        const char *name = ((cypher_identifier *)order_expr)->name;
        if (alias_shadows_sql_column(name)) {
            for (int i = 0; i < ret->items->count; i++) {
                cypher_return_item *item = (cypher_return_item *)ret->items->items[i];
                if (!item || !item->alias || !item->expr) continue;
                if (strcmp(item->alias, name) != 0) continue;
                /* `RETURN id AS id` would recurse straight back here. */
                if (item->expr->type == AST_NODE_IDENTIFIER &&
                    strcmp(((cypher_identifier *)item->expr)->name, name) == 0) break;
                char *sub = transform_expression_to_string(ctx, item->expr);
                if (sub) return sub;
                break;
            }
        }
    }
    return transform_expression_to_string(ctx, order_expr);
}

/*
 * Transform an expression to a dynamically allocated string.
 * Uses a temporary buffer to capture output, then returns the result.
 * Caller must free the returned string.
 * Returns NULL on error.
 */
/* Recursively check if `expr` contains any aggregate function call.
 * Cypher's "aggregate names" are count/sum/avg/min/max/collect/stdev/
 * stdevp/percentilecont/percentiledisc. Used by the implicit-GROUP-BY
 * detection — aggregates can be buried inside map/list/binary-op
 * literals (e.g. `{kids: collect(x)}` is grouped on the surrounding
 * projection key). Without this, MATCH-over-empty + such a projection
 * returns one all-null row instead of zero rows. */
static bool ast_contains_aggregate_func(ast_node *expr)
{
    if (!expr) return false;
    switch (expr->type) {
        case AST_NODE_FUNCTION_CALL: {
            cypher_function_call *fc = (cypher_function_call *)expr;
            if (fc->function_name) {
                const char *n = fc->function_name;
                if (!strcasecmp(n, "count") || !strcasecmp(n, "sum") ||
                    !strcasecmp(n, "avg") || !strcasecmp(n, "min") ||
                    !strcasecmp(n, "max") || !strcasecmp(n, "collect") ||
                    !strcasecmp(n, "stdev") || !strcasecmp(n, "stdevp") ||
                    !strcasecmp(n, "percentilecont") ||
                    !strcasecmp(n, "percentiledisc"))
                    return true;
            }
            if (fc->args) {
                for (int i = 0; i < fc->args->count; i++) {
                    if (ast_contains_aggregate_func(fc->args->items[i])) return true;
                }
            }
            return false;
        }
        case AST_NODE_BINARY_OP: {
            cypher_binary_op *b = (cypher_binary_op *)expr;
            return ast_contains_aggregate_func(b->left) ||
                   ast_contains_aggregate_func(b->right);
        }
        case AST_NODE_NOT_EXPR:
            return ast_contains_aggregate_func(((cypher_not_expr *)expr)->expr);
        case AST_NODE_LIST: {
            cypher_list *lst = (cypher_list *)expr;
            if (lst->items) {
                for (int i = 0; i < lst->items->count; i++) {
                    if (ast_contains_aggregate_func(lst->items->items[i])) return true;
                }
            }
            return false;
        }
        case AST_NODE_MAP: {
            cypher_map *m = (cypher_map *)expr;
            if (m->pairs) {
                for (int i = 0; i < m->pairs->count; i++) {
                    cypher_map_pair *mp = (cypher_map_pair *)m->pairs->items[i];
                    if (mp && ast_contains_aggregate_func(mp->value)) return true;
                }
            }
            return false;
        }
        case AST_NODE_PROPERTY:
            return ast_contains_aggregate_func(((cypher_property *)expr)->expr);
        case AST_NODE_SUBSCRIPT: {
            cypher_subscript *s = (cypher_subscript *)expr;
            return ast_contains_aggregate_func(s->expr) ||
                   ast_contains_aggregate_func(s->index);
        }
        case AST_NODE_NULL_CHECK:
            return ast_contains_aggregate_func(((cypher_null_check *)expr)->expr);
        case AST_NODE_LIST_COMPREHENSION:
            /* T-0370: the comprehension's source list may aggregate
             * (`[x IN collect(p) | ...]`). */
            return ast_contains_aggregate_func(((cypher_list_comprehension *)expr)->list_expr);
        default:
            return false;
    }
}

static char *transform_expression_to_string(cypher_transform_context *ctx, ast_node *expr)
{
    if (!ctx || !expr) return NULL;

    /* Save current buffer state */
    char *saved_buffer = ctx->sql_buffer;
    size_t saved_size = ctx->sql_size;
    size_t saved_capacity = ctx->sql_capacity;

    /* Allocate temporary buffer */
    size_t temp_capacity = 4096;
    char *temp_buffer = malloc(temp_capacity);
    if (!temp_buffer) return NULL;
    temp_buffer[0] = '\0';

    /* Switch to temporary buffer */
    ctx->sql_buffer = temp_buffer;
    ctx->sql_size = 0;
    ctx->sql_capacity = temp_capacity;

    /* Transform the expression */
    int result = transform_expression(ctx, expr);

    /* Capture the result */
    char *expr_str = NULL;
    if (result == 0 && ctx->sql_size > 0) {
        expr_str = strdup(ctx->sql_buffer);
    }

    /* Restore original buffer */
    free(ctx->sql_buffer);
    ctx->sql_buffer = saved_buffer;
    ctx->sql_size = saved_size;
    ctx->sql_capacity = saved_capacity;

    return expr_str;
}

/* Transform a RETURN clause */
int transform_return_clause(cypher_transform_context *ctx, cypher_return *ret)
{
    CYPHER_DEBUG("Transforming RETURN clause");

    if (!ctx || !ret) {
        return -1;
    }

    /* Discard any deferred property JOINs left over from an earlier
     * clause; this RETURN starts with an empty deferred section. */
    sql_builder_clear_deferred_joins(ctx->unified_builder);

    /* For write queries, RETURN means we need to select the created data */
    if (ctx->query_type == QUERY_TYPE_WRITE) {
        /* TODO: Handle returning created nodes/relationships */
        ctx->has_error = true;
        ctx->error_message = strdup("RETURN after CREATE not yet implemented");
        return -1;
    }

    /*
     * NEW: Unified builder path for MATCH + RETURN
     * When unified_builder is active and has FROM clause content,
     * use sql_select(), sql_order_by(), sql_limit() to build the query.
     */
    if (ctx->unified_builder && !dbuf_is_empty(&ctx->unified_builder->from)) {
        CYPHER_DEBUG("Using unified builder path for RETURN");

        /* Handle DISTINCT */
        if (ret->distinct) {
            sql_distinct(ctx->unified_builder);
        }

        /* Handle RETURN * - expand all visible variables */
        if (ret->return_all) {
            CYPHER_DEBUG("Expanding RETURN * with all bound variables");
            int var_count = transform_var_count(ctx->var_ctx);
            int added = 0;
            for (int vi = 0; vi < var_count; vi++) {
                transform_var *var = transform_var_at(ctx->var_ctx, vi);
                if (!var || !var->is_visible || !var->name) continue;
                /* RETURN * exposes only user-named variables, not the synthetic
                 * aliases generated for anonymous nodes/rels (With1 [1]/[2]).
                 * Named paths additionally synthesize `_pv_n<base>_<j>` /
                 * `_pv_e<base>_<j>` for their anonymous path elements (Return7 [1]). */
                if (strncmp(var->name, "_gql_default_alias_", 19) == 0 ||
                    strncmp(var->name, "__unnamed_rel_", 14) == 0 ||
                    strncmp(var->name, "_pv_n", 5) == 0 ||
                    strncmp(var->name, "_pv_e", 5) == 0) continue;

                /* Build expression for this variable based on its kind */
                if (var->kind == VAR_KIND_NODE) {
                    /* Return full node object using json_object */
                    char expr_buf[4096];
                    const char *alias = var->table_alias;
                    bool skip_id = var->alias_is_id;
                    char id_expr[300];
                    snprintf(id_expr, sizeof(id_expr), "%s%s", alias, skip_id ? "" : ".id");
                    char *node_json = gql_sql_node_json_expr("", id_expr);
                    if (!node_json) return -1;
                    snprintf(expr_buf, sizeof(expr_buf), "(SELECT %s)", node_json);
                    free(node_json);
                    { char vb[256]; sql_select(ctx->unified_builder, expr_buf, sql_ident(vb, sizeof(vb), var->name)); }
                } else if (var->kind == VAR_KIND_EDGE) {
                    /* Return edge as its type */
                    char expr_buf[256];
                    snprintf(expr_buf, sizeof(expr_buf), "%s.type", var->table_alias);
                    { char vb[256]; sql_select(ctx->unified_builder, expr_buf, sql_ident(vb, sizeof(vb), var->name)); }
                } else if (var->kind == VAR_KIND_PROJECTED) {
                    /* Projected variable — the source expression (e.g. CTE
                     * column ref) lives on source_expr; table_alias is NULL.
                     * T-0333: use get_alias to pick the right one. */
                    const char *src = transform_var_get_alias(ctx->var_ctx, var->name);
                    if (src) {
                        char vb[256];
                        if (var->tag_expr) {
                            /* GQLITE-A-0007 */
                            size_t n = strlen(src) + strlen(var->tag_expr) + 24;
                            char *rt = malloc(n);
                            if (!rt) return -1;
                            snprintf(rt, n, "_gql_retag(%s, %s)", src, var->tag_expr);
                            sql_select(ctx->unified_builder, rt, sql_ident(vb, sizeof(vb), var->name));
                            free(rt);
                        } else {
                            sql_select(ctx->unified_builder, src, sql_ident(vb, sizeof(vb), var->name));
                        }
                    }
                }
                added++;
            }
            if (added == 0) {
                ctx->has_error = true;
                ctx->error_message = strdup("RETURN * used but no variables are bound");
                return -1;
            }
            goto return_star_done;
        }

        /* Defer registration of explicit aliases until after all items are
         * transformed so an alias name like `RETURN n.num AS n, count(n)`
         * doesn't shadow the original binding of `n` mid-projection. */
        const char **deferred_aliases = NULL;
        int deferred_count = 0;
        if (ret->items->count > 0) {
            deferred_aliases = calloc(ret->items->count, sizeof(*deferred_aliases));
        }

        /* Add SELECT columns to unified builder */
        char **item_tags = ret->items->count > 0 ? calloc(ret->items->count, sizeof(char *)) : NULL;
        for (int i = 0; i < ret->items->count; i++) {
            cypher_return_item *item = (cypher_return_item*)ret->items->items[i];

            /* Transform the expression to a string */
            char *expr_str = transform_expression_to_string(ctx, item->expr);
            if (!expr_str) {
                /* Preserve existing error message from transformation, or set generic one */
                if (!ctx->error_message) {
                    ctx->has_error = true;
                    ctx->error_message = strdup("Failed to transform return item expression");
                }
                return -1;
            }

            /* Boolean-producing expressions: wrap so the output is JSON
             * true/false rather than SQLite integer 0/1. NULL stays NULL
             * for openCypher three-valued logic. */
            if (item->expr && (
                item->expr->type == AST_NODE_NOT_EXPR ||
                item->expr->type == AST_NODE_NULL_CHECK ||
                item->expr->type == AST_NODE_LIST_PREDICATE ||
                item->expr->type == AST_NODE_EXISTS_EXPR ||
                (item->expr->type == AST_NODE_BINARY_OP && (
                    ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_AND ||
                    ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_OR  ||
                    ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_XOR ||
                    ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_EQ  ||
                    ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_NEQ ||
                    ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_LT  ||
                    ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_GT  ||
                    ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_LTE ||
                    ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_GTE ||
                    ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_IN  ||
                    ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_STARTS_WITH ||
                    ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_ENDS_WITH ||
                    ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_CONTAINS ||
                    ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_REGEX_MATCH))))
            {
                size_t need = strlen(expr_str) * 2 + 96;
                char *wrapped = malloc(need);
                if (wrapped) {
                    snprintf(wrapped, need,
                             "_gql_bool_str(CASE WHEN (%s) IS NULL THEN NULL WHEN (%s) THEN 1 ELSE 0 END)",
                             expr_str, expr_str);
                    free(expr_str);
                    expr_str = wrapped;
                }
            }

            /* Determine alias - use explicit alias, or generate one for properties/functions */
            const char *alias = item->alias;
            char auto_alias[256] = "";
            if (!alias && item->expr->type == AST_NODE_PROPERTY) {
                cypher_property *prop = (cypher_property*)item->expr;
                if (prop->expr && prop->expr->type == AST_NODE_IDENTIFIER) {
                    cypher_identifier *id = (cypher_identifier*)prop->expr;
                    snprintf(auto_alias, sizeof(auto_alias), "\"%s.%s\"", id->name, prop->property_name);
                    alias = auto_alias;
                } else if (prop->expr && prop->expr->type == AST_NODE_FUNCTION_CALL) {
                    /* Property access on function call: startNode(r).name → "startNode(r).name" */
                    cypher_function_call *func = (cypher_function_call*)prop->expr;
                    if (func->function_name) {
                        size_t pos = 0;
                        pos += snprintf(auto_alias + pos, sizeof(auto_alias) - pos, "\"%s(", func->function_name);
                        /* count(*) carries one NULL argument (or none). */
                        if (strcasecmp(func->function_name, "count") == 0 &&
                            (!func->args || func->args->count == 0 ||
                             (func->args->count == 1 && !func->args->items[0]))) {
                            pos += snprintf(auto_alias + pos, sizeof(auto_alias) - pos, "*");
                        }
                        if (func->args) {
                            for (int j = 0; j < func->args->count && pos < sizeof(auto_alias) - 20; j++) {
                                if (j > 0) pos += snprintf(auto_alias + pos, sizeof(auto_alias) - pos, ", ");
                                ast_node *arg = func->args->items[j];
                                if (arg && arg->type == AST_NODE_IDENTIFIER) {
                                    pos += snprintf(auto_alias + pos, sizeof(auto_alias) - pos, "%s",
                                                    ((cypher_identifier*)arg)->name);
                                }
                            }
                        }
                        snprintf(auto_alias + pos, sizeof(auto_alias) - pos, ").%s\"", prop->property_name);
                        alias = auto_alias;
                    }
                }
            } else if (!alias && item->expr->type == AST_NODE_FUNCTION_CALL) {
                /* Generate function name as column alias: funcname(arg1, arg2, ...) */
                cypher_function_call *func = (cypher_function_call*)item->expr;
                if (func->function_name) {
                    size_t pos = 0;
                    pos += snprintf(auto_alias + pos, sizeof(auto_alias) - pos, "\"%s(", func->function_name);
                        /* count(*) carries one NULL argument (or none). */
                        if (strcasecmp(func->function_name, "count") == 0 &&
                            (!func->args || func->args->count == 0 ||
                             (func->args->count == 1 && !func->args->items[0]))) {
                            pos += snprintf(auto_alias + pos, sizeof(auto_alias) - pos, "*");
                        }
                    if (func->args) {
                        for (int j = 0; j < func->args->count && pos < sizeof(auto_alias) - 10; j++) {
                            if (j > 0) {
                                pos += snprintf(auto_alias + pos, sizeof(auto_alias) - pos, ", ");
                            }
                            ast_node *arg = func->args->items[j];
                            if (arg && arg->type == AST_NODE_IDENTIFIER) {
                                cypher_identifier *arg_id = (cypher_identifier*)arg;
                                pos += snprintf(auto_alias + pos, sizeof(auto_alias) - pos, "%s", arg_id->name);
                            } else if (arg && arg->type == AST_NODE_PROPERTY) {
                                cypher_property *arg_prop = (cypher_property*)arg;
                                if (arg_prop->expr && arg_prop->expr->type == AST_NODE_IDENTIFIER) {
                                    cypher_identifier *prop_id = (cypher_identifier*)arg_prop->expr;
                                    pos += snprintf(auto_alias + pos, sizeof(auto_alias) - pos, "%s.%s", prop_id->name, arg_prop->property_name);
                                }
                            } else {
                                pos += snprintf(auto_alias + pos, sizeof(auto_alias) - pos, "...");
                            }
                        }
                    }
                    snprintf(auto_alias + pos, sizeof(auto_alias) - pos, ")\"");
                    alias = auto_alias;
                }
            }

            /* Add to unified builder; a user alias is quoted as needed
             * (GQLITE-T-0396), an auto alias is quoted already. */
            char alias_buf[256];
            if (alias && alias == item->alias) alias = sql_ident(alias_buf, sizeof(alias_buf), alias);
            /* A bare identifier keeps its name as the column name, also
             * when its SQL is wrapped in _gql_retag (GQLITE-A-0007). */
            if (!alias && item->expr->type == AST_NODE_IDENTIFIER)
                alias = sql_ident(alias_buf, sizeof(alias_buf), ((cypher_identifier *)item->expr)->name);
            sql_select(ctx->unified_builder, expr_str, alias);
            if (item_tags) item_tags[i] = return_item_tag_sql(ctx, ret, item->expr, expr_str);
            free(expr_str);

            /* Defer alias registration so subsequent return items can still
             * resolve identifiers using their original bindings. */
            if (item->alias && deferred_aliases) {
                deferred_aliases[deferred_count++] = item->alias;
            }
        }
        return_append_tags(ctx, item_tags, ret->items->count);
        for (int i = 0; i < deferred_count; i++) {
            { char rb[256]; transform_var_register_projected(ctx->var_ctx, deferred_aliases[i], sql_ident(rb, sizeof(rb), deferred_aliases[i])); }
        }
        free(deferred_aliases);

return_star_done:
        /* Implicit GROUP BY: when RETURN mixes aggregating and
         * non-aggregating items, the non-aggregating expressions
         * become grouping keys (Cypher 9 spec). Detect by walking
         * items: count names like count/sum/avg/min/max/collect/
         * stdev/stdevp/percentilecont/percentiledisc as aggregating;
         * emit GROUP BY for the non-aggregating ones.
         *
         * Aggregates may be nested inside map/list/binary-op
         * expressions (e.g. `{kids: collect(x)}` or
         * `collect(x) + [1]`). Recursive walker required so the
         * grouped-empty case `MATCH ... RETURN n.name, {kids:
         * collect(x)}` returns 0 rows on empty input instead of 1.
         * (Return6 [6]/[18]/[19], With6 [6]/[7].) */
        if (ret->items && ret->items->count > 0) {
            bool has_agg = false;
            bool has_non_agg = false;
            for (int i = 0; i < ret->items->count; i++) {
                cypher_return_item *it = (cypher_return_item *)ret->items->items[i];
                bool is_agg = it && it->expr && ast_contains_aggregate_func(it->expr);
                if (is_agg) has_agg = true; else has_non_agg = true;
            }
            if (has_agg && has_non_agg) {
                for (int i = 0; i < ret->items->count; i++) {
                    cypher_return_item *it = (cypher_return_item *)ret->items->items[i];
                    bool is_agg = it && it->expr && ast_contains_aggregate_func(it->expr);
                    if (!is_agg) {
                        char *gb_expr = transform_expression_to_string(ctx, it->expr);
                        if (gb_expr) {
                            sql_group_by(ctx->unified_builder, gb_expr);
                            free(gb_expr);
                        }
                    }
                }
            }
        }

        /* Add ORDER BY */
        if (ret->order_by && ret->order_by->count > 0) {
            for (int i = 0; i < ret->order_by->count; i++) {
                cypher_order_by_item *order_item = (cypher_order_by_item*)ret->order_by->items[i];
                char *order_expr = transform_order_by_term(ctx, ret, order_item->expr);
                if (order_expr) {
                    sql_order_by(ctx->unified_builder, order_expr, order_item->descending);
                    free(order_expr);
                }
            }
        }

        /* Add LIMIT/OFFSET */
        if (ret->limit || ret->skip) {
            int limit_val = -1;
            int offset_val = -1;
            char *limit_param = NULL;
            char *offset_param = NULL;

            if (ret->limit && ret->limit->type == AST_NODE_LITERAL) {
                cypher_literal *lit = (cypher_literal*)ret->limit;
                if (lit->literal_type == LITERAL_INTEGER) {
                    limit_val = (int)lit->value.integer;
                }
            } else if (ret->limit && ret->limit->type == AST_NODE_PARAMETER) {
                cypher_parameter *p = (cypher_parameter*)ret->limit;
                if (p->name) {
                    char buf[128]; snprintf(buf, sizeof(buf), ":%s", p->name);
                    limit_param = strdup(buf);
                    register_parameter(ctx, p->name);
                }
            }
            if (ret->skip && ret->skip->type == AST_NODE_LITERAL) {
                cypher_literal *lit = (cypher_literal*)ret->skip;
                if (lit->literal_type == LITERAL_INTEGER) {
                    offset_val = (int)lit->value.integer;
                }
            } else if (ret->skip && ret->skip->type == AST_NODE_PARAMETER) {
                cypher_parameter *p = (cypher_parameter*)ret->skip;
                if (p->name) {
                    char buf[128]; snprintf(buf, sizeof(buf), ":%s", p->name);
                    offset_param = strdup(buf);
                    register_parameter(ctx, p->name);
                }
            }

            /* If skip but no limit, SQLite needs LIMIT -1 */
            if (ret->skip && !ret->limit) {
                limit_val = -1;
            }

            sql_limit(ctx->unified_builder, limit_val, offset_val);
            if (limit_param || offset_param) {
                sql_limit_expr(ctx->unified_builder, limit_param, offset_param);
                free(limit_param); free(offset_param);
            }
        }

        /* Append the property JOINs deferred by aggregate functions
         * (T-0268) after every other JOIN of this query. */
        sql_builder_flush_deferred_joins(ctx->unified_builder);

        /* T-0311 (E2): finalize moved to end of transform_single_query_sql.
         * External callers (executor_match.c, executor_merge_pipeline.c)
         * that invoke transform_return_clause directly add their own
         * explicit finalize. */

        CYPHER_DEBUG("Unified builder path complete (finalize deferred)");
        return 0;
    }

    /*
     * Check for legacy SELECT * pattern - should not occur anymore.
     * All code paths now either use the unified builder (above) or
     * the standalone RETURN handler (below).
     */
    char *select_pos = strstr(ctx->sql_buffer, "SELECT *");
    if (!select_pos) {
        /* No SELECT * - check if this is a standalone RETURN (no MATCH clause) */
        /* This happens for UNION queries or simple RETURN queries without MATCH */
        bool is_standalone = (ctx->sql_size == 0);

        /* Also check if we're after a UNION keyword - means we're starting a new sub-query */
        if (!is_standalone && ctx->sql_size >= 7) {
            /* Check if buffer ends with " UNION " or " UNION ALL " */
            if (strcmp(ctx->sql_buffer + ctx->sql_size - 7, " UNION ") == 0 ||
                (ctx->sql_size >= 11 && strcmp(ctx->sql_buffer + ctx->sql_size - 11, " UNION ALL ") == 0)) {
                is_standalone = true;
            }
        }

        if (is_standalone) {
            /* Standalone RETURN clause - use unified builder for simple SELECT */
            CYPHER_DEBUG("Standalone RETURN clause - using unified builder");

            /* Handle DISTINCT */
            if (ret->distinct) {
                sql_distinct(ctx->unified_builder);
            }

            /* Add SELECT columns to unified builder */
            char **item_tags2 = ret->items->count > 0 ? calloc(ret->items->count, sizeof(char *)) : NULL;
            for (int i = 0; i < ret->items->count; i++) {
                cypher_return_item *item = (cypher_return_item*)ret->items->items[i];

                /* Transform the expression to a string */
                char *expr_str = transform_expression_to_string(ctx, item->expr);
                if (!expr_str) {
                    if (!ctx->error_message) {
                        ctx->has_error = true;
                        ctx->error_message = strdup("Failed to transform return item expression");
                    }
                    return -1;
                }

                /* Boolean-producing expressions: emit JSON true/false. */
                if (item->expr && (
                    item->expr->type == AST_NODE_NOT_EXPR ||
                    item->expr->type == AST_NODE_NULL_CHECK ||
                    item->expr->type == AST_NODE_LIST_PREDICATE ||
                    item->expr->type == AST_NODE_EXISTS_EXPR ||
                    (item->expr->type == AST_NODE_BINARY_OP && (
                        ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_AND ||
                        ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_OR  ||
                        ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_XOR ||
                        ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_EQ  ||
                        ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_NEQ ||
                        ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_LT  ||
                        ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_GT  ||
                        ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_LTE ||
                        ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_GTE ||
                        ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_IN  ||
                        ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_STARTS_WITH ||
                        ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_ENDS_WITH ||
                        ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_CONTAINS ||
                        ((cypher_binary_op*)item->expr)->op_type == BINARY_OP_REGEX_MATCH))))
                {
                    size_t need = strlen(expr_str) * 2 + 96;
                    char *wrapped = malloc(need);
                    if (wrapped) {
                        snprintf(wrapped, need,
                                 "_gql_bool_str(CASE WHEN (%s) IS NULL THEN NULL WHEN (%s) THEN 1 ELSE 0 END)",
                                 expr_str, expr_str);
                        free(expr_str);
                        expr_str = wrapped;
                    }
                }

                /* Determine alias - use explicit alias, or generate one for properties/functions */
                const char *alias = item->alias;
                char auto_alias[256] = "";
                if (!alias && item->expr->type == AST_NODE_PROPERTY) {
                    cypher_property *prop = (cypher_property*)item->expr;
                    if (prop->expr && prop->expr->type == AST_NODE_IDENTIFIER) {
                        cypher_identifier *id = (cypher_identifier*)prop->expr;
                        snprintf(auto_alias, sizeof(auto_alias), "\"%s.%s\"", id->name, prop->property_name);
                        alias = auto_alias;
                    }
                } else if (!alias && item->expr->type == AST_NODE_FUNCTION_CALL) {
                    /* Generate function name as column alias: funcname(arg1, arg2, ...) */
                    cypher_function_call *func = (cypher_function_call*)item->expr;
                    if (func->function_name) {
                        size_t pos = 0;
                        pos += snprintf(auto_alias + pos, sizeof(auto_alias) - pos, "\"%s(", func->function_name);
                        /* count(*) carries one NULL argument (or none). */
                        if (strcasecmp(func->function_name, "count") == 0 &&
                            (!func->args || func->args->count == 0 ||
                             (func->args->count == 1 && !func->args->items[0]))) {
                            pos += snprintf(auto_alias + pos, sizeof(auto_alias) - pos, "*");
                        }
                        if (func->args) {
                            for (int j = 0; j < func->args->count && pos < sizeof(auto_alias) - 10; j++) {
                                if (j > 0) {
                                    pos += snprintf(auto_alias + pos, sizeof(auto_alias) - pos, ", ");
                                }
                                ast_node *arg = func->args->items[j];
                                if (arg && arg->type == AST_NODE_IDENTIFIER) {
                                    cypher_identifier *arg_id = (cypher_identifier*)arg;
                                    pos += snprintf(auto_alias + pos, sizeof(auto_alias) - pos, "%s", arg_id->name);
                                } else if (arg && arg->type == AST_NODE_PROPERTY) {
                                    cypher_property *arg_prop = (cypher_property*)arg;
                                    if (arg_prop->expr && arg_prop->expr->type == AST_NODE_IDENTIFIER) {
                                        cypher_identifier *prop_id = (cypher_identifier*)arg_prop->expr;
                                        pos += snprintf(auto_alias + pos, sizeof(auto_alias) - pos, "%s.%s", prop_id->name, arg_prop->property_name);
                                    }
                                } else {
                                    pos += snprintf(auto_alias + pos, sizeof(auto_alias) - pos, "...");
                                }
                            }
                        }
                        snprintf(auto_alias + pos, sizeof(auto_alias) - pos, ")\"");
                        alias = auto_alias;
                    }
                }

                /* Add to unified builder; a user alias is quoted as needed
                 * (GQLITE-T-0396), an auto alias is quoted already. */
                char alias_buf[256];
                if (alias && alias == item->alias) alias = sql_ident(alias_buf, sizeof(alias_buf), alias);
                if (!alias && item->expr->type == AST_NODE_IDENTIFIER)
                    alias = sql_ident(alias_buf, sizeof(alias_buf), ((cypher_identifier *)item->expr)->name);
                sql_select(ctx->unified_builder, expr_str, alias);
                if (item_tags2) item_tags2[i] = return_item_tag_sql(ctx, ret, item->expr, expr_str);
                free(expr_str);

                /* Register alias for ORDER BY reference */
                if (item->alias) {
                    char ref_buf[256];
                    transform_var_register_projected(ctx->var_ctx, item->alias,
                        sql_ident(ref_buf, sizeof(ref_buf), item->alias));
                }
            }
            return_append_tags(ctx, item_tags2, ret->items->count);

            /* Add ORDER BY */
            if (ret->order_by && ret->order_by->count > 0) {
                for (int i = 0; i < ret->order_by->count; i++) {
                    cypher_order_by_item *order_item = (cypher_order_by_item*)ret->order_by->items[i];
                    char *order_expr = transform_order_by_term(ctx, ret, order_item->expr);
                    if (order_expr) {
                        sql_order_by(ctx->unified_builder, order_expr, order_item->descending);
                        free(order_expr);
                    }
                }
            }

            /* Add LIMIT/OFFSET */
            if (ret->limit || ret->skip) {
                int limit_val = -1;
                int offset_val = -1;
                char *limit_param = NULL;
                char *offset_param = NULL;

                if (ret->limit && ret->limit->type == AST_NODE_LITERAL) {
                    cypher_literal *lit = (cypher_literal*)ret->limit;
                    if (lit->literal_type == LITERAL_INTEGER) {
                        limit_val = (int)lit->value.integer;
                    }
                } else if (ret->limit && ret->limit->type == AST_NODE_PARAMETER) {
                    cypher_parameter *p = (cypher_parameter*)ret->limit;
                    if (p->name) {
                        char buf[128]; snprintf(buf, sizeof(buf), ":%s", p->name);
                        limit_param = strdup(buf);
                        register_parameter(ctx, p->name);
                    }
                }
                if (ret->skip && ret->skip->type == AST_NODE_LITERAL) {
                    cypher_literal *lit = (cypher_literal*)ret->skip;
                    if (lit->literal_type == LITERAL_INTEGER) {
                        offset_val = (int)lit->value.integer;
                    }
                } else if (ret->skip && ret->skip->type == AST_NODE_PARAMETER) {
                    cypher_parameter *p = (cypher_parameter*)ret->skip;
                    if (p->name) {
                        char buf[128]; snprintf(buf, sizeof(buf), ":%s", p->name);
                        offset_param = strdup(buf);
                        register_parameter(ctx, p->name);
                    }
                }

                sql_limit(ctx->unified_builder, limit_val, offset_val);
                if (limit_param || offset_param) {
                    sql_limit_expr(ctx->unified_builder, limit_param, offset_param);
                    free(limit_param); free(offset_param);
                }
            }

            /* T-0311 (E2): finalize moved to end of transform_single_query_sql.
             * External callers add their own explicit finalize. */

            CYPHER_DEBUG("Standalone RETURN complete (finalize deferred)");
            return 0;
        }

        /*
         * Legacy "RETURN after WITH" path - should not be reached anymore.
         * WITH now populates unified_builder->from, so RETURN takes the
         * unified builder path at line 124. If we reach here, it indicates
         * a bug in the transformation pipeline.
         */
        /*
         * If sql_size > 0 but not standalone (and no UNION suffix), this is an
         * unexpected state - unified builder path should have handled it.
         */
        CYPHER_DEBUG("ERROR: Legacy RETURN path reached, sql_size=%zu, sql_buffer: %s",
                     ctx->sql_size, ctx->sql_buffer);
        ctx->has_error = true;
        ctx->error_message = strdup("Internal error: Legacy RETURN path should not be reached");
        return -1;
    } else {
        /*
         * SELECT * found in sql_buffer - this should not happen anymore.
         * WITH and UNWIND now use builder state extraction, and RETURN
         * adds explicit SELECT columns before finalizing. If we reach here,
         * it indicates a bug in the transformation pipeline.
         */
        CYPHER_DEBUG("ERROR: Unexpected SELECT * found in sql_buffer: %s", ctx->sql_buffer);
        ctx->has_error = true;
        ctx->error_message = strdup("Internal error: SELECT * pattern should not occur");
        return -1;
    }
}

/* The current json_each element for json_group_array, as JSON: true/false
 * stay booleans (json_each.value gives 1/0). The CASE yields plain JSON
 * text and json() outside it sets the JSON subtype: SQLite 3.45 drops a
 * subtype through CASE, which turned nested lists into strings. */
#define JSON_EACH_ELEMENT \
    "json(CASE json_each.type WHEN 'true' THEN 'true' WHEN 'false' THEN 'false' " \
    "WHEN 'null' THEN 'null' WHEN 'text' THEN json_quote(json_each.value) " \
    "ELSE json_each.value END)"

/* Transform an expression */
static int transform_expression_inner(cypher_transform_context *ctx, ast_node *expr);

/* #123: a boolean property, parameter or function result can reach SQL as
 * 'true'/'false' text, which SQLite reads as false in a condition. These
 * node types take _gql_bool() in a boolean position. Operators already
 * yield a SQL truth value and keep their form, so index-driven comparisons
 * are unchanged. */
static bool needs_condition_coercion(const ast_node *expr)
{
    switch (expr->type) {
        case AST_NODE_IDENTIFIER:
        case AST_NODE_PARAMETER:
        case AST_NODE_PROPERTY:
        case AST_NODE_FUNCTION_CALL:
        case AST_NODE_CASE_EXPR:
        case AST_NODE_REDUCE_EXPR:
        case AST_NODE_SUBSCRIPT:
            return true;
        default:
            return false;
    }
}

int transform_condition(cypher_transform_context *ctx, ast_node *expr)
{
    if (!expr || !needs_condition_coercion(expr)) {
        return transform_expression(ctx, expr);
    }
    bool saved_wc = ctx->where_conjunct;
    ctx->where_conjunct = false;
    append_sql(ctx, "_gql_bool(");
    int rc = transform_expression(ctx, expr);
    append_sql(ctx, ")");
    ctx->where_conjunct = saved_wc;
    return rc;
}

int transform_expression(cypher_transform_context *ctx, ast_node *expr)
{
    /* A WHERE root (or an AND operand under it) is a boolean position. */
    if (ctx->where_conjunct && expr && needs_condition_coercion(expr)) {
        return transform_condition(ctx, expr);
    }

    /* Perf review F7: the WHERE-conjunct flag survives only through binary
     * operators (AND keeps it, comparisons consume it, everything else
     * drops it); any other node type ends the conjunct context. */
    bool saved_wc = ctx->where_conjunct;
    if (expr && expr->type != AST_NODE_BINARY_OP) ctx->where_conjunct = false;
    int rc = transform_expression_inner(ctx, expr);
    ctx->where_conjunct = saved_wc;
    return rc;
}

static int transform_expression_inner(cypher_transform_context *ctx, ast_node *expr)
{
    if (!expr) {
        return -1;
    }
    
    CYPHER_DEBUG("Transforming expression type %s", ast_node_type_name(expr->type));
    
    switch (expr->type) {
        case AST_NODE_IDENTIFIER:
            {
                cypher_identifier *id = (cypher_identifier*)expr;

                /* Check for path variables first - they don't have a table alias */
                if (transform_var_is_path(ctx->var_ctx, id->name)) {
                        CYPHER_DEBUG("Processing path variable '%s' in RETURN", id->name);
                        /* This is a path variable - generate JSON with element IDs */
                        transform_var *path_var = transform_var_lookup_path(ctx->var_ctx, id->name);
                        /* I-0047 P4: a path forwarded through WITH has its
                         * table_alias set to the CTE column holding the already-
                         * hydrated path text (a `_with_N.p` column ref). Emit that
                         * column directly — the element aliases used to rebuild
                         * the path are out of scope in the outer query. The
                         * executor still hydrates it via path_elements (With1 [4]). */
                        if (path_var && path_var->table_alias &&
                            strchr(path_var->table_alias, '.') != NULL) {
                            append_sql(ctx, "%s", path_var->table_alias);
                        } else if (path_var && path_var->path_elements) {
                            CYPHER_DEBUG("Found path variable metadata for '%s' with %d elements", id->name, path_var->path_elements->count);

                            /* Check if this is a variable-length path (shortestPath, etc.).
                             * The interleaved elem_ids path projection only applies
                             * when the WHOLE path is a single varlen segment; a path
                             * mixing fixed and varlen rels (Match7 [19]) has parts
                             * outside this CTE and must keep the legacy behavior. */
                            bool has_varlen = false;
                            const char *varlen_alias = NULL;
                            int rel_count = 0;
                            for (int i = 0; i < path_var->path_elements->count; i++) {
                                if (path_var->path_elements->items[i]->type == AST_NODE_REL_PATTERN)
                                    rel_count++;
                            }
                            for (int i = 0; i < path_var->path_elements->count; i++) {
                                ast_node *element = path_var->path_elements->items[i];
                                if (element->type == AST_NODE_REL_PATTERN) {
                                    cypher_rel_pattern *rel = (cypher_rel_pattern*)element;
                                    if (rel->varlen) {
                                        has_varlen = true;
                                        if (rel->variable) {
                                            varlen_alias = transform_var_get_alias(ctx->var_ctx, rel->variable);
                                        }
                                        break;
                                    }
                                }
                            }
                            /* Only a single-varlen-segment path uses the interleaved
                             * elem_ids array; mixed fixed+varlen paths keep the legacy
                             * path_ids projection (Match7 [19]). */
                            bool single_varlen = has_varlen && rel_count == 1;

                            if (path_var->path_type == VAR_PATH_COMPREHENSION ||
                                (ctx->emit_hydrated_path && !single_varlen)) {
                                /* T-0332: pattern comprehension path — emit fully hydrated
                                 * path JSON ({nodes:[...], rels:[...]}) so it survives nested
                                 * inside a json_group_array aggregate without needing executor
                                 * post-processing of list elements. Build using json_object so
                                 * SQLite quotes/escapes correctly.
                                 * GQLITE-T-0340 sub-C: also used for a path as a list element
                                 * under UNWIND, where executor elem_ids hydration can't reach. */
                                append_sql(ctx, "json_object('nodes', json_array(");
                                bool first_n = true;
                                for (int i = 0; i < path_var->path_elements->count; i++) {
                                    ast_node *element = path_var->path_elements->items[i];
                                    if (element->type != AST_NODE_NODE_PATTERN) continue;
                                    cypher_node_pattern *np = (cypher_node_pattern*)element;
                                    const char *a = np->variable ? transform_var_get_alias(ctx->var_ctx, np->variable) : NULL;
                                    if (!first_n) append_sql(ctx, ", ");
                                    first_n = false;
                                    if (!a) { append_sql(ctx, "null"); continue; }
                                    {
                                        char id_expr[300];
                                        snprintf(id_expr, sizeof(id_expr), "%s.id", a);
                                        char *nj = gql_sql_node_json_expr("", id_expr);
                                        if (!nj) return -1;
                                        append_sql(ctx, "%s", nj);
                                        free(nj);
                                    }
                                }
                                append_sql(ctx, "), 'rels', json_array(");
                                bool first_r = true;
                                for (int i = 0; i < path_var->path_elements->count; i++) {
                                    ast_node *element = path_var->path_elements->items[i];
                                    if (element->type != AST_NODE_REL_PATTERN) continue;
                                    cypher_rel_pattern *rp = (cypher_rel_pattern*)element;
                                    const char *a = rp->variable ? transform_var_get_alias(ctx->var_ctx, rp->variable) : NULL;
                                    if (!first_r) append_sql(ctx, ", ");
                                    first_r = false;
                                    if (!a) { append_sql(ctx, "null"); continue; }
                                    {
                                        char id_e[300], ty_e[300], st_e[300], en_e[300];
                                        snprintf(id_e, sizeof(id_e), "%s.id", a);
                                        snprintf(ty_e, sizeof(ty_e), "%s.type", a);
                                        snprintf(st_e, sizeof(st_e), "%s.source_id", a);
                                        snprintf(en_e, sizeof(en_e), "%s.target_id", a);
                                        char *ej = gql_sql_edge_json_expr("", id_e, ty_e, st_e, en_e);
                                        if (!ej) return -1;
                                        append_sql(ctx, "%s", ej);
                                        free(ej);
                                    }
                                }
                                append_sql(ctx, "))");
                            } else if (has_varlen && varlen_alias) {
                                /* Single-varlen path: use the CTE's interleaved
                                 * elem_ids (node,edge,node,...) so the executor
                                 * hydrates the full path, not just endpoints
                                 * (Match6 [15]/[16]/[19]/[20]). Mixed fixed+varlen
                                 * paths keep the legacy node-only path_ids. */
                                if (single_varlen)
                                    append_sql(ctx, "'[' || %s.elem_ids || ']'", varlen_alias);
                                else
                                    append_sql(ctx, "'[' || %s.path_ids || ']'", varlen_alias);
                            } else {
                                /* Regular path - build from individual element IDs */
                                append_sql(ctx, "'[");
                                for (int i = 0; i < path_var->path_elements->count; i++) {
                                    if (i > 0) append_sql(ctx, ",");

                                    ast_node *element = path_var->path_elements->items[i];
                                    if (element->type == AST_NODE_NODE_PATTERN) {
                                        cypher_node_pattern *node = (cypher_node_pattern*)element;
                                        if (node->variable) {
                                            const char *node_alias = transform_var_get_alias(ctx->var_ctx, node->variable);
                                            if (node_alias && (strchr(node_alias, '.') ||
                                                    transform_var_alias_is_id(ctx->var_ctx, node->variable))) {
                                                /* A node from WITH: the alias is its id column. */
                                                append_sql(ctx, "' || %s || '", node_alias);
                                            } else if (node_alias) {
                                                append_sql(ctx, "' || %s.id || '", node_alias);
                                            } else {
                                                append_sql(ctx, "null");
                                            }
                                        } else {
                                            append_sql(ctx, "null");
                                        }
                                    } else if (element->type == AST_NODE_REL_PATTERN) {
                                        cypher_rel_pattern *rel = (cypher_rel_pattern*)element;
                                        if (rel->variable) {
                                            const char *rel_alias = transform_var_get_alias(ctx->var_ctx, rel->variable);
                                            if (rel_alias) {
                                                append_sql(ctx, "' || %s.id || '", rel_alias);
                                            } else {
                                                append_sql(ctx, "null");
                                            }
                                        } else {
                                            append_sql(ctx, "null");
                                        }
                                    }
                                }
                                append_sql(ctx, "]'");
                            }
                        } else {
                            append_sql(ctx, "'[]'");
                        }
                } else {
                    /* Non-path variables need an alias */
                    const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
                    if (alias) {
                        if (transform_var_is_projected(ctx->var_ctx, id->name)) {
                            /* This is a projected variable from WITH - alias is the full column reference.
                             * GQLITE-A-0007: in a value position, put back the
                             * boolean / JSON subtype from the tag column that
                             * crossed the CTE with it. Comparisons read the raw
                             * value. */
                            const char *tag = transform_var_get_tag(ctx->var_ctx, id->name);
                            if (tag && !ctx->in_comparison) {
                                append_sql(ctx, "_gql_retag(%s, %s)", alias, tag);
                            } else {
                                append_sql(ctx, "%s", alias);
                            }
                        } else if (transform_var_alias_is_id(ctx->var_ctx, id->name)) {
                            /* Post-WITH node/edge - alias IS the id value */
                            if (transform_var_is_edge(ctx->var_ctx, id->name)) {
                                /* Edge that passed through WITH - build relationship object */
                                /* Note: After WITH, we only have the id, not type/source/target.
                                 * Guard for NULL (OPTIONAL miss carried across WITH). */
                                {
                                    char ty_e[400], st_e[400], en_e[400];
                                    snprintf(ty_e, sizeof(ty_e), "(SELECT type FROM edges WHERE id = %s)", alias);
                                    snprintf(st_e, sizeof(st_e), "(SELECT source_id FROM edges WHERE id = %s)", alias);
                                    snprintf(en_e, sizeof(en_e), "(SELECT target_id FROM edges WHERE id = %s)", alias);
                                    char *ej = gql_sql_edge_json_expr("", alias, ty_e, st_e, en_e);
                                    if (!ej) return -1;
                                    append_sql(ctx, "(CASE WHEN %s IS NULL THEN NULL ELSE %s END)", alias, ej);
                                    free(ej);
                                }
                            } else {
                                /* Node that passed through WITH - build node object using id directly.
                                 * Guard for NULL (the id column can be NULL when the value came
                                 * through an OPTIONAL MATCH miss carried across WITH, e.g.
                                 * Match7 [27]); otherwise json_object('id', NULL, …) renders a
                                 * bogus non-null node instead of SQL NULL. */
                                {
                                    char *nj = gql_sql_node_json_expr("", alias);
                                    if (!nj) return -1;
                                    append_sql(ctx, "(CASE WHEN %s IS NULL THEN NULL ELSE %s END)", alias, nj);
                                    free(nj);
                                }
                            }
                        } else if (transform_var_is_edge(ctx->var_ctx, id->name)) {
                            /* T-0309: varlen-bound edge variable holds a path
                             * of edges (CTE alias has columns start_id/end_id/
                             * depth/path_ids/visited, not edge fields). Emit
                             * a JSON array constructed from path_ids — one
                             * edge JSON per id, in path order. */
                            transform_var *evar = transform_var_lookup_edge(ctx->var_ctx, id->name);
                            if (evar && evar->cte_name) {
                                /* I-0047 P4: guard for an OPTIONAL varlen miss.
                                 * When the path didn't match, the CTE alias's
                                 * elem_ids is NULL; json_each(NULL) yields no
                                 * rows and json_group_array() returns '[]' (an
                                 * empty array), but openCypher wants NULL for an
                                 * unmatched OPTIONAL relationship list
                                 * (Match9 [9]). */
                                {
                                    char *ej = gql_sql_edge_json_expr("", "e.id", "e.type", "e.source_id", "e.target_id");
                                    if (!ej) return -1;
                                    append_sql(ctx,
                                      "(CASE WHEN %s.elem_ids IS NULL THEN NULL ELSE "
                                      "(SELECT json_group_array(%s ORDER BY je.key) "
                                      "FROM json_each('[' || %s.elem_ids || ']') je "
                                      "JOIN edges e ON e.id = je.value WHERE (je.key %% 2) = 1) END)",
                                      alias, ej, alias);
                                    free(ej);
                                }
                                goto edge_alias_projection_done;
                            }
                            /* Edge variable - return full relationship object,
                             * or NULL when the row came from an OPTIONAL MATCH
                             * miss (LEFT JOIN with no match → alias.id IS NULL). */
                            {
                                char id_e[300], ty_e[300], st_e[300], en_e[300];
                                snprintf(id_e, sizeof(id_e), "%s.id", alias);
                                snprintf(ty_e, sizeof(ty_e), "%s.type", alias);
                                snprintf(st_e, sizeof(st_e), "%s.source_id", alias);
                                snprintf(en_e, sizeof(en_e), "%s.target_id", alias);
                                char *ej = gql_sql_edge_json_expr("", id_e, ty_e, st_e, en_e);
                                if (!ej) return -1;
                                append_sql(ctx, "(CASE WHEN %s.id IS NULL THEN NULL ELSE %s END)", alias, ej);
                                free(ej);
                            }
                            edge_alias_projection_done: ;
                        } else {
                            /* This is a node variable - return full node object,
                             * or NULL when the row came from an OPTIONAL MATCH
                             * miss (LEFT JOIN with no match → alias.id IS NULL). */
                            {
                                char id_e[300];
                                snprintf(id_e, sizeof(id_e), "%s.id", alias);
                                char *nj = gql_sql_node_json_expr("", id_e);
                                if (!nj) return -1;
                                append_sql(ctx, "(CASE WHEN %s.id IS NULL THEN NULL ELSE %s END)", alias, nj);
                                free(nj);
                            }
                        }
                    } else {
                        /* Unknown identifier */
                        ctx->has_error = true;
                        char error[256];
                        snprintf(error, sizeof(error), "Unknown variable: %s", id->name);
                        ctx->error_message = strdup(error);
                        return -1;
                    }
                }
            }
            break;
            
        case AST_NODE_PROPERTY:
            return transform_property_access(ctx, (cypher_property*)expr);
            
        case AST_NODE_LABEL_EXPR:
            return transform_label_expression(ctx, (cypher_label_expr*)expr);
            
        case AST_NODE_NOT_EXPR:
            return transform_not_expression(ctx, (cypher_not_expr*)expr);

        case AST_NODE_NULL_CHECK:
            return transform_null_check(ctx, (cypher_null_check*)expr);

        case AST_NODE_BINARY_OP:
            return transform_binary_operation(ctx, (cypher_binary_op*)expr);
            
        case AST_NODE_FUNCTION_CALL:
            return transform_function_call(ctx, (cypher_function_call*)expr);
            
        case AST_NODE_EXISTS_EXPR:
            return transform_exists_expression(ctx, (cypher_exists_expr*)expr);

        case AST_NODE_LIST_PREDICATE:
            return transform_list_predicate(ctx, (cypher_list_predicate*)expr);

        case AST_NODE_REDUCE_EXPR:
            return transform_reduce_expr(ctx, (cypher_reduce_expr*)expr);

        case AST_NODE_SUBSCRIPT:
            {
                /* Transform list[index] to json_extract with negative index support
                 * Negative indices count from end: list[-1] = last element
                 * SQL: json_extract(list, '$[' || CASE WHEN idx < 0
                 *        THEN json_array_length(list) + idx ELSE idx END || ']')
                 *
                 * String-key subscripts on identifiers/properties are normalized to
                 * property access: n['status'] → n.status, n['a']['b'] → n.a.b
                 */
                cypher_subscript *subscript = (cypher_subscript*)expr;

                /* Handle list slicing: list[start..end] */
                if (subscript->is_slice) {
                    /* Generate a slice that:
                     *  - normalizes negative start/end to length+offset
                     *  - returns NULL if either bound is NULL (Cypher
                     *    three-valued semantics)
                     *  - returns json_group_array(value) for matching keys.
                     *
                     * Shape:
                     *   CASE WHEN <start> IS NULL OR <end> IS NULL THEN NULL
                     *        ELSE (SELECT json_group_array(value)
                     *              FROM json_each(<list>)
                     *              WHERE key >= CASE WHEN <start> < 0 THEN
                     *                          json_array_length(<list>) + <start>
                     *                        ELSE <start> END
                     *                AND key < CASE WHEN <end> < 0 THEN
                     *                          json_array_length(<list>) + <end>
                     *                        ELSE <end> END)
                     *   END
                     */
                    append_sql(ctx, "(CASE WHEN ");
                    bool has_start = subscript->slice_start != NULL;
                    bool has_end = subscript->slice_end != NULL;
                    if (has_start) {
                        append_sql(ctx, "(");
                        if (transform_expression(ctx, subscript->slice_start) < 0) return -1;
                        append_sql(ctx, ") IS NULL");
                    }
                    if (has_start && has_end) append_sql(ctx, " OR ");
                    if (has_end) {
                        append_sql(ctx, "(");
                        if (transform_expression(ctx, subscript->slice_end) < 0) return -1;
                        append_sql(ctx, ") IS NULL");
                    }
                    if (!has_start && !has_end) append_sql(ctx, "0");
                    append_sql(ctx, " THEN NULL ELSE ");

                    append_sql(ctx, "(SELECT json_group_array(" JSON_EACH_ELEMENT ") FROM json_each(");
                    if (transform_expression(ctx, subscript->expr) < 0) return -1;
                    append_sql(ctx, ")");
                    if (has_start || has_end) {
                        append_sql(ctx, " WHERE ");
                        if (has_start) {
                            append_sql(ctx, "key >= (CASE WHEN (");
                            if (transform_expression(ctx, subscript->slice_start) < 0) return -1;
                            append_sql(ctx, ") < 0 THEN json_array_length(");
                            if (transform_expression(ctx, subscript->expr) < 0) return -1;
                            append_sql(ctx, ") + (");
                            if (transform_expression(ctx, subscript->slice_start) < 0) return -1;
                            append_sql(ctx, ") ELSE (");
                            if (transform_expression(ctx, subscript->slice_start) < 0) return -1;
                            append_sql(ctx, ") END)");
                        }
                        if (has_start && has_end) append_sql(ctx, " AND ");
                        if (has_end) {
                            append_sql(ctx, "key < (CASE WHEN (");
                            if (transform_expression(ctx, subscript->slice_end) < 0) return -1;
                            append_sql(ctx, ") < 0 THEN json_array_length(");
                            if (transform_expression(ctx, subscript->expr) < 0) return -1;
                            append_sql(ctx, ") + (");
                            if (transform_expression(ctx, subscript->slice_end) < 0) return -1;
                            append_sql(ctx, ") ELSE (");
                            if (transform_expression(ctx, subscript->slice_end) < 0) return -1;
                            append_sql(ctx, ") END)");
                        }
                    }
                    append_sql(ctx, ") END)");
                    break;
                }

                /* Normalize string-key subscript to property access */
                if (subscript->index->type == AST_NODE_LITERAL) {
                    cypher_literal *idx_lit = (cypher_literal*)subscript->index;
                    if (idx_lit->literal_type == LITERAL_STRING) {
                        if (subscript->expr->type == AST_NODE_IDENTIFIER ||
                            subscript->expr->type == AST_NODE_PROPERTY ||
                            subscript->expr->type == AST_NODE_SUBSCRIPT) {
                            /* Rewrite n['key'] / n['a']['b'] as property access */
                            cypher_property temp_prop;
                            memset(&temp_prop, 0, sizeof(temp_prop));
                            temp_prop.base.type = AST_NODE_PROPERTY;
                            temp_prop.expr = subscript->expr;
                            temp_prop.property_name = idx_lit->value.string;
                            return transform_property_access(ctx, &temp_prop);
                        }
                    }
                }

                /* Dynamic (non-literal) key on a node/edge variable:
                 * `n['na'+'me']` / `n[$key]`. Resolve via a runtime EAV
                 * lookup by key (Graph7 [1]-[3]). The base node/edge isn't a
                 * JSON object in scope, so _gql_subscript on the hydrated
                 * object can't find the property. */
                if (subscript->expr->type == AST_NODE_IDENTIFIER) {
                    cypher_identifier *bid = (cypher_identifier*)subscript->expr;
                    transform_var *bv = transform_var_lookup(ctx->var_ctx, bid->name);
                    if (bv && (bv->kind == VAR_KIND_NODE || bv->kind == VAR_KIND_EDGE)) {
                        const char *balias = transform_var_get_alias(ctx->var_ctx, bid->name);
                        bool alias_is_id = transform_var_alias_is_id(ctx->var_ctx, bid->name);
                        append_sql(ctx, "%s(%s%s, ",
                                   bv->kind == VAR_KIND_EDGE ? "_gql_edge_prop" : "_gql_node_prop",
                                   balias ? balias : bid->name,
                                   alias_is_id ? "" : ".id");
                        if (transform_expression(ctx, subscript->index) < 0) return -1;
                        append_sql(ctx, ")");
                        break;
                    }
                }

                /* Route through _gql_subscript() which performs runtime
                 * type checking (TypeError for non-list/map values or
                 * mismatched index types) and handles negative indices.
                 * Falls back to plain json_extract semantics when types
                 * match. */
                /* _gql_subscript gives a list's JSON true/false as a
                 * boolean ('true'/'false', GQLITE-T-0387); in a comparison
                 * turn it back into 1/0, which is what `= true` compares. */
                append_sql(ctx, ctx->in_comparison ? "_gql_bool(_gql_subscript(" : "_gql_subscript(");
                if (transform_expression(ctx, subscript->expr) < 0) return -1;
                append_sql(ctx, ", ");
                if (transform_expression(ctx, subscript->index) < 0) return -1;
                append_sql(ctx, ctx->in_comparison ? "))" : ")");
            }
            break;

        case AST_NODE_LITERAL:
            {
                cypher_literal *lit = (cypher_literal*)expr;
                switch (lit->literal_type) {
                    case LITERAL_INTEGER:
                        append_sql(ctx, "%lld", (long long)lit->value.integer);
                        break;
                    case LITERAL_DECIMAL: {
                        /* %.17g preserves full double precision while
                         * avoiding trailing zeros %f would emit. But if
                         * the result has no '.' or 'e' (whole-number
                         * decimals like 2.0 → "2"), SQLite parses it as
                         * an INTEGER and integer arithmetic kicks in
                         * (1 / 2 = 0 instead of 1 / 2.0 = 0.5). Force a
                         * decimal point by appending '.0' in that case. */
                        char buf[64];
                        snprintf(buf, sizeof(buf), "%.17g", lit->value.decimal);
                        bool has_dot = false;
                        for (char *p = buf; *p; p++) {
                            if (*p == '.' || *p == 'e' || *p == 'E' || *p == 'n') { /* 'n' for nan/inf */
                                has_dot = true; break;
                            }
                        }
                        if (!has_dot) {
                            size_t l = strlen(buf);
                            if (l + 2 < sizeof(buf)) { buf[l] = '.'; buf[l+1] = '0'; buf[l+2] = 0; }
                        }
                        append_sql(ctx, "%s", buf);
                        break;
                    }
                    case LITERAL_STRING:
                        append_string_literal(ctx, lit->value.string);
                        break;
                    case LITERAL_BOOLEAN:
                        append_sql(ctx, "%d", lit->value.boolean ? 1 : 0);
                        break;
                    case LITERAL_NULL:
                        append_sql(ctx, "NULL");
                        break;
                }
            }
            break;

        case AST_NODE_PARAMETER:
            {
                /* Transform parameter $name to SQLite named parameter :name */
                cypher_parameter *param = (cypher_parameter*)expr;
                if (param->name) {
                    /* Register parameter for tracking */
                    register_parameter(ctx, param->name);
                    if (ctx->in_comparison) {
                        append_sql(ctx, ":%s", param->name);
                    } else {
                        /* GQLITE-T-0395: the binder sets :name__gqlt to the
                         * JSON type of the value, so a boolean or a list
                         * keeps its type in a value position. */
                        append_sql(ctx, "_gql_retag(:%s, :%s__gqlt)", param->name, param->name);
                    }
                } else {
                    /* Unnamed parameter - use positional placeholder */
                    append_sql(ctx, "?");
                }
            }
            break;

        case AST_NODE_LIST:
            {
                /* Transform list to JSON array using _gql_list (T-0304):
                 * honors GQL_SUBTYPE_BOOLEAN so [true, false] renders as
                 * [true, false] rather than [1, 0], and tags its output
                 * with the JSON subtype so nested lists embed cleanly. */
                cypher_list *list = (cypher_list*)expr;
                if (emit_folded_literal(ctx, expr)) break;
                append_sql(ctx, "_gql_list(");
                if (list->items) {
                    for (int i = 0; i < list->items->count; i++) {
                        if (i > 0) {
                            append_sql(ctx, ", ");
                        }
                        if (transform_list_item_value(ctx, list->items->items[i]) < 0) {
                            return -1;
                        }
                    }
                }
                append_sql(ctx, ")");
            }
            break;

        case AST_NODE_CASE_EXPR:
            {
                /* Transform CASE expression - two forms:
                 *   Searched: CASE WHEN cond THEN val END -> CASE WHEN cond THEN val END
                 *   Simple:   CASE expr WHEN val THEN result END -> CASE WHEN expr = val THEN result END
                 */
                cypher_case_expr *case_expr = (cypher_case_expr*)expr;

                if (!case_expr->when_clauses || case_expr->when_clauses->count == 0) {
                    ctx->has_error = true;
                    ctx->error_message = strdup("CASE expression requires at least one WHEN clause");
                    return -1;
                }

                append_sql(ctx, "CASE");

                for (int i = 0; i < case_expr->when_clauses->count; i++) {
                    cypher_when_clause *when = (cypher_when_clause*)case_expr->when_clauses->items[i];

                    append_sql(ctx, " WHEN ");

                    if (case_expr->operand) {
                        /* Simple CASE: generate "operand = when_value" */
                        append_sql(ctx, "(");
                        if (transform_expression(ctx, case_expr->operand) < 0) {
                            return -1;
                        }
                        append_sql(ctx, ") = (");
                        if (transform_expression(ctx, when->condition) < 0) {
                            return -1;
                        }
                        append_sql(ctx, ")");
                    } else {
                        /* Searched CASE: the condition is a boolean position */
                        if (transform_condition(ctx, when->condition) < 0) {
                            return -1;
                        }
                    }

                    append_sql(ctx, " THEN ");
                    if (transform_expression(ctx, when->result) < 0) {
                        return -1;
                    }
                }

                if (case_expr->else_expr) {
                    append_sql(ctx, " ELSE ");
                    if (transform_expression(ctx, case_expr->else_expr) < 0) {
                        return -1;
                    }
                }

                append_sql(ctx, " END");
            }
            break;

        case AST_NODE_MAP:
            {
                /* Transform map literal using _gql_map (T-0304): honors
                 * GQL_SUBTYPE_BOOLEAN for values and tags result with
                 * the JSON subtype. */
                cypher_map *map = (cypher_map*)expr;
                if (emit_folded_literal(ctx, expr)) break;
                append_sql(ctx, "_gql_map(");
                if (map->pairs) {
                    for (int i = 0; i < map->pairs->count; i++) {
                        if (i > 0) {
                            append_sql(ctx, ", ");
                        }
                        cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[i];
                        { char *ek = escape_sql_string(pair->key);
                          append_sql(ctx, "'%s', ", ek ? ek : pair->key);
                          free(ek); }
                        if (transform_list_item_value(ctx, pair->value) < 0) {
                            return -1;
                        }
                    }
                }
                append_sql(ctx, ")");
            }
            break;

        case AST_NODE_MAP_PROJECTION:
            {
                /* Transform map projection n{.prop1, .prop2} to json_object() */
                cypher_map_projection *proj = (cypher_map_projection*)expr;

                /* Get the base variable alias */
                const char *base_alias = NULL;
                const char *base_name = NULL;
                if (proj->base_expr && proj->base_expr->type == AST_NODE_IDENTIFIER) {
                    cypher_identifier *ident = (cypher_identifier*)proj->base_expr;
                    base_name = ident->name;
                    base_alias = transform_var_get_alias(ctx->var_ctx, ident->name);
                    if (!base_alias) {
                        ctx->has_error = true;
                        char error[256];
                        snprintf(error, sizeof(error), "Unknown variable in map projection: %s", ident->name);
                        ctx->error_message = strdup(error);
                        return -1;
                    }
                }

                bool is_projected = transform_var_is_projected(ctx->var_ctx, base_name);

                /* Check if we have .* (all properties) - use properties() function approach */
                bool has_all_props = false;
                if (proj->items && proj->items->count == 1) {
                    cypher_map_projection_item *item = (cypher_map_projection_item*)proj->items->items[0];
                    if (item->property && strcmp(item->property, "*") == 0) {
                        has_all_props = true;
                    }
                }

                if (has_all_props && base_alias) {
                    /* Use properties() function approach for n{.*} */
                    {
                        char id_e[300];
                        snprintf(id_e, sizeof(id_e), "%s%s", base_alias, is_projected ? "" : ".id");
                        bool proj_edge = base_name && transform_var_is_edge(ctx->var_ctx, base_name);
                        char *pe = proj_edge ? gql_sql_props_expr("edge", "edge_id", "", id_e)
                                             : gql_sql_props_expr("node", "node_id", "", id_e);
                        if (!pe) return -1;
                        append_sql(ctx, "%s", pe);
                        free(pe);
                    }
                } else {
                    append_sql(ctx, "json_object(");
                    if (proj->items) {
                    for (int i = 0; i < proj->items->count; i++) {
                        if (i > 0) {
                            append_sql(ctx, ", ");
                        }
                        cypher_map_projection_item *item = (cypher_map_projection_item*)proj->items->items[i];

                        /* Output key name */
                        const char *key = item->key ? item->key : item->property;
                        { char *ek = escape_sql_string(key);
                          append_sql(ctx, "'%s', ", ek ? ek : key);
                          free(ek); }

                        /* Output value */
                        if (item->property && base_alias) {
                            /* The normal property read: nodes and relationships
                             * (GQLITE-T-0412: a relationship base read the node
                             * tables and gave null), typed; a boolean stays a
                             * JSON boolean in the object. */
                            ast_node *pa = (ast_node *)make_property(
                                (ast_node *)make_identifier((char *)base_name, 0),
                                item->property, 0);
                            append_sql(ctx, "_gql_json_elem(");
                            bool saved_cmp = ctx->in_comparison;
                            ctx->in_comparison = false;
                            int prc = transform_expression(ctx, pa);
                            ctx->in_comparison = saved_cmp;
                            ast_node_free(pa);
                            if (prc < 0) return -1;
                            append_sql(ctx, ")");
                        } else if (item->expr) {
                            /* Computed expression; a boolean stays a JSON
                             * boolean (GQLITE-T-0404). */
                            append_sql(ctx, "_gql_json_elem(");
                            if (transform_expression(ctx, item->expr) < 0) {
                                return -1;
                            }
                            append_sql(ctx, ")");
                        }
                    }
                    }
                    append_sql(ctx, ")");
                }
            }
            break;

        case AST_NODE_LIST_COMPREHENSION:
            {
                /* Transform list comprehension: [x IN list WHERE cond | transform]
                 * to: (SELECT json_group_array(transform_expr) FROM json_each(list_expr) WHERE cond_expr)
                 */
                cypher_list_comprehension *comp = (cypher_list_comprehension*)expr;

                if (!comp->list_expr) {
                    ctx->has_error = true;
                    ctx->error_message = strdup("List comprehension requires list expression");
                    return -1;
                }

                /* Store the comprehension variable for use in nested expressions */
                const char *comp_var = comp->variable;

                /* Save the old alias if this variable name already exists */
                const char *old_alias = transform_var_get_alias(ctx->var_ctx, comp_var);
                char *saved_alias = old_alias ? strdup(old_alias) : NULL;

                /* Register the comprehension variable to map to json_each.value */
                transform_var_register_projected(ctx->var_ctx, comp_var, "json_each.value");

                /* Build the subquery */
                append_sql(ctx, "(SELECT json_group_array(");

                /* The result expression - either the transform or the element itself */
                if (comp->transform_expr &&
                    !(comp->transform_expr->type == AST_NODE_IDENTIFIER &&
                      strcmp(((cypher_identifier*)comp->transform_expr)->name, comp_var) == 0)) {
                    /* Boolean results stay JSON booleans in the list: SQLite's
                     * json_group_array only keeps its own JSON subtype
                     * (GQLITE-T-0387). */
                    bool as_bool = ast_yields_boolean(comp->transform_expr);
                    if (as_bool) append_sql(ctx, "json(CASE _gql_bool(");
                    if (transform_expression(ctx, comp->transform_expr) < 0) {
                        if (saved_alias) free(saved_alias);
                        return -1;
                    }
                    if (as_bool) append_sql(ctx, ") WHEN 1 THEN 'true' WHEN 0 THEN 'false' END)");
                } else {
                    /* The element itself ([x IN l] or [x IN l | x]); JSON true/false stay booleans
                     * (json_each.value gives 1/0, GQLITE-T-0387) */
                    append_sql(ctx, JSON_EACH_ELEMENT);
                }

                append_sql(ctx, ") FROM json_each(");

                /* The source list - transform BEFORE adding comprehension variable binding */
                if (transform_expression(ctx, comp->list_expr) < 0) {
                    if (saved_alias) free(saved_alias);
                    return -1;
                }

                append_sql(ctx, ")");

                /* Optional WHERE filter */
                if (comp->where_expr) {
                    append_sql(ctx, " WHERE ");
                    if (transform_condition(ctx, comp->where_expr) < 0) {
                        if (saved_alias) free(saved_alias);
                        return -1;
                    }
                }

                append_sql(ctx, ")");

                /* Restore the old alias if there was one, otherwise remove the variable */
                if (saved_alias) {
                    transform_var_register_projected(ctx->var_ctx, comp_var, saved_alias);
                    free(saved_alias);
                }
                /* If there was no old alias, leave the variable as is - it won't conflict
                 * with anything since list comprehension creates a new scope */
            }
            break;

        case AST_NODE_PATTERN_COMPREHENSION:
            {
                /* Transform pattern comprehension: [(n)-[r]->(m) WHERE cond | expr]
                 * to: (SELECT json_group_array(expr) FROM nodes n, edges r, nodes m
                 *      WHERE r.source_id = n.id AND r.target_id = m.id [AND cond])
                 */
                cypher_pattern_comprehension *comp = (cypher_pattern_comprehension*)expr;

                if (!comp->pattern || comp->pattern->count == 0) {
                    ctx->has_error = true;
                    ctx->error_message = strdup("Pattern comprehension requires a pattern");
                    return -1;
                }

                if (!comp->collect_expr) {
                    ctx->has_error = true;
                    ctx->error_message = strdup("Pattern comprehension requires a collect expression");
                    return -1;
                }

                /* Get the first (and only) path from the pattern */
                ast_node *pattern = comp->pattern->items[0];
                if (pattern->type != AST_NODE_PATH) {
                    ctx->has_error = true;
                    ctx->error_message = strdup("Pattern comprehension requires a path pattern");
                    return -1;
                }

                cypher_path *path = (cypher_path*)pattern;
                if (!path->elements || path->elements->count == 0) {
                    ctx->has_error = true;
                    ctx->error_message = strdup("Pattern comprehension path is empty");
                    return -1;
                }

                /* Save current variable count to restore later */
                int saved_var_count = transform_var_count(ctx->var_ctx);

                /* Track node aliases and whether they're external */
                char node_aliases[10][32];
                char node_vars[10][32];  /* Variable names from the pattern */
                /* T-0370: SQL expression yielding each node's id. A node
                 * from the comprehension's own FROM is `_pc_nN.id`; an
                 * outer MATCH node is `<alias>.id`; a post-WITH node is
                 * its alias (alias_is_id); a projected outer variable (a
                 * list-comprehension element — Pattern2 [7]) is either a
                 * raw id or an entity object, resolved at run time. */
                char node_ids[10][400];
                bool node_is_outer[10];
                int node_count = 0;

                /* First pass: collect nodes and generate FROM clause */
                append_sql(ctx, "(SELECT json_group_array(");

                /* We'll add the collect expression after setting up variables */
                size_t collect_expr_pos = ctx->sql_size;

                /* Placeholder - we'll come back to fill in the collect expression */
                append_sql(ctx, "/*COLLECT*/");

                append_sql(ctx, ") FROM ");

                bool first_table = true;

                /* Process each element in the path */
                for (int i = 0; i < path->elements->count; i++) {
                    ast_node *element = path->elements->items[i];

                    if (element->type == AST_NODE_NODE_PATTERN) {
                        cypher_node_pattern *node = (cypher_node_pattern*)element;

                        /* Check if this node variable exists in outer context */
                        const char *outer_alias = NULL;
                        if (node->variable) {
                            outer_alias = transform_var_get_alias(ctx->var_ctx, node->variable);
                        }

                        if (outer_alias) {
                            /* Use alias from outer query - don't add to FROM */
                            strncpy(node_aliases[node_count], outer_alias,
                                   sizeof(node_aliases[node_count]) - 1);
                            node_aliases[node_count][sizeof(node_aliases[node_count]) - 1] = '\0';
                            node_is_outer[node_count] = true;
                            if (transform_var_alias_is_id(ctx->var_ctx, node->variable)) {
                                snprintf(node_ids[node_count], sizeof(node_ids[node_count]), "%s", outer_alias);
                            } else if (transform_var_is_projected(ctx->var_ctx, node->variable)) {
                                snprintf(node_ids[node_count], sizeof(node_ids[node_count]),
                                         "(CASE WHEN json_valid(%s) AND json_type(%s) = 'object' "
                                         "THEN json_extract(%s, '$.id') ELSE %s END)",
                                         outer_alias, outer_alias, outer_alias, outer_alias);
                            } else {
                                snprintf(node_ids[node_count], sizeof(node_ids[node_count]), "%s.id", outer_alias);
                            }
                        } else {
                            node_is_outer[node_count] = false;
                            /* Generate new alias and add to FROM */
                            if (!first_table) {
                                append_sql(ctx, ", ");
                            }
                            snprintf(node_aliases[node_count], sizeof(node_aliases[node_count]),
                                    "_pc_n%d", node_count);
                            append_sql(ctx, "nodes AS %s", node_aliases[node_count]);
                            snprintf(node_ids[node_count], sizeof(node_ids[node_count]), "%s.id", node_aliases[node_count]);
                            first_table = false;
                        }

                        /* Store variable name for later registration */
                        if (node->variable) {
                            strncpy(node_vars[node_count], node->variable,
                                   sizeof(node_vars[node_count]) - 1);
                            node_vars[node_count][sizeof(node_vars[node_count]) - 1] = '\0';
                        } else {
                            node_vars[node_count][0] = '\0';
                        }
                        node_count++;

                    } else if (element->type == AST_NODE_REL_PATTERN && i > 0) {
                        /* Relationship pattern: -[variable:TYPE]-> */
                        if (!first_table) {
                            append_sql(ctx, ", ");
                        }
                        append_sql(ctx, "edges AS _pc_e%d", i/2);
                        first_table = false;
                    }
                }

                /* Register pattern variables for use in expressions.
                 * T-0370: an outer-bound variable keeps its own
                 * registration (re-registering a projected element as a
                 * node would break `x.prop` inside the comprehension). */
                for (int i = 0; i < node_count; i++) {
                    if (node_vars[i][0] != '\0' && !node_is_outer[i]) {
                        transform_var_register_node(ctx->var_ctx, node_vars[i], node_aliases[i], NULL);
                    }
                }

                /* T-0332: register relationship variables so r.prop / r.type
                 * resolve inside the projection. The rel's alias is
                 * `_pc_e<rel_index>` matching the FROM-clause emission above.
                 * Also synthesize a variable on anonymous rels when there is
                 * a path-assignment so the path projection can reference the
                 * rel's id (otherwise it emits `null` for the rel slot). */
                {
                    int rel_i = 0;
                    for (int i = 0; i < path->elements->count; i++) {
                        ast_node *element = path->elements->items[i];
                        if (element->type == AST_NODE_REL_PATTERN && i > 0 && i < path->elements->count - 1) {
                            cypher_rel_pattern *rel = (cypher_rel_pattern*)element;
                            if (!rel->variable || !rel->variable[0]) {
                                if (comp->path_var) {
                                    char gen[32];
                                    snprintf(gen, sizeof(gen), "_pc_relv%d", rel_i);
                                    rel->variable = strdup(gen);
                                }
                            }
                            if (rel->variable && rel->variable[0]) {
                                char alias[32];
                                snprintf(alias, sizeof(alias), "_pc_e%d", rel_i);
                                transform_var_register_edge(ctx->var_ctx, rel->variable, alias, NULL);
                            }
                            rel_i++;
                        }
                    }
                }

                /* T-0332: also synthesize variable names on anonymous nodes
                 * when there is a path-assignment, and register the path. */
                if (comp->path_var) {
                    for (int i = 0; i < path->elements->count; i++) {
                        ast_node *element = path->elements->items[i];
                        if (element->type == AST_NODE_NODE_PATTERN) {
                            cypher_node_pattern *np = (cypher_node_pattern*)element;
                            if (!np->variable || !np->variable[0]) {
                                int slot = i / 2;
                                char gen[32];
                                snprintf(gen, sizeof(gen), "_pc_nv%d", slot);
                                np->variable = strdup(gen);
                                char alias[32];
                                snprintf(alias, sizeof(alias), "_pc_n%d", slot);
                                transform_var_register_node(ctx->var_ctx, np->variable, alias, NULL);
                            }
                        }
                    }
                    transform_var_register_path(ctx->var_ctx, comp->path_var, NULL,
                                                path->elements, VAR_PATH_COMPREHENSION);
                }

                /* Add WHERE clause for joins and constraints */
                append_sql(ctx, " WHERE ");

                bool first_condition = true;
                int rel_index = 0;

                /* Generate join conditions between nodes and relationships */
                for (int i = 0; i < path->elements->count; i++) {
                    ast_node *element = path->elements->items[i];

                    if (element->type == AST_NODE_REL_PATTERN && i > 0 && i < path->elements->count - 1) {
                        cypher_rel_pattern *rel = (cypher_rel_pattern*)element;

                        if (!first_condition) {
                            append_sql(ctx, " AND ");
                        }

                        /* Join source node with relationship */
                        int source_node = i / 2;
                        int target_node = source_node + 1;

                        /* Handle direction */
                        if (rel->left_arrow && !rel->right_arrow) {
                            /* <-[r]- means target->source */
                            append_sql(ctx, "_pc_e%d.target_id = %s AND _pc_e%d.source_id = %s",
                                      rel_index, node_ids[source_node],
                                      rel_index, node_ids[target_node]);
                        } else if (!rel->left_arrow && !rel->right_arrow) {
                            /* T-0370: -[r]- is undirected — either
                             * orientation (Pattern2 [11]). */
                            append_sql(ctx, "((_pc_e%d.source_id = %s AND _pc_e%d.target_id = %s)"
                                            " OR (_pc_e%d.source_id = %s AND _pc_e%d.target_id = %s))",
                                      rel_index, node_ids[source_node],
                                      rel_index, node_ids[target_node],
                                      rel_index, node_ids[target_node],
                                      rel_index, node_ids[source_node]);
                        } else {
                            /* -[r]-> means source->target */
                            append_sql(ctx, "_pc_e%d.source_id = %s AND _pc_e%d.target_id = %s",
                                      rel_index, node_ids[source_node],
                                      rel_index, node_ids[target_node]);
                        }

                        /* Add relationship type constraint if specified */
                        if (rel->type) {
                            append_sql(ctx, " AND _pc_e%d.type = ", rel_index);
                            append_string_literal(ctx, rel->type);
                        }

                        rel_index++;
                        first_condition = false;

                    } else if (element->type == AST_NODE_NODE_PATTERN) {
                        cypher_node_pattern *node = (cypher_node_pattern*)element;

                        /* Add label constraints if specified - one condition per label */
                        if (has_labels(node)) {
                            for (int j = 0; j < node->labels->count; j++) {
                                const char *label = get_label_string(node->labels->items[j]);
                                if (label) {
                                    if (!first_condition) {
                                        append_sql(ctx, " AND ");
                                    }

                                    int current_node = (i == 0) ? 0 : i / 2;
                                    append_sql(ctx, "EXISTS (SELECT 1 FROM node_labels WHERE node_id = %s AND label = ",
                                              node_ids[current_node]);
                                    append_string_literal(ctx, label);
                                    append_sql(ctx, ")");
                                    first_condition = false;
                                }
                            }
                        }
                    }
                }

                /* If there were no constraints (e.g., just a node pattern), add TRUE */
                if (first_condition) {
                    append_sql(ctx, "1=1");
                }

                /* Add WHERE clause from pattern comprehension */
                if (comp->where_expr) {
                    append_sql(ctx, " AND (");
                    if (transform_condition(ctx, comp->where_expr) < 0) {
                        return -1;
                    }
                    append_sql(ctx, ")");
                }

                append_sql(ctx, ")");

                /* Now we need to go back and fill in the collect expression */
                /* Create a new buffer for the collect expression */
                /* Heap buffer: append_sql may realloc it when the collect
                 * expression is large (e.g. several entity projections). */
                size_t collect_cap = 4096;
                char *collect_sql = malloc(collect_cap);
                if (!collect_sql) return -1;

                /* Save current buffer state */
                char *temp_buffer = ctx->sql_buffer;
                size_t temp_size = ctx->sql_size;
                size_t temp_capacity = ctx->sql_capacity;

                /* Switch to collect buffer */
                ctx->sql_buffer = collect_sql;
                ctx->sql_size = 0;
                ctx->sql_capacity = collect_cap;

                /* Transform the collect expression */
                if (transform_expression(ctx, comp->collect_expr) < 0) {
                    free(ctx->sql_buffer);
                    ctx->sql_buffer = temp_buffer;
                    ctx->sql_size = temp_size;
                    ctx->sql_capacity = temp_capacity;
                    return -1;
                }

                /* Null terminate (the buffer may have been grown) */
                collect_sql = ctx->sql_buffer;
                collect_sql[ctx->sql_size] = '\0';

                /* Restore original buffer */
                ctx->sql_buffer = temp_buffer;
                ctx->sql_size = temp_size;
                ctx->sql_capacity = temp_capacity;

                /* Now replace the COLLECT placeholder with actual expression */
                char *placeholder = strstr(ctx->sql_buffer + collect_expr_pos, "/*COLLECT*/");
                if (placeholder) {
                    size_t collect_expr_len = strlen(collect_sql);
                    size_t placeholder_len = strlen("/*COLLECT*/");
                    size_t after_len = strlen(placeholder + placeholder_len);

                    /* Calculate required size after replacement */
                    size_t new_size = ctx->sql_size - placeholder_len + collect_expr_len;

                    /* Grow buffer if needed */
                    if (new_size >= ctx->sql_capacity) {
                        size_t new_capacity = ctx->sql_capacity * 2;
                        while (new_capacity <= new_size) {
                            new_capacity *= 2;
                        }

                        /* Calculate placeholder offset before realloc */
                        size_t placeholder_offset = placeholder - ctx->sql_buffer;

                        char *new_buffer = realloc(ctx->sql_buffer, new_capacity);
                        if (!new_buffer) {
                            ctx->has_error = true;
                            ctx->error_message = strdup("Out of memory expanding buffer for pattern comprehension");
                            free(collect_sql);
                            return -1;
                        }

                        ctx->sql_buffer = new_buffer;
                        ctx->sql_capacity = new_capacity;

                        /* Update placeholder pointer after realloc */
                        placeholder = ctx->sql_buffer + placeholder_offset;
                    }

                    /* Now perform the replacement */
                    /* Shift content after placeholder */
                    memmove(placeholder + collect_expr_len,
                           placeholder + placeholder_len,
                           after_len + 1);
                    /* Copy collect expression */
                    memcpy(placeholder, collect_sql, collect_expr_len);
                    /* Update size */
                    ctx->sql_size = new_size;
                }
                free(collect_sql);

                /* Restore variable count (remove pattern-local variables) */
                transform_var_truncate_to(ctx->var_ctx, saved_var_count);
            }
            break;

        default:
            ctx->has_error = true;
            char error[256];
            snprintf(error, sizeof(error), "Unsupported expression type: %s",
                    ast_node_type_name(expr->type));
            ctx->error_message = strdup(error);
            return -1;
    }
    
    return 0;
}
