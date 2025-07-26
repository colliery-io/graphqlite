/*
 * RETURN clause transformation
 * Converts RETURN items into SQL SELECT projections
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "transform/cypher_transform.h"
#include "parser/cypher_debug.h"

/* Forward declarations */
static int transform_return_item(cypher_transform_context *ctx, cypher_return_item *item, bool first);

/* Transform a RETURN clause */
int transform_return_clause(cypher_transform_context *ctx, cypher_return *ret)
{
    CYPHER_DEBUG("Transforming RETURN clause");
    
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
    
    /* Find the SELECT clause and replace the * with actual columns */
    char *select_pos = strstr(ctx->sql_buffer, "SELECT *");
    if (!select_pos) {
        /* No SELECT clause yet - create one */
        if (ctx->sql_size > 0) {
            ctx->has_error = true;
            ctx->error_message = strdup("RETURN without MATCH not supported");
            return -1;
        }
        append_sql(ctx, "SELECT ");
    } else {
        /* Replace the * with actual column list */
        /* Find the end of "SELECT *" and skip any spaces */
        char *after_star = select_pos + strlen("SELECT *");
        while (*after_star == ' ') after_star++; /* Skip any extra spaces */
        char *temp = strdup(after_star);
        
        /* Truncate at SELECT */
        ctx->sql_size = select_pos + strlen("SELECT ") - ctx->sql_buffer;
        ctx->sql_buffer[ctx->sql_size] = '\0';
        
        /* Add DISTINCT if needed */
        if (ret->distinct) {
            append_sql(ctx, "DISTINCT ");
        }
        
        /* Process return items */
        for (int i = 0; i < ret->items->count; i++) {
            cypher_return_item *item = (cypher_return_item*)ret->items->items[i];
            if (transform_return_item(ctx, item, i == 0) < 0) {
                free(temp);
                return -1;
            }
        }
        
        /* Append the rest of the query */
        append_sql(ctx, " %s", temp);
        free(temp);
        
        return 0;
    }
    
    /* Add DISTINCT if specified */
    if (ret->distinct) {
        append_sql(ctx, "DISTINCT ");
    }
    
    /* Process each return item */
    for (int i = 0; i < ret->items->count; i++) {
        cypher_return_item *item = (cypher_return_item*)ret->items->items[i];
        if (transform_return_item(ctx, item, i == 0) < 0) {
            return -1;
        }
    }
    
    /* Handle ORDER BY */
    if (ret->order_by && ret->order_by->count > 0) {
        append_sql(ctx, " ORDER BY ");
        for (int i = 0; i < ret->order_by->count; i++) {
            if (i > 0) {
                append_sql(ctx, ", ");
            }
            cypher_order_by_item *order_item = (cypher_order_by_item*)ret->order_by->items[i];
            if (transform_expression(ctx, order_item->expr) < 0) {
                return -1;
            }
            /* Add sort direction */
            if (order_item->descending) {
                append_sql(ctx, " DESC");
            } else {
                append_sql(ctx, " ASC");
            }
        }
    }
    
    /* Handle LIMIT */
    if (ret->limit) {
        append_sql(ctx, " LIMIT ");
        if (transform_expression(ctx, ret->limit) < 0) {
            return -1;
        }
    }
    
    /* Handle SKIP (using SQLite OFFSET) */
    if (ret->skip) {
        append_sql(ctx, " OFFSET ");
        if (transform_expression(ctx, ret->skip) < 0) {
            return -1;
        }
    }
    
    return 0;
}

