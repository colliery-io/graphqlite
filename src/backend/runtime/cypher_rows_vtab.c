/*
 * cypher_rows_vtab.c — see runtime/cypher_rows_vtab.h.
 *
 * Phase A of ADR GQLITE-A-0006: the executor still materialises the result
 * in C (cypher_result), but everything after that streams — no output
 * buffer assembly, no SQLITE_TRANSIENT copy of the whole result, no host
 * string, no JSON decode of the whole payload — and positional columns carry
 * native types. Phase B (stepping the underlying statement row by row) is a
 * follow-up on the same module boundary.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "runtime/cypher_rows_vtab.h"
#include "executor/agtype.h"
#include "runtime/gql_error.h"

#define ROWS_MAX_POSITIONAL 32
enum {
    COL_ROW = 0,
    COL_COLS = 1,
    COL_NCOLS = 2,
    COL_C0 = 3,
    COL_QUERY = COL_C0 + ROWS_MAX_POSITIONAL,
    COL_PARAMS = COL_QUERY + 1
};

typedef struct rows_vtab {
    sqlite3_vtab base;
    gql_executor_getter getter;
    void *getter_arg;
} rows_vtab;

typedef struct rows_cursor {
    sqlite3_vtab_cursor base;
    cypher_result *result;
    int row;
    int nrows;            /* result rows, or 1 for a statistics row */
    bool stats_row;       /* write query without RETURN */
    char *row_json;       /* lazily rendered for `row` */
    int row_json_for;
    char *cols_json;      /* rendered once */
} rows_cursor;

/* ---- tiny growable buffer ---------------------------------------------- */
typedef struct { char *p; size_t len, cap; } gbuf;
static int gb_reserve(gbuf *b, size_t extra) {
    if (b->len + extra + 1 <= b->cap) return 0;
    size_t cap = b->cap ? b->cap : 256;
    while (cap < b->len + extra + 1) cap *= 2;
    char *np = realloc(b->p, cap);
    if (!np) return -1;
    b->p = np; b->cap = cap;
    return 0;
}
static int gb_put(gbuf *b, const char *s, size_t n) {
    if (gb_reserve(b, n) < 0) return -1;
    memcpy(b->p + b->len, s, n); b->len += n; b->p[b->len] = '\0';
    return 0;
}
static int gb_puts(gbuf *b, const char *s) { return gb_put(b, s, strlen(s)); }
static int gb_putc(gbuf *b, char c) { return gb_put(b, &c, 1); }
/* JSON string literal with the same escaping rules as extension.c (C1). */
static int gb_put_json_string(gbuf *b, const char *s) {
    if (gb_putc(b, '"') < 0) return -1;
    for (; *s; s++) {
        unsigned char ch = (unsigned char)*s;
        const char *esc = NULL; char ubuf[8];
        switch (ch) {
            case '"':  esc = "\\\""; break;
            case '\\': esc = "\\\\"; break;
            case '\n': esc = "\\n"; break;
            case '\r': esc = "\\r"; break;
            case '\t': esc = "\\t"; break;
            case '\b': esc = "\\b"; break;
            case '\f': esc = "\\f"; break;
            default:
                if (ch < 0x20) { snprintf(ubuf, sizeof(ubuf), "\\u%04x", ch); esc = ubuf; }
        }
        if (esc ? gb_puts(b, esc) < 0 : gb_putc(b, *s) < 0) return -1;
    }
    return gb_putc(b, '"');
}

/* ---- cell rendering ------------------------------------------------------ */
static const char *column_name(const cypher_result *r, int col, char *tmp, size_t n) {
    if (r->column_names && r->column_names[col]) return r->column_names[col];
    snprintf(tmp, n, "column_%d", col);
    return tmp;
}

