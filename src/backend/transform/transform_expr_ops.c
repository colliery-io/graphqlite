/*
 * Expression Operators Transformation
 * Handles binary operations, NOT, null checks, and label expressions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "transform/cypher_transform.h"
#include "transform/transform_internal.h"
#include "transform/transform_functions.h"
#include "transform/transform_func_dispatch.h"
#include "parser/cypher_debug.h"

/* Transform label expression (e.g., n:Person) */
int transform_label_expression(cypher_transform_context *ctx, cypher_label_expr *label_expr)
{
    CYPHER_DEBUG("Transforming label expression");
    
    /* Get the base expression (should be an identifier) */
    if (label_expr->expr->type != AST_NODE_IDENTIFIER) {
        ctx->has_error = true;
        ctx->error_message = strdup("Complex label expressions not yet supported");
        return -1;
    }
    
    cypher_identifier *id = (cypher_identifier*)label_expr->expr;
    const char *alias = lookup_variable_alias(ctx, id->name);
    if (!alias) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "Unknown variable in label expression: %s", id->name);
        ctx->error_message = strdup(error);
        return -1;
    }
    
    /* Generate SQL to check if the node has the specified label */
    /* This checks if there's a record in node_labels table with this node_id and label */
    append_sql(ctx, "EXISTS (SELECT 1 FROM node_labels WHERE node_id = %s.id AND label = ", alias);
    append_string_literal(ctx, label_expr->label_name);
    append_sql(ctx, ")");
    
    return 0;
}

/* Transform NOT expression (e.g., NOT n:Person) */
int transform_not_expression(cypher_transform_context *ctx, cypher_not_expr *not_expr)
{
    CYPHER_DEBUG("Transforming NOT expression");

    append_sql(ctx, "NOT (");

    if (transform_expression(ctx, not_expr->expr) < 0) {
        return -1;
    }

    append_sql(ctx, ")");

    return 0;
}

/* Transform null check expression (e.g., n.name IS NULL, n.age IS NOT NULL) */
int transform_null_check(cypher_transform_context *ctx, cypher_null_check *null_check)
{
    CYPHER_DEBUG("Transforming NULL check expression: is_not_null=%d", null_check->is_not_null);

    /* Transform the expression being checked */
    if (transform_expression(ctx, null_check->expr) < 0) {
        return -1;
    }

    /* Append IS NULL or IS NOT NULL */
    if (null_check->is_not_null) {
        append_sql(ctx, " IS NOT NULL");
    } else {
        append_sql(ctx, " IS NULL");
    }

    return 0;
}

