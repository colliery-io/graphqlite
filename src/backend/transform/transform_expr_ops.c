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
    const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
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

    /* Wrap outer parens too — SQLite gives `>=` higher precedence than NOT,
     * so 'NOT (0) >= 0' parses as 'NOT ((0) >= 0)'. Outer parens force the
     * NOT to bind to its single operand. Coerce string-encoded booleans
     * ('true'/'false') to int via _gql_bool(). */
    append_sql(ctx, "(NOT _gql_bool(");

    if (transform_expression(ctx, not_expr->expr) < 0) {
        return -1;
    }

    append_sql(ctx, "))");

    return 0;
}

/* Transform null check expression (e.g., n.name IS NULL, n.age IS NOT NULL).
 * Wrap in outer parens — in Cypher IS NULL has higher precedence than `=`
 * and other comparison operators, but in SQLite the opposite is true, so
 * `false = true IS NULL` parses as `(false = true) IS NULL`. Forcing
 * parens recovers Cypher semantics. */
int transform_null_check(cypher_transform_context *ctx, cypher_null_check *null_check)
{
    CYPHER_DEBUG("Transforming NULL check expression: is_not_null=%d", null_check->is_not_null);

    append_sql(ctx, "(");

    /* Transform the expression being checked */
    if (transform_expression(ctx, null_check->expr) < 0) {
        return -1;
    }

    /* Append IS NULL or IS NOT NULL */
    if (null_check->is_not_null) {
        append_sql(ctx, " IS NOT NULL)");
    } else {
        append_sql(ctx, " IS NULL)");
    }

    return 0;
}