/* JSON text of one cell, mirroring the two rendering paths in extension.c. */
static int render_cell_json(const cypher_result *r, int row, int col, gbuf *b) {
    agtype_value *ag = (r->use_agtype && r->agtype_data && r->agtype_data[row]) ? r->agtype_data[row][col] : NULL;
    const char *val = (r->data && r->data[row]) ? r->data[row][col] : NULL;
    if (ag) {
        char *s = agtype_value_to_string(ag);
        int rc = s ? gb_puts(b, s) : gb_puts(b, "null");
        free(s);
        return rc;
    }
    if (!val) return gb_puts(b, "null");
    int ct = (r->data_types && r->data_types[row]) ? r->data_types[row][col] : SQLITE_TEXT;
    if (val[0] == '[' || val[0] == '{' || val[0] == '"') return gb_puts(b, val);
    if (ct == SQLITE_INTEGER || ct == GQL_COL_TYPE_BOOLEAN) return gb_puts(b, val);
    if (ct == SQLITE_FLOAT) {
        if (gb_puts(b, val) < 0) return -1;
        if (!strpbrk(val, ".eE")) return gb_puts(b, ".0");
        return 0;
    }
    if (strcmp(val, GQL_NAN_SENTINEL) == 0) return gb_puts(b, "NaN");
    return gb_put_json_string(b, val);
}

/* Native SQLite value of one cell. */
static void result_cell_native(sqlite3_context *ctx, const cypher_result *r, int row, int col) {
    agtype_value *ag = (r->use_agtype && r->agtype_data && r->agtype_data[row]) ? r->agtype_data[row][col] : NULL;
    const char *val = (r->data && r->data[row]) ? r->data[row][col] : NULL;
    if (ag) {
        switch (ag->type) {
            case AGTV_NULL:    sqlite3_result_null(ctx); return;
            case AGTV_INTEGER: sqlite3_result_int64(ctx, ag->val.int_value); return;
            case AGTV_FLOAT:   sqlite3_result_double(ctx, ag->val.float_value); return;
            case AGTV_BOOL:
                sqlite3_result_int(ctx, ag->val.boolean ? 1 : 0);
                sqlite3_result_subtype(ctx, GQL_SUBTYPE_BOOLEAN);
                return;
            case AGTV_STRING:
                sqlite3_result_text(ctx, ag->val.string.val ? ag->val.string.val : "", -1, SQLITE_TRANSIENT);
                return;
            default: {
                char *s = agtype_value_to_string(ag);
                if (s) sqlite3_result_text(ctx, s, -1, SQLITE_TRANSIENT); else sqlite3_result_null(ctx);
                free(s);
                return;
            }
        }
    }
    if (!val) { sqlite3_result_null(ctx); return; }
    int ct = (r->data_types && r->data_types[row]) ? r->data_types[row][col] : SQLITE_TEXT;
    if (ct == SQLITE_INTEGER) { sqlite3_result_int64(ctx, strtoll(val, NULL, 10)); return; }
    if (ct == SQLITE_FLOAT) { sqlite3_result_double(ctx, strtod(val, NULL)); return; }
    if (ct == GQL_COL_TYPE_BOOLEAN) {
        sqlite3_result_int(ctx, (strcmp(val, "true") == 0 || strcmp(val, "1") == 0) ? 1 : 0);
        sqlite3_result_subtype(ctx, GQL_SUBTYPE_BOOLEAN);
        return;
    }
    if (strcmp(val, GQL_NAN_SENTINEL) == 0) { sqlite3_result_double(ctx, NAN); return; }
    sqlite3_result_text(ctx, val, -1, SQLITE_TRANSIENT);
}

static const char *const STATS_COLS[5] = {
    "nodes_created", "relationships_created", "nodes_deleted", "relationships_deleted", "properties_set"
};
static long long stats_value(const cypher_result *r, int i) {
    switch (i) {
        case 0: return r->nodes_created;
        case 1: return r->relationships_created;
        case 2: return r->nodes_deleted;
        case 3: return r->relationships_deleted;
        default: return r->properties_set;
    }
}

