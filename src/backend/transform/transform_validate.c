/* transform_validate.c - Compile-time argument-type validation.
 *
 * Phase A of [[GQLITE-T-0230]]: catch the openCypher compile-time errors
 * the grammar would otherwise let through. Today it handles:
 *
 *   - Boolean operators: NOT <e>, <e> AND/OR/XOR <e> reject non-boolean
 *     literal operands (and non-null literals).
 *
 * Variables, function calls, and other dynamically-typed expressions are
 * NOT validated — we only flag operands whose type is statically obvious
 * from the literal. That avoids false positives on cases like
 * `WITH a.x AS b RETURN NOT b` where `b`'s type isn't known here.
 *
 * Validation walks the whole AST. On the first violation it sets
 * `*error_message` (caller frees) and returns -1.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transform/transform_validate.h"
#include "parser/cypher_ast.h"

/* The error string we emit matches the openCypher TCK convention:
 *   "SyntaxError: InvalidArgumentType: <detail>"
 * The extension's existing classifier (extension.c line ~346) already
 * downgrades messages containing "syntax error" / "Line " into
 * code=PARSE_ERROR; we keep the SyntaxError prefix so users see a
 * stable category. */
#define VALIDATE_ERR_FMT "SyntaxError: InvalidArgumentType: %s"

/* ---- helpers --------------------------------------------------------- */

static void set_error(char **out, const char *fmt, ...)
{
    if (!out || *out) {
        /* Already populated — preserve the first error. */
        return;
    }
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    *out = strdup(buf);
}

static bool is_literal_of_type(const ast_node *e, int wanted_type)
{
    if (!e || e->type != AST_NODE_LITERAL) return false;
    const cypher_literal *lit = (const cypher_literal *)e;
    return (int)lit->literal_type == wanted_type;
}

static const char *literal_type_name(const ast_node *e)
{
    if (!e || e->type != AST_NODE_LITERAL) return "expression";
    const cypher_literal *lit = (const cypher_literal *)e;
    switch (lit->literal_type) {
        case LITERAL_INTEGER:  return "Integer";
        case LITERAL_DECIMAL:  return "Float";
        case LITERAL_STRING:   return "String";
        case LITERAL_BOOLEAN:  return "Boolean";
        case LITERAL_NULL:     return "Null";
    }
    return "Unknown";
}

/* A literal is "definitely not boolean" if it's a non-NULL literal of any
 * other type. NULL is acceptable because openCypher's three-valued logic
 * allows it everywhere. */
static bool literal_is_non_boolean(const ast_node *e)
{
    if (!e || e->type != AST_NODE_LITERAL) return false;
    const cypher_literal *lit = (const cypher_literal *)e;
    switch (lit->literal_type) {
        case LITERAL_BOOLEAN:
        case LITERAL_NULL:
            return false;
        case LITERAL_INTEGER:
        case LITERAL_DECIMAL:
        case LITERAL_STRING:
            return true;
    }
    return false;
}

/* ---- recursive AST walk --------------------------------------------- */

static int validate_expr(ast_node *expr, char **error_message);

static int validate_not_expr(cypher_not_expr *not_expr, char **error_message)
{
    if (!not_expr || !not_expr->expr) return 0;
    if (literal_is_non_boolean(not_expr->expr)) {
        set_error(error_message,
                  VALIDATE_ERR_FMT,
                  "NOT operand must be Boolean or Null, got literal of type "
                  "\"the operand\" — actual literal type follows in the AST");
        /* Rewrite with the real type for a tighter message. */
        if (*error_message) free(*error_message);
        *error_message = NULL;
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "SyntaxError: InvalidArgumentType: Type mismatch: expected Boolean but was %s",
                 literal_type_name(not_expr->expr));
        *error_message = strdup(buf);
        return -1;
    }
    return validate_expr(not_expr->expr, error_message);
}

