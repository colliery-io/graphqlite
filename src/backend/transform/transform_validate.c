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
 * allows it everywhere. List/map literals are also rejected. */
static bool literal_is_non_boolean(const ast_node *e)
{
    if (!e) return false;
    /* Composite literals — definitely not boolean. */
    if (e->type == AST_NODE_LIST || e->type == AST_NODE_MAP) return true;
    if (e->type != AST_NODE_LITERAL) return false;
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

/* Type name for a literal-or-composite node (returns "Integer", "List", etc.) */
static const char *expr_type_name(const ast_node *e)
{
    if (!e) return "expression";
    if (e->type == AST_NODE_LIST) return "List";
    if (e->type == AST_NODE_MAP)  return "Map";
    return literal_type_name(e);
}

/* ---- variable-type tracker ------------------------------------------ */

/* A tiny per-query map of variable name → known literal type. Populated
 * from WITH/UNWIND clauses that bind a variable to a literal expression
 * (`WITH 123 AS x`, `WITH [1,2] AS xs`, `UNWIND [1,2] AS x` — the unwound
 * element's type is List elem type which we approximate as Integer/etc).
 * Only used for negative-test validation; missing entries silently skip. */

typedef enum {
    VTYPE_UNKNOWN = 0,
    VTYPE_INTEGER,
    VTYPE_DECIMAL,
    VTYPE_STRING,
    VTYPE_BOOLEAN,
    VTYPE_NULL,
    VTYPE_LIST,
    VTYPE_MAP,
    VTYPE_NODE,
    VTYPE_EDGE,
    VTYPE_PATH,
} var_type;

typedef struct {
    char *name;
    var_type type;
} var_type_binding;

typedef struct {
    var_type_binding *items;
    int count;
    int cap;
} var_type_ctx;

static void vctx_init(var_type_ctx *v) { v->items = NULL; v->count = 0; v->cap = 0; }

static void vctx_free(var_type_ctx *v) {
    for (int i = 0; i < v->count; i++) free(v->items[i].name);
    free(v->items);
    v->items = NULL; v->count = 0; v->cap = 0;
}

static void vctx_register(var_type_ctx *v, const char *name, var_type t) {
    if (!name) return;
    for (int i = 0; i < v->count; i++) {
        if (strcmp(v->items[i].name, name) == 0) {
            v->items[i].type = t;  /* rebind */
            return;
        }
    }
    if (v->count >= v->cap) {
        v->cap = v->cap ? v->cap * 2 : 8;
        v->items = realloc(v->items, v->cap * sizeof(var_type_binding));
    }
    v->items[v->count].name = strdup(name);
    v->items[v->count].type = t;
    v->count++;
}

static var_type vctx_lookup(const var_type_ctx *v, const char *name) {
    if (!name) return VTYPE_UNKNOWN;
    for (int i = 0; i < v->count; i++) {
        if (strcmp(v->items[i].name, name) == 0) return v->items[i].type;
    }
    return VTYPE_UNKNOWN;
}

static var_type type_of_literal_expr(const ast_node *e) {
    if (!e) return VTYPE_UNKNOWN;
    if (e->type == AST_NODE_LIST) return VTYPE_LIST;
    if (e->type == AST_NODE_MAP)  return VTYPE_MAP;
    if (e->type != AST_NODE_LITERAL) return VTYPE_UNKNOWN;
    const cypher_literal *lit = (const cypher_literal *)e;
    switch (lit->literal_type) {
        case LITERAL_INTEGER: return VTYPE_INTEGER;
        case LITERAL_DECIMAL: return VTYPE_DECIMAL;
        case LITERAL_STRING:  return VTYPE_STRING;
        case LITERAL_BOOLEAN: return VTYPE_BOOLEAN;
        case LITERAL_NULL:    return VTYPE_NULL;
    }
    return VTYPE_UNKNOWN;
}

static const char *var_type_name(var_type t) {
    switch (t) {
        case VTYPE_INTEGER: return "Integer";
        case VTYPE_DECIMAL: return "Float";
        case VTYPE_STRING:  return "String";
        case VTYPE_BOOLEAN: return "Boolean";
        case VTYPE_NULL:    return "Null";
        case VTYPE_LIST:    return "List";
        case VTYPE_MAP:     return "Map";
        case VTYPE_NODE:    return "Node";
        case VTYPE_EDGE:    return "Relationship";
        case VTYPE_PATH:    return "Path";
        case VTYPE_UNKNOWN: return "Unknown";
    }
    return "Unknown";
}

/* ---- recursive AST walk --------------------------------------------- */

static int validate_expr_typed(ast_node *expr, const var_type_ctx *vctx, char **error_message);
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
                 expr_type_name(not_expr->expr));
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
                     expr_type_name(bop->left));
            set_error(error_message, "%s", buf);
            return -1;
        }
        if (literal_is_non_boolean(bop->right)) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "SyntaxError: InvalidArgumentType: Type mismatch: expected Boolean but was %s",
                     expr_type_name(bop->right));
            set_error(error_message, "%s", buf);
            return -1;
        }
    }

    if (validate_expr(bop->left, error_message) < 0) return -1;
    if (validate_expr(bop->right, error_message) < 0) return -1;
    return 0;
}

/* Returns true if `e` is a literal whose type is incompatible with the
 * destination kind (used by conversion-function validation). */
static bool literal_incompatible_with(const ast_node *e, const char *fname)
{
    if (!e || e->type != AST_NODE_LITERAL) return false;
    const cypher_literal *lit = (const cypher_literal *)e;
    /* Nulls are always acceptable (openCypher 3VL). */
    if (lit->literal_type == LITERAL_NULL) return false;
    if (strcasecmp(fname, "toBoolean") == 0) {
        /* toBoolean accepts Boolean, String, Null. Rejects Integer, Float. */
        return lit->literal_type == LITERAL_INTEGER ||
               lit->literal_type == LITERAL_DECIMAL;
    }
    if (strcasecmp(fname, "toFloat") == 0 ||
        strcasecmp(fname, "toInteger") == 0) {
        /* toFloat/toInteger accept Integer, Float, String, Null. Reject Boolean. */
        return lit->literal_type == LITERAL_BOOLEAN;
    }
    /* toString accepts all primitives; nothing to reject at literal level. */
    return false;
}

/* Returns true if `e` is a non-primitive composite literal (list/map). */
static bool is_composite_literal(const ast_node *e)
{
    if (!e) return false;
    return e->type == AST_NODE_LIST || e->type == AST_NODE_MAP;
}