static char *render_row_json(rows_cursor *c) {
    gbuf b = {0};
    const cypher_result *r = c->result;
    if (gb_putc(&b, '{') < 0) goto oom;
    if (c->stats_row) {
        for (int i = 0; i < 5; i++) {
            char num[32];
            snprintf(num, sizeof(num), "%lld", stats_value(r, i));
            if ((i && gb_putc(&b, ',') < 0) || gb_put_json_string(&b, STATS_COLS[i]) < 0 ||
                gb_putc(&b, ':') < 0 || gb_puts(&b, num) < 0) goto oom;
        }
    } else {
        for (int col = 0; col < r->column_count; col++) {
            char tmp[32];
            if ((col && gb_putc(&b, ',') < 0) || gb_put_json_string(&b, column_name(r, col, tmp, sizeof(tmp))) < 0 ||
                gb_putc(&b, ':') < 0 || render_cell_json(r, c->row, col, &b) < 0) goto oom;
        }
    }
    if (gb_putc(&b, '}') < 0) goto oom;
    return b.p;
oom:
    free(b.p);
    return NULL;
}

static char *render_cols_json(rows_cursor *c) {
    gbuf b = {0};
    const cypher_result *r = c->result;
    if (gb_putc(&b, '[') < 0) goto oom;
    int n = c->stats_row ? 5 : r->column_count;
    for (int col = 0; col < n; col++) {
        char tmp[32];
        const char *name = c->stats_row ? STATS_COLS[col] : column_name(r, col, tmp, sizeof(tmp));
        if ((col && gb_putc(&b, ',') < 0) || gb_put_json_string(&b, name) < 0) goto oom;
    }
    if (gb_putc(&b, ']') < 0) goto oom;
    return b.p;
oom:
    free(b.p);
    return NULL;
}

/* Same {"error":..., "code":...} shape cypher() raises, so the bindings'
 * error parsing applies to both entry points. */
static int set_error(sqlite3_vtab *vt, const char *msg, const char *code) {
    gbuf b = {0};
    if (gb_puts(&b, "{\"error\":") < 0 || gb_put_json_string(&b, msg) < 0 ||
        gb_puts(&b, ",\"code\":\"") < 0 || gb_puts(&b, code) < 0 || gb_puts(&b, "\"}") < 0) {
        free(b.p);
        return SQLITE_NOMEM;
    }
    sqlite3_free(vt->zErrMsg);
    vt->zErrMsg = sqlite3_mprintf("%s", b.p);
    free(b.p);
    return SQLITE_ERROR;
}

static const char *classify_error(const char *msg) {
    if (msg && (strstr(msg, "syntax error") || strstr(msg, "Line "))) return GQL_ERR_PARSE;
    if (msg && strstr(msg, "not yet implemented")) return GQL_ERR_NOT_IMPL;
    return GQL_ERR_EXECUTION;
}

/* ---- virtual table callbacks -------------------------------------------- */
static int rows_connect(sqlite3 *db, void *aux, int argc, const char *const *argv,
                        sqlite3_vtab **out, char **err) {
    (void)argc; (void)argv; (void)err;
    gbuf schema = {0};
    if (gb_puts(&schema, "CREATE TABLE x(row TEXT, cols TEXT, ncols INTEGER") < 0) return SQLITE_NOMEM;
    for (int i = 0; i < ROWS_MAX_POSITIONAL; i++) {
        char col[16];
        snprintf(col, sizeof(col), ", c%d", i);
        if (gb_puts(&schema, col) < 0) { free(schema.p); return SQLITE_NOMEM; }
    }
    if (gb_puts(&schema, ", query HIDDEN, params HIDDEN)") < 0) { free(schema.p); return SQLITE_NOMEM; }
    int rc = sqlite3_declare_vtab(db, schema.p);
    free(schema.p);
    if (rc != SQLITE_OK) return rc;

    rows_vtab *vt = sqlite3_malloc(sizeof(rows_vtab));
    if (!vt) return SQLITE_NOMEM;
    memset(vt, 0, sizeof(*vt));
    /* aux is a heap struct {getter, arg} owned by the module registration */
    struct { gql_executor_getter getter; void *arg; } *reg = aux;
    vt->getter = reg->getter;
    vt->getter_arg = reg->arg;
    *out = &vt->base;
    return SQLITE_OK;
}

