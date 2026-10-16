/*
 * transform_func_path.c
 *    Path function transformations for Cypher queries
 *
 * This file contains transformations for path navigation functions:
 * - length() for paths - returns number of relationships
 * - nodes() - returns list of nodes in a path
 * - relationships() - returns list of relationships in a path
 * - startNode() - returns start node of a relationship
 * - endNode() - returns end node of a relationship
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transform/cypher_transform.h"
#include "entity_json_sql.h"
#include "transform/transform_func_path.h"
#include "parser/cypher_ast.h"
#include "parser/cypher_debug.h"

/* Transform length() function for paths - returns number of relationships in path */
int transform_path_length_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming path length() function");

    /* Already validated in caller that this is a path variable */
    ast_node *arg = func_call->args->items[0];
    cypher_identifier *id = (cypher_identifier*)arg;

    transform_var *path_var = transform_var_lookup_path(ctx->var_ctx, id->name);
    if (!path_var || !path_var->path_elements) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "Cannot get length of path variable: %s", id->name);
        ctx->error_message = strdup(error);
        return -1;
    }

    /* A path forwarded through WITH: count the relationships in its JSON
     * (the {nodes, rels} object, or the interleaved [n, e, n, ...] array). */
    if (path_var->table_alias && strchr(path_var->table_alias, '.')) {
        char *v = cypher_transform_capture_expression(ctx, arg);
        if (!v) return -1;
        append_sql(ctx,
            "(CASE WHEN %s IS NULL THEN NULL"
            " WHEN json_valid(%s) AND json_type(%s) = 'object' THEN json_array_length(%s, '$.rels')"
            " ELSE (json_array_length(%s) - 1) / 2 END)", v, v, v, v, v);
        free(v);
        return 0;
    }

    /* Path length = number of relationships. A variable-length segment
     * counts the edges in its elem_ids (node,edge,...,node), not 1 (#135). */
    int rel_count = 0;
    dynamic_buffer varlen_terms;
    dbuf_init(&varlen_terms);
    for (int i = 0; i < path_var->path_elements->count; i++) {
        ast_node *el = path_var->path_elements->items[i];
        if (el->type != AST_NODE_REL_PATTERN) continue;
        cypher_rel_pattern *rel = (cypher_rel_pattern*)el;
        const char *alias = (rel->varlen && rel->variable) ?
            transform_var_get_alias(ctx->var_ctx, rel->variable) : NULL;
        if (alias) {
            dbuf_appendf(&varlen_terms,
                " + (json_array_length('[' || %s.elem_ids || ']') - 1) / 2", alias);
        } else {
            rel_count++;
        }
    }

    if (dbuf_len(&varlen_terms) > 0) {
        append_sql(ctx, "(%d%s)", rel_count, dbuf_get(&varlen_terms));
    } else {
        append_sql(ctx, "%d", rel_count);
    }
    dbuf_free(&varlen_terms);
    return 0;
}

/* #135: the nodes (want_nodes) or relationships of a registered path, in
 * path order, as hydrated entity JSON. A variable-length segment adds the
 * entities inside its elem_ids; its end nodes are the node patterns around
 * it. The ids were returned before, so `[n IN nodes(p) | n.name]` gave
 * nulls, and a variable-length segment added only its end nodes. */
