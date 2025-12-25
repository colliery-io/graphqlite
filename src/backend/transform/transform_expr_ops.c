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
        binary_op->op_type == BINARY_OP_REGEX_MATCH || binary_op->op_type == BINARY_OP_IN) {
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
            append_sql(ctx, " + ");
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
    
    /* Handle TYPE function specifically */
    if (strcasecmp(func_call->function_name, "type") == 0) {
        return transform_type_function(ctx, func_call);
    }
    
    /* Handle COUNT function specifically */
    if (strcasecmp(func_call->function_name, "count") == 0) {
        return transform_count_function(ctx, func_call);
    }
    
    /* Handle other aggregate functions */
    if (strcasecmp(func_call->function_name, "min") == 0 ||
        strcasecmp(func_call->function_name, "max") == 0 ||
        strcasecmp(func_call->function_name, "avg") == 0 ||
        strcasecmp(func_call->function_name, "sum") == 0) {
        return transform_aggregate_function(ctx, func_call);
    }

    /* Handle length() function - check for path first */
    if (strcasecmp(func_call->function_name, "length") == 0) {
        /* Check if argument is a path variable */
        if (func_call->args && func_call->args->count == 1 &&
            func_call->args->items[0] &&
            func_call->args->items[0]->type == AST_NODE_IDENTIFIER) {
            cypher_identifier *id = (cypher_identifier*)func_call->args->items[0];
            if (is_path_variable(ctx, id->name)) {
                return transform_path_length_function(ctx, func_call);
            }
        }
        /* Fall through to string length */
        return transform_string_function(ctx, func_call);
    }

    /* Handle string functions */
    if (strcasecmp(func_call->function_name, "toUpper") == 0 ||
        strcasecmp(func_call->function_name, "toLower") == 0 ||
        strcasecmp(func_call->function_name, "trim") == 0 ||
        strcasecmp(func_call->function_name, "ltrim") == 0 ||
        strcasecmp(func_call->function_name, "rtrim") == 0 ||
        strcasecmp(func_call->function_name, "size") == 0 ||
        strcasecmp(func_call->function_name, "reverse") == 0) {
        return transform_string_function(ctx, func_call);
    }

    /* Handle substring function (2 or 3 args) */
    if (strcasecmp(func_call->function_name, "substring") == 0) {
        return transform_substring_function(ctx, func_call);
    }

    /* Handle replace function (3 args) */
    if (strcasecmp(func_call->function_name, "replace") == 0) {
        return transform_replace_function(ctx, func_call);
    }

    /* Handle split function (2 args) */
    if (strcasecmp(func_call->function_name, "split") == 0) {
        return transform_split_function(ctx, func_call);
    }

    /* Handle left/right functions (2 args) */
    if (strcasecmp(func_call->function_name, "left") == 0 ||
        strcasecmp(func_call->function_name, "right") == 0) {
        return transform_leftright_function(ctx, func_call);
    }

    /* Handle pattern matching functions */
    if (strcasecmp(func_call->function_name, "startsWith") == 0 ||
        strcasecmp(func_call->function_name, "endsWith") == 0 ||
        strcasecmp(func_call->function_name, "contains") == 0) {
        return transform_pattern_match_function(ctx, func_call);
    }

    /* Handle single-arg math functions */
    if (strcasecmp(func_call->function_name, "abs") == 0 ||
        strcasecmp(func_call->function_name, "ceil") == 0 ||
        strcasecmp(func_call->function_name, "floor") == 0 ||
        strcasecmp(func_call->function_name, "sign") == 0 ||
        strcasecmp(func_call->function_name, "sqrt") == 0 ||
        strcasecmp(func_call->function_name, "log") == 0 ||
        strcasecmp(func_call->function_name, "log10") == 0 ||
        strcasecmp(func_call->function_name, "exp") == 0 ||
        strcasecmp(func_call->function_name, "sin") == 0 ||
        strcasecmp(func_call->function_name, "cos") == 0 ||
        strcasecmp(func_call->function_name, "tan") == 0 ||
        strcasecmp(func_call->function_name, "asin") == 0 ||
        strcasecmp(func_call->function_name, "acos") == 0 ||
        strcasecmp(func_call->function_name, "atan") == 0) {
        return transform_math_function(ctx, func_call);
    }

    /* Handle round function (1 or 2 args) */
    if (strcasecmp(func_call->function_name, "round") == 0) {
        return transform_round_function(ctx, func_call);
    }

    /* Handle no-arg functions */
    if (strcasecmp(func_call->function_name, "rand") == 0 ||
        strcasecmp(func_call->function_name, "random") == 0 ||
        strcasecmp(func_call->function_name, "pi") == 0 ||
        strcasecmp(func_call->function_name, "e") == 0) {
        return transform_noarg_function(ctx, func_call);
    }

    /* Handle coalesce function (variable args) */
    if (strcasecmp(func_call->function_name, "coalesce") == 0) {
        return transform_coalesce_function(ctx, func_call);
    }

    /* Handle toString function */
    if (strcasecmp(func_call->function_name, "toString") == 0) {
        return transform_tostring_function(ctx, func_call);
    }

    /* Handle toInteger/toFloat/toBoolean functions */
    if (strcasecmp(func_call->function_name, "toInteger") == 0 ||
        strcasecmp(func_call->function_name, "toFloat") == 0 ||
        strcasecmp(func_call->function_name, "toBoolean") == 0) {
        return transform_type_conversion_function(ctx, func_call);
    }

    /* Handle id() function */
    if (strcasecmp(func_call->function_name, "id") == 0) {
        return transform_id_function(ctx, func_call);
    }

    /* Handle labels() function */
    if (strcasecmp(func_call->function_name, "labels") == 0) {
        return transform_labels_function(ctx, func_call);
    }

    /* Handle properties() function */
    if (strcasecmp(func_call->function_name, "properties") == 0) {
        return transform_properties_function(ctx, func_call);
    }

    /* Handle keys() function */
    if (strcasecmp(func_call->function_name, "keys") == 0) {
        return transform_keys_function(ctx, func_call);
    }

    /* Handle path functions */
    if (strcasecmp(func_call->function_name, "nodes") == 0) {
        return transform_path_nodes_function(ctx, func_call);
    }

    if (strcasecmp(func_call->function_name, "relationships") == 0 ||
        strcasecmp(func_call->function_name, "rels") == 0) {
        return transform_path_relationships_function(ctx, func_call);
    }

    /* Handle startNode/endNode functions */
    if (strcasecmp(func_call->function_name, "startNode") == 0) {
        return transform_startnode_function(ctx, func_call);
    }

    if (strcasecmp(func_call->function_name, "endNode") == 0) {
        return transform_endnode_function(ctx, func_call);
    }

    /* Handle list functions: head, tail, last, size (for lists) */
    if (strcasecmp(func_call->function_name, "head") == 0 ||
        strcasecmp(func_call->function_name, "tail") == 0 ||
        strcasecmp(func_call->function_name, "last") == 0) {
        return transform_list_function(ctx, func_call);
    }

    /* Handle range() function */
    if (strcasecmp(func_call->function_name, "range") == 0) {
        return transform_range_function(ctx, func_call);
    }

    /* Handle collect() aggregate function */
    if (strcasecmp(func_call->function_name, "collect") == 0) {
        return transform_collect_function(ctx, func_call);
    }

    /* Handle timestamp() function */
    if (strcasecmp(func_call->function_name, "timestamp") == 0) {
        return transform_timestamp_function(ctx, func_call);
    }

    /* Handle date() function */
    if (strcasecmp(func_call->function_name, "date") == 0) {
        if (func_call->args && func_call->args->count > 0) {
            /* date(string) - parse date from string */
            append_sql(ctx, "date(");
            if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
            append_sql(ctx, ")");
        } else {
            /* date() - current date */
            append_sql(ctx, "date('now')");
        }
        return 0;
    }

    /* Handle time() function */
    if (strcasecmp(func_call->function_name, "time") == 0) {
        if (func_call->args && func_call->args->count > 0) {
            /* time(string) - parse time from string */
            append_sql(ctx, "time(");
            if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
            append_sql(ctx, ")");
        } else {
            /* time() - current time */
            append_sql(ctx, "time('now')");
        }
        return 0;
    }

    /* Handle datetime() function */
    if (strcasecmp(func_call->function_name, "datetime") == 0 ||
        strcasecmp(func_call->function_name, "localdatetime") == 0) {
        if (func_call->args && func_call->args->count > 0) {
            /* datetime(string) - parse datetime from string */
            append_sql(ctx, "datetime(");
            if (transform_expression(ctx, func_call->args->items[0]) < 0) return -1;
            append_sql(ctx, ")");
        } else {
            /* datetime() - current datetime */
            append_sql(ctx, "datetime('now')");
        }
        return 0;
    }

    /* Handle randomUUID() function */
    if (strcasecmp(func_call->function_name, "randomUUID") == 0 ||
        strcasecmp(func_call->function_name, "randomuuid") == 0) {
        return transform_randomuuid_function(ctx, func_call);
    }

    /* Handle PageRank graph algorithm functions */
    if (strcasecmp(func_call->function_name, "pageRank") == 0 ||
        strcasecmp(func_call->function_name, "pagerank") == 0) {
        return transform_pagerank_function(ctx, func_call);
    }

    if (strcasecmp(func_call->function_name, "topPageRank") == 0 ||
        strcasecmp(func_call->function_name, "toppagerank") == 0) {
        return transform_top_pagerank_function(ctx, func_call);
    }

    if (strcasecmp(func_call->function_name, "personalizedPageRank") == 0 ||
        strcasecmp(func_call->function_name, "personalizedpagerank") == 0) {
        return transform_personalized_pagerank_function(ctx, func_call);
    }

    /* Handle Label Propagation community detection functions */
    if (strcasecmp(func_call->function_name, "labelPropagation") == 0 ||
        strcasecmp(func_call->function_name, "labelpropagation") == 0 ||
        strcasecmp(func_call->function_name, "communities") == 0) {
        return transform_label_propagation_function(ctx, func_call);
    }

    if (strcasecmp(func_call->function_name, "communityOf") == 0 ||
        strcasecmp(func_call->function_name, "communityof") == 0) {
        return transform_community_of_function(ctx, func_call);
    }

    if (strcasecmp(func_call->function_name, "communityMembers") == 0 ||
        strcasecmp(func_call->function_name, "communitymembers") == 0) {
        return transform_community_members_function(ctx, func_call);
    }

    if (strcasecmp(func_call->function_name, "communityCount") == 0 ||
        strcasecmp(func_call->function_name, "communitycount") == 0) {
        return transform_community_count_function(ctx, func_call);
    }

    /* Unsupported function */
    ctx->has_error = true;
    char error[256];
    snprintf(error, sizeof(error), "Unsupported function: %s", func_call->function_name);
    ctx->error_message = strdup(error);
    return -1;
}
