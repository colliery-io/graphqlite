/*
 * MATCH clause transformation
 * Converts MATCH patterns into SQL SELECT queries
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "transform/cypher_transform.h"
#include "transform/transform_helpers.h"
#include "transform/sql_builder.h"
#include "parser/cypher_debug.h"

/* Forward declarations */
/* I-0047 P3: accumulate a bound-rel OPTIONAL endpoint constraint to be flushed
 * onto the last LEFT JOIN's ON after the path loop (see pending_optional_on). */
static void transform_ctx_append_pending_optional_on(cypher_transform_context *ctx,
                                                     const char *cond)
{
    if (!ctx || !cond || !cond[0]) return;
    if (!ctx->pending_optional_on) {
        ctx->pending_optional_on = strdup(cond);
        return;
    }
    size_t old_len = strlen(ctx->pending_optional_on);
    size_t add_len = strlen(cond) + 6; /* " AND " + NUL */
    char *grown = realloc(ctx->pending_optional_on, old_len + add_len);
    if (!grown) return;
    strcat(grown, " AND ");
    strcat(grown, cond);
    ctx->pending_optional_on = grown;
}

static int transform_match_pattern(cypher_transform_context *ctx, ast_node *pattern, bool optional);
static int generate_node_match(cypher_transform_context *ctx, cypher_node_pattern *node, const char *alias, bool optional);
static int generate_relationship_match(cypher_transform_context *ctx, cypher_rel_pattern *rel,
                                     cypher_node_pattern *source_node, cypher_node_pattern *target_node,
                                     int rel_index, bool optional, path_type ptype,
                                     bool src_deferred, bool tgt_deferred);
/* T-0320 transitional alias: the previous call sites used the no-defer
 * variant. Keep a thin wrapper for clarity at the path-loop call site
 * that already computed the flags. */
static inline int generate_relationship_match_with_defer(
    cypher_transform_context *ctx, cypher_rel_pattern *rel,
    cypher_node_pattern *source_node, cypher_node_pattern *target_node,
    int rel_index, bool optional, path_type ptype,
    bool src_deferred, bool tgt_deferred)
{
    return generate_relationship_match(ctx, rel, source_node, target_node,
                                       rel_index, optional, ptype,
                                       src_deferred, tgt_deferred);
}

/*
 * Generate the proper node id reference for join conditions.
 * For regular nodes: returns "alias.id"
 * For projected variables (from WITH): returns just the alias (which IS the id)
 *
 * Returns a static buffer - not thread-safe, must be used immediately.
 * If var_name is NULL, falls back to using alias.id.
 */
static const char *get_node_id_ref(cypher_transform_context *ctx,
                                   const char *alias,
                                   const char *var_name)
{
    static char id_ref_buf[256];

    /* Check if this is a projected variable or a post-WITH node/edge (alias_is_id) */
    bool skip_id_suffix = var_name &&
        (transform_var_is_projected(ctx->var_ctx, var_name) ||
         transform_var_alias_is_id(ctx->var_ctx, var_name));

    if (skip_id_suffix) {
        /* For projected variables or post-WITH variables, the alias IS the id value */
        snprintf(id_ref_buf, sizeof(id_ref_buf), "%s", alias);
    } else {
        /* For regular nodes, use alias.id */
        snprintf(id_ref_buf, sizeof(id_ref_buf), "%s.id", alias);
    }

    return id_ref_buf;
}

