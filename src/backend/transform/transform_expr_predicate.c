/*
 * transform_expr_predicate.c
 *    Predicate expression transformations for Cypher queries
 *
 * This file contains transformations for predicate expressions:
 * - EXISTS { pattern } - pattern existence check
 * - EXISTS(n.property) - property existence check
 * - all/any/none/single(x IN list WHERE predicate) - list predicates
 * - reduce(acc = initial, x IN list | expr) - list reduction
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transform/cypher_transform.h"
#include "transform/transform_expr_predicate.h"
#include "transform/transform_helpers.h"
#include "parser/cypher_ast.h"
#include "parser/cypher_debug.h"

/* I-0047 P1: emit the rel-type predicate (bare boolean, no connector)
 * for a varlen EXISTS-pattern subquery. Returns true if a predicate was
 * appended. */
static bool ep_append_type_pred(cypher_transform_context *ctx,
                                cypher_rel_pattern *rel)
{
    if (rel->type) {
        append_sql(ctx, "e.type = ");
        append_string_literal(ctx, rel->type);
        return true;
    } else if (rel->types && rel->types->count > 0) {
        append_sql(ctx, "(");
        for (int t = 0; t < rel->types->count; t++) {
            if (t > 0) append_sql(ctx, " OR ");
            append_sql(ctx, "e.type = ");
            const char *tn = get_label_string(rel->types->items[t]);
            append_string_literal(ctx, tn ? tn : "");
        }
        append_sql(ctx, ")");
        return true;
    }
    return false;
}

/* I-0047 P1: handle the `(a)-[*..]-(b)` shape inside a WHERE existential
 * pattern predicate (Pattern1 [10]/[17]/[18]). The legacy emitter below
 * treats every relationship as a single fixed hop, ignoring `rel->varlen`.
 * For a path of exactly [node, varlen-rel, node] where the left node is
 * bound in the outer scope, emit a correlated recursive-CTE reachability
 * check instead. Returns true if handled (caller appends the closing ")").
 *
 * Mirrors generate_varlen_cte's traversal: directed honors arrows,
 * undirected walks both orientations; node-based cycle prevention. Only
 * start/end/depth/visited columns are needed for an existence test. */
