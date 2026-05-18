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

/* Walk a clause's expressions with the given var-type context, also
 * picking up new var bindings from WITH items along the way. */
static int validate_return_clause(cypher_return *ret, const var_type_ctx *vctx,
                                  char **error_message)
{
    if (!ret) return 0;
    if (ret->items) {
        for (int i = 0; i < ret->items->count; i++) {
            cypher_return_item *item = (cypher_return_item *)ret->items->items[i];
            if (item && item->expr) {
                if (validate_expr(item->expr, error_message) < 0) return -1;
                if (validate_expr_typed(item->expr, vctx, error_message) < 0) return -1;
            }
        }
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
        for (int i = 0; i < with->items->count; i++) {
            cypher_return_item *item = (cypher_return_item *)with->items->items[i];
            if (!item || !item->expr) continue;
            if (validate_expr(item->expr, error_message) < 0) return -1;
            if (validate_expr_typed(item->expr, vctx_out, error_message) < 0) return -1;
            /* Track the alias's bound type for downstream clauses. */
            if (item->alias) {
                vctx_register(vctx_out, item->alias,
                              type_of_literal_expr(item->expr));
            }
        }
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
            /* MERGE single-node `MERGE (a)` on a bound `a` is a legal
             * no-op match. Same for label/property predicates on an
             * already-bound node in MERGE. Re-binding a relationship
             * variable is still an error. */
            if (is_merge && !is_rel) continue;
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
                          "SemanticError: %s with a null property value is not allowed (key '%s')",
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
            case AST_NODE_RETURN:
                rc = validate_return_clause((cypher_return *)clause, &vctx, error_message);
                break;
            case AST_NODE_WITH:
                rc = validate_with_clause((cypher_with *)clause, &vctx, error_message);
                break;
            case AST_NODE_MATCH: {
                cypher_match *m = (cypher_match *)clause;
                rc = validate_match_clause(m, &vctx, error_message);
                collect_pattern_names(m->pattern, &bound);
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
                if (rc == 0) collect_pattern_names(c->pattern, &bound);
                break;
            }
            case AST_NODE_MERGE: {
                cypher_merge *m = (cypher_merge *)clause;
                /* MERGE shares the same re-binding rules as CREATE except
                 * single-node and label/prop predicates on an already-bound
                 * node are legal (matches the existing binding). */
                rc = check_create_rebinds_ex(m->pattern, &bound, true, error_message);
                if (rc == 0) rc = validate_write_rel_patterns(m->pattern, "MERGE", error_message);
                if (rc == 0) collect_pattern_names(m->pattern, &bound);
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
