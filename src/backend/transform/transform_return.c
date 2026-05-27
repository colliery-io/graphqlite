/*
 * RETURN clause transformation
 * Converts RETURN items into SQL SELECT projections
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "transform/cypher_transform.h"
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

/*
 * Pending property JOINs buffer for aggregation optimization.
 * These are accumulated during RETURN item processing and injected
 * into the FROM clause before it's appended back.
 *
 * Uses a dynamically growing buffer to handle arbitrary query complexity.
 */
#define PENDING_JOINS_INITIAL_CAP 1024

void reset_pending_prop_joins(cypher_transform_context *ctx)
{
    if (ctx->pending_prop_joins) {
        ctx->pending_prop_joins[0] = '\0';
    }
    ctx->pending_prop_joins_len = 0;
}

const char* get_pending_prop_joins(cypher_transform_context *ctx)
{
    return ctx->pending_prop_joins ? ctx->pending_prop_joins : "";
}

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

size_t get_pending_prop_joins_len(cypher_transform_context *ctx)
{
    return ctx->pending_prop_joins_len;
}

void add_pending_prop_join(cypher_transform_context *ctx, const char *join_sql)
{
    if (!join_sql) return;

    size_t len = strlen(join_sql);
    size_t needed = ctx->pending_prop_joins_len + len + 1;

    /* Initialize buffer on first use */
    if (!ctx->pending_prop_joins) {
        size_t cap = PENDING_JOINS_INITIAL_CAP;
        while (cap < needed) cap *= 2;
        ctx->pending_prop_joins = malloc(cap);
        if (!ctx->pending_prop_joins) return;
        ctx->pending_prop_joins[0] = '\0';
        ctx->pending_prop_joins_cap = cap;
    }

    /* Grow buffer if needed */
    if (needed > ctx->pending_prop_joins_cap) {
        size_t new_cap = ctx->pending_prop_joins_cap * 2;
        while (new_cap < needed) new_cap *= 2;
        char *new_buf = realloc(ctx->pending_prop_joins, new_cap);
        if (!new_buf) return;
        ctx->pending_prop_joins = new_buf;
        ctx->pending_prop_joins_cap = new_cap;
    }

    memcpy(ctx->pending_prop_joins + ctx->pending_prop_joins_len, join_sql, len + 1);
    ctx->pending_prop_joins_len += len;
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

    /* Reset pending property JOINs for this RETURN clause */
    reset_pending_prop_joins(ctx);

    if (!ctx || !ret) {
        return -1;
    }

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
                 * aliases generated for anonymous nodes/rels (With1 [1]/[2]). */
                if (strncmp(var->name, "_gql_default_alias_", 19) == 0 ||
                    strncmp(var->name, "__unnamed_rel_", 14) == 0) continue;

                /* Build expression for this variable based on its kind */
                if (var->kind == VAR_KIND_NODE) {
                    /* Return full node object using json_object */
                    char expr_buf[2048];
                    const char *alias = var->table_alias;
                    bool skip_id = var->alias_is_id;
                    snprintf(expr_buf, sizeof(expr_buf),
                        "(SELECT json_object("
                        "'id', %s%s, "
                        "'labels', COALESCE((SELECT json_group_array(label) FROM node_labels WHERE node_id = %s%s), json('[]')), "
                        "'properties', COALESCE((SELECT json_group_object(pk.key, COALESCE("
                            "(SELECT npt.value FROM node_props_text npt WHERE npt.node_id = %s%s AND npt.key_id = pk.id), "
                            "(SELECT npi.value FROM node_props_int npi WHERE npi.node_id = %s%s AND npi.key_id = pk.id), "
                            "(SELECT npr.value FROM node_props_real npr WHERE npr.node_id = %s%s AND npr.key_id = pk.id), "
                            "(SELECT json(CASE WHEN npb.value THEN 'true' ELSE 'false' END) FROM node_props_bool npb WHERE npb.node_id = %s%s AND npb.key_id = pk.id), "
                            "(SELECT json(npj.value) FROM node_props_json npj WHERE npj.node_id = %s%s AND npj.key_id = pk.id))) "
                        "FROM property_keys pk WHERE "
                            "EXISTS (SELECT 1 FROM node_props_text WHERE node_id = %s%s AND key_id = pk.id) OR "
                            "EXISTS (SELECT 1 FROM node_props_int WHERE node_id = %s%s AND key_id = pk.id) OR "
                            "EXISTS (SELECT 1 FROM node_props_real WHERE node_id = %s%s AND key_id = pk.id) OR "
                            "EXISTS (SELECT 1 FROM node_props_bool WHERE node_id = %s%s AND key_id = pk.id) OR "
                            "EXISTS (SELECT 1 FROM node_props_json WHERE node_id = %s%s AND key_id = pk.id)"
                        "), json('{}'))"
                        "))",
                        alias, skip_id ? "" : ".id",
                        alias, skip_id ? "" : ".id",
                        alias, skip_id ? "" : ".id",
                        alias, skip_id ? "" : ".id",
                        alias, skip_id ? "" : ".id",
                        alias, skip_id ? "" : ".id",
                        alias, skip_id ? "" : ".id",
                        alias, skip_id ? "" : ".id",
                        alias, skip_id ? "" : ".id",
                        alias, skip_id ? "" : ".id",
                        alias, skip_id ? "" : ".id",
                        alias, skip_id ? "" : ".id");
                    sql_select(ctx->unified_builder, expr_buf, var->name);
                } else if (var->kind == VAR_KIND_EDGE) {
                    /* Return edge as its type */
                    char expr_buf[256];
                    snprintf(expr_buf, sizeof(expr_buf), "%s.type", var->table_alias);
                    sql_select(ctx->unified_builder, expr_buf, var->name);
                } else if (var->kind == VAR_KIND_PROJECTED) {
                    /* Projected variable — the source expression (e.g. CTE
                     * column ref) lives on source_expr; table_alias is NULL.
                     * T-0333: use get_alias to pick the right one. */
                    const char *src = transform_var_get_alias(ctx->var_ctx, var->name);
                    if (src) sql_select(ctx->unified_builder, src, var->name);
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

            /* Add to unified builder */
            sql_select(ctx->unified_builder, expr_str, alias);
            free(expr_str);

            /* Defer alias registration so subsequent return items can still
             * resolve identifiers using their original bindings. */
            if (item->alias && deferred_aliases) {
                deferred_aliases[deferred_count++] = item->alias;
            }
        }
        for (int i = 0; i < deferred_count; i++) {
            transform_var_register_projected(ctx->var_ctx, deferred_aliases[i], deferred_aliases[i]);
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
                char *order_expr = transform_expression_to_string(ctx, order_item->expr);
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

        /* Add pending property JOINs from aggregate functions */
        if (ctx->pending_prop_joins_len > 0) {
            sql_join_raw(ctx->unified_builder, ctx->pending_prop_joins);
            reset_pending_prop_joins(ctx);
        }

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

                /* Add to unified builder */
                sql_select(ctx->unified_builder, expr_str, alias);
                free(expr_str);

                /* Register alias for ORDER BY reference */
                if (item->alias) {
                    transform_var_register_projected(ctx->var_ctx, item->alias, item->alias);
                }
            }

            /* Add ORDER BY */
            if (ret->order_by && ret->order_by->count > 0) {
                for (int i = 0; i < ret->order_by->count; i++) {
                    cypher_order_by_item *order_item = (cypher_order_by_item*)ret->order_by->items[i];
                    char *order_expr = transform_expression_to_string(ctx, order_item->expr);
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

/* Transform an expression */
int transform_expression(cypher_transform_context *ctx, ast_node *expr)
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
                        if (path_var && path_var->path_elements) {
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

                            if (path_var->path_type == VAR_PATH_COMPREHENSION) {
                                /* T-0332: pattern comprehension path — emit fully hydrated
                                 * path JSON ({nodes:[...], rels:[...]}) so it survives nested
                                 * inside a json_group_array aggregate without needing executor
                                 * post-processing of list elements. Build using json_object so
                                 * SQLite quotes/escapes correctly. */
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
                                    append_sql(ctx,
                                        "json_object("
                                        "'id', %s.id, "
                                        "'labels', COALESCE((SELECT json_group_array(label) FROM node_labels WHERE node_id = %s.id), json('[]')), "
                                        "'properties', COALESCE((SELECT json_group_object(pk.key, COALESCE("
                                            "(SELECT npt.value FROM node_props_text npt WHERE npt.node_id = %s.id AND npt.key_id = pk.id), "
                                            "(SELECT npi.value FROM node_props_int npi WHERE npi.node_id = %s.id AND npi.key_id = pk.id), "
                                            "(SELECT npr.value FROM node_props_real npr WHERE npr.node_id = %s.id AND npr.key_id = pk.id), "
                                            "(SELECT json(CASE WHEN npb.value THEN 'true' ELSE 'false' END) FROM node_props_bool npb WHERE npb.node_id = %s.id AND npb.key_id = pk.id), "
                                            "(SELECT json(npj.value) FROM node_props_json npj WHERE npj.node_id = %s.id AND npj.key_id = pk.id))) "
                                        "FROM property_keys pk WHERE "
                                            "EXISTS (SELECT 1 FROM node_props_text WHERE node_id = %s.id AND key_id = pk.id) OR "
                                            "EXISTS (SELECT 1 FROM node_props_int  WHERE node_id = %s.id AND key_id = pk.id) OR "
                                            "EXISTS (SELECT 1 FROM node_props_real WHERE node_id = %s.id AND key_id = pk.id) OR "
                                            "EXISTS (SELECT 1 FROM node_props_bool WHERE node_id = %s.id AND key_id = pk.id) OR "
                                            "EXISTS (SELECT 1 FROM node_props_json WHERE node_id = %s.id AND key_id = pk.id)"
                                        "), json('{}'))"
                                        ")",
                                        a, a, a, a, a, a, a, a, a, a, a, a);
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
                                    append_sql(ctx,
                                        "json_object("
                                        "'id', %s.id, "
                                        "'type', %s.type, "
                                        "'startNode', %s.source_id, "
                                        "'endNode', %s.target_id, "
                                        "'properties', COALESCE((SELECT json_group_object(pk.key, COALESCE("
                                            "(SELECT ept.value FROM edge_props_text ept WHERE ept.edge_id = %s.id AND ept.key_id = pk.id), "
                                            "(SELECT epi.value FROM edge_props_int  epi WHERE epi.edge_id = %s.id AND epi.key_id = pk.id), "
                                            "(SELECT epr.value FROM edge_props_real epr WHERE epr.edge_id = %s.id AND epr.key_id = pk.id), "
                                            "(SELECT json(CASE WHEN epb.value THEN 'true' ELSE 'false' END) FROM edge_props_bool epb WHERE epb.edge_id = %s.id AND epb.key_id = pk.id), "
                                            "(SELECT json(epj.value) FROM edge_props_json epj WHERE epj.edge_id = %s.id AND epj.key_id = pk.id))) "
                                        "FROM property_keys pk WHERE "
                                            "EXISTS (SELECT 1 FROM edge_props_text WHERE edge_id = %s.id AND key_id = pk.id) OR "
                                            "EXISTS (SELECT 1 FROM edge_props_int  WHERE edge_id = %s.id AND key_id = pk.id) OR "
                                            "EXISTS (SELECT 1 FROM edge_props_real WHERE edge_id = %s.id AND key_id = pk.id) OR "
                                            "EXISTS (SELECT 1 FROM edge_props_bool WHERE edge_id = %s.id AND key_id = pk.id) OR "
                                            "EXISTS (SELECT 1 FROM edge_props_json WHERE edge_id = %s.id AND key_id = pk.id)"
                                        "), json('{}'))"
                                        ")",
                                        a, a, a, a, a, a, a, a, a, a, a, a, a, a);
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
                                            if (node_alias) {
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
                            /* This is a projected variable from WITH - alias is the full column reference */
                            append_sql(ctx, "%s", alias);
                        } else if (transform_var_alias_is_id(ctx->var_ctx, id->name)) {
                            /* Post-WITH node/edge - alias IS the id value */
                            if (transform_var_is_edge(ctx->var_ctx, id->name)) {
                                /* Edge that passed through WITH - build relationship object */
                                /* Note: After WITH, we only have the id, not type/source/target */
                                append_sql(ctx, "json_object("
                                    "'id', %s, "
                                    "'type', (SELECT type FROM edges WHERE id = %s), "
                                    "'startNodeId', (SELECT source_id FROM edges WHERE id = %s), "
                                    "'endNodeId', (SELECT target_id FROM edges WHERE id = %s), "
                                    "'properties', COALESCE((SELECT json_group_object(pk.key, COALESCE("
                                        "(SELECT ept.value FROM edge_props_text ept WHERE ept.edge_id = %s AND ept.key_id = pk.id), "
                                        "(SELECT epi.value FROM edge_props_int epi WHERE epi.edge_id = %s AND epi.key_id = pk.id), "
                                        "(SELECT epr.value FROM edge_props_real epr WHERE epr.edge_id = %s AND epr.key_id = pk.id), "
                                        "(SELECT epb.value FROM edge_props_bool epb WHERE epb.edge_id = %s AND epb.key_id = pk.id), "
                                        "(SELECT json(epj.value) FROM edge_props_json epj WHERE epj.edge_id = %s AND epj.key_id = pk.id))) "
                                    "FROM property_keys pk WHERE "
                                        "EXISTS (SELECT 1 FROM edge_props_text WHERE edge_id = %s AND key_id = pk.id) OR "
                                        "EXISTS (SELECT 1 FROM edge_props_int WHERE edge_id = %s AND key_id = pk.id) OR "
                                        "EXISTS (SELECT 1 FROM edge_props_real WHERE edge_id = %s AND key_id = pk.id) OR "
                                        "EXISTS (SELECT 1 FROM edge_props_bool WHERE edge_id = %s AND key_id = pk.id) OR "
                                        "EXISTS (SELECT 1 FROM edge_props_json WHERE edge_id = %s AND key_id = pk.id)"
                                    "), json('{}'))"
                                ")",
                                alias, alias, alias, alias,
                                alias, alias, alias, alias, alias,
                                alias, alias, alias, alias, alias);
                            } else {
                                /* Node that passed through WITH - build node object using id directly */
                                append_sql(ctx, "json_object("
                                    "'id', %s, "
                                    "'labels', COALESCE((SELECT json_group_array(label) FROM node_labels WHERE node_id = %s), json('[]')), "
                                    "'properties', COALESCE((SELECT json_group_object(pk.key, COALESCE("
                                        "(SELECT npt.value FROM node_props_text npt WHERE npt.node_id = %s AND npt.key_id = pk.id), "
                                        "(SELECT npi.value FROM node_props_int npi WHERE npi.node_id = %s AND npi.key_id = pk.id), "
                                        "(SELECT npr.value FROM node_props_real npr WHERE npr.node_id = %s AND npr.key_id = pk.id), "
                                        "(SELECT json(CASE WHEN npb.value THEN 'true' ELSE 'false' END) FROM node_props_bool npb WHERE npb.node_id = %s AND npb.key_id = pk.id), "
                                        "(SELECT json(npj.value) FROM node_props_json npj WHERE npj.node_id = %s AND npj.key_id = pk.id))) "
                                    "FROM property_keys pk WHERE "
                                        "EXISTS (SELECT 1 FROM node_props_text WHERE node_id = %s AND key_id = pk.id) OR "
                                        "EXISTS (SELECT 1 FROM node_props_int WHERE node_id = %s AND key_id = pk.id) OR "
                                        "EXISTS (SELECT 1 FROM node_props_real WHERE node_id = %s AND key_id = pk.id) OR "
                                        "EXISTS (SELECT 1 FROM node_props_bool WHERE node_id = %s AND key_id = pk.id) OR "
                                        "EXISTS (SELECT 1 FROM node_props_json WHERE node_id = %s AND key_id = pk.id)"
                                    "), json('{}'))"
                                ")",
                                alias, alias,
                                alias, alias, alias, alias, alias,
                                alias, alias, alias, alias, alias);
                            }
                        } else if (transform_var_is_edge(ctx->var_ctx, id->name)) {
                            /* T-0309: varlen-bound edge variable holds a path
                             * of edges (CTE alias has columns start_id/end_id/
                             * depth/path_ids/visited, not edge fields). Emit
                             * a JSON array constructed from path_ids — one
                             * edge JSON per id, in path order. */
                            transform_var *evar = transform_var_lookup_edge(ctx->var_ctx, id->name);
                            if (evar && evar->cte_name) {
                                append_sql(ctx,
                                  "(SELECT json_group_array(json_object("
                                    "'id', e.id, "
                                    "'type', e.type, "
                                    "'startNodeId', e.source_id, "
                                    "'endNodeId', e.target_id, "
                                    "'properties', COALESCE((SELECT json_group_object(pk.key, COALESCE("
                                      "(SELECT ept.value FROM edge_props_text ept WHERE ept.edge_id = e.id AND ept.key_id = pk.id), "
                                      "(SELECT epi.value FROM edge_props_int epi WHERE epi.edge_id = e.id AND epi.key_id = pk.id), "
                                      "(SELECT epr.value FROM edge_props_real epr WHERE epr.edge_id = e.id AND epr.key_id = pk.id), "
                                      "(SELECT epb.value FROM edge_props_bool epb WHERE epb.edge_id = e.id AND epb.key_id = pk.id), "
                                      "(SELECT json(epj.value) FROM edge_props_json epj WHERE epj.edge_id = e.id AND epj.key_id = pk.id))) "
                                      "FROM property_keys pk WHERE "
                                        "EXISTS (SELECT 1 FROM edge_props_text WHERE edge_id = e.id AND key_id = pk.id) OR "
                                        "EXISTS (SELECT 1 FROM edge_props_int WHERE edge_id = e.id AND key_id = pk.id) OR "
                                        "EXISTS (SELECT 1 FROM edge_props_real WHERE edge_id = e.id AND key_id = pk.id) OR "
                                        "EXISTS (SELECT 1 FROM edge_props_bool WHERE edge_id = e.id AND key_id = pk.id) OR "
                                        "EXISTS (SELECT 1 FROM edge_props_json WHERE edge_id = e.id AND key_id = pk.id)"
                                      "), json('{}'))"
                                  ") ORDER BY je.key) "
                                  "FROM json_each('[' || %s.elem_ids || ']') je "
                                  "JOIN edges e ON e.id = je.value WHERE (je.key %% 2) = 1)",
                                  alias);
                                goto edge_alias_projection_done;
                            }
                            /* Edge variable - return full relationship object,
                             * or NULL when the row came from an OPTIONAL MATCH
                             * miss (LEFT JOIN with no match → alias.id IS NULL). */
                            append_sql(ctx, "(CASE WHEN %s.id IS NULL THEN NULL ELSE json_object("
                                "'id', %s.id, "
                                "'type', %s.type, "
                                "'startNodeId', %s.source_id, "
                                "'endNodeId', %s.target_id, "
                                "'properties', COALESCE((SELECT json_group_object(pk.key, COALESCE("
                                    "(SELECT ept.value FROM edge_props_text ept WHERE ept.edge_id = %s.id AND ept.key_id = pk.id), "
                                    "(SELECT epi.value FROM edge_props_int epi WHERE epi.edge_id = %s.id AND epi.key_id = pk.id), "
                                    "(SELECT epr.value FROM edge_props_real epr WHERE epr.edge_id = %s.id AND epr.key_id = pk.id), "
                                    "(SELECT epb.value FROM edge_props_bool epb WHERE epb.edge_id = %s.id AND epb.key_id = pk.id), "
                                    "(SELECT json(epj.value) FROM edge_props_json epj WHERE epj.edge_id = %s.id AND epj.key_id = pk.id))) "
                                "FROM property_keys pk WHERE "
                                    "EXISTS (SELECT 1 FROM edge_props_text WHERE edge_id = %s.id AND key_id = pk.id) OR "
                                    "EXISTS (SELECT 1 FROM edge_props_int WHERE edge_id = %s.id AND key_id = pk.id) OR "
                                    "EXISTS (SELECT 1 FROM edge_props_real WHERE edge_id = %s.id AND key_id = pk.id) OR "
                                    "EXISTS (SELECT 1 FROM edge_props_bool WHERE edge_id = %s.id AND key_id = pk.id) OR "
                                    "EXISTS (SELECT 1 FROM edge_props_json WHERE edge_id = %s.id AND key_id = pk.id)"
                                "), json('{}'))"
                            ") END)",
                            alias,
                            alias, alias, alias, alias,
                            alias, alias, alias, alias, alias,
                            alias, alias, alias, alias, alias);
                            edge_alias_projection_done: ;
                        } else {
                            /* This is a node variable - return full node object,
                             * or NULL when the row came from an OPTIONAL MATCH
                             * miss (LEFT JOIN with no match → alias.id IS NULL). */
                            append_sql(ctx, "(CASE WHEN %s.id IS NULL THEN NULL ELSE json_object("
                                "'id', %s.id, "
                                "'labels', COALESCE((SELECT json_group_array(label) FROM node_labels WHERE node_id = %s.id), json('[]')), "
                                "'properties', COALESCE((SELECT json_group_object(pk.key, COALESCE("
                                    "(SELECT npt.value FROM node_props_text npt WHERE npt.node_id = %s.id AND npt.key_id = pk.id), "
                                    "(SELECT npi.value FROM node_props_int npi WHERE npi.node_id = %s.id AND npi.key_id = pk.id), "
                                    "(SELECT npr.value FROM node_props_real npr WHERE npr.node_id = %s.id AND npr.key_id = pk.id), "
                                    "(SELECT json(CASE WHEN npb.value THEN 'true' ELSE 'false' END) FROM node_props_bool npb WHERE npb.node_id = %s.id AND npb.key_id = pk.id), "
                                    "(SELECT json(npj.value) FROM node_props_json npj WHERE npj.node_id = %s.id AND npj.key_id = pk.id))) "
                                "FROM property_keys pk WHERE "
                                    "EXISTS (SELECT 1 FROM node_props_text WHERE node_id = %s.id AND key_id = pk.id) OR "
                                    "EXISTS (SELECT 1 FROM node_props_int WHERE node_id = %s.id AND key_id = pk.id) OR "
                                    "EXISTS (SELECT 1 FROM node_props_real WHERE node_id = %s.id AND key_id = pk.id) OR "
                                    "EXISTS (SELECT 1 FROM node_props_bool WHERE node_id = %s.id AND key_id = pk.id) OR "
                                    "EXISTS (SELECT 1 FROM node_props_json WHERE node_id = %s.id AND key_id = pk.id)"
                                "), json('{}'))"
                            ") END)",
                            alias,
                            alias, alias,
                            alias, alias, alias, alias, alias,
                            alias, alias, alias, alias, alias);
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

                    append_sql(ctx, "(SELECT json_group_array(value) FROM json_each(");
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
                append_sql(ctx, "_gql_subscript(");
                if (transform_expression(ctx, subscript->expr) < 0) return -1;
                append_sql(ctx, ", ");
                if (transform_expression(ctx, subscript->index) < 0) return -1;
                append_sql(ctx, ")");
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
                    append_sql(ctx, ":%s", param->name);
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
                        /* Searched CASE: condition is already a boolean expression */
                        if (transform_expression(ctx, when->condition) < 0) {
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
                append_sql(ctx, "_gql_map(");
                if (map->pairs) {
                    for (int i = 0; i < map->pairs->count; i++) {
                        if (i > 0) {
                            append_sql(ctx, ", ");
                        }
                        cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[i];
                        append_sql(ctx, "'%s', ", pair->key);
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
                    append_sql(ctx, "(SELECT json_group_object(pk.key, COALESCE("
                               "npt.value, "
                               "npi.value, "
                               "npr.value, "
                               "CASE npb.value WHEN 1 THEN 'true' WHEN 0 THEN 'false' END, "
                               "json(npj.value)"
                               ")) FROM property_keys pk "
                               "LEFT JOIN node_props_text npt ON npt.key_id = pk.id AND npt.node_id = %s%s "
                               "LEFT JOIN node_props_int npi ON npi.key_id = pk.id AND npi.node_id = %s%s "
                               "LEFT JOIN node_props_real npr ON npr.key_id = pk.id AND npr.node_id = %s%s "
                               "LEFT JOIN node_props_bool npb ON npb.key_id = pk.id AND npb.node_id = %s%s "
                               "LEFT JOIN node_props_json npj ON npj.key_id = pk.id AND npj.node_id = %s%s "
                               "WHERE npt.value IS NOT NULL OR npi.value IS NOT NULL OR npr.value IS NOT NULL OR npb.value IS NOT NULL OR npj.value IS NOT NULL)",
                               base_alias, is_projected ? "" : ".id",
                               base_alias, is_projected ? "" : ".id",
                               base_alias, is_projected ? "" : ".id",
                               base_alias, is_projected ? "" : ".id",
                               base_alias, is_projected ? "" : ".id");
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
                        append_sql(ctx, "'%s', ", key);

                        /* Output value */
                        if (item->property && base_alias) {
                            /* Property access using same logic as transform_property_access */
                            append_sql(ctx, "(SELECT COALESCE(");
                            append_sql(ctx, "(SELECT npt.value FROM node_props_text npt JOIN property_keys pk ON npt.key_id = pk.id WHERE npt.node_id = %s%s AND pk.key = ",
                                       base_alias, is_projected ? "" : ".id");
                            append_string_literal(ctx, item->property);
                            append_sql(ctx, "), ");
                            append_sql(ctx, "(SELECT npi.value FROM node_props_int npi JOIN property_keys pk ON npi.key_id = pk.id WHERE npi.node_id = %s%s AND pk.key = ",
                                       base_alias, is_projected ? "" : ".id");
                            append_string_literal(ctx, item->property);
                            append_sql(ctx, "), ");
                            append_sql(ctx, "(SELECT npr.value FROM node_props_real npr JOIN property_keys pk ON npr.key_id = pk.id WHERE npr.node_id = %s%s AND pk.key = ",
                                       base_alias, is_projected ? "" : ".id");
                            append_string_literal(ctx, item->property);
                            append_sql(ctx, "), ");
                            append_sql(ctx, "(SELECT CASE WHEN npb.value THEN 'true' ELSE 'false' END FROM node_props_bool npb JOIN property_keys pk ON npb.key_id = pk.id WHERE npb.node_id = %s%s AND pk.key = ",
                                       base_alias, is_projected ? "" : ".id");
                            append_string_literal(ctx, item->property);
                            append_sql(ctx, "), ");
                            append_sql(ctx, "(SELECT json(npj.value) FROM node_props_json npj JOIN property_keys pk ON npj.key_id = pk.id WHERE npj.node_id = %s%s AND pk.key = ",
                                       base_alias, is_projected ? "" : ".id");
                            append_string_literal(ctx, item->property);
                            append_sql(ctx, ")))");
                        } else if (item->expr) {
                            /* Computed expression */
                            if (transform_expression(ctx, item->expr) < 0) {
                                return -1;
                            }
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
                if (comp->transform_expr) {
                    if (transform_expression(ctx, comp->transform_expr) < 0) {
                        if (saved_alias) free(saved_alias);
                        return -1;
                    }
                } else {
                    /* Just return the element */
                    append_sql(ctx, "json_each.value");
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
                    if (transform_expression(ctx, comp->where_expr) < 0) {
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
                        } else {
                            /* Generate new alias and add to FROM */
                            if (!first_table) {
                                append_sql(ctx, ", ");
                            }
                            snprintf(node_aliases[node_count], sizeof(node_aliases[node_count]),
                                    "_pc_n%d", node_count);
                            append_sql(ctx, "nodes AS %s", node_aliases[node_count]);
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

                /* Register pattern variables for use in expressions */
                for (int i = 0; i < node_count; i++) {
                    if (node_vars[i][0] != '\0') {
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
                        if (rel->left_arrow) {
                            /* <-[r]- means target->source */
                            append_sql(ctx, "_pc_e%d.target_id = %s.id AND _pc_e%d.source_id = %s.id",
                                      rel_index, node_aliases[source_node],
                                      rel_index, node_aliases[target_node]);
                        } else {
                            /* -[r]-> or -[r]- means source->target */
                            append_sql(ctx, "_pc_e%d.source_id = %s.id AND _pc_e%d.target_id = %s.id",
                                      rel_index, node_aliases[source_node],
                                      rel_index, node_aliases[target_node]);
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
                                    append_sql(ctx, "EXISTS (SELECT 1 FROM node_labels WHERE node_id = %s.id AND label = ",
                                              node_aliases[current_node]);
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
                    if (transform_expression(ctx, comp->where_expr) < 0) {
                        return -1;
                    }
                    append_sql(ctx, ")");
                }

                append_sql(ctx, ")");

                /* Now we need to go back and fill in the collect expression */
                /* Create a new buffer for the collect expression */
                char collect_sql[4096];

                /* Save current buffer state */
                char *temp_buffer = ctx->sql_buffer;
                size_t temp_size = ctx->sql_size;
                size_t temp_capacity = ctx->sql_capacity;

                /* Switch to collect buffer */
                ctx->sql_buffer = collect_sql;
                ctx->sql_size = 0;
                ctx->sql_capacity = sizeof(collect_sql);

                /* Transform the collect expression */
                if (transform_expression(ctx, comp->collect_expr) < 0) {
                    ctx->sql_buffer = temp_buffer;
                    ctx->sql_size = temp_size;
                    ctx->sql_capacity = temp_capacity;
                    return -1;
                }

                /* Null terminate */
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
