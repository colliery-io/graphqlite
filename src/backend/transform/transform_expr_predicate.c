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
#include "transform/transform_functions.h"
#include "parser/cypher_ast.h"
#include "parser/cypher_debug.h"

/* Helper to get label string from a label literal node */
static const char* get_label_string(ast_node *label_node)
{
    if (!label_node || label_node->type != AST_NODE_LITERAL) return NULL;
    cypher_literal *lit = (cypher_literal*)label_node;
    if (lit->literal_type != LITERAL_STRING) return NULL;
    return lit->value.string;
}

/* Helper to check if a node pattern has any labels */
static bool has_labels(cypher_node_pattern *node)
{
    return node && node->labels && node->labels->count > 0;
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
                                    outer_alias = lookup_variable_alias(ctx, node->variable);
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

                                /* Join source node with relationship */
                                int source_node = i / 2;
                                int target_node = source_node + 1;

                                append_sql(ctx, "e%d.source_id = %s.id AND e%d.target_id = %s.id",
                                          rel_index, node_aliases[source_node],
                                          rel_index, node_aliases[target_node]);

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
                        const char *alias = lookup_variable_alias(ctx, id->name);

                        if (!alias) {
                            ctx->has_error = true;
                            char error[256];
                            snprintf(error, sizeof(error), "Unknown variable in EXISTS property: %s", id->name);
                            ctx->error_message = strdup(error);
                            return -1;
                        }

                        /* Generate SQL to check if property exists */
                        append_sql(ctx, "(");
                        append_sql(ctx, "EXISTS (SELECT 1 FROM node_props_text npt JOIN property_keys pk ON npt.key_id = pk.id WHERE npt.node_id = %s.id AND pk.key = ", alias);
                        append_string_literal(ctx, prop->property_name);
                        append_sql(ctx, ") OR ");
                        append_sql(ctx, "EXISTS (SELECT 1 FROM node_props_int npi JOIN property_keys pk ON npi.key_id = pk.id WHERE npi.node_id = %s.id AND pk.key = ", alias);
                        append_string_literal(ctx, prop->property_name);
                        append_sql(ctx, ") OR ");
                        append_sql(ctx, "EXISTS (SELECT 1 FROM node_props_real npr JOIN property_keys pk ON npr.key_id = pk.id WHERE npr.node_id = %s.id AND pk.key = ", alias);
                        append_string_literal(ctx, prop->property_name);
                        append_sql(ctx, ") OR ");
                        append_sql(ctx, "EXISTS (SELECT 1 FROM node_props_bool npb JOIN property_keys pk ON npb.key_id = pk.id WHERE npb.node_id = %s.id AND pk.key = ", alias);
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
 * SQL generation:
 * - all(x IN list WHERE pred)    -> (SELECT COUNT(*) = json_array_length(list) FROM json_each(list) WHERE pred)
 * - any(x IN list WHERE pred)    -> (SELECT COUNT(*) > 0 FROM json_each(list) WHERE pred)
 * - none(x IN list WHERE pred)   -> (SELECT COUNT(*) = 0 FROM json_each(list) WHERE pred)
 * - single(x IN list WHERE pred) -> (SELECT COUNT(*) = 1 FROM json_each(list) WHERE pred)
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
    const char *old_alias = lookup_variable_alias(ctx, pred->variable);
    char *saved_alias = old_alias ? strdup(old_alias) : NULL;

    /* Register the predicate variable to map to json_each.value */
    register_variable(ctx, pred->variable, "json_each.value");
    /* Update the type to projected so it's treated as a direct value */
    for (int i = 0; i < ctx->variable_count; i++) {
        if (strcmp(ctx->variables[i].name, pred->variable) == 0) {
            ctx->variables[i].type = VAR_TYPE_PROJECTED;
            break;
        }
    }

    /* For 'all' predicate, we need to capture the list expression to compare count */
    if (pred->pred_type == LIST_PRED_ALL) {
        /* Build: (SELECT COUNT(*) = json_array_length(list) FROM json_each(list) WHERE pred) */
        append_sql(ctx, "(SELECT COUNT(*) = json_array_length(");
        if (transform_expression(ctx, pred->list_expr) < 0) {
            if (saved_alias) free(saved_alias);
            return -1;
        }
        append_sql(ctx, ") FROM json_each(");
        if (transform_expression(ctx, pred->list_expr) < 0) {
            if (saved_alias) free(saved_alias);
            return -1;
        }
        append_sql(ctx, ") WHERE ");
        if (transform_expression(ctx, pred->predicate) < 0) {
            if (saved_alias) free(saved_alias);
            return -1;
        }
        append_sql(ctx, ")");
    } else {
        /* Build: (SELECT COUNT(*) <op> <n> FROM json_each(list) WHERE pred) */
        append_sql(ctx, "(SELECT COUNT(*) ");

        switch (pred->pred_type) {
            case LIST_PRED_ANY:
                append_sql(ctx, "> 0");
                break;
            case LIST_PRED_NONE:
                append_sql(ctx, "= 0");
                break;
            case LIST_PRED_SINGLE:
                append_sql(ctx, "= 1");
                break;
            default:
                break;
        }

        append_sql(ctx, " FROM json_each(");
        if (transform_expression(ctx, pred->list_expr) < 0) {
            if (saved_alias) free(saved_alias);
            return -1;
        }
        append_sql(ctx, ") WHERE ");
        if (transform_expression(ctx, pred->predicate) < 0) {
            if (saved_alias) free(saved_alias);
            return -1;
        }
        append_sql(ctx, ")");
    }

    /* Restore the old alias if we saved one */
    if (saved_alias) {
        register_variable(ctx, pred->variable, saved_alias);
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
    static int reduce_counter = 0;

    CYPHER_DEBUG("Transforming reduce expression");

    if (!reduce || !reduce->accumulator || !reduce->initial_value ||
        !reduce->variable || !reduce->list_expr || !reduce->expression) {
        ctx->has_error = true;
        ctx->error_message = strdup("Invalid reduce expression");
        return -1;
    }

    /* Generate unique CTE name */
    char cte_name[32];
    snprintf(cte_name, sizeof(cte_name), "_reduce_%d", reduce_counter++);

    /* Save existing aliases for accumulator and variable names */
    const char *old_acc_alias = lookup_variable_alias(ctx, reduce->accumulator);
    const char *old_var_alias = lookup_variable_alias(ctx, reduce->variable);
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
    register_variable(ctx, reduce->accumulator, acc_ref);
    for (int i = 0; i < ctx->variable_count; i++) {
        if (strcmp(ctx->variables[i].name, reduce->accumulator) == 0) {
            ctx->variables[i].type = VAR_TYPE_PROJECTED;
            break;
        }
    }

    register_variable(ctx, reduce->variable, "json_each.value");
    for (int i = 0; i < ctx->variable_count; i++) {
        if (strcmp(ctx->variables[i].name, reduce->variable) == 0) {
            ctx->variables[i].type = VAR_TYPE_PROJECTED;
            break;
        }
    }

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
        register_variable(ctx, reduce->accumulator, saved_acc_alias);
        free(saved_acc_alias);
    }
    if (saved_var_alias) {
        register_variable(ctx, reduce->variable, saved_var_alias);
        free(saved_var_alias);
    }

    return 0;
}