static int validate_conversion_call(cypher_function_call *func, char **error_message)
{
    if (!func || !func->function_name || !func->args || func->args->count == 0) return 0;
    const char *fname = func->function_name;
    /* Only the toX conversion functions are validated here. */
    if (strcasecmp(fname, "toBoolean") != 0 &&
        strcasecmp(fname, "toInteger") != 0 &&
        strcasecmp(fname, "toFloat") != 0 &&
        strcasecmp(fname, "toString") != 0) return 0;

    ast_node *arg = func->args->items[0];
    /* List/map literal arguments fail across all the toX functions. */
    if (is_composite_literal(arg)) {
        char buf[256];
        const char *kind = arg->type == AST_NODE_LIST ? "List" : "Map";
        snprintf(buf, sizeof(buf),
                 "TypeError: InvalidArgumentValue: %s() does not accept argument of type %s",
                 fname, kind);
        set_error(error_message, "%s", buf);
        return -1;
    }
    if (literal_incompatible_with(arg, fname)) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "TypeError: InvalidArgumentValue: %s() does not accept argument of type %s",
                 fname, literal_type_name(arg));
        set_error(error_message, "%s", buf);
        return -1;
    }
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
            if (validate_conversion_call(func, error_message) < 0) return -1;
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

static int validate_property_access(cypher_property *prop, const var_type_ctx *vctx,
                                     char **error_message)
{
    if (!prop || !prop->expr) return 0;
    if (prop->expr->type != AST_NODE_IDENTIFIER) return 0;
    const cypher_identifier *id = (const cypher_identifier *)prop->expr;
    var_type t = vctx_lookup(vctx, id->name);
    /* Only reject when the binding's type is statically known AND it isn't a
     * Map / Node / Relationship (those allow property access). */
    if (t == VTYPE_INTEGER || t == VTYPE_DECIMAL || t == VTYPE_STRING ||
        t == VTYPE_BOOLEAN || t == VTYPE_LIST) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "TypeError: InvalidArgumentType: Cannot access property '%s' on %s",
                 prop->property_name ? prop->property_name : "?",
                 var_type_name(t));
        set_error(error_message, "%s", buf);
        return -1;
    }
    return 0;
}

static int validate_subscript(cypher_subscript *sub, const var_type_ctx *vctx,
                              char **error_message)
{
    if (!sub || !sub->expr) return 0;
    if (sub->expr->type != AST_NODE_IDENTIFIER) return 0;
    const cypher_identifier *id = (const cypher_identifier *)sub->expr;
    var_type t = vctx_lookup(vctx, id->name);
    /* Subscript is valid on List, Map, Null. Reject Integer / Decimal /
     * Boolean / String (openCypher TCK does not allow string subscripting). */
    if (t == VTYPE_INTEGER || t == VTYPE_DECIMAL || t == VTYPE_BOOLEAN || t == VTYPE_STRING) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "TypeError: InvalidArgumentType: Cannot subscript value of type %s",
                 var_type_name(t));
        set_error(error_message, "%s", buf);
        return -1;
    }
    /* Index-type validation depends on the kind of the indexed value:
     *   - List:    index must be Integer
     *   - Map:     index must be String
     *   - Null:    passthrough (returns null per spec)
     *   - Unknown: skip (we can't statically validate parameters etc.)
     * Slices use slice_start/slice_end; only the index path applies here. */
    if (!sub->is_slice && sub->index) {
        var_type idx_t = VTYPE_UNKNOWN;
        if (sub->index->type == AST_NODE_IDENTIFIER) {
            idx_t = vctx_lookup(vctx, ((const cypher_identifier *)sub->index)->name);
        } else if (sub->index->type == AST_NODE_LITERAL) {
            cypher_literal *lit = (cypher_literal *)sub->index;
            switch (lit->literal_type) {
                case LITERAL_INTEGER: idx_t = VTYPE_INTEGER; break;
                case LITERAL_DECIMAL: idx_t = VTYPE_DECIMAL; break;
                case LITERAL_BOOLEAN: idx_t = VTYPE_BOOLEAN; break;
                case LITERAL_STRING:  idx_t = VTYPE_STRING; break;
                case LITERAL_NULL:    idx_t = VTYPE_NULL; break;
            }
        } else if (sub->index->type == AST_NODE_LIST) {
            idx_t = VTYPE_LIST;
        } else if (sub->index->type == AST_NODE_MAP) {
            idx_t = VTYPE_MAP;
        }

        /* Nothing to validate if either side is unknown or null. */
        if (idx_t == VTYPE_UNKNOWN || idx_t == VTYPE_NULL) return 0;
        if (t == VTYPE_UNKNOWN || t == VTYPE_NULL) return 0;

        if (t == VTYPE_LIST) {
            if (idx_t != VTYPE_INTEGER) {
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "TypeError: InvalidArgumentType: List index must be Integer, got %s",
                         var_type_name(idx_t));
                set_error(error_message, "%s", buf);
                return -1;
            }
        } else if (t == VTYPE_MAP) {
            if (idx_t != VTYPE_STRING) {
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "TypeError: InvalidArgumentType: Map index must be String, got %s",
                         var_type_name(idx_t));
                set_error(error_message, "%s", buf);
                return -1;
            }
        }
    }
    return 0;
}

static int validate_expr_typed(ast_node *expr, const var_type_ctx *vctx, char **error_message)
{
    if (!expr) return 0;
    if (expr->type == AST_NODE_PROPERTY) {
        if (validate_property_access((cypher_property *)expr, vctx, error_message) < 0) return -1;
    }
    if (expr->type == AST_NODE_SUBSCRIPT) {
        if (validate_subscript((cypher_subscript *)expr, vctx, error_message) < 0) return -1;
    }
    /* Recurse into operands for nested expressions. */
    switch (expr->type) {
        case AST_NODE_NOT_EXPR:
            return validate_expr_typed(((cypher_not_expr *)expr)->expr, vctx, error_message);
        case AST_NODE_BINARY_OP: {
            cypher_binary_op *bop = (cypher_binary_op *)expr;
            if (validate_expr_typed(bop->left, vctx, error_message) < 0) return -1;
            if (validate_expr_typed(bop->right, vctx, error_message) < 0) return -1;
            return 0;
        }
        case AST_NODE_FUNCTION_CALL: {
            cypher_function_call *func = (cypher_function_call *)expr;
            /* length() accepts paths, strings, and lists per the openCypher
             * spec — but not nodes or relationships. Catch the common
             * 'MATCH (n) RETURN length(n)' / 'length(r)' mistakes here. */
            if (func->function_name && strcasecmp(func->function_name, "length") == 0 &&
                func->args && func->args->count == 1) {
                ast_node *a = func->args->items[0];
                if (a && a->type == AST_NODE_IDENTIFIER) {
                    var_type t = vctx_lookup(vctx, ((cypher_identifier *)a)->name);
                    if (t == VTYPE_NODE || t == VTYPE_EDGE) {
                        char buf[200];
                        snprintf(buf, sizeof(buf),
                                 "SyntaxError: InvalidArgumentType: length() does not accept %s arguments",
                                 t == VTYPE_NODE ? "Node" : "Relationship");
                        set_error(error_message, "%s", buf);
                        return -1;
                    }
                }
            }
            if (func->args) {
                for (int i = 0; i < func->args->count; i++) {
                    if (validate_expr_typed(func->args->items[i], vctx, error_message) < 0) return -1;
                }
            }
            return 0;
        }
        case AST_NODE_PROPERTY:
            return validate_expr_typed(((cypher_property *)expr)->expr, vctx, error_message);
        case AST_NODE_SUBSCRIPT: {
            cypher_subscript *sub = (cypher_subscript *)expr;
            if (validate_expr_typed(sub->expr, vctx, error_message) < 0) return -1;
            return validate_expr_typed(sub->index, vctx, error_message);
        }
        case AST_NODE_NULL_CHECK:
            return validate_expr_typed(((cypher_null_check *)expr)->expr, vctx, error_message);
        default:
            return 0;
    }
}