static int emit_registered_path_members(cypher_transform_context *ctx, transform_var *path_var,
                                        bool want_nodes)
{
    dynamic_buffer u;
    dbuf_init(&u);
    /* An OPTIONAL path matches all or nothing: test its first relationship
     * (an end node can be bound before the OPTIONAL MATCH). */
    char null_guard[300] = "";
    for (int i = 0; i < path_var->path_elements->count && !null_guard[0]; i++) {
        ast_node *el = path_var->path_elements->items[i];
        if (el->type != AST_NODE_REL_PATTERN) continue;
        cypher_rel_pattern *rp = (cypher_rel_pattern*)el;
        const char *a = rp->variable ? transform_var_get_alias(ctx->var_ctx, rp->variable) : NULL;
        if (a) snprintf(null_guard, sizeof(null_guard), "%s.%s", a, rp->varlen ? "elem_ids" : "id");
    }
    int seg = 0;
    const char *prev_varlen = NULL;   /* alias of a varlen segment just before */
    for (int i = 0; i < path_var->path_elements->count; i++) {
        ast_node *el = path_var->path_elements->items[i];
        if (el->type == AST_NODE_NODE_PATTERN) {
            cypher_node_pattern *np = (cypher_node_pattern*)el;
            const char *a = np->variable ? transform_var_get_alias(ctx->var_ctx, np->variable) : NULL;
            /* A node from WITH: its alias is the id column itself (Path1 [1]). */
            bool a_is_id = a && np->variable &&
                (transform_var_alias_is_id(ctx->var_ctx, np->variable) || strchr(a, '.'));
            if (!null_guard[0] && a) snprintf(null_guard, sizeof(null_guard), a_is_id ? "%s" : "%s.id", a);
            const char *after_varlen = prev_varlen;
            prev_varlen = NULL;
            if (!want_nodes) continue;
            if (dbuf_len(&u) > 0) dbuf_append(&u, " UNION ALL ");
            if (a) dbuf_appendf(&u, a_is_id ? "SELECT %d AS seg, 0 AS k, %s AS v"
                                            : "SELECT %d AS seg, 0 AS k, %s.id AS v", seg, a);
            else dbuf_appendf(&u, "SELECT %d AS seg, 0 AS k, NULL AS v", seg);
            /* After a zero-hop segment this node is the node before it. */
            if (after_varlen)
                dbuf_appendf(&u, " WHERE json_array_length('[' || %s.elem_ids || ']') > 1", after_varlen);
            seg++;
        } else if (el->type == AST_NODE_REL_PATTERN) {
            cypher_rel_pattern *rp = (cypher_rel_pattern*)el;
            const char *a = rp->variable ? transform_var_get_alias(ctx->var_ctx, rp->variable) : NULL;
            if (rp->varlen && a) {
                prev_varlen = a;
                if (dbuf_len(&u) > 0) dbuf_append(&u, " UNION ALL ");
                if (want_nodes) {
                    dbuf_appendf(&u,
                        "SELECT %d AS seg, je.key AS k, je.value AS v FROM json_each('[' || %s.elem_ids || ']') je "
                        "WHERE (je.key %% 2) = 0 AND je.key > 0 AND "
                        "je.key < json_array_length('[' || %s.elem_ids || ']') - 1", seg, a, a);
                } else {
                    dbuf_appendf(&u,
                        "SELECT %d AS seg, je.key AS k, je.value AS v FROM json_each('[' || %s.elem_ids || ']') je "
                        "WHERE (je.key %% 2) = 1", seg, a);
                }
                seg++;
            } else if (!want_nodes) {
                if (dbuf_len(&u) > 0) dbuf_append(&u, " UNION ALL ");
                if (a) dbuf_appendf(&u, "SELECT %d AS seg, 0 AS k, %s.id AS v", seg, a);
                else dbuf_appendf(&u, "SELECT %d AS seg, 0 AS k, NULL AS v", seg);
                seg++;
            }
        }
    }

    if (dbuf_len(&u) == 0) {
        dbuf_free(&u);
        append_sql(ctx, "json_array()");
        return 0;
    }

    char *ej = want_nodes ? gql_sql_node_json_expr("", "_pm.v")
                          : gql_sql_edge_json_expr("", "e.id", "e.type", "e.source_id", "e.target_id");
    if (!ej) { dbuf_free(&u); return -1; }
    /* A null path (OPTIONAL pattern that did not match) gives null. */
    if (null_guard[0]) append_sql(ctx, "(CASE WHEN %s IS NULL THEN NULL ELSE ", null_guard);
    append_sql(ctx,
        "(SELECT json_group_array(json(%s) ORDER BY _pm.seg, _pm.k) FROM (%s) _pm%s)",
        ej, dbuf_get(&u), want_nodes ? "" : " JOIN edges e ON e.id = _pm.v");
    if (null_guard[0]) append_sql(ctx, " END)");
    free(ej);
    dbuf_free(&u);
    return 0;
}