/* Transform a MATCH clause into SQL */
int transform_match_clause(cypher_transform_context *ctx, cypher_match *match)
{
    CYPHER_DEBUG("Transforming %s MATCH clause", match->optional ? "OPTIONAL" : "regular");

    if (!ctx || !match) {
        return -1;
    }

    /* Mark this as a read query */
    if (ctx->query_type == QUERY_TYPE_UNKNOWN) {
        ctx->query_type = QUERY_TYPE_READ;
    } else if (ctx->query_type == QUERY_TYPE_WRITE) {
        ctx->query_type = QUERY_TYPE_MIXED;
    }

    /* SQL builder mode is now determined at query level */

    /* Unified builder handles SELECT in RETURN clause - no need to start SELECT here */

    /* Multi-graph support: set current graph for table prefixing */
    ctx->current_graph = match->from_graph;

    /* Save variable count before processing patterns (for multi-graph support) */
    int var_count_before = transform_var_count(ctx->var_ctx);

    /* Process each pattern in the MATCH - this only adds table joins */
    for (int i = 0; i < match->pattern->count; i++) {
        ast_node *pattern = match->pattern->items[i];

        if (pattern->type != AST_NODE_PATH) {
            ctx->has_error = true;
            ctx->error_message = strdup("Invalid pattern type in MATCH");
            return -1;
        }

        if (transform_match_pattern(ctx, pattern, match->optional) < 0) {
            return -1;
        }
    }

    /* Multi-graph support: set graph on all newly registered variables */
    if (match->from_graph) {
        int var_count_after = transform_var_count(ctx->var_ctx);
        for (int i = var_count_before; i < var_count_after; i++) {
            transform_var *var = transform_var_at(ctx->var_ctx, i);
            if (var && var->name) {
                transform_var_set_graph(ctx->var_ctx, var->name, match->from_graph);
            }
        }
    }

    /* Now add WHERE constraints for all patterns */
    /* All constraints go through unified builder's sql_where() */
    
    /* For OPTIONAL MATCH, skip pattern constraint generation (constraints are in JOIN ON clauses) */
    if (match->optional) {
        goto handle_where_clause;
    }
    for (int i = 0; i < match->pattern->count; i++) {
        ast_node *pattern = match->pattern->items[i];
        cypher_path *path = (cypher_path*)pattern;
        
        /* Add constraints for each node in this pattern */
        for (int j = 0; j < path->elements->count; j++) {
            ast_node *element = path->elements->items[j];
            
            if (element->type == AST_NODE_NODE_PATTERN) {
                cypher_node_pattern *node = (cypher_node_pattern*)element;
                
                /* Use unified variable system for node variables */
                const char *alias;
                if (node->variable) {
                    transform_var *var = transform_var_lookup(ctx->var_ctx, node->variable);
                    if (var && (var->kind == VAR_KIND_EDGE || var->kind == VAR_KIND_PATH)) {
                        ctx->has_error = true;
                        char msg[256];
                        snprintf(msg, sizeof(msg),
                                 "Variable `%s` is already bound as a %s and cannot be used as a node (SyntaxError: VariableTypeConflict)",
                                 node->variable,
                                 var->kind == VAR_KIND_EDGE ? "relationship" : "path");
                        ctx->error_message = strdup(msg);
                        return -1;
                    }
                    if (!var) {
                        /* New variable - register it */
                        char *gen_alias = get_next_default_alias(ctx);
                        if (!gen_alias) {
                            ctx->has_error = true;
                            ctx->error_message = strdup("Failed to allocate alias");
                            return -1;
                        }
                        const char *label = has_labels(node) ? get_label_string(node->labels->items[0]) : NULL;
                        if (transform_var_register_node(ctx->var_ctx, node->variable, gen_alias, label) < 0) {
                            free(gen_alias);
                            ctx->has_error = true;
                            ctx->error_message = strdup("Failed to add node variable");
                            return -1;
                        }
                        free(gen_alias);
                    }
                    alias = transform_var_get_alias(ctx->var_ctx, node->variable);
                    if (!alias) {
                        ctx->has_error = true;
                        ctx->error_message = strdup("Failed to get node variable alias");
                        return -1;
                    }
                } else {
                    /* Anonymous node - use legacy approach for now */
                    char temp_alias[32];
                    snprintf(temp_alias, sizeof(temp_alias), "n_%d", ctx->anon_node_base + j);
                    alias = temp_alias;
                }
                
                /* Labels are now handled via JOIN in generate_node_match - skip EXISTS */

                /* Add property VALUE constraints (the JOINs are in generate_node_match) */
                if (node->properties && node->properties->type == AST_NODE_MAP) {
                    cypher_map *map = (cypher_map*)node->properties;
                    if (map->pairs) {
                        for (int k = 0; k < map->pairs->count; k++) {
                            cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[k];
                            /* Skip if key is NULL (already handled in JOIN) */
                            if (!pair->key) continue;
                            if (pair->value && pair->value->type == AST_NODE_LITERAL) {
                                /* Emit a per-property EXISTS subquery keyed by the pair's key
                                 * instead of reusing the first-pair join's _prop_<alias>.value.
                                 * Fixes GQLITE-T-0191: {k1:v1, k2:v2, ...} previously generated
                                 * `_prop_<alias>.value = v1 AND _prop_<alias>.value = v2 ...`
                                 * which is impossible. */
                                cypher_literal *lit = (cypher_literal*)pair->value;
                                dynamic_buffer cond;
                                dbuf_init(&cond);

                                /* T-0307 follow-on: use get_node_id_ref so a
                                 * WITH-bound node's alias (a column reference
                                 * like _with_0.a) is used directly instead of
                                 * appending an invalid `.id` suffix. */
                                char node_id_ref[256];
                                snprintf(node_id_ref, sizeof(node_id_ref), "%s",
                                         get_node_id_ref(ctx, alias, node->variable));

                                if (lit->literal_type == LITERAL_NULL) {
                                    /* Key should not exist on the node (in any scalar table). */
                                    dbuf_appendf(&cond,
                                        "NOT EXISTS(SELECT 1 FROM node_props_text npt "
                                        "JOIN property_keys pk ON npt.key_id = pk.id "
                                        "WHERE npt.node_id = %s AND pk.key = '%s') AND "
                                        "NOT EXISTS(SELECT 1 FROM node_props_int npi "
                                        "JOIN property_keys pk ON npi.key_id = pk.id "
                                        "WHERE npi.node_id = %s AND pk.key = '%s') AND "
                                        "NOT EXISTS(SELECT 1 FROM node_props_real npr "
                                        "JOIN property_keys pk ON npr.key_id = pk.id "
                                        "WHERE npr.node_id = %s AND pk.key = '%s') AND "
                                        "NOT EXISTS(SELECT 1 FROM node_props_bool npb "
                                        "JOIN property_keys pk ON npb.key_id = pk.id "
                                        "WHERE npb.node_id = %s AND pk.key = '%s')",
                                        node_id_ref, pair->key, node_id_ref, pair->key,
                                        node_id_ref, pair->key, node_id_ref, pair->key);
                                } else {
                                    const char *prop_table = "node_props_text";
                                    char value_sql[128];
                                    switch (lit->literal_type) {
                                        case LITERAL_STRING: {
                                            prop_table = "node_props_text";
                                            char *escaped = escape_sql_string(lit->value.string);
                                            snprintf(value_sql, sizeof(value_sql), "'%s'",
                                                     escaped ? escaped : lit->value.string);
                                            free(escaped);
                                            break;
                                        }
                                        case LITERAL_INTEGER:
                                            prop_table = "node_props_int";
                                            snprintf(value_sql, sizeof(value_sql), "%lld",
                                                     (long long)lit->value.integer);
                                            break;
                                        case LITERAL_DECIMAL:
                                            prop_table = "node_props_real";
                                            snprintf(value_sql, sizeof(value_sql), "%.17g", lit->value.decimal);
                                            break;
                                        case LITERAL_BOOLEAN:
                                            prop_table = "node_props_bool";
                                            snprintf(value_sql, sizeof(value_sql), "%d",
                                                     lit->value.boolean ? 1 : 0);
                                            break;
                                        default:
                                            value_sql[0] = '\0';
                                            break;
                                    }
                                    dbuf_appendf(&cond,
                                        "EXISTS(SELECT 1 FROM %s t "
                                        "JOIN property_keys pk ON pk.id = t.key_id "
                                        "WHERE t.node_id = %s AND pk.key = '%s' AND t.value = %s)",
                                        prop_table, node_id_ref, pair->key, value_sql);
                                }

                                if (!dbuf_is_empty(&cond)) {
                                    sql_where(ctx->unified_builder, dbuf_get(&cond));
                                }
                                dbuf_free(&cond);
                            } else if (pair->value && pair->value->type == AST_NODE_PARAMETER) {
                                /* Handle parameter in property filter */
                                cypher_parameter *param = (cypher_parameter*)pair->value;
                                dynamic_buffer cond;
                                dbuf_init(&cond);
                                char node_id_ref[256];
                                snprintf(node_id_ref, sizeof(node_id_ref), "%s",
                                         get_node_id_ref(ctx, alias, node->variable));

                                /* Use OR conditions to check each property type table
                                 * This handles string, int, real, and bool params correctly */
                                dbuf_appendf(&cond,
                                    "("
                                    /* String match */
                                    "EXISTS(SELECT 1 FROM node_props_text npt "
                                    "JOIN property_keys pk ON npt.key_id = pk.id "
                                    "WHERE npt.node_id = %s AND pk.key = '%s' AND npt.value = :%s) OR "
                                    /* Integer match */
                                    "EXISTS(SELECT 1 FROM node_props_int npi "
                                    "JOIN property_keys pk ON npi.key_id = pk.id "
                                    "WHERE npi.node_id = %s AND pk.key = '%s' AND npi.value = :%s) OR "
                                    /* Real match */
                                    "EXISTS(SELECT 1 FROM node_props_real npr "
                                    "JOIN property_keys pk ON npr.key_id = pk.id "
                                    "WHERE npr.node_id = %s AND pk.key = '%s' AND npr.value = :%s) OR "
                                    /* Boolean match */
                                    "EXISTS(SELECT 1 FROM node_props_bool npb "
                                    "JOIN property_keys pk ON npb.key_id = pk.id "
                                    "WHERE npb.node_id = %s AND pk.key = '%s' AND npb.value = :%s)"
                                    ")",
                                    node_id_ref, pair->key, param->name,
                                    node_id_ref, pair->key, param->name,
                                    node_id_ref, pair->key, param->name,
                                    node_id_ref, pair->key, param->name);

                                sql_where(ctx->unified_builder, dbuf_get(&cond));
                                dbuf_free(&cond);
                            } else if (pair->value && pair->value->type == AST_NODE_PROPERTY) {
                                /* Handle property access RHS like `item.id` where
                                 * `item` is an UNWIND-projected variable (GQLITE-T-0185).
                                 * Resolve the base through var_ctx to its SQL alias
                                 * (e.g. `_unwind_0.value`), then emit
                                 * json_extract(<alias>, '$.<field>') as the comparison RHS. */
                                cypher_property *prop = (cypher_property*)pair->value;
                                if (prop->expr && prop->expr->type == AST_NODE_IDENTIFIER && prop->property_name) {
                                    cypher_identifier *base_id = (cypher_identifier*)prop->expr;
                                    const char *base_alias = transform_var_get_alias(ctx->var_ctx, base_id->name);
                                    if (base_alias) {
                                        dynamic_buffer cond;
                                        dbuf_init(&cond);
                                        dbuf_appendf(&cond,
                                            "("
                                            "EXISTS(SELECT 1 FROM node_props_text npt "
                                            "JOIN property_keys pk ON npt.key_id = pk.id "
                                            "WHERE npt.node_id = %s.id AND pk.key = '%s' "
                                            "AND npt.value = json_extract(%s, '$.%s')) OR "
                                            "EXISTS(SELECT 1 FROM node_props_int npi "
                                            "JOIN property_keys pk ON npi.key_id = pk.id "
                                            "WHERE npi.node_id = %s.id AND pk.key = '%s' "
                                            "AND npi.value = json_extract(%s, '$.%s')) OR "
                                            "EXISTS(SELECT 1 FROM node_props_real npr "
                                            "JOIN property_keys pk ON npr.key_id = pk.id "
                                            "WHERE npr.node_id = %s.id AND pk.key = '%s' "
                                            "AND npr.value = json_extract(%s, '$.%s')) OR "
                                            "EXISTS(SELECT 1 FROM node_props_bool npb "
                                            "JOIN property_keys pk ON npb.key_id = pk.id "
                                            "WHERE npb.node_id = %s.id AND pk.key = '%s' "
                                            "AND npb.value = json_extract(%s, '$.%s'))"
                                            ")",
                                            alias, pair->key, base_alias, prop->property_name,
                                            alias, pair->key, base_alias, prop->property_name,
                                            alias, pair->key, base_alias, prop->property_name,
                                            alias, pair->key, base_alias, prop->property_name);
                                        sql_where(ctx->unified_builder, dbuf_get(&cond));
                                        dbuf_free(&cond);
                                    }
                                }
                            }
                        }
                    }
                }
            } else if (element->type == AST_NODE_REL_PATTERN) {
                /* Handle relationship patterns - need surrounding nodes */
                if (j == 0 || j + 1 >= path->elements->count) {
                    continue; /* Skip invalid relationship positions */
                }
                
                ast_node *prev_element = path->elements->items[j - 1];
                ast_node *next_element = path->elements->items[j + 1];
                
                if (prev_element->type != AST_NODE_NODE_PATTERN || next_element->type != AST_NODE_NODE_PATTERN) {
                    continue; /* Skip if not properly connected to nodes */
                }
                
                cypher_rel_pattern *rel = (cypher_rel_pattern*)element;
                cypher_node_pattern *source_node = (cypher_node_pattern*)prev_element;
                cypher_node_pattern *target_node = (cypher_node_pattern*)next_element;

                /* Skip variable-length relationships - they're handled in generate_relationship_match */
                if (rel->varlen) {
                    continue;
                }

                /* Get aliases using unified variable system */
                const char *source_alias, *target_alias, *edge_alias;

                /* Source node alias */
                if (source_node->variable) {
                    source_alias = transform_var_get_alias(ctx->var_ctx, source_node->variable);
                    if (!source_alias) {
                        /* Should have been added already, but add if missing */
                        char *gen_alias = get_next_default_alias(ctx);
                        if (gen_alias) {
                            transform_var_register_node(ctx->var_ctx, source_node->variable, gen_alias, NULL);
                            free(gen_alias);
                            source_alias = transform_var_get_alias(ctx->var_ctx, source_node->variable);
                        }
                    }
                    if (!source_alias) continue;
                } else {
                    static char temp_source[32];
                    snprintf(temp_source, sizeof(temp_source), "n_%d", ctx->anon_node_base + (j - 1));
                    source_alias = temp_source;
                }

                /* Target node alias */
                if (target_node->variable) {
                    target_alias = transform_var_get_alias(ctx->var_ctx, target_node->variable);
                    if (!target_alias) {
                        /* Should have been added already, but add if missing */
                        char *gen_alias = get_next_default_alias(ctx);
                        if (gen_alias) {
                            transform_var_register_node(ctx->var_ctx, target_node->variable, gen_alias, NULL);
                            free(gen_alias);
                            target_alias = transform_var_get_alias(ctx->var_ctx, target_node->variable);
                        }
                    }
                    if (!target_alias) continue;
                } else {
                    static char temp_target[32];
                    snprintf(temp_target, sizeof(temp_target), "n_%d", ctx->anon_node_base + (j + 1));
                    target_alias = temp_target;
                }

                /* Edge alias */
                if (rel->variable) {
                    edge_alias = transform_var_get_alias(ctx->var_ctx, rel->variable);
                    if (!edge_alias) {
                        /* New relationship variable */
                        char *gen_alias = get_next_default_alias(ctx);
                        if (gen_alias) {
                            transform_var_register_edge(ctx->var_ctx, rel->variable, gen_alias, rel->type);
                            free(gen_alias);
                            edge_alias = transform_var_get_alias(ctx->var_ctx, rel->variable);
                        }
                    }
                    if (!edge_alias) continue;
                } else {
                    /* This shouldn't happen with AGE pattern - anonymous rels get names assigned */
                    ctx->has_error = true;
                    ctx->error_message = strdup("Internal error: anonymous relationship without assigned name");
                    return -1;
                }

                /* Bound relationship from WITH: endpoint/edge constraints already
                 * emitted by generate_relationship_match via subqueries; skip the
                 * legacy edge_alias.source_id constraint that would produce
                 * invalid SQL like `_with_0.r.source_id`. */
                if (rel->variable &&
                    transform_var_alias_is_id(ctx->var_ctx, rel->variable)) {
                    continue;
                }

                /* Add relationship direction constraints using unified builder */
                dynamic_buffer rel_cond;
                dbuf_init(&rel_cond);

                /* Handle relationship direction */
                /* Get proper id references (handles projected variables from WITH) */
                char source_id_ref[256], target_id_ref[256];
                snprintf(source_id_ref, sizeof(source_id_ref), "%s",
                         get_node_id_ref(ctx, source_alias, source_node->variable));
                snprintf(target_id_ref, sizeof(target_id_ref), "%s",
                         get_node_id_ref(ctx, target_alias, target_node->variable));

                if (rel->left_arrow && !rel->right_arrow) {
                    /* <-[:TYPE]- (reversed: target -> source) */
                    dbuf_appendf(&rel_cond, "%s.source_id = %s AND %s.target_id = %s",
                              edge_alias, target_id_ref, edge_alias, source_id_ref);
                } else if (!rel->left_arrow && !rel->right_arrow) {
                    /* -[:TYPE]- or -- (undirected: match both directions) */
                    dbuf_appendf(&rel_cond, "((%s.source_id = %s AND %s.target_id = %s) OR (%s.source_id = %s AND %s.target_id = %s))",
                              edge_alias, source_id_ref, edge_alias, target_id_ref,
                              edge_alias, target_id_ref, edge_alias, source_id_ref);
                } else {
                    /* -[:TYPE]-> (forward only) */
                    dbuf_appendf(&rel_cond, "%s.source_id = %s AND %s.target_id = %s",
                              edge_alias, source_id_ref, edge_alias, target_id_ref);
                }

                /* Add relationship type constraint if specified */
                if (rel->type) {
                    /* Single type (legacy support) */
                    char *escaped = escape_sql_string(rel->type);
                    dbuf_appendf(&rel_cond, " AND %s.type = '%s'", edge_alias, escaped ? escaped : rel->type);
                    free(escaped);
                } else if (rel->types && rel->types->count > 0) {
                    /* Multiple types - generate OR conditions */
                    dbuf_appendf(&rel_cond, " AND (");
                    for (int t = 0; t < rel->types->count; t++) {
                        if (t > 0) {
                            dbuf_appendf(&rel_cond, " OR ");
                        }
                        /* Type names are stored as string literals in the list */
                        cypher_literal *type_lit = (cypher_literal*)rel->types->items[t];
                        char *type_escaped = escape_sql_string(type_lit->value.string);
                        dbuf_appendf(&rel_cond, "%s.type = '%s'", edge_alias, type_escaped ? type_escaped : type_lit->value.string);
                        free(type_escaped);
                    }
                    dbuf_appendf(&rel_cond, ")");
                }

                sql_where(ctx->unified_builder, dbuf_get(&rel_cond));
                dbuf_free(&rel_cond);

                /* Add inline property constraints for the relationship.
                 * Pattern like (a)-[r:KNOWS {name: 'x'}]->(b) should match
                 * only edges whose `name` property is 'x'. */
                if (rel->properties && rel->properties->type == AST_NODE_MAP) {
                    cypher_map *m = (cypher_map*)rel->properties;
                    if (m->pairs) {
                        for (int pi = 0; pi < m->pairs->count; pi++) {
                            cypher_map_pair *pair = (cypher_map_pair*)m->pairs->items[pi];
                            if (!pair->key || !pair->value) continue;
                            if (pair->value->type == AST_NODE_PARAMETER) {
                                /* Parameter in relationship property filter, e.g.
                                 * ()-[r:KNOWS {tag: $t}]->(). Mirror the node-pattern
                                 * parameter handling: OR conditions across each edge
                                 * property type table so string, int, real, and bool
                                 * params all match correctly (GitHub #96 — previously
                                 * parameters here were silently skipped, so the
                                 * filter matched every edge of the type). */
                                cypher_parameter *param = (cypher_parameter*)pair->value;
                                char *esc_key = escape_sql_string(pair->key);
                                const char *key_sql = esc_key ? esc_key : pair->key;
                                dynamic_buffer cond;
                                dbuf_init(&cond);
                                dbuf_appendf(&cond,
                                    "("
                                    "EXISTS(SELECT 1 FROM edge_props_text ept "
                                    "JOIN property_keys pk ON ept.key_id = pk.id "
                                    "WHERE ept.edge_id = %s.id AND pk.key = '%s' AND ept.value = :%s) OR "
                                    "EXISTS(SELECT 1 FROM edge_props_int epi "
                                    "JOIN property_keys pk ON epi.key_id = pk.id "
                                    "WHERE epi.edge_id = %s.id AND pk.key = '%s' AND epi.value = :%s) OR "
                                    "EXISTS(SELECT 1 FROM edge_props_real epr "
                                    "JOIN property_keys pk ON epr.key_id = pk.id "
                                    "WHERE epr.edge_id = %s.id AND pk.key = '%s' AND epr.value = :%s) OR "
                                    "EXISTS(SELECT 1 FROM edge_props_bool epb "
                                    "JOIN property_keys pk ON epb.key_id = pk.id "
                                    "WHERE epb.edge_id = %s.id AND pk.key = '%s' AND epb.value = :%s)"
                                    ")",
                                    edge_alias, key_sql, param->name,
                                    edge_alias, key_sql, param->name,
                                    edge_alias, key_sql, param->name,
                                    edge_alias, key_sql, param->name);
                                free(esc_key);
                                sql_where(ctx->unified_builder, dbuf_get(&cond));
                                dbuf_free(&cond);
                                continue;
                            }
                            if (pair->value->type != AST_NODE_LITERAL) continue;
                            cypher_literal *lit = (cypher_literal*)pair->value;
                            const char *tbl = NULL;
                            char val_buf[256] = "";
                            switch (lit->literal_type) {
                                case LITERAL_STRING: tbl = "edge_props_text";
                                    { char *esc = escape_sql_string(lit->value.string);
                                      snprintf(val_buf, sizeof(val_buf), "'%s'", esc ? esc : lit->value.string);
                                      free(esc); } break;
                                case LITERAL_INTEGER: tbl = "edge_props_int";
                                    snprintf(val_buf, sizeof(val_buf), "%lld", (long long)lit->value.integer); break;
                                case LITERAL_DECIMAL: tbl = "edge_props_real";
                                    snprintf(val_buf, sizeof(val_buf), "%.17g", lit->value.decimal); break;
                                case LITERAL_BOOLEAN: tbl = "edge_props_bool";
                                    snprintf(val_buf, sizeof(val_buf), "%d", lit->value.boolean ? 1 : 0); break;
                                default: continue;
                            }
                            if (!tbl) continue;
                            char *esc_key = escape_sql_string(pair->key);
                            char prop_cond[1024];
                            snprintf(prop_cond, sizeof(prop_cond),
                                "EXISTS (SELECT 1 FROM %s ep JOIN property_keys pk ON ep.key_id = pk.id "
                                "WHERE ep.edge_id = %s.id AND pk.key = '%s' AND ep.value = %s)",
                                tbl, edge_alias, esc_key ? esc_key : pair->key, val_buf);
                            free(esc_key);
                            sql_where(ctx->unified_builder, prop_cond);
                        }
                    }
                }
            }
        }
    }

handle_where_clause:
    /* Handle WHERE clause if present - capture expression to unified builder */
    if (match->where) {
        /* Save current sql_buffer state (transform_expression uses it temporarily) */
        char *saved_buffer = NULL;
        size_t saved_size = ctx->sql_size;
        if (saved_size > 0) {
            saved_buffer = strdup(ctx->sql_buffer);
            if (!saved_buffer) {
                ctx->has_error = true;
                ctx->error_message = strdup("Memory allocation failed");
                return -1;
            }
        }

        /* Clear sql_buffer temporarily */
        ctx->sql_size = 0;
        if (ctx->sql_buffer) {
            ctx->sql_buffer[0] = '\0';
        }

        /* Transform the WHERE expression - appends to sql_buffer */
        if (transform_expression(ctx, match->where) < 0) {
            free(saved_buffer);
            return -1;
        }

        /* Add the expression to the appropriate clause.
         * For OPTIONAL MATCH, append to the last LEFT JOIN's ON clause
         * rather than the outer WHERE, so NULL rows are preserved (issue #34b). */
        if (ctx->sql_size > 0) {
            if (match->optional) {
                sql_join_append_on(ctx->unified_builder, ctx->sql_buffer);

                /* T-0320: for each defer pair recorded by the rel
                 * handler, produce a rewritten copy of the WHERE
                 * (replacing `<deferred_alias>.id` with
                 * `<edge_alias>.<endpoint_col>`) and inject it
                 * into the edge JOIN's ON clause. This pushes the
                 * predicate filter PRE-LEFT-JOIN so non-matching
                 * inner rows don't multiply outer rows.
                 * MatchWhere6 [1]/[2]/[3] family. */
                for (int dp = 0; dp < ctx->optional_defer_pairs_count; dp++) {
                    const char *def_alias = ctx->optional_defer_pairs[dp].deferred_alias;
                    const char *edge_a = ctx->optional_defer_pairs[dp].edge_alias;
                    const char *col = ctx->optional_defer_pairs[dp].endpoint_col;
                    /* Build needle `<def_alias>.id` and replacement
                     * `<edge_a>.<col>`. Scan the WHERE SQL and
                     * produce a rewritten copy. */
                    char needle[80];
                    snprintf(needle, sizeof(needle), "%s.id", def_alias);
                    size_t nlen = strlen(needle);
                    char repl[80];
                    snprintf(repl, sizeof(repl), "%s.%s", edge_a, col);
                    size_t rlen = strlen(repl);
                    const char *src_buf = ctx->sql_buffer;
                    size_t src_len = strlen(src_buf);
                    /* Count occurrences for sizing. */
                    int n_occ = 0;
                    for (const char *p = src_buf; (p = strstr(p, needle)) != NULL; p += nlen) {
                        n_occ++;
                    }
                    if (n_occ == 0) continue;
                    size_t out_cap = src_len + n_occ * (rlen - nlen) + 1;
                    char *rewritten = malloc(out_cap);
                    if (!rewritten) continue;
                    char *out_p = rewritten;
                    const char *cur = src_buf;
                    while (1) {
                        const char *hit = strstr(cur, needle);
                        if (!hit) {
                            strcpy(out_p, cur);
                            break;
                        }
                        size_t prefix_len = hit - cur;
                        memcpy(out_p, cur, prefix_len);
                        out_p += prefix_len;
                        memcpy(out_p, repl, rlen);
                        out_p += rlen;
                        cur = hit + nlen;
                    }
                    /* Inject into the edge JOIN's ON. Find ` AS
                     * <edge_a> ` in the joins buffer; the end of
                     * that JOIN's ON is the position of the next
                     * " LEFT JOIN" / " JOIN" / " CROSS JOIN" or
                     * end-of-buffer. Insert " AND <rewritten>" there. */
                    sql_builder *b = ctx->unified_builder;
                    const char *jbuf = dbuf_get(&b->joins);
                    if (!jbuf) { free(rewritten); continue; }
                    char marker[80];
                    snprintf(marker, sizeof(marker), " AS %s", edge_a);
                    const char *anchor = NULL;
                    /* Find last occurrence of the marker. */
                    for (const char *p = jbuf; (p = strstr(p, marker)) != NULL; p++) {
                        anchor = p;
                    }
                    if (!anchor) { free(rewritten); continue; }
                    /* Move past the marker. */
                    anchor += strlen(marker);
                    /* Find the start of the next JOIN keyword. */
                    const char *next_left = strstr(anchor, " LEFT JOIN ");
                    const char *next_inner = strstr(anchor, " JOIN ");
                    const char *next_cross = strstr(anchor, " CROSS JOIN ");
                    const char *insertion = NULL;
                    if (next_left) insertion = next_left;
                    if (next_inner && (!insertion || next_inner < insertion)) insertion = next_inner;
                    if (next_cross && (!insertion || next_cross < insertion)) insertion = next_cross;
                    if (!insertion) {
                        /* End of buffer — just append. */
                        dbuf_appendf(&b->joins, " AND %s", rewritten);
                    } else {
                        /* Build replacement: prefix + " AND <rewritten>" + suffix. */
                        size_t prefix_len = insertion - jbuf;
                        size_t old_jlen = strlen(jbuf);
                        size_t inj_len = 5 + strlen(rewritten); /* " AND " */
                        char *new_jbuf = malloc(old_jlen + inj_len + 1);
                        if (new_jbuf) {
                            memcpy(new_jbuf, jbuf, prefix_len);
                            memcpy(new_jbuf + prefix_len, " AND ", 5);
                            memcpy(new_jbuf + prefix_len + 5, rewritten, strlen(rewritten));
                            memcpy(new_jbuf + prefix_len + inj_len,
                                   jbuf + prefix_len, old_jlen - prefix_len);
                            new_jbuf[old_jlen + inj_len] = '\0';
                            /* Replace the joins buffer. */
                            dbuf_clear(&b->joins);
                            dbuf_append(&b->joins, new_jbuf);
                            free(new_jbuf);
                        }
                    }
                    free(rewritten);
                }
            } else {
                sql_where(ctx->unified_builder, ctx->sql_buffer);
            }
        }

        /* T-0320: clear defer-pair tracking after WHERE is processed
         * so subsequent MATCH clauses don't see stale pairs. */
        cypher_transform_clear_defer_pairs(ctx);

        /* Restore sql_buffer */
        ctx->sql_size = saved_size;
        if (saved_buffer) {
            strcpy(ctx->sql_buffer, saved_buffer);
            free(saved_buffer);
        } else if (ctx->sql_buffer) {
            ctx->sql_buffer[0] = '\0';
        }
    }

    /* I-0047 P3: flush any stashed bound-rel OPTIONAL endpoint constraint onto
     * the last LEFT JOIN's ON (the fresh target node's join, now emitted).
     * Runs whether or not a WHERE clause was present (Match7 [4],
     * MatchWhere6 [5]). */
    if (ctx->pending_optional_on) {
        sql_join_append_on(ctx->unified_builder, ctx->pending_optional_on);
        free(ctx->pending_optional_on);
        ctx->pending_optional_on = NULL;
    }

    /* Clear current graph after MATCH processing */
    ctx->current_graph = NULL;

    return 0;
}