/* SKIP / LIMIT must be a non-negative integer constant or a parameter.
 * Expressions referencing variables (e.g. `LIMIT n.count`) and negative
 * literals are rejected at compile time per the openCypher spec. */
static int validate_skip_limit(ast_node *expr, const char *kw, char **error_message)
{
    if (!expr) return 0;
    if (expr->type == AST_NODE_LITERAL) {
        cypher_literal *lit = (cypher_literal *)expr;
        if (lit->literal_type == LITERAL_INTEGER) {
            if (lit->value.integer < 0) {
                char buf[128];
                snprintf(buf, sizeof(buf),
                         "SyntaxError: NegativeIntegerArgument: %s must be non-negative", kw);
                set_error(error_message, "%s", buf);
                return -1;
            }
            return 0;
        }
        /* Non-integer literal — wrong type. */
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "SyntaxError: InvalidArgumentType: %s expects Integer, got %s",
                 kw, expr_type_name(expr));
        set_error(error_message, "%s", buf);
        return -1;
    }
    if (expr->type == AST_NODE_PARAMETER) return 0;
    /* Identifiers, property access, function calls, etc. — non-constant. */
    char buf[160];
    snprintf(buf, sizeof(buf),
             "SyntaxError: NonConstantExpression: %s must be a constant integer or parameter",
             kw);
    set_error(error_message, "%s", buf);
    return -1;
}

/* Track names bound by MATCH patterns so CREATE can detect re-binding. */
typedef struct {
    const char **names;
    int count;
    int cap;
} name_set;

static void nset_init(name_set *s) { s->names = NULL; s->count = 0; s->cap = 0; }
static void nset_free(name_set *s) { free(s->names); s->names = NULL; s->count = 0; s->cap = 0; }
static bool nset_contains(const name_set *s, const char *name) {
    if (!name) return false;
    for (int i = 0; i < s->count; i++) {
        if (s->names[i] && strcmp(s->names[i], name) == 0) return true;
    }
    return false;
}
static void nset_add(name_set *s, const char *name) {
    if (!name || nset_contains(s, name)) return;
    if (s->count >= s->cap) {
        s->cap = s->cap ? s->cap * 2 : 8;
        s->names = realloc(s->names, s->cap * sizeof(const char *));
    }
    s->names[s->count++] = name;
}

/* Walk a clause's expressions with the given var-type context, also
 * picking up new var bindings from WITH items along the way. */
static int validate_return_clause(cypher_return *ret, const var_type_ctx *vctx,
                                  char **error_message)
{
    if (!ret) return 0;
    if (ret->items) {
        /* Duplicate explicit aliases in RETURN are a ColumnNameConflict. */
        name_set seen; nset_init(&seen);
        for (int i = 0; i < ret->items->count; i++) {
            cypher_return_item *item = (cypher_return_item *)ret->items->items[i];
            if (!item) continue;
            /* Patterns are not valid expressions in RETURN projection
             * (Pattern1 [22]). The parser routes a bare pattern in
             * expression context to AST_NODE_EXISTS_EXPR (pattern-type);
             * Cypher 9 only permits that inside a WHERE / boolean
             * context, not as a RETURN value. */
            if (item->expr &&
                (item->expr->type == AST_NODE_PATH ||
                 (item->expr->type == AST_NODE_EXISTS_EXPR &&
                  ((cypher_exists_expr *)item->expr)->expr_type == EXISTS_TYPE_PATTERN))) {
                set_error(error_message,
                          "SyntaxError: UnexpectedSyntax: a path pattern is not a valid expression in RETURN");
                nset_free(&seen);
                return -1;
            }
            if (item->alias) {
                if (nset_contains(&seen, item->alias)) {
                    set_error(error_message,
                              "SyntaxError: ColumnNameConflict: Multiple result columns with the same name `%s`",
                              item->alias);
                    nset_free(&seen);
                    return -1;
                }
                nset_add(&seen, item->alias);
            }
            if (item->expr) {
                if (validate_expr(item->expr, error_message) < 0) { nset_free(&seen); return -1; }
                if (validate_expr_typed(item->expr, vctx, error_message) < 0) { nset_free(&seen); return -1; }
            }
        }
        nset_free(&seen);
    }
    if (validate_skip_limit(ret->skip, "SKIP", error_message) < 0) return -1;
    if (validate_skip_limit(ret->limit, "LIMIT", error_message) < 0) return -1;
    return 0;
}

static int validate_with_clause(cypher_with *with, var_type_ctx *vctx_out,
                                 char **error_message)
{
    if (!with) return 0;
    if (with->items) {
        /* Duplicate aliases in WITH are a ColumnNameConflict. */
        name_set seen; nset_init(&seen);
        for (int i = 0; i < with->items->count; i++) {
            cypher_return_item *item = (cypher_return_item *)with->items->items[i];
            if (!item || !item->expr) continue;
            /* Patterns are not valid expressions in WITH projection
             * (Pattern1 [23]). See the matching RETURN comment for why
             * EXISTS_EXPR also triggers this. */
            if (item->expr->type == AST_NODE_PATH ||
                (item->expr->type == AST_NODE_EXISTS_EXPR &&
                 ((cypher_exists_expr *)item->expr)->expr_type == EXISTS_TYPE_PATTERN)) {
                set_error(error_message,
                          "SyntaxError: UnexpectedSyntax: a path pattern is not a valid expression in WITH");
                nset_free(&seen);
                return -1;
            }
            /* Every non-identifier expression in WITH must be aliased.
             * `WITH a` is fine (carries the bare variable forward), but
             * `WITH a, count(*)` requires `count(*) AS c` (With4 [5]). */
            if (!item->alias && item->expr->type != AST_NODE_IDENTIFIER) {
                set_error(error_message,
                          "SyntaxError: NoExpressionAlias: every WITH item that is not a bare variable must be aliased");
                nset_free(&seen);
                return -1;
            }
            if (item->alias) {
                if (nset_contains(&seen, item->alias)) {
                    set_error(error_message,
                              "SyntaxError: ColumnNameConflict: Multiple result columns with the same name `%s`",
                              item->alias);
                    nset_free(&seen);
                    return -1;
                }
                nset_add(&seen, item->alias);
            }
            if (validate_expr(item->expr, error_message) < 0) { nset_free(&seen); return -1; }
            if (validate_expr_typed(item->expr, vctx_out, error_message) < 0) { nset_free(&seen); return -1; }
            /* Track the alias's bound type for downstream clauses. */
            if (item->alias) {
                vctx_register(vctx_out, item->alias,
                              type_of_literal_expr(item->expr));
            }
        }
        nset_free(&seen);
    }
    if (with->where) {
        if (validate_expr(with->where, error_message) < 0) return -1;
        if (validate_expr_typed(with->where, vctx_out, error_message) < 0) return -1;
    }
    if (validate_skip_limit(with->skip, "SKIP", error_message) < 0) return -1;
    if (validate_skip_limit(with->limit, "LIMIT", error_message) < 0) return -1;
    return 0;
}