/* Transform binary operation (e.g., expr AND expr, expr OR expr) */
int transform_binary_operation(cypher_transform_context *ctx, cypher_binary_op *binary_op)
{
    CYPHER_DEBUG("Transforming binary operation: op_type=%d", binary_op->op_type);
    
    /* Set comparison context for comparison operators */
    bool was_in_comparison = ctx->in_comparison;
    if (binary_op->op_type == BINARY_OP_EQ || binary_op->op_type == BINARY_OP_NEQ ||
        binary_op->op_type == BINARY_OP_LT || binary_op->op_type == BINARY_OP_GT ||
        binary_op->op_type == BINARY_OP_LTE || binary_op->op_type == BINARY_OP_GTE ||
        binary_op->op_type == BINARY_OP_REGEX_MATCH || binary_op->op_type == BINARY_OP_IN ||
        binary_op->op_type == BINARY_OP_STARTS_WITH || binary_op->op_type == BINARY_OP_ENDS_WITH ||
        binary_op->op_type == BINARY_OP_CONTAINS) {
        ctx->in_comparison = true;
    }

    /* Handle REGEX_MATCH specially - convert to regexp(pattern, string) function call */
    if (binary_op->op_type == BINARY_OP_REGEX_MATCH) {
        append_sql(ctx, "regexp(");
        /* Pattern is the right operand */
        if (transform_expression(ctx, binary_op->right) < 0) {
            return -1;
        }
        append_sql(ctx, ", ");
        /* String to match is the left operand */
        if (transform_expression(ctx, binary_op->left) < 0) {
            return -1;
        }
        append_sql(ctx, ")");
        ctx->in_comparison = was_in_comparison;
        return 0;
    }

    /* Handle IN operator specially - check membership in list */
    if (binary_op->op_type == BINARY_OP_IN) {
        append_sql(ctx, "(");
        /* Transform the left operand (value to check) */
        if (transform_expression(ctx, binary_op->left) < 0) {
            return -1;
        }
        append_sql(ctx, " IN ");

        /* Check if right side is a literal list */
        if (binary_op->right->type == AST_NODE_LIST) {
            /* Literal list: generate IN (val1, val2, val3) */
            cypher_list *list = (cypher_list*)binary_op->right;
            append_sql(ctx, "(");
            for (int i = 0; i < list->items->count; i++) {
                if (i > 0) append_sql(ctx, ", ");
                if (transform_expression(ctx, list->items->items[i]) < 0) {
                    return -1;
                }
            }
            append_sql(ctx, ")");
        } else {
            /* Variable or expression: use json_each subquery */
            append_sql(ctx, "(SELECT value FROM json_each(");
            if (transform_expression(ctx, binary_op->right) < 0) {
                return -1;
            }
            append_sql(ctx, "))");
        }
        append_sql(ctx, ")");
        ctx->in_comparison = was_in_comparison;
        return 0;
    }

    /* Handle STARTS WITH operator - string starts with prefix */
    if (binary_op->op_type == BINARY_OP_STARTS_WITH) {
        append_sql(ctx, "(");
        if (transform_expression(ctx, binary_op->left) < 0) {
            return -1;
        }
        append_sql(ctx, " LIKE ");
        if (transform_expression(ctx, binary_op->right) < 0) {
            return -1;
        }
        append_sql(ctx, " || '%%')");  /* %% escapes to literal % in printf */
        ctx->in_comparison = was_in_comparison;
        return 0;
    }

    /* Handle ENDS WITH operator - string ends with suffix */
    if (binary_op->op_type == BINARY_OP_ENDS_WITH) {
        append_sql(ctx, "(");
        if (transform_expression(ctx, binary_op->left) < 0) {
            return -1;
        }
        append_sql(ctx, " LIKE '%%' || ");  /* %% escapes to literal % in printf */
        if (transform_expression(ctx, binary_op->right) < 0) {
            return -1;
        }
        append_sql(ctx, ")");
        ctx->in_comparison = was_in_comparison;
        return 0;
    }

    /* Handle CONTAINS operator - string contains substring */
    if (binary_op->op_type == BINARY_OP_CONTAINS) {
        append_sql(ctx, "(INSTR(");
        if (transform_expression(ctx, binary_op->left) < 0) {
            return -1;
        }
        append_sql(ctx, ", ");
        if (transform_expression(ctx, binary_op->right) < 0) {
            return -1;
        }
        append_sql(ctx, ") > 0)");
        ctx->in_comparison = was_in_comparison;
        return 0;
    }

    /* Add left parenthesis for precedence */
    append_sql(ctx, "(");
    
    /* Transform left expression */
    CYPHER_DEBUG("Transforming left operand");
    if (transform_expression(ctx, binary_op->left) < 0) {
        CYPHER_DEBUG("Left operand transformation failed");
        return -1;
    }
    CYPHER_DEBUG("Left operand done, SQL so far: %s", ctx->sql_buffer);
    
    /* Add operator */
    switch (binary_op->op_type) {
        case BINARY_OP_AND:
            append_sql(ctx, " AND ");
            break;
        case BINARY_OP_OR:
            append_sql(ctx, " OR ");
            break;
        case BINARY_OP_XOR:
            /* XOR in SQL: (a AND NOT b) OR (NOT a AND b), but for booleans <> works */
            append_sql(ctx, " <> ");
            break;
        case BINARY_OP_EQ:
            append_sql(ctx, " = ");
            break;
        case BINARY_OP_NEQ:
            append_sql(ctx, " <> ");
            break;
        case BINARY_OP_LT:
            append_sql(ctx, " < ");
            break;
        case BINARY_OP_GT:
            CYPHER_DEBUG("Adding > operator");
            append_sql(ctx, " > ");
            break;
        case BINARY_OP_LTE:
            append_sql(ctx, " <= ");
            break;
        case BINARY_OP_GTE:
            append_sql(ctx, " >= ");
            break;
        case BINARY_OP_ADD:
            /* In Cypher, + on strings is concatenation. SQLite uses || for that.
             * Check if left operand is a string literal or a string concat chain. */
            {
                bool is_string_concat = false;
                if (binary_op->left->type == AST_NODE_LITERAL) {
                    cypher_literal *lit = (cypher_literal*)binary_op->left;
                    if (lit->literal_type == LITERAL_STRING) {
                        is_string_concat = true;
                    }
                }
                /* Also check for chained string concat like 'a' + 'b' + 'c' */
                if (!is_string_concat && binary_op->left->type == AST_NODE_BINARY_OP) {
                    cypher_binary_op *left_op = (cypher_binary_op*)binary_op->left;
                    if (left_op->op_type == BINARY_OP_ADD) {
                        /* Recursively check leftmost operand */
                        ast_node *leftmost = left_op->left;
                        while (leftmost->type == AST_NODE_BINARY_OP) {
                            cypher_binary_op *inner = (cypher_binary_op*)leftmost;
                            if (inner->op_type != BINARY_OP_ADD) break;
                            leftmost = inner->left;
                        }
                        if (leftmost->type == AST_NODE_LITERAL) {
                            cypher_literal *lit = (cypher_literal*)leftmost;
                            if (lit->literal_type == LITERAL_STRING) {
                                is_string_concat = true;
                            }
                        }
                    }
                }
                append_sql(ctx, is_string_concat ? " || " : " + ");
            }
            break;
        case BINARY_OP_SUB:
            append_sql(ctx, " - ");
            break;
        case BINARY_OP_MUL:
            append_sql(ctx, " * ");
            break;
        case BINARY_OP_DIV:
            append_sql(ctx, " / ");
            break;
        case BINARY_OP_MOD:
            append_sql(ctx, " %% ");
            break;
        default:
            CYPHER_DEBUG("Unknown binary operator: %d", binary_op->op_type);
            ctx->has_error = true;
            ctx->error_message = strdup("Unknown binary operator");
            return -1;
    }
    
    CYPHER_DEBUG("Operator added, SQL so far: %s", ctx->sql_buffer);
    
    /* Transform right expression */
    CYPHER_DEBUG("Transforming right operand");
    if (transform_expression(ctx, binary_op->right) < 0) {
        CYPHER_DEBUG("Right operand transformation failed");
        return -1;
    }
    
    CYPHER_DEBUG("Right operand done, SQL so far: %s", ctx->sql_buffer);
    
    /* Close parenthesis */
    append_sql(ctx, ")");
    
    /* Restore comparison context */
    ctx->in_comparison = was_in_comparison;
    
    CYPHER_DEBUG("Binary operation complete: %s", ctx->sql_buffer);
    
    return 0;
}