/* T-0370: nodes()/relationships() of a path that is a JSON value rather than
 * a registered path variable — a comprehension/quantifier element over
 * collect(p) (`[x IN collect(p) | nodes(x)]`, List12 [4]/[5]), an unwound
 * path, or a projected column. Two encodings exist: the hydrated object
 * {nodes:[...], rels:[...]} (pattern comprehensions, paths under UNWIND)
 * and the interleaved elem-id array [n0, e0, n1, ...] text that the regular
 * path projection and collect(p) store. Both yield fully hydrated entity
 * JSON so the result renders as nodes/relationships. */
static int emit_json_path_members(cypher_transform_context *ctx, ast_node *arg, bool want_nodes)
{
    char *v = cypher_transform_capture_expression(ctx, arg);
    if (!v) return -1;
    char *ej = want_nodes ? gql_sql_node_json_expr("", "je.value")
                          : gql_sql_edge_json_expr("", "e.id", "e.type", "e.source_id", "e.target_id");
    if (!ej) { free(v); return -1; }
    append_sql(ctx,
        "(CASE WHEN %s IS NULL THEN NULL"
        " WHEN json_valid(%s) AND json_type(%s) = 'object' THEN json_extract(%s, '$.%s')"
        " ELSE (SELECT json_group_array(json(%s) ORDER BY je.key) FROM json_each(%s) je%s"
        " WHERE (je.key %% 2) = %d) END)",
        v, v, v, v, want_nodes ? "nodes" : "rels",
        ej, v, want_nodes ? "" : " JOIN edges e ON e.id = je.value",
        want_nodes ? 0 : 1);
    free(ej);
    free(v);
    return 0;
}

/* True when `arg` must be treated as a JSON path value: any expression that
 * is not an identifier, or an identifier that is a projected (list element /
 * WITH column) variable rather than a path, node or relationship. */
static bool arg_is_json_path_value(cypher_transform_context *ctx, ast_node *arg)
{
    if (arg->type != AST_NODE_IDENTIFIER) return true;
    cypher_identifier *id = (cypher_identifier*)arg;
    if (transform_var_is_path(ctx->var_ctx, id->name)) return false;
    return transform_var_is_projected(ctx->var_ctx, id->name);
}

/* Transform nodes() function - returns list of nodes in a path */
int transform_path_nodes_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming nodes() function");

    /* nodes() requires exactly one argument */
    if (!func_call->args || func_call->args->count != 1 || func_call->args->items[0] == NULL) {
        ctx->has_error = true;
        ctx->error_message = strdup("nodes() function requires exactly one argument");
        return -1;
    }

    ast_node *arg = func_call->args->items[0];

    /* nodes(null) → null. */
    if (arg->type == AST_NODE_LITERAL) {
        cypher_literal *lit = (cypher_literal *)arg;
        if (lit->literal_type == LITERAL_NULL) {
            append_sql(ctx, "NULL");
            return 0;
        }
        ctx->has_error = true;
        ctx->error_message = strdup("nodes() function argument must be a path variable");
        return -1;
    }

    if (arg_is_json_path_value(ctx, arg)) {
        return emit_json_path_members(ctx, arg, true);
    }

    if (arg->type != AST_NODE_IDENTIFIER) {
        ctx->has_error = true;
        ctx->error_message = strdup("nodes() function argument must be a path variable");
        return -1;
    }

    cypher_identifier *id = (cypher_identifier*)arg;

    /* Check if this is a path variable */
    if (!transform_var_is_path(ctx->var_ctx, id->name)) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "nodes() function argument must be a path variable, got: %s", id->name);
        ctx->error_message = strdup(error);
        return -1;
    }

    transform_var *path_var = transform_var_lookup_path(ctx->var_ctx, id->name);
    if (!path_var || !path_var->path_elements) {
        ctx->has_error = true;
        ctx->error_message = strdup("Cannot get nodes from path variable");
        return -1;
    }

    /* A path forwarded through WITH is a column holding its JSON; the
     * element aliases of the MATCH are out of scope (With6 [4]). */
    if (path_var->table_alias && strchr(path_var->table_alias, '.')) {
        return emit_json_path_members(ctx, arg, true);
    }

    return emit_registered_path_members(ctx, path_var, true);
}