/* Transform a single return item */
static int transform_return_item(cypher_transform_context *ctx, cypher_return_item *item, bool first)
{
    if (!first) {
        append_sql(ctx, ", ");
    }
    
    /* Special handling for identifiers with aliases */
    if (item->alias && item->expr->type == AST_NODE_IDENTIFIER) {
        cypher_identifier *id = (cypher_identifier*)item->expr;
        const char *table_alias = lookup_variable_alias(ctx, id->name);
        if (table_alias) {
            /* For variables with alias, select the ID and alias it */
            append_sql(ctx, "%s.id AS ", table_alias);
            append_identifier(ctx, item->alias);
            return 0;
        }
    }
    
    /* Transform the expression */
    if (transform_expression(ctx, item->expr) < 0) {
        return -1;
    }
    
    /* Add alias if specified (for non-wildcard expressions) */
    if (item->alias && item->expr->type != AST_NODE_IDENTIFIER) {
        append_sql(ctx, " AS ");
        append_identifier(ctx, item->alias);
    }
    
    return 0;
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
                const char *alias = lookup_variable_alias(ctx, id->name);
                if (alias) {
                    if (is_edge_variable(ctx, id->name)) {
                        /* This is an edge variable - return the edge ID */
                        append_sql(ctx, "%s.id", alias);
                    } else {
                        /* This is a node variable - return the node ID for now */
                        append_sql(ctx, "%s.id", alias);
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
            break;
            
        case AST_NODE_PROPERTY:
            return transform_property_access(ctx, (cypher_property*)expr);
            
        case AST_NODE_LABEL_EXPR:
            return transform_label_expression(ctx, (cypher_label_expr*)expr);
            
        case AST_NODE_NOT_EXPR:
            return transform_not_expression(ctx, (cypher_not_expr*)expr);
            
        case AST_NODE_BINARY_OP:
            return transform_binary_operation(ctx, (cypher_binary_op*)expr);
            
        case AST_NODE_FUNCTION_CALL:
            return transform_function_call(ctx, (cypher_function_call*)expr);
            
        case AST_NODE_LITERAL:
            {
                cypher_literal *lit = (cypher_literal*)expr;
                switch (lit->literal_type) {
                    case LITERAL_INTEGER:
                        append_sql(ctx, "%d", lit->value.integer);
                        break;
                    case LITERAL_DECIMAL:
                        append_sql(ctx, "%f", lit->value.decimal);
                        break;
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
    
    /* Generate property access query using our actual schema */
    /* We need to check multiple property tables based on type */
    
    /* For comparisons, preserve proper types instead of casting everything to text */
    if (ctx->in_comparison) {
        /* Use COALESCE with all property types for comparisons */
        append_sql(ctx, "(SELECT COALESCE(");
        /* Text properties (both numeric and non-numeric strings) */
        append_sql(ctx, "(SELECT npt.value FROM node_props_text npt JOIN property_keys pk ON npt.key_id = pk.id WHERE npt.node_id = %s.id AND pk.key = ", alias);
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");
        /* Integer properties */
        append_sql(ctx, "(SELECT npi.value FROM node_props_int npi JOIN property_keys pk ON npi.key_id = pk.id WHERE npi.node_id = %s.id AND pk.key = ", alias);
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");
        /* Real properties */
        append_sql(ctx, "(SELECT npr.value FROM node_props_real npr JOIN property_keys pk ON npr.key_id = pk.id WHERE npr.node_id = %s.id AND pk.key = ", alias);
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");
        /* Boolean properties (cast to integer for comparison) */
        append_sql(ctx, "(SELECT CAST(npb.value AS INTEGER) FROM node_props_bool npb JOIN property_keys pk ON npb.key_id = pk.id WHERE npb.node_id = %s.id AND pk.key = ", alias);
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, ")))");
    } else {
        /* For RETURN clauses, convert everything to text as before */
        append_sql(ctx, "(SELECT COALESCE(");
        append_sql(ctx, "(SELECT npt.value FROM node_props_text npt JOIN property_keys pk ON npt.key_id = pk.id WHERE npt.node_id = %s.id AND pk.key = ", alias);
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");
        append_sql(ctx, "(SELECT CAST(npi.value AS TEXT) FROM node_props_int npi JOIN property_keys pk ON npi.key_id = pk.id WHERE npi.node_id = %s.id AND pk.key = ", alias);
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");
        append_sql(ctx, "(SELECT CAST(npr.value AS TEXT) FROM node_props_real npr JOIN property_keys pk ON npr.key_id = pk.id WHERE npr.node_id = %s.id AND pk.key = ", alias);
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");
        append_sql(ctx, "(SELECT CASE WHEN npb.value THEN 'true' ELSE 'false' END FROM node_props_bool npb JOIN property_keys pk ON npb.key_id = pk.id WHERE npb.node_id = %s.id AND pk.key = ", alias);
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, ")))");
    }
    
    return 0;
}

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

/* Transform binary operation (e.g., expr AND expr, expr OR expr) */
int transform_binary_operation(cypher_transform_context *ctx, cypher_binary_op *binary_op)
{
    CYPHER_DEBUG("Transforming binary operation: op_type=%d", binary_op->op_type);
    
    /* Set comparison context for comparison operators */
    bool was_in_comparison = ctx->in_comparison;
    if (binary_op->op_type == BINARY_OP_EQ || binary_op->op_type == BINARY_OP_NEQ ||
        binary_op->op_type == BINARY_OP_LT || binary_op->op_type == BINARY_OP_GT ||
        binary_op->op_type == BINARY_OP_LTE || binary_op->op_type == BINARY_OP_GTE) {
        ctx->in_comparison = true;
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
    
    /* Unsupported function */
    ctx->has_error = true;
    char error[256];
    snprintf(error, sizeof(error), "Unsupported function: %s", func_call->function_name);
    ctx->error_message = strdup(error);
    return -1;
}

/* Transform COUNT function specifically */
int transform_count_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming COUNT function");
    
    /* COUNT() with no arguments - treat as COUNT(*) */
    if (!func_call->args || func_call->args->count == 0) {
        append_sql(ctx, "COUNT(*)");
        return 0;
    }
    
    /* COUNT(*) case - represented as single NULL argument */
    if (func_call->args->count == 1 && func_call->args->items[0] == NULL) {
        append_sql(ctx, "COUNT(*)");
        return 0;
    }
    
    /* COUNT(expression) case */
    if (func_call->args->count == 1 && func_call->args->items[0] != NULL) {
        if (func_call->distinct) {
            append_sql(ctx, "COUNT(DISTINCT ");
        } else {
            append_sql(ctx, "COUNT(");
        }
        
        if (transform_expression(ctx, func_call->args->items[0]) < 0) {
            return -1;
        }
        
        append_sql(ctx, ")");
        return 0;
    }
    
    /* Invalid COUNT usage */
    ctx->has_error = true;
    ctx->error_message = strdup("COUNT function accepts 0 or 1 argument");
    return -1;
}

/* Transform other aggregate functions (MIN, MAX, AVG, SUM) */
int transform_aggregate_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming aggregate function: %s", func_call->function_name);
    
    /* These functions require exactly one argument */
    if (!func_call->args || func_call->args->count != 1 || func_call->args->items[0] == NULL) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "%s function requires exactly one non-null argument", func_call->function_name);
        ctx->error_message = strdup(error);
        return -1;
    }
    
    /* Generate SQL function call - convert to uppercase for SQL compliance */
    char upper_func[64];
    strncpy(upper_func, func_call->function_name, sizeof(upper_func) - 1);
    upper_func[sizeof(upper_func) - 1] = '\0';
    for (int i = 0; upper_func[i]; i++) {
        upper_func[i] = toupper(upper_func[i]);
    }
    
    if (func_call->distinct) {
        append_sql(ctx, "%s(DISTINCT ", upper_func);
    } else {
        append_sql(ctx, "%s(", upper_func);
    }
    
    if (transform_expression(ctx, func_call->args->items[0]) < 0) {
        return -1;
    }
    
    append_sql(ctx, ")");
    return 0;
}