static int validate_match_clause(cypher_match *match, const var_type_ctx *vctx,
                                  char **error_message)
{
    if (!match) return 0;
    if (match->where) {
        if (validate_expr(match->where, error_message) < 0) return -1;
        if (validate_expr_typed(match->where, vctx, error_message) < 0) return -1;
    }
    return 0;
}

/* Helper: walk a list of paths (a CREATE/MATCH pattern list) and append all
 * named node/rel/path variables into `out`. */
static void collect_pattern_names(ast_list *patterns, name_set *out)
{
    if (!patterns) return;
    for (int i = 0; i < patterns->count; i++) {
        ast_node *node = patterns->items[i];
        if (!node) continue;
        if (node->type == AST_NODE_PATH) {
            cypher_path *p = (cypher_path *)node;
            if (p->var_name) nset_add(out, p->var_name);
            if (!p->elements) continue;
            for (int j = 0; j < p->elements->count; j++) {
                ast_node *el = p->elements->items[j];
                if (!el) continue;
                if (el->type == AST_NODE_NODE_PATTERN) {
                    cypher_node_pattern *np = (cypher_node_pattern *)el;
                    if (np->variable) nset_add(out, np->variable);
                } else if (el->type == AST_NODE_REL_PATTERN) {
                    cypher_rel_pattern *rp = (cypher_rel_pattern *)el;
                    if (rp->variable) nset_add(out, rp->variable);
                }
            }
        }
    }
}

/* Parallel collector that registers each pattern variable's kind
 * (node / relationship / path) in vctx so callers can type-check
 * functions like length() that reject node/relationship arguments. */
static void register_pattern_kinds(ast_list *patterns, var_type_ctx *vctx)
{
    if (!patterns || !vctx) return;
    for (int i = 0; i < patterns->count; i++) {
        ast_node *node = patterns->items[i];
        if (!node || node->type != AST_NODE_PATH) continue;
        cypher_path *p = (cypher_path *)node;
        if (p->var_name) vctx_register(vctx, p->var_name, VTYPE_PATH);
        if (!p->elements) continue;
        for (int j = 0; j < p->elements->count; j++) {
            ast_node *el = p->elements->items[j];
            if (!el) continue;
            if (el->type == AST_NODE_NODE_PATTERN) {
                cypher_node_pattern *np = (cypher_node_pattern *)el;
                if (np->variable) vctx_register(vctx, np->variable, VTYPE_NODE);
            } else if (el->type == AST_NODE_REL_PATTERN) {
                cypher_rel_pattern *rp = (cypher_rel_pattern *)el;
                if (rp->variable) vctx_register(vctx, rp->variable, VTYPE_EDGE);
            }
        }
    }
}

/* Walk an expression looking for patterns (paths) used in a WHERE/predicate
 * context. Every variable inside such a pattern must already be bound in
 * `bound` — otherwise emit SyntaxError(UndefinedVariable). Pattern
 * existence checks aren't allowed to introduce fresh variables.
 *
 * Anonymous elements (no variable name) are always allowed. */
static int validate_where_pattern_vars(ast_node *expr, const name_set *bound,
                                       char **error_message)
{
    if (!expr) return 0;
    if (expr->type == AST_NODE_EXISTS_EXPR) {
        cypher_exists_expr *ex = (cypher_exists_expr *)expr;
        if (ex->expr_type == EXISTS_TYPE_PATTERN && ex->expr.pattern) {
            for (int i = 0; i < ex->expr.pattern->count; i++) {
                if (validate_where_pattern_vars(ex->expr.pattern->items[i], bound, error_message) < 0) return -1;
            }
        }
        return 0;
    }
    if (expr->type == AST_NODE_PATH) {
        cypher_path *p = (cypher_path *)expr;
        if (p->elements) {
            for (int j = 0; j < p->elements->count; j++) {
                ast_node *el = p->elements->items[j];
                if (!el) continue;
                const char *var = NULL;
                if (el->type == AST_NODE_NODE_PATTERN) {
                    var = ((cypher_node_pattern *)el)->variable;
                } else if (el->type == AST_NODE_REL_PATTERN) {
                    var = ((cypher_rel_pattern *)el)->variable;
                }
                if (var && !nset_contains(bound, var)) {
                    char buf[256];
                    snprintf(buf, sizeof(buf),
                             "SyntaxError: UndefinedVariable: pattern in WHERE introduces fresh variable `%s`",
                             var);
                    set_error(error_message, "%s", buf);
                    return -1;
                }
            }
        }
        if (p->var_name && !nset_contains(bound, p->var_name)) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "SyntaxError: UndefinedVariable: pattern in WHERE introduces fresh path variable `%s`",
                     p->var_name);
            set_error(error_message, "%s", buf);
            return -1;
        }
        return 0;
    }
    /* `WHERE (a)` parses as a parenthesised NODE_PATTERN at the top of
     * the WHERE expression (not a PATH). Apply the same rule. */
    if (expr->type == AST_NODE_NODE_PATTERN) {
        const char *var = ((cypher_node_pattern *)expr)->variable;
        if (var && !nset_contains(bound, var)) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "SyntaxError: UndefinedVariable: pattern in WHERE introduces fresh variable `%s`",
                     var);
            set_error(error_message, "%s", buf);
            return -1;
        }
        return 0;
    }
    /* Recurse into operands for nested expressions. */
    switch (expr->type) {
        case AST_NODE_NOT_EXPR:
            return validate_where_pattern_vars(((cypher_not_expr *)expr)->expr, bound, error_message);
        case AST_NODE_NULL_CHECK:
            return validate_where_pattern_vars(((cypher_null_check *)expr)->expr, bound, error_message);
        case AST_NODE_BINARY_OP: {
            cypher_binary_op *bop = (cypher_binary_op *)expr;
            if (validate_where_pattern_vars(bop->left, bound, error_message) < 0) return -1;
            return validate_where_pattern_vars(bop->right, bound, error_message);
        }
        case AST_NODE_FUNCTION_CALL: {
            cypher_function_call *fc = (cypher_function_call *)expr;
            if (fc->args) {
                for (int i = 0; i < fc->args->count; i++) {
                    if (validate_where_pattern_vars(fc->args->items[i], bound, error_message) < 0) return -1;
                }
            }
            return 0;
        }
        case AST_NODE_LIST: {
            cypher_list *l = (cypher_list *)expr;
            if (l->items) {
                for (int i = 0; i < l->items->count; i++) {
                    if (validate_where_pattern_vars(l->items->items[i], bound, error_message) < 0) return -1;
                }
            }
            return 0;
        }
        default:
            return 0;
    }
}