/* Transform relationships() function - returns list of relationships in a path */
int transform_path_relationships_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming relationships() function");

    /* relationships() requires exactly one argument */
    if (!func_call->args || func_call->args->count != 1 || func_call->args->items[0] == NULL) {
        ctx->has_error = true;
        ctx->error_message = strdup("relationships() function requires exactly one argument");
        return -1;
    }

    ast_node *arg = func_call->args->items[0];

    /* relationships(null) → null. */
    if (arg->type == AST_NODE_LITERAL) {
        cypher_literal *lit = (cypher_literal *)arg;
        if (lit->literal_type == LITERAL_NULL) {
            append_sql(ctx, "NULL");
            return 0;
        }
        ctx->has_error = true;
        ctx->error_message = strdup("relationships() function argument must be a path variable");
        return -1;
    }

    if (arg_is_json_path_value(ctx, arg)) {
        return emit_json_path_members(ctx, arg, false);
    }

    if (arg->type != AST_NODE_IDENTIFIER) {
        ctx->has_error = true;
        ctx->error_message = strdup("relationships() function argument must be a path variable");
        return -1;
    }

    cypher_identifier *id = (cypher_identifier*)arg;

    /* Check if this is a path variable */
    if (!transform_var_is_path(ctx->var_ctx, id->name)) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "relationships() function argument must be a path variable, got: %s", id->name);
        ctx->error_message = strdup(error);
        return -1;
    }

    transform_var *path_var = transform_var_lookup_path(ctx->var_ctx, id->name);
    if (!path_var || !path_var->path_elements) {
        ctx->has_error = true;
        ctx->error_message = strdup("Cannot get relationships from path variable");
        return -1;
    }

    /* A path forwarded through WITH is a column holding its JSON; the
     * element aliases of the MATCH are out of scope (With6 [4]). */
    if (path_var->table_alias && strchr(path_var->table_alias, '.')) {
        return emit_json_path_members(ctx, arg, false);
    }

    return emit_registered_path_members(ctx, path_var, false);
}

/* GQLITE-T-0181: shared validation for startNode()/endNode(). Emits the SQL
 * expression that yields the endpoint node id (source_id or target_id of the
 * relationship bound to the argument), or NULL for a null argument / an
 * OPTIONAL MATCH miss. Returns 1 when an id expression was emitted, 0 when a
 * literal NULL was emitted, -1 on error. */