static int rows_disconnect(sqlite3_vtab *vt) { sqlite3_free(vt); return SQLITE_OK; }

static int rows_best_index(sqlite3_vtab *vt, sqlite3_index_info *info) {
    (void)vt;
    int q = -1, p = -1;
    for (int i = 0; i < info->nConstraint; i++) {
        const struct sqlite3_index_constraint *c = &info->aConstraint[i];
        if (!c->usable || c->op != SQLITE_INDEX_CONSTRAINT_EQ) continue;
        if (c->iColumn == COL_QUERY) q = i;
        else if (c->iColumn == COL_PARAMS) p = i;
    }
    if (q < 0) {
        /* Not callable without the query text. */
        return SQLITE_CONSTRAINT;
    }
    info->aConstraintUsage[q].argvIndex = 1;
    info->aConstraintUsage[q].omit = 1;
    if (p >= 0) {
        info->aConstraintUsage[p].argvIndex = 2;
        info->aConstraintUsage[p].omit = 1;
    }
    info->idxNum = p >= 0 ? 2 : 1;
    info->estimatedCost = 1000.0;
    info->estimatedRows = 100;
    return SQLITE_OK;
}

static int rows_open(sqlite3_vtab *vt, sqlite3_vtab_cursor **out) {
    (void)vt;
    rows_cursor *c = sqlite3_malloc(sizeof(rows_cursor));
    if (!c) return SQLITE_NOMEM;
    memset(c, 0, sizeof(*c));
    c->row_json_for = -1;
    *out = &c->base;
    return SQLITE_OK;
}

static void cursor_reset(rows_cursor *c) {
    if (c->result) { cypher_result_free(c->result); c->result = NULL; }
    free(c->row_json); c->row_json = NULL; c->row_json_for = -1;
    free(c->cols_json); c->cols_json = NULL;
    c->row = 0; c->nrows = 0; c->stats_row = false;
}

static int rows_close(sqlite3_vtab_cursor *cur) {
    rows_cursor *c = (rows_cursor *)cur;
    cursor_reset(c);
    sqlite3_free(c);
    return SQLITE_OK;
}

static int rows_filter(sqlite3_vtab_cursor *cur, int idxNum, const char *idxStr,
                       int argc, sqlite3_value **argv) {
    (void)idxNum; (void)idxStr;
    rows_cursor *c = (rows_cursor *)cur;
    rows_vtab *vt = (rows_vtab *)cur->pVtab;
    cursor_reset(c);

    if (argc < 1 || sqlite3_value_type(argv[0]) != SQLITE_TEXT) {
        return set_error(&vt->base, "cypher_rows() first argument (query) must be text", GQL_ERR_VALIDATION);
    }
    const char *query = (const char *)sqlite3_value_text(argv[0]);
    const char *params = NULL;
    if (argc >= 2 && sqlite3_value_type(argv[1]) != SQLITE_NULL) {
        if (sqlite3_value_type(argv[1]) != SQLITE_TEXT) {
            return set_error(&vt->base, "cypher_rows() second argument (params) must be JSON text or NULL", GQL_ERR_VALIDATION);
        }
        params = (const char *)sqlite3_value_text(argv[1]);
    }

    cypher_executor *ex = vt->getter ? vt->getter(vt->getter_arg) : NULL;
    if (!ex) return set_error(&vt->base, "Failed to create cypher executor", GQL_ERR_INTERNAL);

    cypher_result *r = params ? cypher_executor_execute_params(ex, query, params)
                              : cypher_executor_execute(ex, query);
    if (!r) return set_error(&vt->base, "Failed to execute cypher query", GQL_ERR_EXECUTION);
    if (!r->success) {
        const char *msg = r->error_message ? r->error_message : "Query execution failed";
        int rc = set_error(&vt->base, msg, classify_error(msg));
        cypher_result_free(r);
        return rc;
    }
    c->result = r;
    if (r->column_count == 0 && r->row_count == 0) {
        c->stats_row = true;
        c->nrows = 1;
    } else {
        c->nrows = r->row_count;
    }
    c->row = 0;
    return SQLITE_OK;
}