/* Transform TYPE function specifically */
int transform_type_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming TYPE function");
    
    /* TYPE function requires exactly one argument */
    if (!func_call->args || func_call->args->count != 1 || func_call->args->items[0] == NULL) {
        ctx->has_error = true;
        ctx->error_message = strdup("type() function requires exactly one non-null argument");
        return -1;
    }
    
    /* The argument must be an identifier (variable) */
    ast_node *arg = func_call->args->items[0];
    if (arg->type != AST_NODE_IDENTIFIER) {
        ctx->has_error = true;
        ctx->error_message = strdup("type() function argument must be a relationship variable");
        return -1;
    }
    
    cypher_identifier *id = (cypher_identifier*)arg;
    
    /* Check if the variable is registered */
    const char *alias = lookup_variable_alias(ctx, id->name);
    if (!alias) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "Unknown variable in type() function: %s", id->name);
        ctx->error_message = strdup(error);
        return -1;
    }
    
    /* Check if the variable is a relationship/edge variable */
    if (!is_edge_variable(ctx, id->name)) {
        ctx->has_error = true;
        ctx->error_message = strdup("type() function argument must be a relationship variable");
        return -1;
    }
    
    /* Generate SQL to extract the relationship type from the edges table */
    /* The relationship alias should point to the edges table with an 'id' column */
    /* We extract the 'type' column which contains the relationship type */
    append_sql(ctx, "(SELECT type FROM edges WHERE id = %s.id)", alias);
    
    return 0;
}