static bool emit_exists_varlen_path(cypher_transform_context *ctx, cypher_path *path)
{
    if (!path->elements || path->elements->count != 3) return false;
    ast_node *e0 = path->elements->items[0];
    ast_node *e1 = path->elements->items[1];
    ast_node *e2 = path->elements->items[2];
    if (e0->type != AST_NODE_NODE_PATTERN ||
        e1->type != AST_NODE_REL_PATTERN ||
        e2->type != AST_NODE_NODE_PATTERN) return false;

    cypher_rel_pattern *rel = (cypher_rel_pattern*)e1;
    if (!rel->varlen) return false;

    cypher_node_pattern *lnode = (cypher_node_pattern*)e0;
    cypher_node_pattern *rnode = (cypher_node_pattern*)e2;

    /* Inline endpoint property maps aren't modelled here — bail so we
     * don't silently ignore them (preserves pre-I-0047 behavior for that
     * shape rather than emitting a wrong-but-different result). */
    if (lnode->properties || rnode->properties) return false;

    /* Left endpoint must be bound in the outer scope to correlate. */
    const char *lalias = lnode->variable
        ? transform_var_get_alias(ctx->var_ctx, lnode->variable) : NULL;
    if (!lalias) return false;
    const char *ralias = rnode->variable
        ? transform_var_get_alias(ctx->var_ctx, rnode->variable) : NULL;

    cypher_varlen_range *range = (cypher_varlen_range*)rel->varlen;
    int min_hops = range->min_hops >= 0 ? range->min_hops : 1;
    int max_hops = range->max_hops > 0 ? range->max_hops : 100;

    const char *src_col = "source_id", *tgt_col = "target_id";
    if (rel->left_arrow && !rel->right_arrow) { src_col = "target_id"; tgt_col = "source_id"; }
    bool undirected = (!rel->left_arrow && !rel->right_arrow);

    char cte[32];
    snprintf(cte, sizeof(cte), "_epv_%d", ctx->cte_count++);

    append_sql(ctx, "WITH RECURSIVE %s(start_id, end_id, depth, visited) AS (", cte);

    /* Zero-hop base (only for *0..N): each node maps to itself. */
    if (min_hops == 0) {
        append_sql(ctx, "SELECT n.id, n.id, 0, ',' || n.id || ',' FROM nodes n UNION ALL ");
    }

    /* Depth-1 base case. */
    append_sql(ctx, "SELECT e.%s, e.%s, 1, ',' || e.%s || ',' || e.%s || ',' FROM edges e",
               src_col, tgt_col, src_col, tgt_col);
    if (rel->type || (rel->types && rel->types->count > 0)) {
        append_sql(ctx, " WHERE ");
        ep_append_type_pred(ctx, rel);
    }
    if (undirected) {
        append_sql(ctx, " UNION ALL SELECT e.%s, e.%s, 1, ',' || e.%s || ',' || e.%s || ',' FROM edges e",
                   tgt_col, src_col, tgt_col, src_col);
        if (rel->type || (rel->types && rel->types->count > 0)) {
            append_sql(ctx, " WHERE ");
            ep_append_type_pred(ctx, rel);
        }
    }

    /* Recursive step. */
    append_sql(ctx, " UNION ALL ");
    if (undirected) {
        const char *other = "(CASE WHEN e.source_id = cte.end_id THEN e.target_id ELSE e.source_id END)";
        append_sql(ctx,
            "SELECT cte.start_id, %s, cte.depth + 1, cte.visited || %s || ',' "
            "FROM %s cte JOIN edges e ON (e.source_id = cte.end_id OR e.target_id = cte.end_id) "
            "WHERE cte.depth >= 1 AND cte.depth < %d "
            "AND cte.visited NOT LIKE '%%,' || CAST(%s AS TEXT) || ',%%'",
            other, other, cte, max_hops, other);
    } else {
        append_sql(ctx,
            "SELECT cte.start_id, e.%s, cte.depth + 1, cte.visited || e.%s || ',' "
            "FROM %s cte JOIN edges e ON e.%s = cte.end_id "
            "WHERE cte.depth >= 1 AND cte.depth < %d "
            "AND cte.visited NOT LIKE '%%,' || CAST(e.%s AS TEXT) || ',%%'",
            tgt_col, tgt_col, cte, src_col, max_hops, tgt_col);
    }
    if (rel->type || (rel->types && rel->types->count > 0)) {
        append_sql(ctx, " AND ");
        ep_append_type_pred(ctx, rel);
    }

    append_sql(ctx, ") SELECT 1 FROM %s WHERE %s.start_id = %s.id", cte, cte, lalias);
    if (ralias) {
        append_sql(ctx, " AND %s.end_id = %s.id", cte, ralias);
    }
    if (min_hops >= 0) append_sql(ctx, " AND %s.depth >= %d", cte, min_hops);
    append_sql(ctx, " AND %s.depth <= %d", cte, max_hops);

    /* Honor endpoint label constraints against the CTE's start/end ids
     * (e.g. `(a)-[:T*]->(b:MissingLabel)`). */
    if (has_labels(lnode)) {
        for (int j = 0; j < lnode->labels->count; j++) {
            const char *lbl = get_label_string(lnode->labels->items[j]);
            if (!lbl) continue;
            append_sql(ctx, " AND EXISTS (SELECT 1 FROM node_labels WHERE node_id = %s.start_id AND label = ", cte);
            append_string_literal(ctx, lbl);
            append_sql(ctx, ")");
        }
    }
    if (has_labels(rnode)) {
        for (int j = 0; j < rnode->labels->count; j++) {
            const char *lbl = get_label_string(rnode->labels->items[j]);
            if (!lbl) continue;
            append_sql(ctx, " AND EXISTS (SELECT 1 FROM node_labels WHERE node_id = %s.end_id AND label = ", cte);
            append_string_literal(ctx, lbl);
            append_sql(ctx, ")");
        }
    }
    return true;
}