static int validate_binary_op(cypher_binary_op *bop, char **error_message)
{
    if (!bop) return 0;

    /* Boolean operators: both operands must be Boolean (or Null) when known
     * at compile time. */
    bool is_bool_op = (bop->op_type == BINARY_OP_AND ||
                       bop->op_type == BINARY_OP_OR  ||
                       bop->op_type == BINARY_OP_XOR);
    if (is_bool_op) {
        if (literal_is_non_boolean(bop->left)) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "SyntaxError: InvalidArgumentType: Type mismatch: expected Boolean but was %s",
                     literal_type_name(bop->left));
            set_error(error_message, "%s", buf);
            return -1;
        }
        if (literal_is_non_boolean(bop->right)) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "SyntaxError: InvalidArgumentType: Type mismatch: expected Boolean but was %s",
                     literal_type_name(bop->right));
            set_error(error_message, "%s", buf);
            return -1;
        }
    }

    if (validate_expr(bop->left, error_message) < 0) return -1;
    if (validate_expr(bop->right, error_message) < 0) return -1;
    return 0;
}

static int validate_expr(ast_node *expr, char **error_message)
{
    if (!expr) return 0;

    switch (expr->type) {
        case AST_NODE_NOT_EXPR:
            return validate_not_expr((cypher_not_expr *)expr, error_message);
        case AST_NODE_BINARY_OP:
            return validate_binary_op((cypher_binary_op *)expr, error_message);
        case AST_NODE_FUNCTION_CALL: {
            cypher_function_call *func = (cypher_function_call *)expr;
            if (func->args) {
                for (int i = 0; i < func->args->count; i++) {
                    if (validate_expr(func->args->items[i], error_message) < 0)
                        return -1;
                }
            }
            return 0;
        }
        case AST_NODE_LIST: {
            cypher_list *lst = (cypher_list *)expr;
            if (lst->items) {
                for (int i = 0; i < lst->items->count; i++) {
                    if (validate_expr(lst->items->items[i], error_message) < 0)
                        return -1;
                }
            }
            return 0;
        }
        case AST_NODE_NULL_CHECK: {
            cypher_null_check *nc = (cypher_null_check *)expr;
            return validate_expr(nc->expr, error_message);
        }
        /* Identifiers, literals, parameters, property access etc. have no
         * sub-expressions to validate at this layer. */
        default:
            return 0;
    }
}

static int validate_return_clause(cypher_return *ret, char **error_message)
{
    if (!ret || !ret->items) return 0;
    for (int i = 0; i < ret->items->count; i++) {
        cypher_return_item *item = (cypher_return_item *)ret->items->items[i];
        if (item && item->expr) {
            if (validate_expr(item->expr, error_message) < 0) return -1;
        }
    }
    return 0;
}

static int validate_with_clause(cypher_with *with, char **error_message)
{
    if (!with) return 0;
    if (with->items) {
        for (int i = 0; i < with->items->count; i++) {
            cypher_return_item *item = (cypher_return_item *)with->items->items[i];
            if (item && item->expr) {
                if (validate_expr(item->expr, error_message) < 0) return -1;
            }
        }
    }
    if (with->where) {
        if (validate_expr(with->where, error_message) < 0) return -1;
    }
    return 0;
}

static int validate_match_clause(cypher_match *match, char **error_message)
{
    if (!match) return 0;
    if (match->where) {
        if (validate_expr(match->where, error_message) < 0) return -1;
    }
    return 0;
}

int transform_validate_query(cypher_query *query, char **error_message)
{
    if (!query || !query->clauses) return 0;
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        if (!clause) continue;
        switch (clause->type) {
            case AST_NODE_RETURN:
                if (validate_return_clause((cypher_return *)clause, error_message) < 0)
                    return -1;
                break;
            case AST_NODE_WITH:
                if (validate_with_clause((cypher_with *)clause, error_message) < 0)
                    return -1;
                break;
            case AST_NODE_MATCH:
                if (validate_match_clause((cypher_match *)clause, error_message) < 0)
                    return -1;
                break;
            default:
                break;
        }
    }
    return 0;
}