/* Transform a single pattern (path) */
static int transform_match_pattern(cypher_transform_context *ctx, ast_node *pattern, bool optional)
{
    cypher_path *path = (cypher_path*)pattern;

    CYPHER_DEBUG("Transforming %s path with %d elements", optional ? "OPTIONAL" : "regular", path->elements->count);

    /* Allocate a fresh range of anon-node alias indices for this pattern
     * so anonymous `n_<j>` aliases don't collide with anon nodes from
     * sibling patterns / preceding MATCH clauses. */
    ctx->anon_node_base = ctx->anon_node_counter;
    ctx->anon_node_counter += path->elements->count;
    
    /* If path has a variable name, register it as a path variable.
     * T-0333 (Match6 [5]/[7]/[9]): if the path has a name (e.g.
     * `MATCH p = (a:L)<--(:M)`), the path projection needs to reference
     * every element by alias to build the hydrated path. Anonymous
     * nodes/rels would emit `null` and produce broken `[id,null,null]`
     * strings. Synthesize names here so the subsequent MATCH-emission
     * pass registers each one and the path projection can resolve them. */
    if (path->var_name) {
        for (int j = 0; j < path->elements->count; j++) {
            ast_node *el = path->elements->items[j];
            if (el->type == AST_NODE_NODE_PATTERN) {
                cypher_node_pattern *np = (cypher_node_pattern*)el;
                if (!np->variable || !np->variable[0]) {
                    char gen[48];
                    snprintf(gen, sizeof(gen), "_pv_n%d_%d", ctx->anon_node_base, j);
                    np->variable = strdup(gen);
                }
            } else if (el->type == AST_NODE_REL_PATTERN) {
                cypher_rel_pattern *rp = (cypher_rel_pattern*)el;
                if (!rp->variable || !rp->variable[0]) {
                    char gen[48];
                    snprintf(gen, sizeof(gen), "_pv_e%d_%d", ctx->anon_node_base, j);
                    rp->variable = strdup(gen);
                }
            }
        }
        CYPHER_DEBUG("Registering path variable: %s with %d elements", path->var_name, path->elements->count);
        if (register_path_variable(ctx, path->var_name, path) < 0) {
            /* register_path_variable already set a specific message if it
             * detected a conflict; only fall back to a generic message if
             * none is present. */
            if (!ctx->error_message) {
                ctx->has_error = true;
                ctx->error_message = strdup("Failed to register path variable");
            }
            return -1;
        }
        CYPHER_DEBUG("Successfully registered path variable: %s", path->var_name);
    } else {
        CYPHER_DEBUG("Path has no variable name - skipping registration");
    }
    
    /* For now, handle simple node patterns */
    /* TODO: Handle relationship patterns */

    /* T-0320: for OPTIONAL MATCH paths, compute per-element "defer to
     * rel handler" flags. A node element is deferred when:
     *   - this MATCH is OPTIONAL,
     *   - the node is NEW in this MATCH (not bound by prior clause),
     *   - the node is adjacent to a non-varlen relationship pattern,
     *   - AT LEAST ONE other endpoint of that rel IS bound.
     *
     * Deferred nodes are NOT emitted by the path-element loop —
     * the rel handler emits them as `LEFT JOIN nodes AS X ON X.id
     * = edge.<src|tgt>_id` AFTER the edge LEFT JOIN, so X is
     * correlated through the edge (preserves OPTIONAL semantics:
     * X is null when no edge match). The varlen path uses a
     * separate code path (already restructured for some shapes). */
    /* T-0330: detect multi-rel OPTIONAL paths eligible for the
     * combined-EXISTS collapse. Eligibility:
     *  - OPTIONAL MATCH.
     *  - 2+ non-varlen rels.
     *  - No user-visible rel variables (synthetic ones from AGE
     *    are OK).
     *  - No named path captures this rel.
     *  - At least one unbound intermediate node.
     *
     * When eligible, emit ONE `LEFT JOIN nodes AS X ON EXISTS(...)`
     * encoding the FULL chain (and skip all per-rel emissions).
     * Without this, the intermediate b gets populated by rel1's
     * EXISTS even when rel2 doesn't match — wrong per Cypher
     * 'all-or-none' OPTIONAL semantics. */
    bool path_combined_exists = false;
    if (optional && path->elements && path->elements->count >= 5) {
        int n_rels = 0;
        bool all_eligible = true;
        bool has_user_rel_var = false;
        for (int j = 0; j < path->elements->count; j++) {
            ast_node *el = path->elements->items[j];
            if (el->type != AST_NODE_REL_PATTERN) continue;
            cypher_rel_pattern *r = (cypher_rel_pattern *)el;
            if (r->varlen) { all_eligible = false; break; }
            if (r->variable &&
                strncmp(r->variable, "_gql_default_alias_", 19) != 0) {
                has_user_rel_var = true;
                break;
            }
            n_rels++;
        }
        bool has_path_var = false;
        for (int vi = 0; vi < ctx->var_ctx->count; vi++) {
            if (ctx->var_ctx->vars[vi].kind == VAR_KIND_PATH) {
                has_path_var = true;
                break;
            }
        }
        /* Restrict to paths where every INTERMEDIATE node (between
         * two rels) has a user variable. Anonymous intermediates
         * can't be referenced as `<alias>.id` inside the EXISTS
         * subquery, and the existing per-rel emission handles
         * them correctly via edge.target_id = edge2.source_id
         * implicit chains. */
        bool intermediates_named = true;
        for (int j = 2; j < path->elements->count - 2; j += 2) {
            ast_node *el = path->elements->items[j];
            if (el->type != AST_NODE_NODE_PATTERN) continue;
            cypher_node_pattern *np = (cypher_node_pattern *)el;
            if (!np->variable) {
                intermediates_named = false;
                break;
            }
        }
        if (all_eligible && !has_user_rel_var && !has_path_var &&
            n_rels >= 2 && intermediates_named) {
            path_combined_exists = true;
        }
    }

    bool *defer_to_rel = NULL;
    if (optional && path->elements && path->elements->count > 0) {
        defer_to_rel = calloc(path->elements->count, sizeof(bool));
        if (!defer_to_rel) {
            ctx->has_error = true;
            ctx->error_message = strdup("Out of memory in path emission analysis");
            return -1;
        }
        for (int j = 0; j < path->elements->count; j++) {
            ast_node *el = path->elements->items[j];
            if (el->type != AST_NODE_REL_PATTERN) continue;
            if (j == 0 || j + 1 >= path->elements->count) continue;
            cypher_rel_pattern *r = (cypher_rel_pattern *)el;
            /* Bound rels (e.g. `WITH r ... OPTIONAL MATCH ...-[r]-...`)
             * use a WHERE-based constraint emission, not a fresh edge
             * JOIN. The endpoint node JOINs must come from the path
             * loop's generate_node_match — don't defer them. */
            if (r->variable &&
                transform_var_alias_is_id(ctx->var_ctx, r->variable)) {
                continue;
            }
            ast_node *prev = path->elements->items[j - 1];
            ast_node *next = path->elements->items[j + 1];
            if (prev->type != AST_NODE_NODE_PATTERN ||
                next->type != AST_NODE_NODE_PATTERN) continue;
            cypher_node_pattern *src = (cypher_node_pattern *)prev;
            cypher_node_pattern *tgt = (cypher_node_pattern *)next;

            /* An endpoint is "available" for the edge JOIN's ON if it
             * is either (a) bound by a prior clause OR (b) has been
             * deferred by an earlier rel in THIS path (which will be
             * in scope by the time this rel handler runs). */
            bool src_avail = false, tgt_avail = false;
            if (src->variable) {
                transform_var *v = transform_var_lookup(ctx->var_ctx, src->variable);
                src_avail = (v != NULL);
            }
            if (tgt->variable) {
                transform_var *v = transform_var_lookup(ctx->var_ctx, tgt->variable);
                tgt_avail = (v != NULL);
            }
            /* Pre-deferred from an earlier rel in this same path:
             * a deferred node is emitted by the previous rel
             * handler, so it's in scope for the next rel. */
            if (!src_avail && defer_to_rel[j - 1]) src_avail = true;
            if (!tgt_avail && defer_to_rel[j + 1]) tgt_avail = true;

            /* Defer the unavailable endpoint when the OTHER endpoint
             * is available (so this rel's ON can anchor to it). */
            if (!src_avail && tgt_avail) {
                defer_to_rel[j - 1] = true;
            }
            if (src_avail && !tgt_avail) {
                defer_to_rel[j + 1] = true;
            }
        }
    }

    /* T-0330: emit the combined-EXISTS LEFT JOIN for eligible
     * multi-rel OPTIONAL paths, BEFORE the per-element loop.
     * Skips the per-rel and per-intermediate-node emissions
     * inside the loop. */
    bool *combined_node_skip = NULL;
    if (path_combined_exists) {
        combined_node_skip = calloc(path->elements->count, sizeof(bool));
        if (!combined_node_skip) {
            ctx->has_error = true;
            ctx->error_message = strdup("Out of memory in path combined-EXISTS");
            free(defer_to_rel);
            return -1;
        }
        /* Assign aliases to all node positions in the path
         * (bound vars already have aliases; new vars get registered;
         * anonymous get synthetic n_<index>). */
        const char *node_alias[64] = {0};
        int n_count = path->elements->count;
        if (n_count > 64) n_count = 64;
        for (int j = 0; j < n_count; j++) {
            ast_node *el = path->elements->items[j];
            if (el->type != AST_NODE_NODE_PATTERN) continue;
            cypher_node_pattern *np = (cypher_node_pattern *)el;
            if (np->variable) {
                const char *al = transform_var_get_alias(ctx->var_ctx, np->variable);
                if (!al) {
                    char *gen = get_next_default_alias(ctx);
                    if (gen) {
                        const char *lbl = has_labels(np) ? get_label_string(np->labels->items[0]) : NULL;
                        transform_var_register_node(ctx->var_ctx, np->variable, gen, lbl);
                        free(gen);
                        al = transform_var_get_alias(ctx->var_ctx, np->variable);
                    }
                }
                node_alias[j] = al;
            } else {
                static char anon_buf[64][32];
                snprintf(anon_buf[j], sizeof(anon_buf[j]), "n_%d", ctx->anon_node_base + j);
                node_alias[j] = anon_buf[j];
            }
        }

        /* Build combined EXISTS body:
         *   SELECT 1 FROM edges e_0, edges e_1, ...
         *   WHERE e_k.src/tgt = node_aliases AND pairwise e_i.id <> e_j.id.
         * Direction handled per rel (left/right arrow flags). */
        dynamic_buffer ec; dbuf_init(&ec);
        dbuf_append(&ec, "EXISTS (SELECT 1 FROM ");
        int rel_idx = 0;
        for (int j = 0; j < path->elements->count; j++) {
            ast_node *el = path->elements->items[j];
            if (el->type != AST_NODE_REL_PATTERN) continue;
            if (rel_idx > 0) dbuf_append(&ec, ", ");
            dbuf_appendf(&ec, "edges _ce%d", rel_idx);
            rel_idx++;
        }
        int total_rels = rel_idx;
        dbuf_append(&ec, " WHERE ");
        rel_idx = 0;
        bool first_cond = true;
        for (int j = 0; j < path->elements->count; j++) {
            ast_node *el = path->elements->items[j];
            if (el->type != AST_NODE_REL_PATTERN) continue;
            cypher_rel_pattern *r = (cypher_rel_pattern *)el;
            const char *src_a = node_alias[j - 1];
            const char *tgt_a = node_alias[j + 1];
            if (!src_a || !tgt_a) {
                dbuf_free(&ec);
                free(combined_node_skip);
                free(defer_to_rel);
                /* Degrade: fall back to per-rel emission. */
                path_combined_exists = false;
                goto skip_combined_exists;
            }
            /* get_node_id_ref returns a static buffer — copy each
             * result into a local before calling again, otherwise
             * both src_id and tgt_id alias the same memory. */
            char src_id[256], tgt_id[256];
            snprintf(src_id, sizeof(src_id), "%s",
                     get_node_id_ref(ctx, src_a, NULL));
            snprintf(tgt_id, sizeof(tgt_id), "%s",
                     get_node_id_ref(ctx, tgt_a, NULL));
            if (!first_cond) dbuf_append(&ec, " AND ");
            first_cond = false;
            if (r->left_arrow && !r->right_arrow) {
                /* <-[…]- reversed */
                dbuf_appendf(&ec,
                    "(_ce%d.source_id = %s AND _ce%d.target_id = %s)",
                    rel_idx, tgt_id, rel_idx, src_id);
            } else if (!r->left_arrow && !r->right_arrow) {
                /* Undirected */
                dbuf_appendf(&ec,
                    "((_ce%d.source_id = %s AND _ce%d.target_id = %s) "
                    "OR (_ce%d.source_id = %s AND _ce%d.target_id = %s))",
                    rel_idx, src_id, rel_idx, tgt_id,
                    rel_idx, tgt_id, rel_idx, src_id);
            } else {
                dbuf_appendf(&ec,
                    "(_ce%d.source_id = %s AND _ce%d.target_id = %s)",
                    rel_idx, src_id, rel_idx, tgt_id);
            }
            if (r->type) {
                char *esc = escape_sql_string(r->type);
                dbuf_appendf(&ec, " AND _ce%d.type = '%s'", rel_idx, esc ? esc : r->type);
                free(esc);
            }
            rel_idx++;
        }
        /* Pairwise edge-uniqueness. */
        for (int a = 0; a < total_rels; a++) {
            for (int b = a + 1; b < total_rels; b++) {
                dbuf_appendf(&ec, " AND _ce%d.id <> _ce%d.id", a, b);
            }
        }
        dbuf_append(&ec, ")");

        /* Emit the LEFT JOIN for each unbound intermediate node.
         * Bound endpoints (a, c) are already in scope from prior
         * MATCH/clause. New intermediate nodes get a LEFT JOIN
         * nodes AS X ON <combined EXISTS>. */
        for (int j = 0; j < path->elements->count; j++) {
            ast_node *el = path->elements->items[j];
            if (el->type != AST_NODE_NODE_PATTERN) continue;
            cypher_node_pattern *np = (cypher_node_pattern *)el;
            /* Bound? (variable existed pre-this-MATCH) */
            bool bound = false;
            if (np->variable) {
                transform_var *v = transform_var_lookup(ctx->var_ctx, np->variable);
                if (v && transform_var_is_bound(ctx->var_ctx, np->variable)) bound = true;
            }
            /* Already in joins/from? */
            const char *al = node_alias[j];
            if (al && !bound) {
                const char *fs = dbuf_get(&ctx->unified_builder->from);
                const char *js = dbuf_get(&ctx->unified_builder->joins);
                char needle[80];
                snprintf(needle, sizeof(needle), " AS %s", al);
                if ((fs && strstr(fs, needle)) || (js && strstr(js, needle))) {
                    bound = true;
                }
            }
            if (bound) {
                combined_node_skip[j] = true;  /* Already in scope, skip path-loop emission. */
                continue;
            }
            /* Unbound intermediate: emit LEFT JOIN nodes AS al ON <ec>. */
            sql_join(ctx->unified_builder, SQL_JOIN_LEFT,
                     get_graph_table(ctx, "nodes"), al, dbuf_get(&ec));
            combined_node_skip[j] = true;
        }
        dbuf_free(&ec);
    }
skip_combined_exists:

    for (int i = 0; i < path->elements->count; i++) {
        /* T-0330: skip path-element emission for nodes already
         * handled by the combined-EXISTS LEFT JOIN, and rel
         * patterns (which are subsumed by the combined EXISTS). */
        if (combined_node_skip) {
            ast_node *el = path->elements->items[i];
            if (el->type == AST_NODE_REL_PATTERN) continue;
            if (el->type == AST_NODE_NODE_PATTERN && combined_node_skip[i]) continue;
        }
        if (defer_to_rel && defer_to_rel[i]) {
            /* T-0320: this node will be emitted by the rel handler
             * via the edge JOIN. Still need to register the variable
             * so downstream lookups resolve to the alias the rel
             * handler will use. */
            ast_node *el = path->elements->items[i];
            if (el->type == AST_NODE_NODE_PATTERN) {
                cypher_node_pattern *n = (cypher_node_pattern *)el;
                if (n->variable) {
                    transform_var *v = transform_var_lookup(ctx->var_ctx, n->variable);
                    if (!v) {
                        char *gen_alias = get_next_default_alias(ctx);
                        if (gen_alias) {
                            const char *label = has_labels(n) ? get_label_string(n->labels->items[0]) : NULL;
                            transform_var_register_node(ctx->var_ctx, n->variable, gen_alias, label);
                            free(gen_alias);
                        }
                    }
                }
            }
            continue;
        }
        ast_node *element = path->elements->items[i];
        
        if (element->type == AST_NODE_NODE_PATTERN) {
            cypher_node_pattern *node = (cypher_node_pattern*)element;
            
            /* Use unified variable system */
            const char *alias;
            bool need_from_clause = false;

            if (node->variable) {
                transform_var *var = transform_var_lookup(ctx->var_ctx, node->variable);
                if (var && (var->kind == VAR_KIND_EDGE || var->kind == VAR_KIND_PATH)) {
                    ctx->has_error = true;
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                             "Variable `%s` is already bound as a %s and cannot be used as a node (SyntaxError: VariableTypeConflict)",
                             node->variable,
                             var->kind == VAR_KIND_EDGE ? "relationship" : "path");
                    ctx->error_message = strdup(msg);
                    return -1;
                }
                if (var && var->is_scalar_value) {
                    ctx->has_error = true;
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                             "Variable `%s` is already bound to a scalar value and cannot be used as a node (SyntaxError: VariableTypeConflict)",
                             node->variable);
                    ctx->error_message = strdup(msg);
                    return -1;
                }
                if (var) {
                    /* Variable exists - check if it's from WITH (projected or alias_is_id) */
                    bool is_from_with = (var->kind == VAR_KIND_PROJECTED) ||
                                       transform_var_alias_is_id(ctx->var_ctx, node->variable);
                    if (is_from_with) {
                        /* Variable from WITH - use the CTE, don't add nodes table */
                        alias = transform_var_get_alias(ctx->var_ctx, node->variable);
                        if (!alias) {
                            ctx->has_error = true;
                            ctx->error_message = strdup("Failed to get alias for projected variable");
                            return -1;
                        }
                        /* Extract CTE name from source_expr (e.g., "_with_0.p" -> "_with_0").
                         * The alias may be a function-wrapped id reference
                         * (e.g. "json_extract(_unwind_0.value, '$.id')" for a
                         * node unwound from collect()); in that case the CTE
                         * token is the identifier ending at the first '.', so
                         * scan back from the dot to the start of that token. */
                        char cte_name[64];
                        const char *dot = strchr(alias, '.');
                        if (dot) {
                            const char *start = dot;
                            while (start > alias &&
                                   (isalnum((unsigned char)start[-1]) || start[-1] == '_')) {
                                start--;
                            }
                            size_t len = dot - start;
                            if (len >= sizeof(cte_name)) len = sizeof(cte_name) - 1;
                            strncpy(cte_name, start, len);
                            cte_name[len] = '\0';
                        } else {
                            strncpy(cte_name, alias, sizeof(cte_name) - 1);
                            cte_name[sizeof(cte_name) - 1] = '\0';
                        }
                        /* Add CTE to FROM clause if not already there */
                        bool cte_in_from = !dbuf_is_empty(&ctx->unified_builder->from) &&
                                           strstr(dbuf_get(&ctx->unified_builder->from), cte_name) != NULL;
                        bool cte_in_joins = !dbuf_is_empty(&ctx->unified_builder->joins) &&
                                            strstr(dbuf_get(&ctx->unified_builder->joins), cte_name) != NULL;
                        if (!cte_in_from && !cte_in_joins) {
                            /* Add as CROSS JOIN if FROM already exists, otherwise as FROM */
                            if (!dbuf_is_empty(&ctx->unified_builder->from)) {
                                sql_join(ctx->unified_builder, SQL_JOIN_CROSS, cte_name, NULL, NULL);
                            } else {
                                sql_from(ctx->unified_builder, cte_name, NULL);
                            }
                        }
                        need_from_clause = false; /* Don't add nodes table */

                        /* T-0307: when re-binding a WITH-bound variable with
                         * additional label predicates (e.g. `WITH a1 MATCH
                         * (a1:X)`), the label check must still be enforced.
                         * Previously the entire node attach was skipped,
                         * silently dropping the predicate. Emit each label
                         * as an EXISTS WHERE condition against node_labels. */
                        if (has_labels(node)) {
                            const char *graph_prefix = ctx->current_graph ? ctx->current_graph : "";
                            for (int li = 0; li < node->labels->count; li++) {
                                const char *label = get_label_string(node->labels->items[li]);
                                if (!label) continue;
                                char *esc_label = escape_sql_string(label);
                                char where_buf[512];
                                snprintf(where_buf, sizeof(where_buf),
                                    "EXISTS (SELECT 1 FROM %snode_labels WHERE node_id = %s AND label = '%s')",
                                    graph_prefix, alias,
                                    esc_label ? esc_label : label);
                                free(esc_label);
                                sql_where(ctx->unified_builder, where_buf);
                            }
                        }
                    } else {
                        /* Regular variable - reuse alias, check if we need FROM clause */
                        alias = transform_var_get_alias(ctx->var_ctx, node->variable);
                        if (!alias) {
                            ctx->has_error = true;
                            ctx->error_message = strdup("Failed to get alias for existing variable");
                            return -1;
                        }
                        /* Check if this alias is already in the unified builder */
                        bool alias_in_builder_from = !dbuf_is_empty(&ctx->unified_builder->from) &&
                                                     strstr(dbuf_get(&ctx->unified_builder->from), alias) != NULL;
                        bool alias_in_builder_joins = !dbuf_is_empty(&ctx->unified_builder->joins) &&
                                                      strstr(dbuf_get(&ctx->unified_builder->joins), alias) != NULL;
                        if (!alias_in_builder_from && !alias_in_builder_joins) {
                            need_from_clause = true;
                        }
                    }
                } else {
                    /* New variable - register it */
                    char *gen_alias = get_next_default_alias(ctx);
                    if (!gen_alias) {
                        ctx->has_error = true;
                        ctx->error_message = strdup("Failed to allocate alias");
                        return -1;
                    }
                    const char *label = has_labels(node) ? get_label_string(node->labels->items[0]) : NULL;
                    if (transform_var_register_node(ctx->var_ctx, node->variable, gen_alias, label) < 0) {
                        free(gen_alias);
                        ctx->has_error = true;
                        ctx->error_message = strdup("Failed to add node variable in pattern");
                        return -1;
                    }
                    free(gen_alias);
                    alias = transform_var_get_alias(ctx->var_ctx, node->variable);
                    if (!alias) {
                        ctx->has_error = true;
                        ctx->error_message = strdup("Failed to get alias for node variable in pattern");
                        return -1;
                    }
                    need_from_clause = true;
                }
            } else {
                /* Anonymous node - use generated alias */
                static char temp_alias[32];
                snprintf(temp_alias, sizeof(temp_alias), "n_%d", ctx->anon_node_base + i);
                alias = temp_alias;
                need_from_clause = true;
            }
            
            /* Generate SQL for this node if needed */
            if (need_from_clause) {
                if (generate_node_match(ctx, node, alias, optional) < 0) {
                    return -1;
                }
            }
            
        } else if (element->type == AST_NODE_REL_PATTERN) {
            /* Handle relationship patterns - need surrounding nodes */
            if (i == 0 || i + 1 >= path->elements->count) {
                ctx->has_error = true;
                ctx->error_message = strdup("Relationship pattern must be between nodes");
                return -1;
            }
            
            ast_node *prev_element = path->elements->items[i - 1];
            ast_node *next_element = path->elements->items[i + 1];
            
            if (prev_element->type != AST_NODE_NODE_PATTERN || next_element->type != AST_NODE_NODE_PATTERN) {
                ctx->has_error = true;
                ctx->error_message = strdup("Relationship must connect node patterns");
                return -1;
            }
            
            cypher_rel_pattern *rel = (cypher_rel_pattern*)element;
            cypher_node_pattern *source_node = (cypher_node_pattern*)prev_element;
            cypher_node_pattern *target_node = (cypher_node_pattern*)next_element;
            
            /* AGE pattern: Assign default name to anonymous relationships */
            if (!rel->variable) {
                char *default_name = get_next_default_alias(ctx);
                rel->variable = default_name;
                /* Note: This modifies the AST, ensuring consistent naming across passes */
            }
            
            /* Generate relationship match SQL.
             * T-0320: pass the defer flags so the rel handler knows
             * to emit deferred-source/target node LEFT JOINs after
             * the edge LEFT JOIN. */
            bool src_deferred = defer_to_rel ? defer_to_rel[i - 1] : false;
            bool tgt_deferred = defer_to_rel ? defer_to_rel[i + 1] : false;
            if (generate_relationship_match_with_defer(ctx, rel, source_node, target_node, i, optional,
                                                       path->type, src_deferred, tgt_deferred) < 0) {
                free(defer_to_rel);
                return -1;
            }
        }
    }

    /* T-0327: enforce Cypher relationship uniqueness — within a single
     * MATCH path, each physical edge can appear at most once across
     * all rel patterns. Collect the edge aliases used by this path
     * and emit pairwise `e_i.id <> e_j.id` constraints. Without this,
     * patterns like `(n)-->(k)<--(n)` over a 2-edge graph produced
     * extra rows by reusing the same edge in both rel slots, and
     * undirected patterns like `(a)--(b)--(c)` over symmetric data
     * gave double-count rows. Match6 [10]/[11]/[12]/[13] family.
     *
     * Skip rels whose edge alias isn't actually emitted as a table
     * (varlen CTE, bound-rel column-ref, or T-0320 EXISTS collapse
     * where the edge is hidden inside an EXISTS subquery — the
     * alias wouldn't be present in the joins buffer). */
    {
        const char *edge_aliases[16];  /* Plenty for any sane path. */
        int n_edges = 0;
        const char *js = dbuf_get(&ctx->unified_builder->joins);
        for (int i = 0; i < path->elements->count && n_edges < 16; i++) {
            ast_node *el = path->elements->items[i];
            if (el->type != AST_NODE_REL_PATTERN) continue;
            cypher_rel_pattern *r = (cypher_rel_pattern *)el;
            if (r->varlen) continue;  /* Varlen has its own uniqueness via path_ids. */
            if (r->variable && transform_var_alias_is_id(ctx->var_ctx, r->variable)) continue;
            if (!r->variable) continue;
            const char *ea = transform_var_get_alias(ctx->var_ctx, r->variable);
            if (!ea) continue;
            /* Confirm the edge is actually emitted as a table in
             * the joins buffer — `edges AS <alias>` substring. If
             * not, the edge is implicit (e.g. inside an EXISTS
             * subquery from the T-0320 collapse) and has no `id`
             * column to reference. */
            char needle[80];
            snprintf(needle, sizeof(needle), "edges AS %s", ea);
            if (!js || !strstr(js, needle)) continue;
            edge_aliases[n_edges++] = ea;
        }
        for (int i = 0; i < n_edges; i++) {
            for (int j = i + 1; j < n_edges; j++) {
                char where_buf[160];
                snprintf(where_buf, sizeof(where_buf), "%s.id <> %s.id",
                         edge_aliases[i], edge_aliases[j]);
                sql_where(ctx->unified_builder, where_buf);
            }
        }

        /* T-0339 follow-up: extend path-wide rel-uniqueness across varlen
         * segments — each varlen's `visited` (the comma-separated edge ids
         * it traversed) must not include any non-varlen rel id used by the
         * same path. Without this, `MATCH …()-[r]-()-[*0..1]-(m)` would
         * count varlen paths that re-use the bound r (Match4 [7]: 84 → 32).
         * Cross-varlen disjointness is a further refinement; not yet
         * implemented (no current TCK scenario depends on it). */
        const char *varlen_aliases[16];
        int n_varlen = 0;
        if (js) {
            const char *p = js;
            while ((p = strstr(p, "_varlen_path_")) != NULL && n_varlen < 16) {
                const char *as = strstr(p, " AS ");
                if (!as) break;
                as += 4;
                const char *end = as;
                while (*end && (isalnum((unsigned char)*end) || *end == '_')) end++;
                if (end > as) {
                    /* dup the alias substring into a small static pool so
                     * its lifetime spans the WHERE emission. */
                    static char pool[16][80];
                    static int pool_idx = 0;
                    int slot = pool_idx++ % 16;
                    size_t alen = (size_t)(end - as);
                    if (alen >= sizeof(pool[slot])) alen = sizeof(pool[slot]) - 1;
                    memcpy(pool[slot], as, alen);
                    pool[slot][alen] = '\0';
                    varlen_aliases[n_varlen++] = pool[slot];
                }
                p = end;
            }
        }
        /* Emit as a WHERE constraint, ONLY when the current path is NOT
         * optional. For OPTIONAL MATCH a WHERE constraint on the varlen
         * filters the preserved-anchor row when the varlen unexpectedly
         * matched something (Match7 [19]); a proper ON-level filter requires
         * deeper join-builder work and is deferred. */
        if (!optional) {
            for (int vi = 0; vi < n_varlen; vi++) {
                for (int ei = 0; ei < n_edges; ei++) {
                    char where_buf[320];
                    snprintf(where_buf, sizeof(where_buf),
                        "%s.visited NOT LIKE '%%,' || CAST(%s.id AS TEXT) || ',%%'",
                        varlen_aliases[vi], edge_aliases[ei]);
                    sql_where(ctx->unified_builder, where_buf);
                }
            }
            for (int vi = 0; vi < n_varlen; vi++) {
                for (int i = 0; i < path->elements->count; i++) {
                    ast_node *el = path->elements->items[i];
                    if (el->type != AST_NODE_REL_PATTERN) continue;
                    cypher_rel_pattern *r = (cypher_rel_pattern *)el;
                    if (r->varlen) continue;
                    if (!r->variable) continue;
                    if (!transform_var_alias_is_id(ctx->var_ctx, r->variable)) continue;
                    const char *colref = transform_var_get_alias(ctx->var_ctx, r->variable);
                    if (!colref) continue;
                    char where_buf[320];
                    snprintf(where_buf, sizeof(where_buf),
                        "%s.visited NOT LIKE '%%,' || CAST(%s AS TEXT) || ',%%'",
                        varlen_aliases[vi], colref);
                    sql_where(ctx->unified_builder, where_buf);
                }
            }
        }
    }

    free(defer_to_rel);
    free(combined_node_skip);
    return 0;
}