/* Transform EXISTS expression */
int transform_exists_expression(cypher_transform_context *ctx, cypher_exists_expr *exists_expr)
{
    CYPHER_DEBUG("Transforming EXISTS expression");

    if (!exists_expr) {
        ctx->has_error = true;
        ctx->error_message = strdup("Invalid EXISTS expression");
        return -1;
    }

    switch (exists_expr->expr_type) {
        case EXISTS_TYPE_PATTERN:
            {
                CYPHER_DEBUG("Transforming EXISTS pattern expression");

                if (!exists_expr->expr.pattern || exists_expr->expr.pattern->count == 0) {
                    ctx->has_error = true;
                    ctx->error_message = strdup("EXISTS pattern expression is empty");
                    return -1;
                }

                /* Generate SQL EXISTS subquery with pattern matching */
                append_sql(ctx, "EXISTS (");

                /* For each pattern in the EXISTS clause, we need to generate a subquery */
                /* that checks if the pattern exists in the database */
                ast_node *pattern = exists_expr->expr.pattern->items[0];

                if (pattern->type == AST_NODE_PATH) {
                    cypher_path *path = (cypher_path*)pattern;

                    /* I-0047 P1: variable-length rel inside the predicate
                     * (`(a)-[*..]-(b)`) needs a recursive reachability CTE,
                     * not the single-hop join the legacy emitter below
                     * produces. Handle that shape first; fall through
                     * otherwise. */
                    if (emit_exists_varlen_path(ctx, path)) {
                        append_sql(ctx, ")");
                        return 0;
                    }

                    /* Simple pattern like (n)-[r]->(m) */
                    if (path->elements && path->elements->count >= 1) {

                        /* Start with SELECT 1 to make it an existence check */
                        append_sql(ctx, "SELECT 1 FROM ");

                        bool first_table = true;
                        char node_aliases[10][32];  /* Support up to 10 nodes in pattern */
                        bool node_is_external[10];  /* Track which nodes are from outer context */
                        int node_count = 0;

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
                                    node_is_external[node_count] = true;
                                } else {
                                    /* Generate new alias and add to FROM */
                                    if (!first_table) {
                                        append_sql(ctx, ", ");
                                    }
                                    snprintf(node_aliases[node_count], sizeof(node_aliases[node_count]),
                                            "n%d", node_count);
                                    append_sql(ctx, "nodes AS %s", node_aliases[node_count]);
                                    node_is_external[node_count] = false;
                                    first_table = false;
                                }
                                node_count++;

                            } else if (element->type == AST_NODE_REL_PATTERN && i > 0) {
                                /* Relationship pattern: -[variable:TYPE]-> */
                                if (!first_table) {
                                    append_sql(ctx, ", ");
                                }
                                append_sql(ctx, "edges AS e%d", i/2);  /* Relationships are at odd indices */
                                first_table = false;
                            }
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

                                /* Join source node with relationship, respecting direction */
                                int left_node = i / 2;
                                int right_node = left_node + 1;

                                if (rel->left_arrow && !rel->right_arrow) {
                                    /* Incoming: <-[]-  means edge goes right_node -> left_node */
                                    append_sql(ctx, "e%d.source_id = %s.id AND e%d.target_id = %s.id",
                                              rel_index, node_aliases[right_node],
                                              rel_index, node_aliases[left_node]);
                                } else if (!rel->left_arrow && !rel->right_arrow) {
                                    /* Undirected: -[]-  match either direction */
                                    append_sql(ctx, "((e%d.source_id = %s.id AND e%d.target_id = %s.id) OR (e%d.source_id = %s.id AND e%d.target_id = %s.id))",
                                              rel_index, node_aliases[left_node],
                                              rel_index, node_aliases[right_node],
                                              rel_index, node_aliases[right_node],
                                              rel_index, node_aliases[left_node]);
                                } else {
                                    /* Outgoing: -[]->  (default) */
                                    append_sql(ctx, "e%d.source_id = %s.id AND e%d.target_id = %s.id",
                                              rel_index, node_aliases[left_node],
                                              rel_index, node_aliases[right_node]);
                                }

                                /* Add relationship type constraint if specified */
                                if (rel->type) {
                                    append_sql(ctx, " AND e%d.type = ", rel_index);
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
                    } else {
                        /* Empty pattern - should not happen */
                        append_sql(ctx, "SELECT 0");
                    }
                } else {
                    /* Unsupported pattern type */
                    ctx->has_error = true;
                    ctx->error_message = strdup("Unsupported pattern type in EXISTS expression");
                    return -1;
                }

                append_sql(ctx, ")");
                return 0;
            }

        case EXISTS_TYPE_PROPERTY:
            {
                CYPHER_DEBUG("Transforming EXISTS property expression");

                if (!exists_expr->expr.property) {
                    ctx->has_error = true;
                    ctx->error_message = strdup("EXISTS property expression is empty");
                    return -1;
                }

                /* Generate property existence check */
                /* This should be a property access like n.property */
                if (exists_expr->expr.property->type == AST_NODE_PROPERTY) {
                    cypher_property *prop = (cypher_property*)exists_expr->expr.property;

                    if (prop->expr->type == AST_NODE_IDENTIFIER) {
                        cypher_identifier *id = (cypher_identifier*)prop->expr;
                        const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);

                        if (!alias) {
                            ctx->has_error = true;
                            char error[256];
                            snprintf(error, sizeof(error), "Unknown variable in EXISTS property: %s", id->name);
                            ctx->error_message = strdup(error);
                            return -1;
                        }

                        /* Multi-graph support: get graph prefix for table references */
                        const char *graph = transform_var_get_graph(ctx->var_ctx, id->name);
                        const char *gprefix = "";
                        char gprefix_buf[64] = "";
                        if (graph && graph[0] != '\0') {
                            snprintf(gprefix_buf, sizeof(gprefix_buf), "%s.", graph);
                            gprefix = gprefix_buf;
                        }

                        /* Generate SQL to check if property exists */
                        append_sql(ctx, "(");
                        append_sql(ctx, "EXISTS (SELECT 1 FROM %snode_props_text npt JOIN %sproperty_keys pk ON npt.key_id = pk.id WHERE npt.node_id = %s.id AND pk.key = ", gprefix, gprefix, alias);
                        append_string_literal(ctx, prop->property_name);
                        append_sql(ctx, ") OR ");
                        append_sql(ctx, "EXISTS (SELECT 1 FROM %snode_props_int npi JOIN %sproperty_keys pk ON npi.key_id = pk.id WHERE npi.node_id = %s.id AND pk.key = ", gprefix, gprefix, alias);
                        append_string_literal(ctx, prop->property_name);
                        append_sql(ctx, ") OR ");
                        append_sql(ctx, "EXISTS (SELECT 1 FROM %snode_props_real npr JOIN %sproperty_keys pk ON npr.key_id = pk.id WHERE npr.node_id = %s.id AND pk.key = ", gprefix, gprefix, alias);
                        append_string_literal(ctx, prop->property_name);
                        append_sql(ctx, ") OR ");
                        append_sql(ctx, "EXISTS (SELECT 1 FROM %snode_props_bool npb JOIN %sproperty_keys pk ON npb.key_id = pk.id WHERE npb.node_id = %s.id AND pk.key = ", gprefix, gprefix, alias);
                        append_string_literal(ctx, prop->property_name);
                        append_sql(ctx, ")");
                        append_sql(ctx, ")");

                        return 0;
                    } else {
                        ctx->has_error = true;
                        ctx->error_message = strdup("EXISTS property must reference a variable");
                        return -1;
                    }
                } else {
                    ctx->has_error = true;
                    ctx->error_message = strdup("EXISTS property expression must be a property access");
                    return -1;
                }
            }

        default:
            ctx->has_error = true;
            ctx->error_message = strdup("Unknown EXISTS expression type");
            return -1;
    }
}