/* For each NODE_PATTERN / REL_PATTERN in `patterns`, if its variable is
 * already in `bound`, emit a "VariableAlreadyBound" error.
 *
 * Re-binding rules per openCypher:
 *  - `CREATE (a)` (single-node path) where `a` is already bound → error.
 *  - `CREATE (a {prop:1})` or `CREATE (a:Label)` where `a` is bound → error.
 *  - `CREATE (a)-[:R]->(b)` where `a`/`b` are already bound → OK (reference).
 *  - `CREATE (a)-[r:T]->(b)` where `r` was already bound to a relationship
 *    variable → error.
 */
static int check_create_rebinds_ex(ast_list *patterns, const name_set *bound,
                                    bool is_merge, char **error_message)
{
    if (!patterns) return 0;
    /* Track variables introduced *within* this CREATE/MERGE that have
     * labels/props, so a second occurrence with new labels/props on the
     * same name (e.g. `CREATE (n:Foo), (n:Bar)`) raises an error too. */
    name_set local_labeled; nset_init(&local_labeled);
    for (int i = 0; i < patterns->count; i++) {
        ast_node *node = patterns->items[i];
        if (!node || node->type != AST_NODE_PATH) continue;
        cypher_path *p = (cypher_path *)node;
        if (!p->elements) continue;

        /* A path with a single NODE_PATTERN element is a "create this node"
         * statement — re-binding the only var is always an error. */
        bool single_node = (p->elements->count == 1 &&
                            p->elements->items[0] &&
                            p->elements->items[0]->type == AST_NODE_NODE_PATTERN);

        for (int j = 0; j < p->elements->count; j++) {
            ast_node *el = p->elements->items[j];
            if (!el) continue;
            char *var = NULL;
            bool has_labels = false, has_props = false;
            bool is_rel = false;
            if (el->type == AST_NODE_NODE_PATTERN) {
                cypher_node_pattern *np = (cypher_node_pattern *)el;
                var = np->variable;
                has_labels = (np->labels && np->labels->count > 0);
                has_props  = (np->properties != NULL);
            } else if (el->type == AST_NODE_REL_PATTERN) {
                cypher_rel_pattern *rp = (cypher_rel_pattern *)el;
                var = rp->variable;
                has_labels = (rp->type != NULL || (rp->types && rp->types->count > 0));
                has_props  = (rp->properties != NULL);
                is_rel = true;
            }
            if (!var) continue;

            /* Intra-pattern rebind: same var with labels/props after
             * already being introduced with labels/props in this same
             * CREATE (Create1 [15]/[16]/[19]). */
            if (!nset_contains(bound, var) && nset_contains(&local_labeled, var)) {
                if (is_rel || has_labels || has_props) {
                    char buf[200];
                    snprintf(buf, sizeof(buf),
                             "SyntaxError: VariableAlreadyBound: %s is already bound", var);
                    set_error(error_message, "%s", buf);
                    nset_free(&local_labeled);
                    return -1;
                }
            }
            /* Remember this var with labels/props so a later occurrence
             * in this same pattern flags. */
            if (has_labels || has_props || is_rel) nset_add(&local_labeled, var);

            if (!nset_contains(bound, var)) continue;
            /* MERGE re-binding rules:
             *  - MERGE (a) single-node: error (cannot re-merge an already-
             *    bound node).
             *  - MERGE (a:L) / MERGE (a {p:1}) imposing new labels/props on
             *    an already-bound node: error (VariableAlreadyBound).
             *  - MERGE (a)-[:R]->(b) with bare `a`/`b` (no labels/props):
             *    legal reference.
             *  - Re-binding a relationship variable: always error. */
            if (is_merge && !is_rel && !single_node && !has_labels && !has_props) continue;
            /* A relationship variable in CREATE always means "create this
             * relationship" — re-binding is an error. */
            if (is_rel || single_node || has_labels || has_props) {
                char buf[200];
                snprintf(buf, sizeof(buf),
                         "SyntaxError: VariableAlreadyBound: %s is already bound", var);
                set_error(error_message, "%s", buf);
                nset_free(&local_labeled);
                return -1;
            }
        }
    }
    nset_free(&local_labeled);
    return 0;
}

/* Walk an expression; if any AST_NODE_IDENTIFIER references a name
 * not in `bound`, emit a SyntaxError(UndefinedVariable). Used by
 * CREATE/MERGE property-map validation so things like
 *   CREATE (b {name: missing}) RETURN b
 * are rejected (the value `missing` is a reference, not a literal). */
static int check_undef_in_expr(ast_node *expr, const name_set *bound,
                               const char *kw, char **error_message)
{
    if (!expr) return 0;
    switch (expr->type) {
        case AST_NODE_IDENTIFIER: {
            cypher_identifier *id = (cypher_identifier *)expr;
            if (id->name && !nset_contains(bound, id->name)) {
                set_error(error_message,
                          "SyntaxError: UndefinedVariable: %s references undefined variable `%s`",
                          kw, id->name);
                return -1;
            }
            return 0;
        }
        case AST_NODE_NOT_EXPR:
            return check_undef_in_expr(((cypher_not_expr *)expr)->expr, bound, kw, error_message);
        case AST_NODE_NULL_CHECK:
            return check_undef_in_expr(((cypher_null_check *)expr)->expr, bound, kw, error_message);
        case AST_NODE_BINARY_OP: {
            cypher_binary_op *bop = (cypher_binary_op *)expr;
            if (check_undef_in_expr(bop->left, bound, kw, error_message) < 0) return -1;
            return check_undef_in_expr(bop->right, bound, kw, error_message);
        }
        case AST_NODE_FUNCTION_CALL: {
            cypher_function_call *fc = (cypher_function_call *)expr;
            if (fc->args) {
                for (int i = 0; i < fc->args->count; i++) {
                    if (check_undef_in_expr(fc->args->items[i], bound, kw, error_message) < 0) return -1;
                }
            }
            return 0;
        }
        case AST_NODE_LIST: {
            cypher_list *l = (cypher_list *)expr;
            if (l->items) {
                for (int i = 0; i < l->items->count; i++) {
                    if (check_undef_in_expr(l->items->items[i], bound, kw, error_message) < 0) return -1;
                }
            }
            return 0;
        }
        case AST_NODE_PROPERTY: {
            cypher_property *p = (cypher_property *)expr;
            return check_undef_in_expr(p->expr, bound, kw, error_message);
        }
        case AST_NODE_SUBSCRIPT: {
            cypher_subscript *s = (cypher_subscript *)expr;
            if (check_undef_in_expr(s->expr, bound, kw, error_message) < 0) return -1;
            return check_undef_in_expr(s->index, bound, kw, error_message);
        }
        default:
            return 0;
    }
}