/* Transform property access (e.g., n.name) */
int transform_property_access(cypher_transform_context *ctx, cypher_property *prop)
{
    CYPHER_DEBUG("Transforming property access");

    /* Get the base expression (should be an identifier) */
    if (prop->expr->type != AST_NODE_IDENTIFIER) {
        ctx->has_error = true;
        ctx->error_message = strdup("Complex property access not yet supported");
        return -1;
    }

    cypher_identifier *id = (cypher_identifier*)prop->expr;
    const char *alias = lookup_variable_alias(ctx, id->name);
    if (!alias) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "Unknown variable in property access: %s", id->name);
        ctx->error_message = strdup(error);
        return -1;
    }

    /* Check if this is a projected variable from WITH - if so, alias IS the node id */
    bool is_projected = is_projected_variable(ctx, id->name);
    bool is_edge = is_edge_variable(ctx, id->name);

    /* Generate property access query using our actual schema */
    /* We need to check multiple property tables based on type */

    if (is_edge) {
        /* Edge property access - use edge_props_* tables */
        if (ctx->in_comparison) {
            append_sql(ctx, "(SELECT COALESCE(");
            append_sql(ctx, "(SELECT ept.value FROM edge_props_text ept JOIN property_keys pk ON ept.key_id = pk.id WHERE ept.edge_id = %s.id AND pk.key = ", alias);
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, "), ");
            append_sql(ctx, "(SELECT epi.value FROM edge_props_int epi JOIN property_keys pk ON epi.key_id = pk.id WHERE epi.edge_id = %s.id AND pk.key = ", alias);
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, "), ");
            append_sql(ctx, "(SELECT epr.value FROM edge_props_real epr JOIN property_keys pk ON epr.key_id = pk.id WHERE epr.edge_id = %s.id AND pk.key = ", alias);
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, "), ");
            append_sql(ctx, "(SELECT CAST(epb.value AS INTEGER) FROM edge_props_bool epb JOIN property_keys pk ON epb.key_id = pk.id WHERE epb.edge_id = %s.id AND pk.key = ", alias);
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, ")))");
        } else {
            append_sql(ctx, "(SELECT COALESCE(");
            append_sql(ctx, "(SELECT ept.value FROM edge_props_text ept JOIN property_keys pk ON ept.key_id = pk.id WHERE ept.edge_id = %s.id AND pk.key = ", alias);
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, "), ");
            append_sql(ctx, "(SELECT CAST(epi.value AS TEXT) FROM edge_props_int epi JOIN property_keys pk ON epi.key_id = pk.id WHERE epi.edge_id = %s.id AND pk.key = ", alias);
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, "), ");
            append_sql(ctx, "(SELECT CAST(epr.value AS TEXT) FROM edge_props_real epr JOIN property_keys pk ON epr.key_id = pk.id WHERE epr.edge_id = %s.id AND pk.key = ", alias);
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, "), ");
            append_sql(ctx, "(SELECT CASE WHEN epb.value THEN 'true' ELSE 'false' END FROM edge_props_bool epb JOIN property_keys pk ON epb.key_id = pk.id WHERE epb.edge_id = %s.id AND pk.key = ", alias);
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, ")))");
        }
    } else if (ctx->in_comparison) {
        /* Node property access for comparisons - preserve proper types */
        append_sql(ctx, "(SELECT COALESCE(");
        /* Text properties (both numeric and non-numeric strings) */
        append_sql(ctx, "(SELECT npt.value FROM node_props_text npt JOIN property_keys pk ON npt.key_id = pk.id WHERE npt.node_id = %s%s AND pk.key = ",
                   alias, is_projected ? "" : ".id");
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");
        /* Integer properties */
        append_sql(ctx, "(SELECT npi.value FROM node_props_int npi JOIN property_keys pk ON npi.key_id = pk.id WHERE npi.node_id = %s%s AND pk.key = ",
                   alias, is_projected ? "" : ".id");
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");
        /* Real properties */
        append_sql(ctx, "(SELECT npr.value FROM node_props_real npr JOIN property_keys pk ON npr.key_id = pk.id WHERE npr.node_id = %s%s AND pk.key = ",
                   alias, is_projected ? "" : ".id");
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");
        /* Boolean properties (cast to integer for comparison) */
        append_sql(ctx, "(SELECT CAST(npb.value AS INTEGER) FROM node_props_bool npb JOIN property_keys pk ON npb.key_id = pk.id WHERE npb.node_id = %s%s AND pk.key = ",
                   alias, is_projected ? "" : ".id");
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, ")))");
    } else {
        /* Node property access for RETURN clauses - convert everything to text */
        append_sql(ctx, "(SELECT COALESCE(");
        append_sql(ctx, "(SELECT npt.value FROM node_props_text npt JOIN property_keys pk ON npt.key_id = pk.id WHERE npt.node_id = %s%s AND pk.key = ",
                   alias, is_projected ? "" : ".id");
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");
        append_sql(ctx, "(SELECT CAST(npi.value AS TEXT) FROM node_props_int npi JOIN property_keys pk ON npi.key_id = pk.id WHERE npi.node_id = %s%s AND pk.key = ",
                   alias, is_projected ? "" : ".id");
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");
        append_sql(ctx, "(SELECT CAST(npr.value AS TEXT) FROM node_props_real npr JOIN property_keys pk ON npr.key_id = pk.id WHERE npr.node_id = %s%s AND pk.key = ",
                   alias, is_projected ? "" : ".id");
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");
        append_sql(ctx, "(SELECT CASE WHEN npb.value THEN 'true' ELSE 'false' END FROM node_props_bool npb JOIN property_keys pk ON npb.key_id = pk.id WHERE npb.node_id = %s%s AND pk.key = ",
                   alias, is_projected ? "" : ".id");
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, ")))");
    }

    return 0;
}

/* Transform function call (e.g., count(n), count(*)) */
int transform_function_call(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming function call");

    if (!func_call || !func_call->function_name) {
        ctx->has_error = true;
        ctx->error_message = strdup("Invalid function call");
        return -1;
    }

    /* Look up handler in dispatch table */
    transform_func_handler handler = lookup_function_handler(func_call->function_name);

    if (handler) {
        return handler(ctx, func_call);
    }

    /* Unsupported function */
    ctx->has_error = true;
    char error[256];
    snprintf(error, sizeof(error), "Unsupported function: %s", func_call->function_name);
    ctx->error_message = strdup(error);
    return -1;
}