/* Transform binary operation (e.g., expr AND expr, expr OR expr) */
int transform_binary_operation(cypher_transform_context *ctx, cypher_binary_op *binary_op)
{
    CYPHER_DEBUG("Transforming binary operation: op_type=%d", binary_op->op_type);
    
    /* Set comparison context for comparison operators */
    bool was_in_comparison = ctx->in_comparison;
    bool is_cmp = (binary_op->op_type == BINARY_OP_EQ || binary_op->op_type == BINARY_OP_NEQ ||
        binary_op->op_type == BINARY_OP_LT || binary_op->op_type == BINARY_OP_GT ||
        binary_op->op_type == BINARY_OP_LTE || binary_op->op_type == BINARY_OP_GTE ||
        binary_op->op_type == BINARY_OP_REGEX_MATCH || binary_op->op_type == BINARY_OP_IN ||
        binary_op->op_type == BINARY_OP_STARTS_WITH || binary_op->op_type == BINARY_OP_ENDS_WITH ||
        binary_op->op_type == BINARY_OP_CONTAINS);
    if (is_cmp) {
        ctx->in_comparison = true;
    }

    /* Chained comparison: openCypher allows `1 < n < 3` meaning
     * `(1 < n) AND (n < 3)`. Bison gives a left-associative tree
     * `((1 < n) < 3)`, which the naïve compile treats as
     * `(true < 3)` and lets every row through. Detect when both this
     * op and its LHS are *ordering* comparisons (<, <=, >, >=) and
     * rewrite to the conjunction.
     *
     * Limited to ordering operators on purpose: a `(a < b) = true`
     * expression is genuinely "compare a boolean result against true",
     * not a chained comparison, and rewriting it would be wrong. */
    bool is_order_cmp = (binary_op->op_type == BINARY_OP_LT ||
                         binary_op->op_type == BINARY_OP_GT ||
                         binary_op->op_type == BINARY_OP_LTE ||
                         binary_op->op_type == BINARY_OP_GTE);
    if (is_order_cmp && binary_op->left &&
        binary_op->left->type == AST_NODE_BINARY_OP) {
        cypher_binary_op *lop = (cypher_binary_op *)binary_op->left;
        bool lhs_is_order_cmp = (lop->op_type == BINARY_OP_LT ||
                                  lop->op_type == BINARY_OP_GT ||
                                  lop->op_type == BINARY_OP_LTE ||
                                  lop->op_type == BINARY_OP_GTE);
        if (lhs_is_order_cmp) {
            /* Build SQL: (<lhs>) AND (<lop.right> <cur op> <rhs>) */
            append_sql(ctx, "((");
            if (transform_expression(ctx, binary_op->left) < 0) return -1;
            append_sql(ctx, ") AND (");
            if (transform_expression(ctx, lop->right) < 0) return -1;
            const char *op_sql = NULL;
            switch (binary_op->op_type) {
                case BINARY_OP_EQ: op_sql = "="; break;
                case BINARY_OP_NEQ: op_sql = "<>"; break;
                case BINARY_OP_LT: op_sql = "<"; break;
                case BINARY_OP_GT: op_sql = ">"; break;
                case BINARY_OP_LTE: op_sql = "<="; break;
                case BINARY_OP_GTE: op_sql = ">="; break;
                default: op_sql = "="; break;
            }
            append_sql(ctx, " %s ", op_sql);
            if (transform_expression(ctx, binary_op->right) < 0) return -1;
            append_sql(ctx, "))");
            ctx->in_comparison = was_in_comparison;
            return 0;
        }
    }

    /* Handle list/map equality with Cypher three-valued semantics via the
     * _gql_eq() UDF. Triggered only when at least one operand is a literal
     * list/map (which is when the existing JSON-string equality breaks down
     * due to embedded nulls). */
    if ((binary_op->op_type == BINARY_OP_EQ || binary_op->op_type == BINARY_OP_NEQ) &&
        (binary_op->left->type == AST_NODE_LIST || binary_op->left->type == AST_NODE_MAP ||
         binary_op->right->type == AST_NODE_LIST || binary_op->right->type == AST_NODE_MAP)) {
        if (binary_op->op_type == BINARY_OP_NEQ) append_sql(ctx, "(NOT _gql_eq(");
        else                                    append_sql(ctx, "_gql_eq(");
        if (transform_expression(ctx, binary_op->left) < 0) return -1;
        append_sql(ctx, ", ");
        if (transform_expression(ctx, binary_op->right) < 0) return -1;
        append_sql(ctx, ")");
        if (binary_op->op_type == BINARY_OP_NEQ) append_sql(ctx, ")");
        ctx->in_comparison = was_in_comparison;
        return 0;
    }

    /* Handle boolean logical operators: wrap operands in _gql_bool() to coerce
     * string 'true'/'false' (from boolean property access) to int 1/0 so SQL
     * AND / OR / XOR behave correctly. */
    if (binary_op->op_type == BINARY_OP_AND ||
        binary_op->op_type == BINARY_OP_OR ||
        binary_op->op_type == BINARY_OP_XOR) {
        append_sql(ctx, "(_gql_bool(");
        if (transform_expression(ctx, binary_op->left) < 0) return -1;
        append_sql(ctx, ") %s _gql_bool(",
                   binary_op->op_type == BINARY_OP_AND ? "AND" :
                   binary_op->op_type == BINARY_OP_OR  ? "OR"  : "<>");
        if (transform_expression(ctx, binary_op->right) < 0) return -1;
        append_sql(ctx, "))");
        ctx->in_comparison = was_in_comparison;
        return 0;
    }

    /* For =/<> where one operand is a logical operator result (NOT, AND,
     * OR, XOR) and the other might be a 'true'/'false' string, coerce
     * both sides via _gql_bool so comparison works regardless of whether
     * a side flows as int 1/0 or text. */
    if ((binary_op->op_type == BINARY_OP_EQ || binary_op->op_type == BINARY_OP_NEQ)) {
        bool l_is_logic = binary_op->left && (
            binary_op->left->type == AST_NODE_NOT_EXPR ||
            (binary_op->left->type == AST_NODE_BINARY_OP &&
             ((cypher_binary_op*)binary_op->left)->op_type >= BINARY_OP_AND &&
             ((cypher_binary_op*)binary_op->left)->op_type <= BINARY_OP_XOR));
        bool r_is_logic = binary_op->right && (
            binary_op->right->type == AST_NODE_NOT_EXPR ||
            (binary_op->right->type == AST_NODE_BINARY_OP &&
             ((cypher_binary_op*)binary_op->right)->op_type >= BINARY_OP_AND &&
             ((cypher_binary_op*)binary_op->right)->op_type <= BINARY_OP_XOR));
        if (l_is_logic || r_is_logic) {
            append_sql(ctx, "(_gql_bool(");
            if (transform_expression(ctx, binary_op->left) < 0) return -1;
            append_sql(ctx, ") %s _gql_bool(",
                       binary_op->op_type == BINARY_OP_EQ ? "=" : "<>");
            if (transform_expression(ctx, binary_op->right) < 0) return -1;
            append_sql(ctx, "))");
            ctx->in_comparison = was_in_comparison;
            return 0;
        }
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
        /* Right side must be a list. Reject non-list literals at compile
         * time (TCK list/[42]). Parameters and identifiers pass through. */
        if (binary_op->right && binary_op->right->type == AST_NODE_LITERAL) {
            cypher_literal *rl = (cypher_literal*)binary_op->right;
            if (rl->literal_type == LITERAL_INTEGER ||
                rl->literal_type == LITERAL_DECIMAL ||
                rl->literal_type == LITERAL_STRING  ||
                rl->literal_type == LITERAL_BOOLEAN) {
                ctx->has_error = true;
                ctx->error_message = strdup(
                    "SyntaxError: InvalidArgumentType: IN operator requires a list on its right side");
                return -1;
            }
        } else if (binary_op->right && binary_op->right->type == AST_NODE_MAP) {
            ctx->has_error = true;
            ctx->error_message = strdup(
                "SyntaxError: InvalidArgumentType: IN operator requires a list on its right side");
            return -1;
        }
        /* Use _gql_in() UDF for proper Cypher three-valued logic:
         *   null IN []         -> false
         *   null IN <nonempty> -> null
         *   x    IN coll       -> true / null (if coll has null) / false */
        append_sql(ctx, "_gql_in(");
        if (transform_expression(ctx, binary_op->left) < 0) return -1;
        append_sql(ctx, ", ");
        if (binary_op->right->type == AST_NODE_LIST) {
            /* Literal list: wrap with json_array() */
            cypher_list *list = (cypher_list*)binary_op->right;
            append_sql(ctx, "json_array(");
            if (list->items) {
                for (int i = 0; i < list->items->count; i++) {
                    if (i > 0) append_sql(ctx, ", ");
                    if (transform_expression(ctx, list->items->items[i]) < 0) return -1;
                }
            }
            append_sql(ctx, ")");
        } else {
            if (transform_expression(ctx, binary_op->right) < 0) return -1;
        }
        append_sql(ctx, ")");
        ctx->in_comparison = was_in_comparison;
        return 0;
    }

    /* Handle STARTS WITH operator — case-sensitive prefix match.
     * Spec returns NULL when either operand is not a string. Use the
     * typeof() guard to gate the substr comparison; otherwise NULL. */
    if (binary_op->op_type == BINARY_OP_STARTS_WITH) {
        append_sql(ctx, "(CASE WHEN typeof(");
        if (transform_expression(ctx, binary_op->left) < 0) return -1;
        append_sql(ctx, ") = 'text' AND typeof(");
        if (transform_expression(ctx, binary_op->right) < 0) return -1;
        append_sql(ctx, ") = 'text' THEN substr(");
        if (transform_expression(ctx, binary_op->left) < 0) return -1;
        append_sql(ctx, ", 1, length(");
        if (transform_expression(ctx, binary_op->right) < 0) return -1;
        append_sql(ctx, ")) = ");
        if (transform_expression(ctx, binary_op->right) < 0) return -1;
        append_sql(ctx, " ELSE NULL END)");
        ctx->in_comparison = was_in_comparison;
        return 0;
    }

    /* Handle ENDS WITH operator — case-sensitive suffix match.
     * Same NULL-on-non-string semantics as STARTS WITH. */
    if (binary_op->op_type == BINARY_OP_ENDS_WITH) {
        append_sql(ctx, "(CASE WHEN typeof(");
        if (transform_expression(ctx, binary_op->left) < 0) return -1;
        append_sql(ctx, ") = 'text' AND typeof(");
        if (transform_expression(ctx, binary_op->right) < 0) return -1;
        append_sql(ctx, ") = 'text' THEN substr(");
        if (transform_expression(ctx, binary_op->left) < 0) return -1;
        append_sql(ctx, ", length(");
        if (transform_expression(ctx, binary_op->left) < 0) return -1;
        append_sql(ctx, ") - length(");
        if (transform_expression(ctx, binary_op->right) < 0) return -1;
        append_sql(ctx, ") + 1) = ");
        if (transform_expression(ctx, binary_op->right) < 0) return -1;
        append_sql(ctx, " ELSE NULL END)");
        ctx->in_comparison = was_in_comparison;
        return 0;
    }

    /* Handle CONTAINS operator - case-sensitive substring match. */
    if (binary_op->op_type == BINARY_OP_CONTAINS) {
        append_sql(ctx, "(CASE WHEN typeof(");
        if (transform_expression(ctx, binary_op->left) < 0) return -1;
        append_sql(ctx, ") = 'text' AND typeof(");
        if (transform_expression(ctx, binary_op->right) < 0) return -1;
        append_sql(ctx, ") = 'text' THEN INSTR(");
        if (transform_expression(ctx, binary_op->left) < 0) return -1;
        append_sql(ctx, ", ");
        if (transform_expression(ctx, binary_op->right) < 0) return -1;
        append_sql(ctx, ") > 0 ELSE NULL END)");
        ctx->in_comparison = was_in_comparison;
        return 0;
    }

    /* Add left parenthesis for precedence */
    append_sql(ctx, "(");
    
    /* ADD/SUB: Cypher's `+` and `-` are polymorphic over numbers, strings,
     * lists, durations, and temporal+duration. We can't distinguish these
     * statically, so emit a runtime helper `_gql_dyn_add` / `_gql_dyn_sub`
     * that dispatches on operand types. Fall through to native SQL for
     * the obvious integer-literal-on-both-sides case (avoids overhead for
     * `1 + 2`). */
    /* POW: emit as function call since SQLite lacks the ** operator. */
    if (binary_op->op_type == BINARY_OP_POW) {
        append_sql(ctx, "power(");
        if (transform_expression(ctx, binary_op->left) < 0) return -1;
        append_sql(ctx, ", ");
        if (transform_expression(ctx, binary_op->right) < 0) return -1;
        append_sql(ctx, "))");
        ctx->in_comparison = was_in_comparison;
        return 0;
    }
    if (binary_op->op_type == BINARY_OP_ADD || binary_op->op_type == BINARY_OP_SUB) {
        bool both_int_lit = false;
        if (binary_op->left->type == AST_NODE_LITERAL && binary_op->right->type == AST_NODE_LITERAL) {
            cypher_literal *ll = (cypher_literal *)binary_op->left;
            cypher_literal *rl = (cypher_literal *)binary_op->right;
            if (ll->literal_type == LITERAL_INTEGER && rl->literal_type == LITERAL_INTEGER)
                both_int_lit = true;
        }
        if (!both_int_lit) {
            append_sql(ctx, binary_op->op_type == BINARY_OP_ADD ? "_gql_dyn_add(" : "_gql_dyn_sub(");
            if (transform_expression(ctx, binary_op->left) < 0) return -1;
            append_sql(ctx, ", ");
            if (transform_expression(ctx, binary_op->right) < 0) return -1;
            append_sql(ctx, "))");  /* closes the helper + the outer paren */
            ctx->in_comparison = was_in_comparison;
            return 0;
        }
    }

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
             * Check if left operand is a string literal or a string concat chain.
             * Note: list concatenation ([1,2] + [3,4]) is not yet supported. */
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
        case BINARY_OP_POW:
            /* Handled above via early return — should never reach here. */
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

    /* Handle nested property access: n.metadata.name → json_extract(n.metadata_sql, '$.name') */
    if (prop->expr->type == AST_NODE_PROPERTY) {
        append_sql(ctx, "json_extract(");
        if (transform_property_access(ctx, (cypher_property*)prop->expr) < 0) return -1;
        append_sql(ctx, ", '$.");
        { char *esc_prop = escape_sql_string(prop->property_name);
          append_sql(ctx, "%s", esc_prop ? esc_prop : prop->property_name);
          free(esc_prop); }
        append_sql(ctx, "')");
        return 0;
    }

    /* Handle subscript base: list[0].name → json_extract(list_subscript_sql, '$.name') */
    if (prop->expr->type == AST_NODE_SUBSCRIPT) {
        append_sql(ctx, "json_extract(");
        if (transform_expression(ctx, prop->expr) < 0) return -1;
        append_sql(ctx, ", '$.");
        { char *esc_prop = escape_sql_string(prop->property_name);
          append_sql(ctx, "%s", esc_prop ? esc_prop : prop->property_name);
          free(esc_prop); }
        append_sql(ctx, "')");
        return 0;
    }

    /* Handle function call base: startNode(r).name, endNode(r).name */
    if (prop->expr->type == AST_NODE_FUNCTION_CALL) {
        cypher_function_call *func = (cypher_function_call*)prop->expr;
        if (func->function_name &&
            (strcasecmp(func->function_name, "startNode") == 0 ||
             strcasecmp(func->function_name, "endNode") == 0)) {
            /* Generate property lookup using the node ID from startNode/endNode.
             * The function generates (SELECT source_id/target_id FROM edges WHERE id = alias.id)
             * so we use that as the node_id in the property lookup. */
            const char *gprefix = "";
            append_sql(ctx, "(SELECT COALESCE(");
            append_sql(ctx, "(SELECT npt.value FROM %snode_props_text npt JOIN %sproperty_keys pk ON npt.key_id = pk.id WHERE npt.node_id = ", gprefix, gprefix);
            if (transform_expression(ctx, prop->expr) < 0) return -1;
            append_sql(ctx, " AND pk.key = ");
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, "), ");
            append_sql(ctx, "(SELECT npi.value FROM %snode_props_int npi JOIN %sproperty_keys pk ON npi.key_id = pk.id WHERE npi.node_id = ", gprefix, gprefix);
            if (transform_expression(ctx, prop->expr) < 0) return -1;
            append_sql(ctx, " AND pk.key = ");
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, "), ");
            append_sql(ctx, "(SELECT npr.value FROM %snode_props_real npr JOIN %sproperty_keys pk ON npr.key_id = pk.id WHERE npr.node_id = ", gprefix, gprefix);
            if (transform_expression(ctx, prop->expr) < 0) return -1;
            append_sql(ctx, " AND pk.key = ");
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, "), ");
            append_sql(ctx, "(SELECT CASE WHEN npb.value THEN 'true' ELSE 'false' END FROM %snode_props_bool npb JOIN %sproperty_keys pk ON npb.key_id = pk.id WHERE npb.node_id = ", gprefix, gprefix);
            if (transform_expression(ctx, prop->expr) < 0) return -1;
            append_sql(ctx, " AND pk.key = ");
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, "), ");
            append_sql(ctx, "(SELECT npj.value FROM %snode_props_json npj JOIN %sproperty_keys pk ON npj.key_id = pk.id WHERE npj.node_id = ", gprefix, gprefix);
            if (transform_expression(ctx, prop->expr) < 0) return -1;
            append_sql(ctx, " AND pk.key = ");
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, ")))");
            return 0;
        }
        ctx->has_error = true;
        ctx->error_message = strdup("Property access on function call not supported for this function");
        return -1;
    }

    /* Get the base expression (should be an identifier) */
    if (prop->expr->type != AST_NODE_IDENTIFIER) {
        ctx->has_error = true;
        ctx->error_message = strdup("Complex property access not yet supported");
        return -1;
    }

    cypher_identifier *id = (cypher_identifier*)prop->expr;
    const char *alias = transform_var_get_alias(ctx->var_ctx, id->name);
    if (!alias) {
        ctx->has_error = true;
        char error[256];
        snprintf(error, sizeof(error), "Unknown variable in property access: %s", id->name);
        ctx->error_message = strdup(error);
        return -1;
    }

    /* Check if alias IS the id value (projected or post-WITH node/edge) */
    bool is_projected = transform_var_is_projected(ctx->var_ctx, id->name);
    bool alias_is_id = transform_var_alias_is_id(ctx->var_ctx, id->name);
    bool skip_id_suffix = is_projected || alias_is_id;
    bool is_edge = transform_var_is_edge(ctx->var_ctx, id->name);

    /* For UNWIND variables holding JSON values (from json_each), use json_extract
     * instead of property table lookup. Detect by checking if the alias source
     * references an UNWIND CTE value column. */
    if (is_projected && alias && strstr(alias, "_unwind_") && strstr(alias, ".value")) {
        { char *esc_prop = escape_sql_string(prop->property_name);
          append_sql(ctx, "json_extract(%s, '$.%s')", alias, esc_prop ? esc_prop : prop->property_name);
          free(esc_prop); }
        return 0;
    }

    /* Projected variables that are NOT nodes/edges (i.e., scalar/JSON values
     * from a WITH-expression like `duration.between(...) AS dur`, or a
     * temporal string like `date(...)`) — try json_extract first (works for
     * JSON objects like durations); fall back to _gql_temporal_field which
     * parses temporal strings for `.year`/`.hour`/etc. */
    if (is_projected && !is_edge && !alias_is_id) {
        /* json_extract raises 'malformed JSON' on non-JSON inputs (e.g. a
         * temporal string like '1984-10-11'), so guard with json_valid. */
        { char *esc_prop = escape_sql_string(prop->property_name);
          append_sql(ctx, "CASE WHEN json_valid(%s) THEN json_extract(%s, '$.%s')"
                          " ELSE _gql_temporal_field(%s, '%s') END",
                     alias, alias, esc_prop ? esc_prop : prop->property_name,
                     alias, esc_prop ? esc_prop : prop->property_name);
          free(esc_prop); }
        return 0;
    }

    /* Multi-graph support: get graph prefix for property table references */
    const char *graph = transform_var_get_graph(ctx->var_ctx, id->name);
    const char *gprefix = "";
    char gprefix_buf[64] = "";
    if (graph && graph[0] != '\0') {
        snprintf(gprefix_buf, sizeof(gprefix_buf), "%s.", graph);
        gprefix = gprefix_buf;
    }

    /* Generate property access query using our actual schema */
    /* We need to check multiple property tables based on type */

    if (is_edge) {
        /* Edge property access - use edge_props_* tables */
        const char *id_suffix = skip_id_suffix ? "" : ".id";
        if (ctx->in_comparison) {
            append_sql(ctx, "(SELECT COALESCE(");
            append_sql(ctx, "(SELECT ept.value FROM %sedge_props_text ept JOIN %sproperty_keys pk ON ept.key_id = pk.id WHERE ept.edge_id = %s%s AND pk.key = ", gprefix, gprefix, alias, id_suffix);
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, "), ");
            append_sql(ctx, "(SELECT epi.value FROM %sedge_props_int epi JOIN %sproperty_keys pk ON epi.key_id = pk.id WHERE epi.edge_id = %s%s AND pk.key = ", gprefix, gprefix, alias, id_suffix);
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, "), ");
            append_sql(ctx, "(SELECT epr.value FROM %sedge_props_real epr JOIN %sproperty_keys pk ON epr.key_id = pk.id WHERE epr.edge_id = %s%s AND pk.key = ", gprefix, gprefix, alias, id_suffix);
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, "), ");
            append_sql(ctx, "(SELECT CAST(epb.value AS INTEGER) FROM %sedge_props_bool epb JOIN %sproperty_keys pk ON epb.key_id = pk.id WHERE epb.edge_id = %s%s AND pk.key = ", gprefix, gprefix, alias, id_suffix);
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, "), ");
            append_sql(ctx, "(SELECT epj.value FROM %sedge_props_json epj JOIN %sproperty_keys pk ON epj.key_id = pk.id WHERE epj.edge_id = %s%s AND pk.key = ", gprefix, gprefix, alias, id_suffix);
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, ")))");
        } else {
            append_sql(ctx, "(SELECT COALESCE(");
            append_sql(ctx, "(SELECT ept.value FROM %sedge_props_text ept JOIN %sproperty_keys pk ON ept.key_id = pk.id WHERE ept.edge_id = %s%s AND pk.key = ", gprefix, gprefix, alias, id_suffix);
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, "), ");
            append_sql(ctx, "(SELECT CAST(epi.value AS TEXT) FROM %sedge_props_int epi JOIN %sproperty_keys pk ON epi.key_id = pk.id WHERE epi.edge_id = %s%s AND pk.key = ", gprefix, gprefix, alias, id_suffix);
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, "), ");
            append_sql(ctx, "(SELECT CAST(epr.value AS TEXT) FROM %sedge_props_real epr JOIN %sproperty_keys pk ON epr.key_id = pk.id WHERE epr.edge_id = %s%s AND pk.key = ", gprefix, gprefix, alias, id_suffix);
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, "), ");
            append_sql(ctx, "(SELECT CASE WHEN epb.value THEN 'true' ELSE 'false' END FROM %sedge_props_bool epb JOIN %sproperty_keys pk ON epb.key_id = pk.id WHERE epb.edge_id = %s%s AND pk.key = ", gprefix, gprefix, alias, id_suffix);
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, "), ");
            append_sql(ctx, "(SELECT epj.value FROM %sedge_props_json epj JOIN %sproperty_keys pk ON epj.key_id = pk.id WHERE epj.edge_id = %s%s AND pk.key = ", gprefix, gprefix, alias, id_suffix);
            append_string_literal(ctx, prop->property_name);
            append_sql(ctx, ")))");
        }
    } else if (ctx->in_comparison) {
        /* Node property access for comparisons - preserve proper types */
        append_sql(ctx, "(SELECT COALESCE(");
        /* Text properties (both numeric and non-numeric strings) */
        append_sql(ctx, "(SELECT npt.value FROM %snode_props_text npt JOIN %sproperty_keys pk ON npt.key_id = pk.id WHERE npt.node_id = %s%s AND pk.key = ",
                   gprefix, gprefix, alias, skip_id_suffix ? "" : ".id");
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");
        /* Integer properties */
        append_sql(ctx, "(SELECT npi.value FROM %snode_props_int npi JOIN %sproperty_keys pk ON npi.key_id = pk.id WHERE npi.node_id = %s%s AND pk.key = ",
                   gprefix, gprefix, alias, skip_id_suffix ? "" : ".id");
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");
        /* Real properties */
        append_sql(ctx, "(SELECT npr.value FROM %snode_props_real npr JOIN %sproperty_keys pk ON npr.key_id = pk.id WHERE npr.node_id = %s%s AND pk.key = ",
                   gprefix, gprefix, alias, skip_id_suffix ? "" : ".id");
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");
        /* Boolean properties (cast to integer for comparison) */
        append_sql(ctx, "(SELECT CAST(npb.value AS INTEGER) FROM %snode_props_bool npb JOIN %sproperty_keys pk ON npb.key_id = pk.id WHERE npb.node_id = %s%s AND pk.key = ",
                   gprefix, gprefix, alias, skip_id_suffix ? "" : ".id");
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");
        /* JSON properties */
        append_sql(ctx, "(SELECT npj.value FROM %snode_props_json npj JOIN %sproperty_keys pk ON npj.key_id = pk.id WHERE npj.node_id = %s%s AND pk.key = ",
                   gprefix, gprefix, alias, skip_id_suffix ? "" : ".id");
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, ")))");
    } else {
        /* Node property access for RETURN clauses - convert everything to text */
        append_sql(ctx, "(SELECT COALESCE(");
        append_sql(ctx, "(SELECT npt.value FROM %snode_props_text npt JOIN %sproperty_keys pk ON npt.key_id = pk.id WHERE npt.node_id = %s%s AND pk.key = ",
                   gprefix, gprefix, alias, skip_id_suffix ? "" : ".id");
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");
        append_sql(ctx, "(SELECT npi.value FROM %snode_props_int npi JOIN %sproperty_keys pk ON npi.key_id = pk.id WHERE npi.node_id = %s%s AND pk.key = ",
                   gprefix, gprefix, alias, skip_id_suffix ? "" : ".id");
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");
        append_sql(ctx, "(SELECT npr.value FROM %snode_props_real npr JOIN %sproperty_keys pk ON npr.key_id = pk.id WHERE npr.node_id = %s%s AND pk.key = ",
                   gprefix, gprefix, alias, skip_id_suffix ? "" : ".id");
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");
        append_sql(ctx, "(SELECT CASE WHEN npb.value THEN 'true' ELSE 'false' END FROM %snode_props_bool npb JOIN %sproperty_keys pk ON npb.key_id = pk.id WHERE npb.node_id = %s%s AND pk.key = ",
                   gprefix, gprefix, alias, skip_id_suffix ? "" : ".id");
        append_string_literal(ctx, prop->property_name);
        append_sql(ctx, "), ");
        /* JSON properties (already TEXT) */
        append_sql(ctx, "(SELECT npj.value FROM %snode_props_json npj JOIN %sproperty_keys pk ON npj.key_id = pk.id WHERE npj.node_id = %s%s AND pk.key = ",
                   gprefix, gprefix, alias, skip_id_suffix ? "" : ".id");
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