/* Transform list predicate: all/any/none/single(x IN list WHERE predicate)
 *
 * Emits openCypher three-valued-logic semantics:
 *
 *   Let T = count of elements where (pred) is TRUE
 *       F = count where (pred) is FALSE
 *       N = count where (pred) is NULL
 *       TOT = T+F+N
 *
 *   all:    TOT=0 → TRUE ; F>0 → FALSE ; N>0 → NULL ; else TRUE
 *   any:    T>0 → TRUE  ; N>0 → NULL  ; else FALSE
 *   none:   T>0 → FALSE ; N>0 → NULL  ; else TRUE   (TOT=0 also TRUE)
 *   single: T>1 → FALSE ; N>0 → NULL  ; T=1 → TRUE  ; else FALSE
 *
 * SQL pattern:
 *   (SELECT CASE ... END FROM (SELECT (<pred>) AS p FROM json_each(<list>)))
 */
int transform_list_predicate(cypher_transform_context *ctx, cypher_list_predicate *pred)
{
    CYPHER_DEBUG("Transforming list predicate type %d", pred->pred_type);

    if (!pred || !pred->variable || !pred->list_expr || !pred->predicate) {
        ctx->has_error = true;
        ctx->error_message = strdup("Invalid list predicate");
        return -1;
    }

    /* Save the old alias if this variable name already exists */
    const char *old_alias = transform_var_get_alias(ctx->var_ctx, pred->variable);
    char *saved_alias = old_alias ? strdup(old_alias) : NULL;

    /* Give this level's json_each a unique alias so nested quantifiers
     * (e.g. single(x IN ... WHERE none(y IN x ...))) don't shadow each
     * other — the inner json_each must not capture the outer iteration
     * variable. Map the predicate variable to "<alias>.value".
     * (Quantifier6 family.) */
    char je_alias[32], je_value[40];
    snprintf(je_alias, sizeof(je_alias), "_je_%d", ctx->quantifier_counter++);
    snprintf(je_value, sizeof(je_value), "%s.value", je_alias);

    /* Register the predicate variable to map to <alias>.value */
    transform_var_register_projected(ctx->var_ctx, pred->variable, je_value);

    /* all/any/none/single all use strict openCypher 3VL via the SUM-over-
     * json_each CASE below. (An earlier note claimed `all` had to keep a
     * legacy "ignore NULL → TRUE" form to satisfy Precedence1 [14]–[28];
     * that conflict no longer exists — Precedence1 stays 55/55 under strict
     * 3VL, and strict 3VL fixes Quantifier4 [10].) */
    {
        append_sql(ctx, "(SELECT CASE ");
        switch (pred->pred_type) {
            case LIST_PRED_ALL:
                /* F>0 → FALSE ; N>0 → NULL ; else TRUE (strict 3VL,
                 * Quantifier4 [10]). */
                append_sql(ctx,
                    "WHEN SUM(CASE WHEN p IS NOT NULL AND NOT p THEN 1 ELSE 0 END) > 0 THEN 0 "
                    "WHEN SUM(CASE WHEN p IS NULL THEN 1 ELSE 0 END) > 0 THEN NULL "
                    "ELSE 1 ");
                break;
            case LIST_PRED_ANY:
                /* T>0 → TRUE ; N>0 → NULL ; else FALSE */
                append_sql(ctx,
                    "WHEN SUM(CASE WHEN p IS NOT NULL AND p THEN 1 ELSE 0 END) > 0 THEN 1 "
                    "WHEN SUM(CASE WHEN p IS NULL THEN 1 ELSE 0 END) > 0 THEN NULL "
                    "ELSE 0 ");
                break;
            case LIST_PRED_NONE:
                /* T>0 → FALSE ; N>0 → NULL ; else TRUE */
                append_sql(ctx,
                    "WHEN SUM(CASE WHEN p IS NOT NULL AND p THEN 1 ELSE 0 END) > 0 THEN 0 "
                    "WHEN SUM(CASE WHEN p IS NULL THEN 1 ELSE 0 END) > 0 THEN NULL "
                    "ELSE 1 ");
                break;
            case LIST_PRED_SINGLE:
                /* T>1 → FALSE ; N>0 → NULL ; T=1 → TRUE ; else FALSE */
                append_sql(ctx,
                    "WHEN SUM(CASE WHEN p IS NOT NULL AND p THEN 1 ELSE 0 END) > 1 THEN 0 "
                    "WHEN SUM(CASE WHEN p IS NULL THEN 1 ELSE 0 END) > 0 THEN NULL "
                    "WHEN SUM(CASE WHEN p IS NOT NULL AND p THEN 1 ELSE 0 END) = 1 THEN 1 "
                    "ELSE 0 ");
                break;
            default:
                break;
        }
        append_sql(ctx, "END FROM (SELECT (");
        if (transform_expression(ctx, pred->predicate) < 0) {
            if (saved_alias) free(saved_alias);
            return -1;
        }
        append_sql(ctx, ") AS p FROM json_each(");
        if (transform_expression(ctx, pred->list_expr) < 0) {
            if (saved_alias) free(saved_alias);
            return -1;
        }
        append_sql(ctx, ") AS %s))", je_alias);
    }

    /* Restore the old alias if we saved one */
    if (saved_alias) {
        transform_var_register_projected(ctx->var_ctx, pred->variable, saved_alias);
        free(saved_alias);
    }

    return 0;
}