/* Walk every property-map value in a CREATE/MERGE pattern checking for
 * identifier references that aren't bound. Pattern variables being
 * introduced by this same clause are NOT in `bound` yet — that matches
 * Cypher scoping (you can't reference a sibling variable being
 * introduced in the same write clause). */
static int validate_write_undef_in_props(ast_list *patterns, const name_set *bound,
                                         const char *kw, char **error_message)
{
    if (!patterns) return 0;
    for (int pi = 0; pi < patterns->count; pi++) {
        ast_node *pn = patterns->items[pi];
        if (!pn || pn->type != AST_NODE_PATH) continue;
        cypher_path *p = (cypher_path *)pn;
        if (!p->elements) continue;
        for (int ei = 0; ei < p->elements->count; ei++) {
            ast_node *el = p->elements->items[ei];
            if (!el) continue;
            ast_node *props = NULL;
            if (el->type == AST_NODE_NODE_PATTERN) props = ((cypher_node_pattern *)el)->properties;
            else if (el->type == AST_NODE_REL_PATTERN) props = ((cypher_rel_pattern *)el)->properties;
            if (!props || props->type != AST_NODE_MAP) continue;
            cypher_map *m = (cypher_map *)props;
            if (!m->pairs) continue;
            for (int i = 0; i < m->pairs->count; i++) {
                cypher_map_pair *pair = (cypher_map_pair *)m->pairs->items[i];
                if (!pair || !pair->value) continue;
                if (check_undef_in_expr(pair->value, bound, kw, error_message) < 0) return -1;
            }
        }
    }
    return 0;
}

/* Reject NULL literals in CREATE/MERGE property maps. openCypher
 * classifies this as SemanticError: PropertyNotFound or NullValue. */
static int validate_write_property_map(ast_node *props, const char *kw,
                                       char **error_message)
{
    if (!props || props->type != AST_NODE_MAP) return 0;
    cypher_map *m = (cypher_map *)props;
    if (!m->pairs) return 0;
    for (int i = 0; i < m->pairs->count; i++) {
        cypher_map_pair *pair = (cypher_map_pair *)m->pairs->items[i];
        if (!pair || !pair->value) continue;
        if (pair->value->type == AST_NODE_LITERAL) {
            cypher_literal *lit = (cypher_literal *)pair->value;
            if (lit->literal_type == LITERAL_NULL) {
                set_error(error_message,
                          "SemanticError: MergeReadOwnWrites: %s with null property value '%s' is not allowed",
                          kw, pair->key ? pair->key : "?");
                return -1;
            }
        }
    }
    return 0;
}

/* Walk a CREATE/MERGE pattern list checking every NODE_PATTERN and
 * REL_PATTERN property map for NULL literal values. */
static int validate_write_no_null_props(ast_list *patterns, const char *kw,
                                        char **error_message)
{
    if (!patterns) return 0;
    for (int pi = 0; pi < patterns->count; pi++) {
        ast_node *pn = patterns->items[pi];
        if (!pn || pn->type != AST_NODE_PATH) continue;
        cypher_path *p = (cypher_path *)pn;
        if (!p->elements) continue;
        for (int ei = 0; ei < p->elements->count; ei++) {
            ast_node *el = p->elements->items[ei];
            if (!el) continue;
            if (el->type == AST_NODE_NODE_PATTERN) {
                cypher_node_pattern *np = (cypher_node_pattern *)el;
                if (validate_write_property_map(np->properties, kw, error_message) < 0)
                    return -1;
            } else if (el->type == AST_NODE_REL_PATTERN) {
                cypher_rel_pattern *rp = (cypher_rel_pattern *)el;
                if (validate_write_property_map(rp->properties, kw, error_message) < 0)
                    return -1;
            }
        }
    }
    return 0;
}

/* CREATE / MERGE must use a single explicit relationship type, no
 * multi-type (`[:T1|T2]`), and no variable-length range. MATCH allows
 * all of those — this validator is only for writes. */
static int validate_write_rel_patterns(ast_list *patterns, const char *kw,
                                       char **error_message)
{
    if (!patterns) return 0;
    for (int pi = 0; pi < patterns->count; pi++) {
        ast_node *pn = patterns->items[pi];
        if (!pn || pn->type != AST_NODE_PATH) continue;
        cypher_path *p = (cypher_path *)pn;
        if (!p->elements) continue;
        for (int ei = 0; ei < p->elements->count; ei++) {
            ast_node *el = p->elements->items[ei];
            if (!el || el->type != AST_NODE_REL_PATTERN) continue;
            cypher_rel_pattern *rp = (cypher_rel_pattern *)el;

            if (rp->varlen) {
                set_error(error_message,
                          "SyntaxError: CreatingVarLength: %s does not allow variable-length relationships",
                          kw);
                return -1;
            }
            /* CREATE/MERGE requires exactly one direction (left XOR right).
             * Undirected `()-[:T]-()` and bidirectional `()<-[:T]->()`
             * both error per openCypher. (MERGE-undirected is technically
             * legal per Cypher 9 spec — matches any direction, defaults
             * to outgoing on create — but the executor doesn't yet
             * implement that semantic correctly, and tests that *would*
             * exercise it currently regress more than the syntax check
             * helps. Re-enable per-kw once executor supports it.) */
            if (rp->left_arrow == rp->right_arrow) {
                set_error(error_message,
                          "SyntaxError: RequiresDirectedRelationship: %s requires a directed relationship",
                          kw);
                return -1;
            }
            int type_count = 0;
            if (rp->type) type_count++;
            if (rp->types) type_count += rp->types->count;
            if (type_count == 0) {
                set_error(error_message,
                          "SyntaxError: NoSingleRelationshipType: %s requires a single relationship type",
                          kw);
                return -1;
            }
            if (type_count > 1) {
                set_error(error_message,
                          "SyntaxError: NoSingleRelationshipType: %s does not allow multiple relationship types",
                          kw);
                return -1;
            }
        }
    }
    return 0;
}