/* Generate SQL for matching a node pattern
 *
 * Optimized approach: Use explicit JOINs for labels and properties instead of
 * EXISTS subqueries. This allows SQLite's optimizer to choose efficient join order.
 *
 * For a node (a:Label {prop: value}), we generate:
 *   JOIN node_labels nl_a ON nl_a.node_id = a.id AND nl_a.label = 'Label'
 *   JOIN node_props_int npi_a ON npi_a.node_id = a.id
 *   JOIN property_keys pk_a ON pk_a.id = npi_a.key_id AND pk_a.key = 'prop'
 *   WHERE npi_a.value = value
 */
static int generate_node_match(cypher_transform_context *ctx, cypher_node_pattern *node, const char *alias, bool optional)
{
    const char *first_label = has_labels(node) ? get_label_string(node->labels->items[0]) : NULL;
    CYPHER_DEBUG("Generating %s match for node %s (labels: %s, count: %d)",
                 optional ? "OPTIONAL" : "regular",
                 node->variable ? node->variable : "<anonymous>",
                 first_label ? first_label : "<no label>",
                 node->labels ? node->labels->count : 0);

    /* If this alias has already been added to FROM/joins by an earlier
     * generate_relationship_match call (OPTIONAL MATCH joins the target
     * node through the edge), skip re-adding it to avoid duplicate-alias
     * SQL errors.
     *
     * T-0261: also catch the case where the alias is at the END of the
     * joins buffer (no trailing space/newline). The varlen rel handler
     * emits `CROSS JOIN nodes AS n_3` via sql_join which doesn't add a
     * trailing separator; the path loop then runs generate_node_match
     * for the same anon node and previously emitted a duplicate join.
     * Match4 [6] / Match5 [19]/[21]/[23]/[28]/[29] family. */
    {
        const char *from_str = dbuf_get(&ctx->unified_builder->from);
        const char *joins_str = dbuf_get(&ctx->unified_builder->joins);
        char needle[80];
        snprintf(needle, sizeof(needle), " AS %s ", alias);
        char needle_end[80];
        snprintf(needle_end, sizeof(needle_end), " AS %s\n", alias);
        size_t alias_len = strlen(alias);
        char suffix[80];
        snprintf(suffix, sizeof(suffix), " AS %s", alias);
        size_t suffix_len = strlen(suffix);
        bool found = false;
        if (from_str) {
            if (strstr(from_str, needle) || strstr(from_str, needle_end)) found = true;
            size_t flen = strlen(from_str);
            if (!found && flen >= suffix_len &&
                strcmp(from_str + flen - suffix_len, suffix) == 0) found = true;
        }
        if (!found && joins_str) {
            if (strstr(joins_str, needle) || strstr(joins_str, needle_end)) found = true;
            size_t jlen = strlen(joins_str);
            if (!found && jlen >= suffix_len &&
                strcmp(joins_str + jlen - suffix_len, suffix) == 0) found = true;
        }
        (void)alias_len;
        if (found) {
            CYPHER_DEBUG("Skipping duplicate node match for %s", alias);
            return 0;
        }
    }

    /* Check if there's already a FROM clause using the unified builder */
    bool has_from = !dbuf_is_empty(&ctx->unified_builder->from);
    sql_join_type jtype = optional ? SQL_JOIN_LEFT : SQL_JOIN_INNER;

    /* Check if node has properties - if so, start from property table for better selectivity */
    bool has_properties = (node->properties && node->properties->type == AST_NODE_MAP);
    cypher_map *prop_map = has_properties ? (cypher_map*)node->properties : NULL;
    bool has_prop_pairs = has_properties && prop_map->pairs && prop_map->pairs->count > 0;

    /* Buffer for building ON conditions */
    dynamic_buffer on_cond;
    char prop_alias[64], pk_alias[64], node_alias_buf[64];

    if (!has_from) {
        /* First table in query - use FROM */
        if (has_prop_pairs) {
            /* Start with property table for better selectivity */
            cypher_map_pair *first_pair = (cypher_map_pair*)prop_map->pairs->items[0];
            if (first_pair->key && first_pair->value && first_pair->value->type == AST_NODE_LITERAL) {
                cypher_literal *lit = (cypher_literal*)first_pair->value;
                const char *prop_table = NULL;
                switch (lit->literal_type) {
                    case LITERAL_INTEGER: prop_table = "node_props_int"; break;
                    case LITERAL_STRING:  prop_table = "node_props_text"; break;
                    case LITERAL_DECIMAL: prop_table = "node_props_real"; break;
                    case LITERAL_BOOLEAN: prop_table = "node_props_bool"; break;
                    default: break;
                }
                if (prop_table) {
                    /* FROM property_table */
                    snprintf(prop_alias, sizeof(prop_alias), "_prop_%s", alias);
                    sql_from(ctx->unified_builder, get_graph_table(ctx, prop_table), prop_alias);

                    /* JOIN property_keys with key filter and value filter */
                    snprintf(pk_alias, sizeof(pk_alias), "_pk_%s", alias);
                    dbuf_init(&on_cond);
                    dbuf_appendf(&on_cond, "%s.id = %s.key_id AND %s.key = '%s'",
                                 pk_alias, prop_alias, pk_alias, first_pair->key);
                    switch (lit->literal_type) {
                        case LITERAL_INTEGER:
                            dbuf_appendf(&on_cond, " AND %s.value = %lld", prop_alias, (long long)lit->value.integer);
                            break;
                        case LITERAL_STRING:
                            dbuf_appendf(&on_cond, " AND %s.value = '%s'", prop_alias, lit->value.string);
                            break;
                        case LITERAL_DECIMAL:
                            dbuf_appendf(&on_cond, " AND %s.value = %f", prop_alias, lit->value.decimal);
                            break;
                        case LITERAL_BOOLEAN:
                            dbuf_appendf(&on_cond, " AND %s.value = %d", prop_alias, lit->value.boolean ? 1 : 0);
                            break;
                        default:
                            break;
                    }
                    sql_join(ctx->unified_builder, SQL_JOIN_INNER, get_graph_table(ctx, "property_keys"), pk_alias, dbuf_get(&on_cond));
                    dbuf_free(&on_cond);

                    /* JOIN nodes */
                    dbuf_init(&on_cond);
                    dbuf_appendf(&on_cond, "%s.id = %s.node_id", alias, prop_alias);
                    sql_join(ctx->unified_builder, SQL_JOIN_INNER, get_graph_table(ctx, "nodes"), alias, dbuf_get(&on_cond));
                    dbuf_free(&on_cond);

                    /* Mark first property as handled */
                    first_pair->key = NULL;
                } else {
                    sql_from(ctx->unified_builder, get_graph_table(ctx, "nodes"), alias);
                }
            } else {
                if (optional) {
                    /* GQLITE-T-0218 follow-up: OPTIONAL MATCH on an empty graph
                     * must return one row with NULL bindings, not zero rows.
                     * Anchor the FROM on a synthetic 1-row table so the LEFT
                     * JOIN to nodes always produces at least one row. */
                    sql_from(ctx->unified_builder, "(SELECT 1)", "_gql_anchor");
                    sql_join(ctx->unified_builder, SQL_JOIN_LEFT,
                             get_graph_table(ctx, "nodes"), alias, "1=1");
                } else {
                    sql_from(ctx->unified_builder, get_graph_table(ctx, "nodes"), alias);
                }
            }
        } else {
            if (optional) {
                sql_from(ctx->unified_builder, "(SELECT 1)", "_gql_anchor");
                /* Inline the label check into the LEFT JOIN's ON condition
                 * so the anchor row produces NULL bindings when no node
                 * matches the label. The post-JOIN label JOINs below are
                 * then skipped (they'd inner-join away the anchor). */
                if (has_labels(node)) {
                    dynamic_buffer on_buf;
                    dbuf_init(&on_buf);
                    for (int li = 0; li < node->labels->count; li++) {
                        const char *lbl = get_label_string(node->labels->items[li]);
                        if (!lbl) continue;
                        char *esc = escape_sql_string(lbl);
                        if (li > 0) dbuf_append(&on_buf, " AND ");
                        dbuf_appendf(&on_buf,
                                     "EXISTS (SELECT 1 FROM node_labels nl WHERE nl.node_id = %s.id AND nl.label = '%s')",
                                     alias, esc ? esc : lbl);
                        free(esc);
                    }
                    sql_join(ctx->unified_builder, SQL_JOIN_LEFT,
                             get_graph_table(ctx, "nodes"), alias, dbuf_get(&on_buf));
                    dbuf_free(&on_buf);
                } else {
                    sql_join(ctx->unified_builder, SQL_JOIN_LEFT,
                             get_graph_table(ctx, "nodes"), alias, "1=1");
                }
            } else {
                sql_from(ctx->unified_builder, get_graph_table(ctx, "nodes"), alias);
            }
        }
    } else {
        /* Subsequent tables - use JOIN */
        if (has_prop_pairs) {
            /* Join via property table for better selectivity */
            cypher_map_pair *first_pair = (cypher_map_pair*)prop_map->pairs->items[0];
            if (first_pair->key && first_pair->value && first_pair->value->type == AST_NODE_LITERAL) {
                cypher_literal *lit = (cypher_literal*)first_pair->value;
                const char *prop_table = NULL;
                switch (lit->literal_type) {
                    case LITERAL_INTEGER: prop_table = "node_props_int"; break;
                    case LITERAL_STRING:  prop_table = "node_props_text"; break;
                    case LITERAL_DECIMAL: prop_table = "node_props_real"; break;
                    case LITERAL_BOOLEAN: prop_table = "node_props_bool"; break;
                    default: break;
                }
                if (prop_table) {
                    /* JOIN property_table */
                    snprintf(prop_alias, sizeof(prop_alias), "_prop_%s", alias);
                    sql_join(ctx->unified_builder, jtype, get_graph_table(ctx, prop_table), prop_alias, "1=1");

                    /* JOIN property_keys with key filter and value filter */
                    snprintf(pk_alias, sizeof(pk_alias), "_pk_%s", alias);
                    dbuf_init(&on_cond);
                    dbuf_appendf(&on_cond, "%s.id = %s.key_id AND %s.key = '%s'",
                                 pk_alias, prop_alias, pk_alias, first_pair->key);
                    switch (lit->literal_type) {
                        case LITERAL_INTEGER:
                            dbuf_appendf(&on_cond, " AND %s.value = %lld", prop_alias, (long long)lit->value.integer);
                            break;
                        case LITERAL_STRING:
                            dbuf_appendf(&on_cond, " AND %s.value = '%s'", prop_alias, lit->value.string);
                            break;
                        case LITERAL_DECIMAL:
                            dbuf_appendf(&on_cond, " AND %s.value = %f", prop_alias, lit->value.decimal);
                            break;
                        case LITERAL_BOOLEAN:
                            dbuf_appendf(&on_cond, " AND %s.value = %d", prop_alias, lit->value.boolean ? 1 : 0);
                            break;
                        default:
                            break;
                    }
                    sql_join(ctx->unified_builder, SQL_JOIN_INNER, get_graph_table(ctx, "property_keys"), pk_alias, dbuf_get(&on_cond));
                    dbuf_free(&on_cond);

                    /* JOIN nodes */
                    dbuf_init(&on_cond);
                    dbuf_appendf(&on_cond, "%s.id = %s.node_id", alias, prop_alias);
                    sql_join(ctx->unified_builder, SQL_JOIN_INNER, get_graph_table(ctx, "nodes"), alias, dbuf_get(&on_cond));
                    dbuf_free(&on_cond);

                    first_pair->key = NULL;
                } else {
                    sql_join(ctx->unified_builder, jtype, get_graph_table(ctx, "nodes"), alias, "1=1");
                }
            } else {
                sql_join(ctx->unified_builder, jtype, get_graph_table(ctx, "nodes"), alias, "1=1");
            }
        } else {
            if (optional) {
                sql_join(ctx->unified_builder, SQL_JOIN_LEFT, get_graph_table(ctx, "nodes"), alias, "1=1");
            } else {
                sql_join(ctx->unified_builder, SQL_JOIN_CROSS, get_graph_table(ctx, "nodes"), alias, NULL);
            }
        }
    }

    /* Add label JOINs if specified - one JOIN per label for multi-label support.
     * Skip when we already inlined the label check into the LEFT JOIN's ON
     * condition (OPTIONAL MATCH first-node-no-FROM-no-props path above). */
    if (has_labels(node) && !(optional && !has_from && !has_prop_pairs)) {
        /* Get proper node id reference (handles projected variables from WITH) */
        const char *node_id = get_node_id_ref(ctx, alias, node->variable);

        if (optional) {
            /* I-0047 P3: an OPTIONAL node's label constraint must be inlined
             * into the node's own LEFT JOIN ON (as a correlated EXISTS), not
             * emitted as a separate INNER node_labels join. An INNER join
             * drops the preserved anchor row whenever the optional pattern
             * doesn't match (e.g. a second `OPTIONAL MATCH (b:Missing)` after
             * a null-seeded first optional). The first-node-no-FROM path above
             * already inlines; this covers every other optional node. */
            for (int i = 0; i < node->labels->count; i++) {
                const char *label = get_label_string(node->labels->items[i]);
                if (!label) continue;
                char *esc_label = escape_sql_string(label);
                char cond[512];
                /* sql_join_append_on prepends " AND " itself. */
                snprintf(cond, sizeof(cond),
                    "EXISTS (SELECT 1 FROM %s WHERE node_id = %s AND label = '%s')",
                    get_graph_table(ctx, "node_labels"), node_id,
                    esc_label ? esc_label : label);
                free(esc_label);
                sql_join_append_on(ctx->unified_builder, cond);
            }
        } else {
            for (int i = 0; i < node->labels->count; i++) {
                const char *label = get_label_string(node->labels->items[i]);
                if (label) {
                    char nl_alias[64];
                    snprintf(nl_alias, sizeof(nl_alias), "_nl_%s_%d", alias, i);
                    dbuf_init(&on_cond);
                    { char *esc_label = escape_sql_string(label);
                      dbuf_appendf(&on_cond, "%s.node_id = %s AND %s.label = '%s'",
                                   nl_alias, node_id, nl_alias, esc_label ? esc_label : label);
                      free(esc_label); }
                    sql_join(ctx->unified_builder, SQL_JOIN_INNER, get_graph_table(ctx, "node_labels"), nl_alias, dbuf_get(&on_cond));
                    dbuf_free(&on_cond);
                }
            }
        }
    }

    return 0;
}