static int emit_endpoint_id(cypher_transform_context *ctx, cypher_function_call *func_call,
                            const char *fname, const char *column)
{
    char err[256];

    if (!func_call->args || func_call->args->count != 1 || func_call->args->items[0] == NULL) {
        ctx->has_error = true;
        snprintf(err, sizeof(err), "%s() function requires exactly one argument", fname);
        ctx->error_message = strdup(err);
        return -1;
    }

    ast_node *arg = func_call->args->items[0];

    /* startNode(null) / endNode(null) → null. */
    if (arg->type == AST_NODE_LITERAL) {
        cypher_literal *lit = (cypher_literal *)arg;
        if (lit->literal_type == LITERAL_NULL) {
            append_sql(ctx, "NULL");
            return 0;
        }
        ctx->has_error = true;
        snprintf(err, sizeof(err), "%s() function argument must be a relationship variable", fname);
        ctx->error_message = strdup(err);
        return -1;
    }

    if (arg->type != AST_NODE_IDENTIFIER) {
        ctx->has_error = true;
        snprintf(err, sizeof(err), "%s() function argument must be a relationship variable", fname);
        ctx->error_message = strdup(err);
        return -1;
    }

    cypher_identifier *id = (cypher_identifier *)arg;
    const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
    if (!alias) {
        ctx->has_error = true;
        snprintf(err, sizeof(err), "Unknown variable in %s() function: %s", fname, id->name);
        ctx->error_message = strdup(err);
        return -1;
    }

    /* Only relationships have endpoints. */
    if (!transform_var_is_edge(ctx->var_ctx, id->name)) {
        ctx->has_error = true;
        snprintf(err, sizeof(err), "%s() function argument must be a relationship variable", fname);
        ctx->error_message = strdup(err);
        return -1;
    }

    /* A post-WITH / projected edge alias IS the id value; a MATCH-bound edge
     * alias is the edges row (same rule as type()). */
    bool skip_id = transform_var_is_projected(ctx->var_ctx, id->name) ||
                   transform_var_alias_is_id(ctx->var_ctx, id->name);
    append_sql(ctx, "(SELECT %s FROM edges WHERE id = %s%s)", column, alias, skip_id ? "" : ".id");
    return 1;
}

/* Emit the full node JSON ({"id","labels","properties"}) for the endpoint,
 * the same shape `RETURN n` produces (perf review F5 pass-through), guarded
 * so a NULL endpoint id (null argument, OPTIONAL MATCH miss) stays NULL. */
static int emit_endpoint_node(cypher_transform_context *ctx, cypher_function_call *func_call,
                              const char *fname, const char *column)
{
    /* Render the id expression into a scratch buffer so it can be spliced
     * into the node JSON template several times. */
    size_t saved_size = ctx->sql_size;
    int rc = emit_endpoint_id(ctx, func_call, fname, column);
    if (rc < 0) return -1;
    if (rc == 0) return 0; /* literal NULL already emitted */

    char *id_expr = strdup(ctx->sql_buffer + saved_size);
    if (!id_expr) return -1;
    ctx->sql_size = saved_size;
    ctx->sql_buffer[saved_size] = '\0';

    char *nj = gql_sql_node_json_expr("", id_expr);
    if (!nj) { free(id_expr); return -1; }
    append_sql(ctx, "(CASE WHEN %s IS NULL THEN NULL ELSE %s END)", id_expr, nj);
    free(nj);
    free(id_expr);
    return 0;
}

/* Transform startNode() function - returns the start node of a relationship
 * as a node object (GQLITE-T-0181; was the bare source_id integer). */
int transform_startnode_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming startNode() function");
    return emit_endpoint_node(ctx, func_call, "startNode", "source_id");
}

/* Transform endNode() function - returns the end node of a relationship
 * as a node object (GQLITE-T-0181; was the bare target_id integer). */
int transform_endnode_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming endNode() function");
    return emit_endpoint_node(ctx, func_call, "endNode", "target_id");
}

/* Emit only the endpoint node id for startNode(r)/endNode(r). Used where the
 * caller needs the id rather than the node object: property access
 * (`startNode(r).name`) and `id(startNode(r))`. Returns -1 if func_call is
 * not a startNode/endNode call or on a validation error. */
int transform_endpoint_node_id(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    if (!func_call || !func_call->function_name) return -1;
    if (strcasecmp(func_call->function_name, "startNode") == 0)
        return emit_endpoint_id(ctx, func_call, "startNode", "source_id") < 0 ? -1 : 0;
    if (strcasecmp(func_call->function_name, "endNode") == 0)
        return emit_endpoint_id(ctx, func_call, "endNode", "target_id") < 0 ? -1 : 0;
    ctx->has_error = true;
    ctx->error_message = strdup("Expected startNode() or endNode()");
    return -1;
}