static int validate_unwind_clause(cypher_unwind *uw, var_type_ctx *vctx_out,
                                   char **error_message)
{
    if (!uw) return 0;
    if (uw->expr && validate_expr(uw->expr, error_message) < 0) return -1;
    /* UNWIND [1,2,3] AS x — best-effort: if elements are uniform, register
     * x with that type. Otherwise leave it Unknown. */
    if (uw->alias && uw->expr && uw->expr->type == AST_NODE_LIST) {
        cypher_list *lst = (cypher_list *)uw->expr;
        var_type elem_t = VTYPE_UNKNOWN;
        if (lst->items && lst->items->count > 0) {
            elem_t = type_of_literal_expr(lst->items->items[0]);
            for (int i = 1; i < lst->items->count; i++) {
                if (type_of_literal_expr(lst->items->items[i]) != elem_t) {
                    elem_t = VTYPE_UNKNOWN;
                    break;
                }
            }
        }
        vctx_register(vctx_out, uw->alias, elem_t);
    }
    return 0;
}

/* Return the RETURN clause of a single-query AST, or NULL if absent. */
static cypher_return *find_terminal_return(cypher_query *q)
{
    if (!q || !q->clauses) return NULL;
    for (int i = q->clauses->count - 1; i >= 0; i--) {
        ast_node *c = q->clauses->items[i];
        if (c && c->type == AST_NODE_RETURN) return (cypher_return *)c;
    }
    return NULL;
}

/* Collect column names from a query's RETURN clause into `out`. Uses
 * alias if present, otherwise a synthesized name from the expression
 * (currently the alias-or-NULL slot). Returns -1 if no RETURN. */
static int collect_branch_columns(cypher_query *q, ast_list **out_items)
{
    cypher_return *r = find_terminal_return(q);
    if (!r) return -1;
    *out_items = r->items;
    return 0;
}

/* Effective column name for a RETURN item: explicit alias if present,
 * else the rendered expression (bare identifier, property access, …).
 * Returns a temporary buffer pointer per call; not thread-safe. */
static const char *column_name_of(cypher_return_item *it)
{
    if (!it) return NULL;
    if (it->alias) return it->alias;
    if (!it->expr) return NULL;
    if (it->expr->type == AST_NODE_IDENTIFIER) {
        return ((cypher_identifier *)it->expr)->name;
    }
    if (it->expr->type == AST_NODE_PROPERTY) {
        static char buf[256];
        cypher_property *p = (cypher_property *)it->expr;
        const char *base = (p->expr && p->expr->type == AST_NODE_IDENTIFIER)
            ? ((cypher_identifier *)p->expr)->name : "?";
        snprintf(buf, sizeof(buf), "%s.%s", base, p->property_name ? p->property_name : "?");
        return buf;
    }
    return NULL;
}

static int validate_union_recursive(cypher_union *u, bool *first_all_seen,
                                    bool *first_all, ast_list **first_items,
                                    char **error_message)
{
    /* Check this node's UNION/UNION ALL flag against the first seen. */
    if (!*first_all_seen) {
        *first_all = u->all;
        *first_all_seen = true;
    } else if (u->all != *first_all) {
        set_error(error_message,
                  "SyntaxError: InvalidClauseComposition: cannot mix UNION and UNION ALL");
        return -1;
    }
    /* Recurse left if it's another UNION; otherwise treat as query. */
    if (u->left && u->left->type == AST_NODE_UNION) {
        if (validate_union_recursive((cypher_union *)u->left, first_all_seen,
                                     first_all, first_items, error_message) < 0)
            return -1;
    } else if (u->left && (u->left->type == AST_NODE_QUERY ||
                            u->left->type == AST_NODE_SINGLE_QUERY)) {
        ast_list *items = NULL;
        if (collect_branch_columns((cypher_query *)u->left, &items) == 0 && items) {
            if (!*first_items) {
                *first_items = items;
            } else {
                /* Compare column count and alias names. */
                if (items->count != (*first_items)->count) {
                    set_error(error_message,
                              "SyntaxError: DifferentColumnsInUnion: UNION branches must return the same columns");
                    return -1;
                }
                for (int i = 0; i < items->count; i++) {
                    cypher_return_item *a = (cypher_return_item *)(*first_items)->items[i];
                    cypher_return_item *b = (cypher_return_item *)items->items[i];
                    const char *an = column_name_of(a);
                    char a_copy[256]; if (an) { snprintf(a_copy, sizeof(a_copy), "%s", an); an = a_copy; }
                    const char *bn = column_name_of(b);
                    if (!an || !bn || strcmp(an, bn) != 0) {
                        set_error(error_message,
                                  "SyntaxError: DifferentColumnsInUnion: UNION branches must return the same columns");
                        return -1;
                    }
                }
            }
        }
    }
    /* Right side is always a query. */
    if (u->right && (u->right->type == AST_NODE_QUERY ||
                     u->right->type == AST_NODE_SINGLE_QUERY)) {
        ast_list *items = NULL;
        if (collect_branch_columns((cypher_query *)u->right, &items) == 0 && items) {
            if (!*first_items) {
                *first_items = items;
            } else {
                if (items->count != (*first_items)->count) {
                    set_error(error_message,
                              "SyntaxError: DifferentColumnsInUnion: UNION branches must return the same columns");
                    return -1;
                }
                for (int i = 0; i < items->count; i++) {
                    cypher_return_item *a = (cypher_return_item *)(*first_items)->items[i];
                    cypher_return_item *b = (cypher_return_item *)items->items[i];
                    const char *an = column_name_of(a);
                    char a_copy[256]; if (an) { snprintf(a_copy, sizeof(a_copy), "%s", an); an = a_copy; }
                    const char *bn = column_name_of(b);
                    if (!an || !bn || strcmp(an, bn) != 0) {
                        set_error(error_message,
                                  "SyntaxError: DifferentColumnsInUnion: UNION branches must return the same columns");
                        return -1;
                    }
                }
            }
        }
    }
    return 0;
}

int transform_validate_union(cypher_union *u, char **error_message)
{
    if (!u) return 0;
    bool first_all_seen = false;
    bool first_all = false;
    ast_list *first_items = NULL;
    return validate_union_recursive(u, &first_all_seen, &first_all,
                                    &first_items, error_message);
}