static int rows_next(sqlite3_vtab_cursor *cur) { ((rows_cursor *)cur)->row++; return SQLITE_OK; }
static int rows_eof(sqlite3_vtab_cursor *cur) { rows_cursor *c = (rows_cursor *)cur; return !c->result || c->row >= c->nrows; }

static int rows_column(sqlite3_vtab_cursor *cur, sqlite3_context *ctx, int col) {
    rows_cursor *c = (rows_cursor *)cur;
    if (!c->result) { sqlite3_result_null(ctx); return SQLITE_OK; }
    if (col == COL_ROW) {
        if (c->row_json_for != c->row) {
            free(c->row_json);
            c->row_json = render_row_json(c);
            c->row_json_for = c->row;
        }
        if (!c->row_json) return SQLITE_NOMEM;
        sqlite3_result_text(ctx, c->row_json, -1, SQLITE_TRANSIENT);
        sqlite3_result_subtype(ctx, 74 /* JSON subtype 'J' */);
        return SQLITE_OK;
    }
    if (col == COL_COLS) {
        if (!c->cols_json) c->cols_json = render_cols_json(c);
        if (!c->cols_json) return SQLITE_NOMEM;
        sqlite3_result_text(ctx, c->cols_json, -1, SQLITE_TRANSIENT);
        return SQLITE_OK;
    }
    if (col == COL_NCOLS) {
        sqlite3_result_int(ctx, c->stats_row ? 5 : c->result->column_count);
        return SQLITE_OK;
    }
    if (col >= COL_C0 && col < COL_QUERY) {
        int i = col - COL_C0;
        if (c->stats_row) {
            if (i < 5) sqlite3_result_int64(ctx, stats_value(c->result, i)); else sqlite3_result_null(ctx);
        } else if (i < c->result->column_count) {
            result_cell_native(ctx, c->result, c->row, i);
        } else {
            sqlite3_result_null(ctx);
        }
        return SQLITE_OK;
    }
    sqlite3_result_null(ctx);   /* query / params hidden columns */
    return SQLITE_OK;
}

static int rows_rowid(sqlite3_vtab_cursor *cur, sqlite3_int64 *rowid) {
    *rowid = ((rows_cursor *)cur)->row + 1;
    return SQLITE_OK;
}

static sqlite3_module cypher_rows_module = {
    /* iVersion    */ 0,
    /* xCreate     */ NULL,            /* eponymous only */
    /* xConnect    */ rows_connect,
    /* xBestIndex  */ rows_best_index,
    /* xDisconnect */ rows_disconnect,
    /* xDestroy    */ NULL,
    /* xOpen       */ rows_open,
    /* xClose      */ rows_close,
    /* xFilter     */ rows_filter,
    /* xNext       */ rows_next,
    /* xEof        */ rows_eof,
    /* xColumn     */ rows_column,
    /* xRowid      */ rows_rowid,
    /* xUpdate     */ NULL,
    /* xBegin      */ NULL,
    /* xSync       */ NULL,
    /* xCommit     */ NULL,
    /* xRollback   */ NULL,
    /* xFindFunction */ NULL,
    /* xRename     */ NULL,
    /* xSavepoint  */ NULL,
    /* xRelease    */ NULL,
    /* xRollbackTo */ NULL,
    /* xShadowName */ NULL,
    /* xIntegrity  */ NULL
};

typedef struct { gql_executor_getter getter; void *arg; } rows_registration;

int graphqlite_register_cypher_rows(sqlite3 *db, gql_executor_getter getter, void *getter_arg) {
    rows_registration *reg = sqlite3_malloc(sizeof(rows_registration));
    if (!reg) return SQLITE_NOMEM;
    reg->getter = getter;
    reg->arg = getter_arg;
    return sqlite3_create_module_v2(db, "cypher_rows", &cypher_rows_module, reg, sqlite3_free);
}