/* Generate SQL for matching a relationship pattern.
 *
 * T-0320: src_deferred / tgt_deferred indicate that the corresponding
 * endpoint node was NOT emitted by the path-element loop (because
 * the pattern is OPTIONAL MATCH with one bound + one new endpoint,
 * and the path-loop chose to defer the new endpoint to be emitted
 * VIA the edge join here). When set, this handler:
 *   1. Builds the edge JOIN's ON referencing ONLY the bound endpoint(s).
 *   2. After the edge JOIN, emits `LEFT JOIN nodes AS X ON X.id =
 *      edge.<src|tgt>_id` so X correlates through the edge —
 *      preserves OPTIONAL semantics (X is null when no edge matched).
 *
 * Both flags are zero on the non-OPTIONAL / non-deferred path. */
static int generate_relationship_match(cypher_transform_context *ctx, cypher_rel_pattern *rel,
                                     cypher_node_pattern *source_node, cypher_node_pattern *target_node,
                                     int rel_index, bool optional, path_type ptype,
                                     bool src_deferred, bool tgt_deferred)
{
    (void)src_deferred; (void)tgt_deferred;  /* TODO: wire into emission */
    CYPHER_DEBUG("Generating %s match for relationship %s between nodes (varlen=%s)",
                 optional ? "OPTIONAL" : "regular",
                 rel->type ? rel->type : "<no type>",
                 rel->varlen ? "yes" : "no");

    /* Get aliases using unified variable system */
    const char *source_alias, *target_alias, *edge_alias;

    /* Source node */
    if (source_node->variable) {
        transform_var *src_var = transform_var_lookup(ctx->var_ctx, source_node->variable);
        if (src_var && (src_var->kind == VAR_KIND_EDGE || src_var->kind == VAR_KIND_PATH)) {
            ctx->has_error = true;
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Variable `%s` is already bound as a %s and cannot be used as a node (SyntaxError: VariableTypeConflict)",
                     source_node->variable,
                     src_var->kind == VAR_KIND_EDGE ? "relationship" : "path");
            ctx->error_message = strdup(msg);
            return -1;
        }
        source_alias = transform_var_get_alias(ctx->var_ctx, source_node->variable);
        if (!source_alias) {
            /* Add missing variable */
            char *gen_alias = get_next_default_alias(ctx);
            if (!gen_alias) return -1;
            transform_var_register_node(ctx->var_ctx, source_node->variable, gen_alias, NULL);
            free(gen_alias);
            source_alias = transform_var_get_alias(ctx->var_ctx, source_node->variable);
        }
        if (!source_alias) return -1;
    } else {
        static char temp_source[32];
        snprintf(temp_source, sizeof(temp_source), "n_%d", ctx->anon_node_base + (rel_index - 1));
        source_alias = temp_source;
    }

    /* Target node */
    if (target_node->variable) {
        transform_var *tgt_var = transform_var_lookup(ctx->var_ctx, target_node->variable);
        if (tgt_var && (tgt_var->kind == VAR_KIND_EDGE || tgt_var->kind == VAR_KIND_PATH)) {
            ctx->has_error = true;
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Variable `%s` is already bound as a %s and cannot be used as a node (SyntaxError: VariableTypeConflict)",
                     target_node->variable,
                     tgt_var->kind == VAR_KIND_EDGE ? "relationship" : "path");
            ctx->error_message = strdup(msg);
            return -1;
        }
        target_alias = transform_var_get_alias(ctx->var_ctx, target_node->variable);
        if (!target_alias) {
            /* Add missing variable */
            char *gen_alias = get_next_default_alias(ctx);
            if (!gen_alias) return -1;
            const char *label = has_labels(target_node) ? get_label_string(target_node->labels->items[0]) : NULL;
            transform_var_register_node(ctx->var_ctx, target_node->variable, gen_alias, label);
            free(gen_alias);
            target_alias = transform_var_get_alias(ctx->var_ctx, target_node->variable);
        }
        if (!target_alias) return -1;
    } else {
        static char temp_target[32];
        snprintf(temp_target, sizeof(temp_target), "n_%d", ctx->anon_node_base + (rel_index + 1));
        target_alias = temp_target;
    }

    /* Edge */
    if (rel->variable) {
        transform_var *rel_var = transform_var_lookup(ctx->var_ctx, rel->variable);
        if (rel_var && (rel_var->kind == VAR_KIND_NODE || rel_var->kind == VAR_KIND_PATH)) {
            ctx->has_error = true;
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Variable `%s` is already bound as a %s and cannot be used as a relationship (SyntaxError: VariableTypeConflict)",
                     rel->variable,
                     rel_var->kind == VAR_KIND_NODE ? "node" : "path");
            ctx->error_message = strdup(msg);
            return -1;
        }
        edge_alias = transform_var_get_alias(ctx->var_ctx, rel->variable);
        if (!edge_alias) {
            /* Add new edge variable */
            char *gen_alias = get_next_default_alias(ctx);
            if (!gen_alias) return -1;
            transform_var_register_edge(ctx->var_ctx, rel->variable, gen_alias, rel->type);
            free(gen_alias);
            edge_alias = transform_var_get_alias(ctx->var_ctx, rel->variable);
        }
        if (!edge_alias) return -1;
    } else {
        /* Anonymous relationship - generate name and register */
        char *default_name = get_next_default_alias(ctx);
        char *gen_alias = get_next_default_alias(ctx);
        if (!default_name || !gen_alias) {
            free(default_name);
            free(gen_alias);
            return -1;
        }
        transform_var_register_edge(ctx->var_ctx, default_name, gen_alias, rel->type);
        edge_alias = transform_var_get_alias(ctx->var_ctx, default_name);
        free(default_name);
        free(gen_alias);
        if (!edge_alias) return -1;
    }
    
    /* Handle variable-length relationships differently - use recursive CTE */
    if (rel->varlen) {
        CYPHER_DEBUG("Handling variable-length relationship");

        /* Generate unique CTE name */
        char cte_name[64];
        snprintf(cte_name, sizeof(cte_name), "_varlen_path_%d", rel_index);

        /* T-0309: mark this rel variable as varlen-bound so the RETURN
         * projector emits a list-of-edges JSON array (and the result
         * builder skips single-edge re-fetch). */
        if (rel->variable) {
            transform_var_set_cte(ctx->var_ctx, rel->variable, cte_name);
        }

        /* Generate the recursive CTE (added to unified builder) */
        if (generate_varlen_cte(ctx, rel, source_alias, target_alias, cte_name) < 0) {
            ctx->has_error = true;
            ctx->error_message = strdup("Failed to generate variable-length CTE");
            return -1;
        }

        /* Get min/max hops for filtering. min_hops == -1 → unspecified
         * (default to 1); explicit 0 must be preserved so zero-hop matches
         * include the start node. */
        cypher_varlen_range *range = (cypher_varlen_range*)rel->varlen;
        int min_hops = range->min_hops >= 0 ? range->min_hops : 1;
        int max_hops = range->max_hops;  /* -1 → unbounded */
        /* Empty interval `*M..N` with M > N must return zero rows. */
        bool empty_interval = (max_hops >= 0 && min_hops > max_hops);

        /* T-0306 follow-on: when the rel/target variables came from a prior
         * WITH (alias_is_id), their "alias" is actually a column reference
         * like `_with_0.rs` — using that as a SQL table alias produces
         * `CROSS JOIN _varlen_path_1 AS _with_0.rs` which doesn't parse
         * (SQLite sees `.` as a column accessor). Generate fresh internal
         * aliases for the JOIN and constrain via WHERE using the column
         * refs. */
        /* A bound variable (via WITH) has an alias that's a column ref
         * like `_with_0.rs` — either alias_is_id=true (node/edge id from
         * WITH) or kind=VAR_KIND_PROJECTED (scalar projection). Either
         * way, the alias contains a dot and can't be used as a SQL
         * table alias. The simplest reliable detection: substring `.`. */
        bool edge_alias_is_colref = (edge_alias && strchr(edge_alias, '.') != NULL);
        bool target_alias_is_colref = (target_alias && strchr(target_alias, '.') != NULL);
        char fresh_cte_alias[64];
        char fresh_target_alias[64];
        if (edge_alias_is_colref) {
            snprintf(fresh_cte_alias, sizeof(fresh_cte_alias),
                     "_vp_inner_%d", rel_index);
            edge_alias = fresh_cte_alias;
        }
        if (target_alias_is_colref) {
            snprintf(fresh_target_alias, sizeof(fresh_target_alias),
                     "_vp_tgt_%d", rel_index);
        }

        /* Join the main query with the CTE result using unified builder.
         * T-0261: for OPTIONAL MATCH, use LEFT JOIN with a placeholder
         * ON `1=1` so that (1) the no-match outer row is preserved and
         * (2) subsequent constraint emission can use sql_join_append_on
         * to attach to this JOIN's ON clause (CROSS JOIN has no ON, so
         * append_on previously produced malformed SQL like
         * `CROSS JOIN ... AS alias AND (...)`). */
        if (optional) {
            sql_join(ctx->unified_builder, SQL_JOIN_LEFT, cte_name, edge_alias, "1=1");
        } else {
            sql_join(ctx->unified_builder, SQL_JOIN_CROSS, cte_name, edge_alias, NULL);
        }

        /* T-0306 follow-on: skip the target-node JOIN entirely when the
         * target var is bound via WITH (its column-ref IS the id; no
         * fresh `nodes AS _with_0.x` is needed and would be invalid SQL
         * anyway). The WHERE constraint downstream uses the column-ref
         * directly. */
        if (target_alias_is_colref) {
            goto skip_target_node_join;
        }

        /* T-0320: when the path-loop deferred the target endpoint to
         * this handler (OPTIONAL+varlen with unbound target), the
         * target-node LEFT JOIN is emitted AFTER constraints below
         * via `LEFT JOIN nodes AS target ON target.id = cte.end_id`.
         * Skip the existing in-handler target-node JOIN to avoid
         * a duplicate `CROSS JOIN nodes AS target` (with no ON). */
        if (tgt_deferred) {
            goto skip_target_node_join;
        }

        /* T-0261: also skip when the target alias is already in FROM/
         * joins (e.g. previously bound by a sibling MATCH clause). The
         * varlen path otherwise emits a duplicate `CROSS JOIN nodes
         * AS <alias>` that SQLite rejects with `ambiguous column name`.
         * Match7 [13]/[20] family. */
        {
            const char *from_str = dbuf_get(&ctx->unified_builder->from);
            const char *joins_str = dbuf_get(&ctx->unified_builder->joins);
            char needle1[80], needle2[80], suffix[80];
            snprintf(needle1, sizeof(needle1), " AS %s ", target_alias);
            snprintf(needle2, sizeof(needle2), " AS %s\n", target_alias);
            snprintf(suffix, sizeof(suffix), " AS %s", target_alias);
            size_t slen = strlen(suffix);
            bool target_in_scope = false;
            if (from_str) {
                if (strstr(from_str, needle1) || strstr(from_str, needle2)) target_in_scope = true;
                size_t flen = strlen(from_str);
                if (!target_in_scope && flen >= slen &&
                    strcmp(from_str + flen - slen, suffix) == 0) target_in_scope = true;
            }
            if (!target_in_scope && joins_str) {
                if (strstr(joins_str, needle1) || strstr(joins_str, needle2)) target_in_scope = true;
                size_t jlen = strlen(joins_str);
                if (!target_in_scope && jlen >= slen &&
                    strcmp(joins_str + jlen - slen, suffix) == 0) target_in_scope = true;
            }
            if (target_in_scope) goto skip_target_node_join;
        }

        /* Add target node to FROM clause - needed for the CTE join */
        bool target_has_properties = (target_node->properties && target_node->properties->type == AST_NODE_MAP);
        cypher_map *target_prop_map = target_has_properties ? (cypher_map*)target_node->properties : NULL;
        bool target_has_prop_pairs = target_has_properties && target_prop_map->pairs && target_prop_map->pairs->count > 0;

        dynamic_buffer on_cond;
        char prop_alias[64], pk_alias[64];

        if (target_has_prop_pairs) {
            /* Target node has properties - need to join via property table */
            cypher_map_pair *first_pair = (cypher_map_pair*)target_prop_map->pairs->items[0];
            if (first_pair->key && first_pair->value && first_pair->value->type == AST_NODE_LITERAL) {
                cypher_literal *lit = (cypher_literal*)first_pair->value;
                const char *prop_table = NULL;
                switch (lit->literal_type) {
                    case LITERAL_INTEGER: prop_table = "node_props_int"; break;
                    case LITERAL_STRING:  prop_table = "node_props_text"; break;
                    case LITERAL_DECIMAL: prop_table = "node_props_real"; break;
                    case LITERAL_BOOLEAN: prop_table = "node_props_bool"; break;
                    default: break;
                }
                if (prop_table) {
                    /* CROSS JOIN property table */
                    snprintf(prop_alias, sizeof(prop_alias), "_prop_%s", target_alias);
                    sql_join(ctx->unified_builder, SQL_JOIN_CROSS, get_graph_table(ctx, prop_table), prop_alias, NULL);

                    /* JOIN property_keys with key and value filter */
                    snprintf(pk_alias, sizeof(pk_alias), "_pk_%s", target_alias);
                    dbuf_init(&on_cond);
                    dbuf_appendf(&on_cond, "%s.id = %s.key_id AND %s.key = '%s'",
                                 pk_alias, prop_alias, pk_alias, first_pair->key);
                    switch (lit->literal_type) {
                        case LITERAL_INTEGER:
                            dbuf_appendf(&on_cond, " AND %s.value = %lld", prop_alias, (long long)lit->value.integer);
                            break;
                        case LITERAL_STRING:
                            dbuf_appendf(&on_cond, " AND %s.value = '%s'", prop_alias, lit->value.string);
                            break;
                        case LITERAL_DECIMAL:
                            dbuf_appendf(&on_cond, " AND %s.value = %f", prop_alias, lit->value.decimal);
                            break;
                        case LITERAL_BOOLEAN:
                            dbuf_appendf(&on_cond, " AND %s.value = %d", prop_alias, lit->value.boolean ? 1 : 0);
                            break;
                        default:
                            break;
                    }
                    sql_join(ctx->unified_builder, SQL_JOIN_INNER, get_graph_table(ctx, "property_keys"), pk_alias, dbuf_get(&on_cond));
                    dbuf_free(&on_cond);

                    /* JOIN nodes */
                    dbuf_init(&on_cond);
                    dbuf_appendf(&on_cond, "%s.id = %s.node_id", target_alias, prop_alias);
                    sql_join(ctx->unified_builder, SQL_JOIN_INNER, get_graph_table(ctx, "nodes"), target_alias, dbuf_get(&on_cond));
                    dbuf_free(&on_cond);

                    first_pair->key = NULL;
                } else {
                    sql_join(ctx->unified_builder, SQL_JOIN_CROSS, get_graph_table(ctx, "nodes"), target_alias, NULL);
                }
            } else {
                sql_join(ctx->unified_builder, SQL_JOIN_CROSS, get_graph_table(ctx, "nodes"), target_alias, NULL);
            }
        } else {
            sql_join(ctx->unified_builder, SQL_JOIN_CROSS, get_graph_table(ctx, "nodes"), target_alias, NULL);
        }

skip_target_node_join: ;  /* null statement: a label may not precede a
        * declaration in C17 and earlier — Apple clang rejects
        * `label: char x[..]` outright (the release macos build). */
        /* Add label constraints for target node if specified.
         *
         * I-0047 P3: for OPTIONAL + varlen with a DEFERRED target
         * (`OPTIONAL MATCH (a)-[:T*]->(c:Label)` where c is unbound), the
         * target is LEFT-joined later via `c.id = cte.end_id`. Emitting the
         * label as a separate INNER node_labels join here (a) references the
         * not-yet-joined target alias and (b) drops the anchor row when the
         * optional doesn't match. Instead, fold the label into the deferred
         * target's LEFT JOIN ON as a correlated EXISTS (built below). */
        char deferred_tgt_label_cond[512] = "";
        if (has_labels(target_node)) {
            const char *target_id = get_node_id_ref(ctx, target_alias, target_node->variable);

            if (optional && tgt_deferred) {
                for (int i = 0; i < target_node->labels->count; i++) {
                    const char *label = get_label_string(target_node->labels->items[i]);
                    if (!label) continue;
                    char *esc_label = escape_sql_string(label);
                    char one[256];
                    snprintf(one, sizeof(one),
                        " AND EXISTS (SELECT 1 FROM %s WHERE node_id = %s AND label = '%s')",
                        get_graph_table(ctx, "node_labels"), target_id,
                        esc_label ? esc_label : label);
                    free(esc_label);
                    strncat(deferred_tgt_label_cond, one,
                            sizeof(deferred_tgt_label_cond) - strlen(deferred_tgt_label_cond) - 1);
                }
            } else {
                for (int i = 0; i < target_node->labels->count; i++) {
                    const char *label = get_label_string(target_node->labels->items[i]);
                    if (label) {
                        char nl_alias[64];
                        snprintf(nl_alias, sizeof(nl_alias), "_nl_%s_%d", target_alias, i);
                        dbuf_init(&on_cond);
                        { char *esc_label = escape_sql_string(label);
                          dbuf_appendf(&on_cond, "%s.node_id = %s AND %s.label = '%s'",
                                       nl_alias, target_id, nl_alias, esc_label ? esc_label : label);
                          free(esc_label); }
                        sql_join(ctx->unified_builder, SQL_JOIN_INNER, get_graph_table(ctx, "node_labels"), nl_alias, dbuf_get(&on_cond));
                        dbuf_free(&on_cond);
                    }
                }
            }
        }

        CYPHER_DEBUG("Added varlen CTE join: %s for relationship between %s and %s",
                     cte_name, source_alias, target_alias);

        /* Add WHERE constraints for the CTE join using unified builder.
         * T-0320: when the target is deferred (OPTIONAL+varlen with
         * unbound target), the target alias isn't yet in scope —
         * skip the end_id constraint here, and emit a target-node
         * LEFT JOIN tied via CTE.end_id AFTER the CTE join (below).
         * Same applies to src_deferred for OPTIONAL+varlen with
         * unbound source. */
        char src_id_ref[256], tgt_id_ref[256];
        snprintf(src_id_ref, sizeof(src_id_ref), "%s",
                 get_node_id_ref(ctx, source_alias, source_node->variable));
        snprintf(tgt_id_ref, sizeof(tgt_id_ref), "%s",
                 get_node_id_ref(ctx, target_alias, target_node->variable));

        dbuf_init(&on_cond);
        bool varlen_first_cond = true;
        if (!src_deferred) {
            dbuf_appendf(&on_cond, "%s.start_id = %s", edge_alias, src_id_ref);
            varlen_first_cond = false;
        }
        if (!tgt_deferred) {
            if (varlen_first_cond) {
                dbuf_appendf(&on_cond, "%s.end_id = %s", edge_alias, tgt_id_ref);
                varlen_first_cond = false;
            } else {
                dbuf_appendf(&on_cond, " AND %s.end_id = %s", edge_alias, tgt_id_ref);
            }
        }
        if (varlen_first_cond) {
            dbuf_append(&on_cond, "1=1");
        }

        /* Always honor min_hops (zero-hop rows come from the CTE base case
         * we just added; bound-1 paths are excluded if min > 1). */
        if (min_hops >= 0) {
            dbuf_appendf(&on_cond, " AND %s.depth >= %d", edge_alias, min_hops);
        }
        /* Honor max_hops (the CTE already caps recursion at the internal
         * `max_hops` ceiling, but the user-given max may be lower). */
        if (max_hops >= 0) {
            dbuf_appendf(&on_cond, " AND %s.depth <= %d", edge_alias, max_hops);
        }
        /* Empty interval (min > max) → unconditionally false. */
        if (empty_interval) {
            dbuf_appendf(&on_cond, " AND 0");
        }

        /* Add shortest path filtering based on path type */
        if (ptype == PATH_TYPE_SHORTEST || ptype == PATH_TYPE_ALL_SHORTEST) {
            CYPHER_DEBUG("Adding shortest path filtering (type=%d)", ptype);
            dbuf_appendf(&on_cond, " AND %s.depth = (SELECT MIN(sp.depth) FROM %s sp WHERE sp.start_id = %s AND sp.end_id = %s)",
                         edge_alias, cte_name, src_id_ref, tgt_id_ref);
        }

        /* T-0261: for OPTIONAL, attach varlen constraints to the LEFT
         * JOIN's ON clause so unmatched outer rows are preserved.
         * Non-optional uses WHERE (post-CROSS-JOIN filter). */
        if (optional) {
            sql_join_append_on(ctx->unified_builder, dbuf_get(&on_cond));
        } else {
            sql_where(ctx->unified_builder, dbuf_get(&on_cond));
        }
        dbuf_free(&on_cond);

        /* T-0320: emit deferred endpoint-node LEFT JOINs tied via the
         * CTE row's start_id / end_id. Preserves OPTIONAL semantics:
         * when no path matches, CTE row is NULL → endpoint node is
         * NULL too. */
        if (optional && src_deferred) {
            const char *fs0 = dbuf_get(&ctx->unified_builder->from);
            const char *js0 = dbuf_get(&ctx->unified_builder->joins);
            char needle[80], suffix[80];
            snprintf(needle, sizeof(needle), " AS %s ", source_alias);
            snprintf(suffix, sizeof(suffix), " AS %s", source_alias);
            size_t slen = strlen(suffix);
            bool already = false;
            if (fs0 && (strstr(fs0, needle) ||
                        (strlen(fs0) >= slen &&
                         strcmp(fs0 + strlen(fs0) - slen, suffix) == 0))) already = true;
            if (!already && js0 && (strstr(js0, needle) ||
                        (strlen(js0) >= slen &&
                         strcmp(js0 + strlen(js0) - slen, suffix) == 0))) already = true;
            if (!already) {
                char src_cond[256];
                snprintf(src_cond, sizeof(src_cond),
                         "%s.id = %s.start_id", source_alias, edge_alias);
                sql_join(ctx->unified_builder, SQL_JOIN_LEFT,
                         get_graph_table(ctx, "nodes"), source_alias, src_cond);
            }
        }
        if (optional && tgt_deferred) {
            const char *fs0 = dbuf_get(&ctx->unified_builder->from);
            const char *js0 = dbuf_get(&ctx->unified_builder->joins);
            char needle[80], suffix[80];
            snprintf(needle, sizeof(needle), " AS %s ", target_alias);
            snprintf(suffix, sizeof(suffix), " AS %s", target_alias);
            size_t slen = strlen(suffix);
            bool already = false;
            if (fs0 && (strstr(fs0, needle) ||
                        (strlen(fs0) >= slen &&
                         strcmp(fs0 + strlen(fs0) - slen, suffix) == 0))) already = true;
            if (!already && js0 && (strstr(js0, needle) ||
                        (strlen(js0) >= slen &&
                         strcmp(js0 + strlen(js0) - slen, suffix) == 0))) already = true;
            if (!already) {
                char tgt_cond[768];
                snprintf(tgt_cond, sizeof(tgt_cond),
                         "%s.id = %s.end_id%s", target_alias, edge_alias,
                         deferred_tgt_label_cond);
                sql_join(ctx->unified_builder, SQL_JOIN_LEFT,
                         get_graph_table(ctx, "nodes"), target_alias, tgt_cond);
            }
        }

        return 0; /* Skip the rest of the relationship handling */
    }
    /* Add edges table - use LEFT JOIN for optional relationships */
    else {
        /* Check if rel variable is bound from a prior WITH (alias_is_id means
         * its "alias" is actually a column expression like _with_0.r, not a
         * table alias). When bound, we must NOT add a new edges JOIN with that
         * column as alias — instead, constrain endpoints to match the bound
         * edge's source/target via subqueries. */
        bool rel_is_bound = rel->variable &&
                            transform_var_alias_is_id(ctx->var_ctx, rel->variable);

        if (rel_is_bound) {
            /* Bound relationship: the edge expression edge_alias is the id of
             * an already-selected edge. Skip the edges JOIN; node JOINs are
             * added separately by generate_node_match. Just emit WHERE
             * constraints that tie the endpoint node aliases to the bound
             * edge's source/target. */
            const char *graph = ctx->current_graph ? ctx->current_graph : "";
            char src_subq[256], tgt_subq[256];
            snprintf(src_subq, sizeof(src_subq),
                     "(SELECT source_id FROM %sedges WHERE id = %s)", graph, edge_alias);
            snprintf(tgt_subq, sizeof(tgt_subq),
                     "(SELECT target_id FROM %sedges WHERE id = %s)", graph, edge_alias);

            char src_id_ref[256], tgt_id_ref[256];
            snprintf(src_id_ref, sizeof(src_id_ref), "%s",
                     get_node_id_ref(ctx, source_alias, source_node->variable));
            snprintf(tgt_id_ref, sizeof(tgt_id_ref), "%s",
                     get_node_id_ref(ctx, target_alias, target_node->variable));

            char cond[1024];
            if (rel->left_arrow && !rel->right_arrow) {
                snprintf(cond, sizeof(cond), "%s = %s AND %s = %s",
                         src_id_ref, tgt_subq, tgt_id_ref, src_subq);
            } else if (!rel->left_arrow && !rel->right_arrow) {
                snprintf(cond, sizeof(cond),
                         "((%s = %s AND %s = %s) OR (%s = %s AND %s = %s))",
                         src_id_ref, src_subq, tgt_id_ref, tgt_subq,
                         src_id_ref, tgt_subq, tgt_id_ref, src_subq);
            } else {
                snprintf(cond, sizeof(cond), "%s = %s AND %s = %s",
                         src_id_ref, src_subq, tgt_id_ref, tgt_subq);
            }
            /* I-0047 P3: for OPTIONAL MATCH the bound-rel endpoint constraints
             * are part of the optional pattern; emitting them as WHERE filters
             * the preserved anchor row (Match7 [4], MatchWhere6 [5]: a bound
             * rel reused in the reverse direction). Stash them to flush onto
             * the target node's LEFT JOIN ON after the path loop. Guard: this
             * only works when exactly *one* endpoint is fresh (the other in
             * outer scope from a prior clause). If both are fresh, the first
             * is CROSS-joined and over-produces (Match7 [5]); if both are
             * bound, there's no fresh LEFT JOIN to attach to. */
            bool src_in_scope = source_node->variable && (
                transform_var_is_bound(ctx->var_ctx, source_node->variable) ||
                transform_var_alias_is_id(ctx->var_ctx, source_node->variable));
            bool tgt_in_scope = target_node->variable && (
                transform_var_is_bound(ctx->var_ctx, target_node->variable) ||
                transform_var_alias_is_id(ctx->var_ctx, target_node->variable));
            bool exactly_one_fresh = (src_in_scope != tgt_in_scope);
            if (optional && exactly_one_fresh) {
                transform_ctx_append_pending_optional_on(ctx, cond);
            } else {
                sql_where(ctx->unified_builder, cond);
            }

            /* If the new pattern specifies a relationship type, enforce it
             * against the bound edge's type. `MATCH ()-[r:T]->() WITH r
             * MATCH ()-[r:Y]->()` should match zero rows because r was
             * bound as type T but the second MATCH requires type Y. */
            if (rel->type) {
                char type_cond[512];
                char *esc = escape_sql_string(rel->type);
                snprintf(type_cond, sizeof(type_cond),
                         "(SELECT type FROM %sedges WHERE id = %s) = '%s'",
                         graph, edge_alias, esc ? esc : rel->type);
                free(esc);
                sql_where(ctx->unified_builder, type_cond);
            } else if (rel->types && rel->types->count > 0) {
                dynamic_buffer tc; dbuf_init(&tc);
                dbuf_appendf(&tc, "(SELECT type FROM %sedges WHERE id = %s) IN (",
                             graph, edge_alias);
                for (int t = 0; t < rel->types->count; t++) {
                    if (t > 0) dbuf_append(&tc, ", ");
                    cypher_literal *type_lit = (cypher_literal*)rel->types->items[t];
                    char *esc = escape_sql_string(type_lit->value.string);
                    dbuf_appendf(&tc, "'%s'", esc ? esc : type_lit->value.string);
                    free(esc);
                }
                dbuf_append(&tc, ")");
                sql_where(ctx->unified_builder, dbuf_get(&tc));
                dbuf_free(&tc);
            }

            CYPHER_DEBUG("Generated bound-rel match: %s as %s connects %s to %s",
                         rel->variable, edge_alias, source_alias, target_alias);
            return 0;
        }

        if (optional) {
            /* For OPTIONAL MATCH, we LEFT JOIN edges first, then target through edge */
            /* This ensures we get NULLs for unmatched patterns, not cartesian products */

            /* T-0320: when the rel has no user-visible variable AND we
             * have exactly one bound + one deferred endpoint AND no
             * named-path captures the rel, use an EXISTS-based LEFT
             * JOIN nodes shape so we get 1 row per outer binding
             * (matching c) or 1 row with c=NULL (no match). This is
             * the correct Cypher OPTIONAL MATCH semantics — WHERE
             * filters inner matches; outer rows are preserved per
             * (a, b, ...) NOT per outer×edge.
             *
             * The existing edge+node LEFT JOIN cascade multiplied rows
             * by the number of candidate edges, which produced extra
             * rows even when WHERE filtered out c. MatchWhere6 family.
             *
             * Named paths (e.g. `MATCH p = (a)-[:X]->(b)`) need the
             * edge as a real binding so the path projector can
             * recover it — skip the EXISTS shortcut in that case. */
            bool synthetic_rel = (rel->variable && strncmp(rel->variable, "_gql_default_alias_", 19) == 0);
            bool no_user_rel_var = (synthetic_rel || !rel->variable);
            bool path_is_named = transform_var_lookup(ctx->var_ctx, NULL) != NULL;
            /* The above is wrong placeholder — actually check if any
             * path var references this rel. Use a simpler heuristic:
             * check whether the rel's variable (if synthetic-generated)
             * is captured as path element. For simplicity, opt out of
             * the EXISTS shortcut whenever ANY VAR_KIND_PATH exists
             * in the var context (path is named somewhere). */
            (void)path_is_named;
            bool has_path_var = false;
            for (int vi = 0; vi < ctx->var_ctx->count; vi++) {
                if (ctx->var_ctx->vars[vi].kind == VAR_KIND_PATH) {
                    has_path_var = true;
                    break;
                }
            }
            /* Check if the deferred endpoint is already in scope (e.g.
             * a previous rel handler already emitted it). In that case
             * the EXISTS shortcut would double-emit the node JOIN.
             * Fall through to the standard path which correctly
             * handles the in-scope case. */
            bool exists_deferred_in_scope = false;
            if (no_user_rel_var && !has_path_var &&
                ((src_deferred && !tgt_deferred) || (!src_deferred && tgt_deferred))) {
                const char *def_alias_chk = src_deferred ? source_alias : target_alias;
                const char *fs0 = dbuf_get(&ctx->unified_builder->from);
                const char *js0 = dbuf_get(&ctx->unified_builder->joins);
                char needle[80], suffix[80];
                snprintf(needle, sizeof(needle), " AS %s ", def_alias_chk);
                snprintf(suffix, sizeof(suffix), " AS %s", def_alias_chk);
                size_t slen = strlen(suffix);
                if (fs0 && (strstr(fs0, needle) ||
                            (strlen(fs0) >= slen &&
                             strcmp(fs0 + strlen(fs0) - slen, suffix) == 0))) {
                    exists_deferred_in_scope = true;
                }
                if (!exists_deferred_in_scope && js0 &&
                    (strstr(js0, needle) ||
                     (strlen(js0) >= slen &&
                      strcmp(js0 + strlen(js0) - slen, suffix) == 0))) {
                    exists_deferred_in_scope = true;
                }
            }
            if (no_user_rel_var && !has_path_var && !exists_deferred_in_scope &&
                ((src_deferred && !tgt_deferred) || (!src_deferred && tgt_deferred))) {
                /* Determine the deferred-endpoint alias and direction. */
                const char *deferred_alias;
                cypher_node_pattern *deferred_node;
                const char *bound_id_ref;
                const char *bound_endpoint_col;  /* edges column the bound side maps to */
                const char *deferred_endpoint_col;
                if (src_deferred) {
                    /* Source is deferred; target is bound. */
                    deferred_alias = source_alias;
                    deferred_node = source_node;
                    bound_id_ref = get_node_id_ref(ctx, target_alias, target_node->variable);
                    bound_endpoint_col = "target_id";
                    deferred_endpoint_col = "source_id";
                } else {
                    /* Target is deferred; source is bound. */
                    deferred_alias = target_alias;
                    deferred_node = target_node;
                    bound_id_ref = get_node_id_ref(ctx, source_alias, source_node->variable);
                    bound_endpoint_col = "source_id";
                    deferred_endpoint_col = "target_id";
                }
                /* Build the EXISTS subquery condition. */
                dynamic_buffer on_cond; dbuf_init(&on_cond);
                const char *graph_p = ctx->current_graph ? ctx->current_graph : "";
                dbuf_appendf(&on_cond,
                    "EXISTS (SELECT 1 FROM %sedges _e WHERE _e.%s = %s AND _e.%s = %s.id",
                    graph_p, bound_endpoint_col, bound_id_ref,
                    deferred_endpoint_col, deferred_alias);
                /* Type filter on the rel. */
                if (rel->type) {
                    char *esc = escape_sql_string(rel->type);
                    dbuf_appendf(&on_cond, " AND _e.type = '%s'",
                                 esc ? esc : rel->type);
                    free(esc);
                } else if (rel->types && rel->types->count > 0) {
                    dbuf_append(&on_cond, " AND _e.type IN (");
                    for (int t = 0; t < rel->types->count; t++) {
                        if (t > 0) dbuf_append(&on_cond, ", ");
                        cypher_literal *tl = (cypher_literal*)rel->types->items[t];
                        char *esc = escape_sql_string(tl->value.string);
                        dbuf_appendf(&on_cond, "'%s'", esc ? esc : tl->value.string);
                        free(esc);
                    }
                    dbuf_append(&on_cond, ")");
                }
                /* Honor direction. For undirected (no arrows), use OR of both directions. */
                if (!rel->left_arrow && !rel->right_arrow) {
                    /* Undirected: also allow swapped endpoints. */
                    dbuf_appendf(&on_cond,
                        " OR (_e.%s = %s AND _e.%s = %s.id",
                        deferred_endpoint_col, bound_id_ref,
                        bound_endpoint_col, deferred_alias);
                    if (rel->type) {
                        char *esc = escape_sql_string(rel->type);
                        dbuf_appendf(&on_cond, " AND _e.type = '%s'",
                                     esc ? esc : rel->type);
                        free(esc);
                    }
                    dbuf_append(&on_cond, "))");
                } else {
                    dbuf_append(&on_cond, ")");
                }
                /* Label filter on the deferred node. */
                if (has_labels(deferred_node)) {
                    for (int li = 0; li < deferred_node->labels->count; li++) {
                        const char *label = get_label_string(deferred_node->labels->items[li]);
                        if (!label) continue;
                        char *esc = escape_sql_string(label);
                        dbuf_appendf(&on_cond,
                            " AND EXISTS (SELECT 1 FROM %snode_labels WHERE node_id = %s.id AND label = '%s')",
                            graph_p, deferred_alias, esc ? esc : label);
                        free(esc);
                    }
                }
                sql_join(ctx->unified_builder, SQL_JOIN_LEFT,
                         get_graph_table(ctx, "nodes"), deferred_alias,
                         dbuf_get(&on_cond));
                dbuf_free(&on_cond);
                /* Register the rel variable (even if synthetic). */
                if (rel->variable) {
                    transform_var_register_edge(ctx->var_ctx, rel->variable, edge_alias, rel->type);
                } else {
                    char synthetic_var[32];
                    snprintf(synthetic_var, sizeof(synthetic_var), "__unnamed_rel_%d", rel_index);
                    transform_var_register_edge(ctx->var_ctx, synthetic_var, edge_alias, rel->type);
                }
                return 0;
            }

            /* Get proper source id reference (handles projected variables from WITH) */
            const char *source_id = get_node_id_ref(ctx, source_alias, source_node->variable);

            /* Check if target node is already added (was bound by a prior MATCH).
             * When it is, fold target_id = bound_target.id into the edge JOIN's
             * ON clause so the OPTIONAL pattern actually constrains on the
             * bound endpoint. Without this, the edge can match ANY target. */
            bool target_already_added = false;
            const char *from_str_pre = dbuf_get(&ctx->unified_builder->from);
            const char *joins_str_pre = dbuf_get(&ctx->unified_builder->joins);
            if (from_str_pre && strstr(from_str_pre, target_alias)) target_already_added = true;
            if (!target_already_added && joins_str_pre && strstr(joins_str_pre, target_alias))
                target_already_added = true;
            /* T-0306: when the target's alias is a cross-clause column ref
             * (e.g. "_with_0.a" from a prior WITH projection), the substring
             * search above misses it because FROM/JOIN holds only the CTE
             * name ("_with_0"). Detect WITH-bound variables (VAR_KIND_PROJECTED
             * or alias_is_id) and treat the target as already-attached so we
             * don't emit `LEFT JOIN nodes AS _with_0.a ON _with_0.a.id = ...`. */
            if (!target_already_added && target_node->variable) {
                transform_var *tv = transform_var_lookup(ctx->var_ctx, target_node->variable);
                if (tv && (tv->kind == VAR_KIND_PROJECTED ||
                           transform_var_alias_is_id(ctx->var_ctx, target_node->variable))) {
                    target_already_added = true;
                }
            }

            /* Build the edge JOIN condition.
             * T-0320: when src_deferred AND source-alias is NOT yet
             * in scope, the edge JOIN's ON can't reference source_id_ref.
             * We'll emit the source node LEFT JOIN AFTER this edge
             * JOIN. But if a PRIOR rel handler already emitted the
             * source's alias (as its target), then source IS in scope
             * — emit the source constraint normally so the multi-rel
             * chain stays connected. */
            bool src_alias_already_in_scope = false;
            {
                const char *fs0 = dbuf_get(&ctx->unified_builder->from);
                const char *js0 = dbuf_get(&ctx->unified_builder->joins);
                char needle[80], needle2[80], suffix[80];
                snprintf(needle, sizeof(needle), " AS %s ", source_alias);
                snprintf(needle2, sizeof(needle2), " AS %s\n", source_alias);
                snprintf(suffix, sizeof(suffix), " AS %s", source_alias);
                size_t slen = strlen(suffix);
                if (fs0) {
                    if (strstr(fs0, needle) || strstr(fs0, needle2)) src_alias_already_in_scope = true;
                    size_t flen = strlen(fs0);
                    if (!src_alias_already_in_scope && flen >= slen &&
                        strcmp(fs0 + flen - slen, suffix) == 0) src_alias_already_in_scope = true;
                }
                if (!src_alias_already_in_scope && js0) {
                    if (strstr(js0, needle) || strstr(js0, needle2)) src_alias_already_in_scope = true;
                    size_t jlen = strlen(js0);
                    if (!src_alias_already_in_scope && jlen >= slen &&
                        strcmp(js0 + jlen - slen, suffix) == 0) src_alias_already_in_scope = true;
                }
            }
            bool effective_src_defer = src_deferred && !src_alias_already_in_scope;
            dynamic_buffer edge_cond;
            dbuf_init(&edge_cond);
            bool first_cond = true;
            if (!effective_src_defer) {
                dbuf_appendf(&edge_cond, "%s.source_id = %s", edge_alias, source_id);
                first_cond = false;
            }
            if (target_already_added) {
                const char *target_id = get_node_id_ref(ctx, target_alias, target_node->variable);
                if (first_cond) {
                    dbuf_appendf(&edge_cond, "%s.target_id = %s", edge_alias, target_id);
                    first_cond = false;
                } else {
                    dbuf_appendf(&edge_cond, " AND %s.target_id = %s", edge_alias, target_id);
                }
            }
            if (first_cond) {
                /* No bound endpoint at all (both deferred) — anchor with
                 * a vacuous condition so subsequent AND's stay valid. */
                dbuf_append(&edge_cond, "1=1");
            }

            /* Add relationship type constraint to edge JOIN */
            if (rel->type) {
                /* Single type (legacy support) */
                { char *esc = escape_sql_string(rel->type);
                  dbuf_appendf(&edge_cond, " AND %s.type = '%s'", edge_alias, esc ? esc : rel->type);
                  free(esc); }
            } else if (rel->types && rel->types->count > 0) {
                /* Multiple types - generate OR conditions */
                dbuf_append(&edge_cond, " AND (");
                for (int t = 0; t < rel->types->count; t++) {
                    if (t > 0) {
                        dbuf_append(&edge_cond, " OR ");
                    }
                    cypher_literal *type_lit = (cypher_literal*)rel->types->items[t];
                    { char *esc = escape_sql_string(type_lit->value.string);
                      dbuf_appendf(&edge_cond, "%s.type = '%s'", edge_alias, esc ? esc : type_lit->value.string);
                      free(esc); }
                }
                dbuf_append(&edge_cond, ")");
            }

            /* If the target node has labels, fold them into the edge JOIN
             * condition so the edge is only matched when its target
             * actually has the required label(s). Otherwise the edge can
             * match and the target node's LEFT JOIN produces NULL, leaving
             * r non-null when the pattern shouldn't match. */
            const char *graph_prefix = ctx->current_graph ? ctx->current_graph : "";
            if (!target_already_added && has_labels(target_node)) {
                for (int li = 0; li < target_node->labels->count; li++) {
                    const char *label = get_label_string(target_node->labels->items[li]);
                    if (!label) continue;
                    char *esc_label = escape_sql_string(label);
                    dbuf_appendf(&edge_cond,
                        " AND EXISTS (SELECT 1 FROM %snode_labels WHERE node_id = %s.target_id AND label = '%s')",
                        graph_prefix, edge_alias, esc_label ? esc_label : label);
                    free(esc_label);
                }
            }

            /* T-0339 follow-up: skip the edges join if this alias is already
             * in scope from a prior MATCH clause (Match4 [7]). Same-pattern
             * re-use is now caught at validate-time
             * (RelationshipUniquenessViolation, Match3 [29]). */
            {
                const char *fs0 = dbuf_get(&ctx->unified_builder->from);
                const char *js0 = dbuf_get(&ctx->unified_builder->joins);
                char needle[80];
                snprintf(needle, sizeof(needle), "AS %s", edge_alias);
                bool already = (fs0 && strstr(fs0, needle)) ||
                               (js0 && strstr(js0, needle));
                if (!already) {
                    sql_join(ctx->unified_builder, SQL_JOIN_LEFT, get_graph_table(ctx, "edges"), edge_alias, dbuf_get(&edge_cond));
                }
            }
            dbuf_free(&edge_cond);

            /* T-0320: emit deferred source-node LEFT JOIN tied through
             * the edge. Preserves OPTIONAL semantics — when the edge
             * doesn't match, edge.source_id is NULL → source row is
             * NULL too. Skip if the source alias is already in
             * FROM/joins (e.g. a previous rel handler joined this
             * node as its own target — happens in multi-rel paths
             * like (a)-[:R]->(x)-[:R]->(y) where x's anon alias
             * serves as both target of the first rel and source of
             * the second). */
            if (effective_src_defer) {
                /* T-0320: record this defer pair so the WHERE handler
                 * can push predicates into the edge JOIN's ON via a
                 * rewritten copy (source_alias.id → edge.source_id). */
                cypher_transform_record_defer_pair(ctx, edge_alias, source_alias, "source_id");
                dynamic_buffer src_cond; dbuf_init(&src_cond);
                dbuf_appendf(&src_cond, "%s.id = %s.source_id",
                             source_alias, edge_alias);
                if (has_labels(source_node)) {
                    for (int li = 0; li < source_node->labels->count; li++) {
                        const char *label = get_label_string(source_node->labels->items[li]);
                        if (!label) continue;
                        char *esc_label = escape_sql_string(label);
                        dbuf_appendf(&src_cond,
                            " AND EXISTS (SELECT 1 FROM %snode_labels WHERE node_id = %s.id AND label = '%s')",
                            graph_prefix, source_alias,
                            esc_label ? esc_label : label);
                        free(esc_label);
                    }
                }
                sql_join(ctx->unified_builder, SQL_JOIN_LEFT,
                         get_graph_table(ctx, "nodes"), source_alias,
                         dbuf_get(&src_cond));
                dbuf_free(&src_cond);
            }
            /* target_already_added still applies to the block below. */

            if (!target_already_added) {
                /* T-0320: record this defer pair so the WHERE handler
                 * can push predicates into the edge JOIN's ON via a
                 * rewritten copy (target_alias.id → edge.target_id). */
                if (tgt_deferred) {
                    cypher_transform_record_defer_pair(ctx, edge_alias, target_alias, "target_id");
                }
                /* For OPTIONAL MATCH with labels on the target, fold the label
                 * condition into the target node's LEFT JOIN ON clause so that
                 * nodes without the required label produce NULLs. */
                if (has_labels(target_node)) {
                    dynamic_buffer target_cond;
                    dbuf_init(&target_cond);
                    dbuf_appendf(&target_cond, "%s.id = %s.target_id", target_alias, edge_alias);

                    for (int i = 0; i < target_node->labels->count; i++) {
                        const char *label = get_label_string(target_node->labels->items[i]);
                        if (label) {
                            { char *esc_label = escape_sql_string(label);
                              dbuf_appendf(&target_cond,
                                " AND EXISTS (SELECT 1 FROM %snode_labels WHERE node_id = %s.id AND label = '%s')",
                                ctx->current_graph ? ctx->current_graph : "",
                                target_alias, esc_label ? esc_label : label);
                              free(esc_label); }
                        }
                    }

                    sql_join(ctx->unified_builder, SQL_JOIN_LEFT,
                             get_graph_table(ctx, "nodes"), target_alias, dbuf_get(&target_cond));
                    dbuf_free(&target_cond);
                } else {
                    char target_cond[256];
                    snprintf(target_cond, sizeof(target_cond), "%s.id = %s.target_id", target_alias, edge_alias);
                    sql_join(ctx->unified_builder, SQL_JOIN_LEFT, get_graph_table(ctx, "nodes"), target_alias, target_cond);
                }
            }
        } else {
            /* Non-optional: use CROSS JOIN for edges (will be filtered by WHERE).
             * T-0339 follow-up: skip if this alias is already in scope from
             * a prior MATCH clause (Match4 [7]). Same-pattern re-use is
             * caught at validate-time (Match3 [29]). */
            const char *fs0 = dbuf_get(&ctx->unified_builder->from);
            const char *js0 = dbuf_get(&ctx->unified_builder->joins);
            char needle[80];
            snprintf(needle, sizeof(needle), "AS %s", edge_alias);
            bool already = (fs0 && strstr(fs0, needle)) ||
                           (js0 && strstr(js0, needle));
            if (!already) {
                sql_join(ctx->unified_builder, SQL_JOIN_CROSS, get_graph_table(ctx, "edges"), edge_alias, NULL);
            }
        }
    }
    
    /* Note: Relationship constraints will be added later in the WHERE clause phase */
    
    /* Register relationship variable if present */
    if (rel->variable) {
        /* Register in unified system */
        transform_var_register_edge(ctx->var_ctx, rel->variable, edge_alias, rel->type);
    } else {
        /* For unnamed relationships, we need a way to track them */
        /* Create a synthetic variable name based on position for tracking */
        char synthetic_var[32];
        snprintf(synthetic_var, sizeof(synthetic_var), "__unnamed_rel_%d", rel_index);
        /* Register in unified system */
        transform_var_register_edge(ctx->var_ctx, synthetic_var, edge_alias, rel->type);
    }
    
    CYPHER_DEBUG("Generated relationship match: %s connects %s to %s", 
                 edge_alias, source_alias, target_alias);
    
    return 0;
}

/* Transform WHERE clause expression (used by other modules) */
int transform_where_clause(cypher_transform_context *ctx, ast_node *where)
{
    CYPHER_DEBUG("Transforming WHERE clause expression, type: %s", 
                 where ? ast_node_type_name(where->type) : "NULL");
    
    if (!where) {
        return 0;
    }
    
    /* Debug the WHERE AST structure */
    if (where->type == AST_NODE_BINARY_OP) {
        cypher_binary_op *binop = (cypher_binary_op*)where;
        CYPHER_DEBUG("WHERE contains binary op: op_type=%d, left=%s, right=%s",
                     binop->op_type,
                     binop->left ? ast_node_type_name(binop->left->type) : "NULL",
                     binop->right ? ast_node_type_name(binop->right->type) : "NULL");
    }
    
    /* Transform the WHERE expression - caller handles WHERE/AND keywords */
    int result = transform_expression(ctx, where);
    CYPHER_DEBUG("WHERE transformation result: %d, SQL so far: %s", result, ctx->sql_buffer);
    return result;
}