int transform_validate_query(cypher_query *query, char **error_message)
{
    if (!query || !query->clauses) return 0;
    var_type_ctx vctx;
    vctx_init(&vctx);
    name_set bound; nset_init(&bound);
    int rc = 0;
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        if (!clause) { continue; }
        switch (clause->type) {
            case AST_NODE_RETURN: {
                cypher_return *r = (cypher_return *)clause;
                /* RETURN * requires at least one variable in scope. */
                if (r->return_all && bound.count == 0) {
                    set_error(error_message,
                              "SyntaxError: NoVariablesInScope: RETURN * requires at least one variable in scope");
                    rc = -1;
                    break;
                }
                rc = validate_return_clause(r, &vctx, error_message);
                break;
            }
            case AST_NODE_WITH: {
                cypher_with *w = (cypher_with *)clause;
                rc = validate_with_clause(w, &vctx, error_message);
                /* WITH projects a new scope: clear previously-bound names
                 * and add only the names this WITH carries forward. Items
                 * with an explicit alias use that alias; bare-identifier
                 * items (e.g. 'WITH a') carry the variable's name. */
                if (rc == 0 && w->items) {
                    nset_free(&bound);
                    nset_init(&bound);
                    for (int wi = 0; wi < w->items->count; wi++) {
                        cypher_return_item *it = (cypher_return_item *)w->items->items[wi];
                        if (!it) continue;
                        if (it->alias) {
                            nset_add(&bound, it->alias);
                        } else if (it->expr && it->expr->type == AST_NODE_IDENTIFIER) {
                            nset_add(&bound, ((cypher_identifier *)it->expr)->name);
                        }
                    }
                }
                break;
            }
            case AST_NODE_MATCH: {
                cypher_match *m = (cypher_match *)clause;
                rc = validate_match_clause(m, &vctx, error_message);
                collect_pattern_names(m->pattern, &bound);
                register_pattern_kinds(m->pattern, &vctx);
                /* WHERE patterns may not introduce fresh variables — every
                 * var in a pattern predicate must already be bound. Run
                 * after collect_pattern_names so the current MATCH's own
                 * variables count as bound. */
                if (rc == 0 && m->where) {
                    rc = validate_where_pattern_vars(m->where, &bound, error_message);
                }
                break;
            }
            case AST_NODE_UNWIND: {
                cypher_unwind *u = (cypher_unwind *)clause;
                rc = validate_unwind_clause(u, &vctx, error_message);
                if (u->alias) nset_add(&bound, u->alias);
                break;
            }
            case AST_NODE_CREATE: {
                cypher_create *c = (cypher_create *)clause;
                rc = check_create_rebinds_ex(c->pattern, &bound, false, error_message);
                if (rc == 0) rc = validate_write_rel_patterns(c->pattern, "CREATE", error_message);
                if (rc == 0) rc = validate_write_undef_in_props(c->pattern, &bound, "CREATE", error_message);
                if (rc == 0) collect_pattern_names(c->pattern, &bound);
                break;
            }
            case AST_NODE_SET: {
                cypher_set *s = (cypher_set *)clause;
                if (s->items) {
                    for (int si = 0; si < s->items->count; si++) {
                        cypher_set_item *it = (cypher_set_item *)s->items->items[si];
                        if (!it) continue;
                        if (it->expr && check_undef_in_expr(it->expr, &bound, "SET", error_message) < 0) { rc = -1; break; }
                        if (it->property && it->property->type == AST_NODE_PROPERTY) {
                            cypher_property *pr = (cypher_property *)it->property;
                            if (pr->expr && check_undef_in_expr(pr->expr, &bound, "SET", error_message) < 0) { rc = -1; break; }
                        }
                        /* A *property*-target SET (n.prop = value) rejects
                         * a list-of-maps RHS (Set1 [10]) because the
                         * storage layer can't represent it. Bulk SET on
                         * a variable (SET n = {…}) is a different
                         * shape — its property is a bare IDENTIFIER, not
                         * AST_NODE_PROPERTY — so it's untouched here. */
                        if (it->expr && it->expr->type == AST_NODE_LIST &&
                            it->property && it->property->type == AST_NODE_PROPERTY) {
                            cypher_list *l = (cypher_list *)it->expr;
                            if (l->items) {
                                for (int li = 0; li < l->items->count; li++) {
                                    ast_node *el = l->items->items[li];
                                    if (el && el->type == AST_NODE_MAP) {
                                        set_error(error_message,
                                                  "TypeError: InvalidPropertyType: a property value cannot be a list of maps");
                                        rc = -1; break;
                                    }
                                }
                            }
                            if (rc < 0) break;
                        }
                    }
                }
                break;
            }
            case AST_NODE_MERGE: {
                cypher_merge *m = (cypher_merge *)clause;
                /* MERGE shares the same re-binding rules as CREATE except
                 * single-node and label/prop predicates on an already-bound
                 * node are legal (matches the existing binding). */
                rc = check_create_rebinds_ex(m->pattern, &bound, true, error_message);
                if (rc == 0) rc = validate_write_rel_patterns(m->pattern, "MERGE", error_message);
                if (rc == 0) rc = validate_write_undef_in_props(m->pattern, &bound, "MERGE", error_message);
                /* MERGE rejects null property values at runtime per spec
                 * (SemanticError: MergeReadOwnWrites) — Merge1 [17] /
                 * Merge5 [29]. CREATE has different semantics (cluster
                 * silently dropped them historically) so we only invoke
                 * the check for MERGE. */
                if (rc == 0) rc = validate_write_no_null_props(m->pattern, "MERGE", error_message);
                if (rc == 0) collect_pattern_names(m->pattern, &bound);
                /* ON CREATE SET / ON MATCH SET reference variables that
                 * must already be bound (either from earlier clauses or
                 * by the MERGE pattern itself, which we just collected). */
                if (rc == 0 && m->on_create) {
                    for (int si = 0; si < m->on_create->count; si++) {
                        cypher_set_item *it = (cypher_set_item *)m->on_create->items[si];
                        if (it && it->expr && check_undef_in_expr(it->expr, &bound, "ON CREATE SET", error_message) < 0) { rc = -1; break; }
                        if (rc < 0) break;
                        /* Also: the SET target itself must reference a
                         * bound variable (e.g. 'x.num = 1' where 'x' is
                         * not in scope). */
                        if (it && it->property && it->property->type == AST_NODE_PROPERTY) {
                            cypher_property *pr = (cypher_property *)it->property;
                            if (pr->expr && check_undef_in_expr(pr->expr, &bound, "ON CREATE SET", error_message) < 0) { rc = -1; break; }
                        }
                    }
                }
                if (rc == 0 && m->on_match) {
                    for (int si = 0; si < m->on_match->count; si++) {
                        cypher_set_item *it = (cypher_set_item *)m->on_match->items[si];
                        if (it && it->expr && check_undef_in_expr(it->expr, &bound, "ON MATCH SET", error_message) < 0) { rc = -1; break; }
                        if (it && it->property && it->property->type == AST_NODE_PROPERTY) {
                            cypher_property *pr = (cypher_property *)it->property;
                            if (pr->expr && check_undef_in_expr(pr->expr, &bound, "ON MATCH SET", error_message) < 0) { rc = -1; break; }
                        }
                    }
                }
                break;
            }
            default:
                break;
        }
        if (rc < 0) break;
    }
    nset_free(&bound);
    vctx_free(&vctx);
    return rc;
}