/* Transform reduce expression: reduce(acc = initial, x IN list | expr)
 *
 * SQL generation using recursive CTE:
 * (WITH RECURSIVE _reduce_N AS (
 *     SELECT initial AS acc, 0 AS idx
 *     UNION ALL
 *     SELECT (expression), idx + 1
 *     FROM _reduce_N, json_each(list)
 *     WHERE idx = json_each.key
 * )
 * SELECT acc FROM _reduce_N WHERE idx = json_array_length(list))
 */
int transform_reduce_expr(cypher_transform_context *ctx, cypher_reduce_expr *reduce)
{
    CYPHER_DEBUG("Transforming reduce expression");

    if (!reduce || !reduce->accumulator || !reduce->initial_value ||
        !reduce->variable || !reduce->list_expr || !reduce->expression) {
        ctx->has_error = true;
        ctx->error_message = strdup("Invalid reduce expression");
        return -1;
    }

    /* Generate unique CTE name */
    char cte_name[32];
    snprintf(cte_name, sizeof(cte_name), "_reduce_%d", ctx->reduce_counter++);

    /* Save existing aliases for accumulator and variable names */
    const char *old_acc_alias = transform_var_get_alias(ctx->var_ctx, reduce->accumulator);
    const char *old_var_alias = transform_var_get_alias(ctx->var_ctx, reduce->variable);
    char *saved_acc_alias = old_acc_alias ? strdup(old_acc_alias) : NULL;
    char *saved_var_alias = old_var_alias ? strdup(old_var_alias) : NULL;

    append_sql(ctx, "(WITH RECURSIVE %s AS (SELECT ", cte_name);

    /* Transform initial value */
    if (transform_expression(ctx, reduce->initial_value) < 0) {
        if (saved_acc_alias) free(saved_acc_alias);
        if (saved_var_alias) free(saved_var_alias);
        return -1;
    }

    append_sql(ctx, " AS acc, 0 AS idx UNION ALL SELECT (");

    /* Register variables for the expression:
     * - accumulator maps to cte_name.acc
     * - variable maps to json_each.value
     */
    char acc_ref[64];
    snprintf(acc_ref, sizeof(acc_ref), "%s.acc", cte_name);
    /* Register in unified system */
    transform_var_register_projected(ctx->var_ctx, reduce->accumulator, acc_ref);
    transform_var_register_projected(ctx->var_ctx, reduce->variable, "json_each.value");

    /* Transform the expression that computes new accumulator value */
    if (transform_expression(ctx, reduce->expression) < 0) {
        if (saved_acc_alias) free(saved_acc_alias);
        if (saved_var_alias) free(saved_var_alias);
        return -1;
    }

    append_sql(ctx, "), idx + 1 FROM %s, json_each(", cte_name);

    /* Transform list expression */
    if (transform_expression(ctx, reduce->list_expr) < 0) {
        if (saved_acc_alias) free(saved_acc_alias);
        if (saved_var_alias) free(saved_var_alias);
        return -1;
    }

    append_sql(ctx, ") WHERE %s.idx = json_each.key) SELECT acc FROM %s WHERE idx = json_array_length(",
               cte_name, cte_name);

    /* Transform list expression again for length check */
    if (transform_expression(ctx, reduce->list_expr) < 0) {
        if (saved_acc_alias) free(saved_acc_alias);
        if (saved_var_alias) free(saved_var_alias);
        return -1;
    }

    append_sql(ctx, "))");

    /* Restore old aliases */
    if (saved_acc_alias) {
        transform_var_register_projected(ctx->var_ctx, reduce->accumulator, saved_acc_alias);
        free(saved_acc_alias);
    }
    if (saved_var_alias) {
        transform_var_register_projected(ctx->var_ctx, reduce->variable, saved_var_alias);
        free(saved_var_alias);
    }

    return 0;
}
