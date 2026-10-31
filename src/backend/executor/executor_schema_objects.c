/*
 * executor_schema_objects.c
 *    CREATE / DROP / SHOW of indexes and constraints (GQLITE-T-0008).
 *
 *    The statements are parsed here, not by the grammar, so INDEX and
 *    CONSTRAINT do not become keywords (`n.index` stays a name). They are
 *    stored in the table graphqlite_schema_objects.
 *
 *    Every property is already indexed by (key, value, node), so an index
 *    is a record: SHOW INDEXES lists it, and it changes no query plan.
 *    A constraint is checked after each write query, inside the query's
 *    savepoint: a violation fails the query and undoes its writes.
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "executor/cypher_executor.h"
#include "executor/executor_internal.h"
#include "runtime/gql_error.h"

#define SCHEMA_TABLE "graphqlite_schema_objects"

/* ---- tokens ------------------------------------------------------------ */

typedef struct {
    char text[256];
    char sym;            /* one of ( ) : . , ; or 0 for a word */
} sch_tok;

typedef struct {
    sch_tok *t;
    int n, i;
} sch_toks;

static int sch_tokenize(const char *q, sch_toks *out)
{
    int cap = 32;
    out->t = calloc((size_t)cap, sizeof(sch_tok));
    out->n = out->i = 0;
    if (!out->t) return -1;
    const char *p = q;
    while (*p) {
        if (isspace((unsigned char)*p)) { p++; continue; }
        if (out->n == cap) {
            cap *= 2;
            sch_tok *g = realloc(out->t, (size_t)cap * sizeof(sch_tok));
            if (!g) return -1;
            out->t = g;
        }
        sch_tok *tk = &out->t[out->n];
        memset(tk, 0, sizeof(*tk));
        if (strchr("():.,;-[]<>", *p)) {
            tk->sym = *p++;
        } else if (*p == '`') {
            const char *s = ++p;
            size_t k = 0;
            while (*p && *p != '`' && k + 1 < sizeof(tk->text)) tk->text[k++] = *p++;
            if (*p != '`') return -1;
            p++;
            (void)s;
        } else if (isalnum((unsigned char)*p) || *p == '_') {
            size_t k = 0;
            while ((isalnum((unsigned char)*p) || *p == '_') && k + 1 < sizeof(tk->text)) tk->text[k++] = *p++;
        } else {
            return -1;      /* not a schema statement token */
        }
        out->n++;
    }
    return 0;
}

static bool sch_word(sch_toks *ts, const char *w)
{
    if (ts->i < ts->n && !ts->t[ts->i].sym && strcasecmp(ts->t[ts->i].text, w) == 0) { ts->i++; return true; }
    return false;
}

static bool sch_sym(sch_toks *ts, char c)
{
    if (ts->i < ts->n && ts->t[ts->i].sym == c) { ts->i++; return true; }
    return false;
}

static const char *sch_name(sch_toks *ts)
{
    if (ts->i < ts->n && !ts->t[ts->i].sym && ts->t[ts->i].text[0]) return ts->t[ts->i++].text;
    return NULL;
}

/* ---- the statement --------------------------------------------------- */

typedef enum { SCH_CREATE, SCH_DROP, SCH_SHOW } sch_verb;

typedef struct {
    sch_verb verb;
    bool constraint;          /* else an index */
    char name[256];
    bool if_flag;             /* IF NOT EXISTS / IF EXISTS */
    char label[256];
    char props[8][256];
    int nprops;
    char ctype[32];           /* UNIQUE or NOT_NULL */
} sch_stmt;

/* `(v:Label)` */
static bool sch_node(sch_toks *ts, char *var, char *label)
{
    if (!sch_sym(ts, '(')) return false;
    const char *v = sch_name(ts);
    if (!v || !sch_sym(ts, ':')) return false;
    const char *l = sch_name(ts);
    if (!l || !sch_sym(ts, ')')) return false;
    snprintf(var, 256, "%s", v);
    snprintf(label, 256, "%s", l);
    return true;
}

/* `v.p` */
static bool sch_prop(sch_toks *ts, const char *var, sch_stmt *st)
{
    const char *v = sch_name(ts);
    if (!v || strcmp(v, var) != 0 || !sch_sym(ts, '.')) return false;
    const char *p = sch_name(ts);
    if (!p || st->nprops >= 8) return false;
    snprintf(st->props[st->nprops++], 256, "%s", p);
    return true;
}

/* `v.p` or `(v.p, v.q)` */
static bool sch_prop_list(sch_toks *ts, const char *var, sch_stmt *st)
{
    if (sch_sym(ts, '(')) {
        do { if (!sch_prop(ts, var, st)) return false; } while (sch_sym(ts, ','));
        return sch_sym(ts, ')');
    }
    return sch_prop(ts, var, st);
}

/* After REQUIRE / ASSERT: `v.p IS UNIQUE`, `v.p IS NOT NULL`, `EXISTS (v.p)`. */
static bool sch_rule(sch_toks *ts, const char *var, sch_stmt *st)
{
    if (sch_word(ts, "EXISTS")) {
        if (!sch_sym(ts, '(') || !sch_prop(ts, var, st) || !sch_sym(ts, ')')) return false;
        snprintf(st->ctype, sizeof(st->ctype), "NOT_NULL");
        return true;
    }
    if (!sch_prop_list(ts, var, st) || !sch_word(ts, "IS")) return false;
    if (sch_word(ts, "UNIQUE")) { snprintf(st->ctype, sizeof(st->ctype), "UNIQUE"); return true; }
    if (sch_word(ts, "NOT") && sch_word(ts, "NULL")) {
        if (st->nprops != 1) return false;
        snprintf(st->ctype, sizeof(st->ctype), "NOT_NULL");
        return true;
    }
    return false;
}

/* 1 when q is a schema statement (st filled), 0 when it is not one, -1 when
 * it is one with a syntax error (msg set). */
static int sch_parse(const char *q, sch_stmt *st, char *msg, size_t msglen)
{
    memset(st, 0, sizeof(*st));
    sch_toks ts;
    if (sch_tokenize(q, &ts) < 0) { free(ts.t); return 0; }
    int rc = 0;
    char var[256] = "";
    if (sch_word(&ts, "SHOW")) {
        sch_word(&ts, "ALL");
        sch_word(&ts, "RANGE");
        if (sch_word(&ts, "INDEX") || sch_word(&ts, "INDEXES")) { st->verb = SCH_SHOW; rc = 1; }
        else if (sch_word(&ts, "CONSTRAINT") || sch_word(&ts, "CONSTRAINTS")) { st->verb = SCH_SHOW; st->constraint = true; rc = 1; }
        /* words after it were ignored (`SHOW INDEXES YIELD name WHERE ...`
         * ran as a plain SHOW, GQLITE-T-0447) */
        if (rc == 1) {
            sch_sym(&ts, ';');
            if (ts.i < ts.n) {
                snprintf(msg, msglen, "SyntaxError: UnexpectedSyntax: SHOW %s takes nothing after it",
                         st->constraint ? "CONSTRAINTS" : "INDEXES");
                rc = -1;
            }
        }
        goto done;
    }
    bool create = sch_word(&ts, "CREATE");
    bool drop = !create && sch_word(&ts, "DROP");
    if (!create && !drop) goto done;
    if (sch_word(&ts, "INDEX")) st->constraint = false;
    else if (sch_word(&ts, "CONSTRAINT")) st->constraint = true;
    else goto done;                       /* CREATE (n) ... is a query */
    st->verb = create ? SCH_CREATE : SCH_DROP;
    rc = -1;                              /* from here a statement: errors are syntax errors */

    /* a name, unless the next word starts the definition */
    if (ts.i < ts.n && !ts.t[ts.i].sym &&
        strcasecmp(ts.t[ts.i].text, "IF") != 0 && strcasecmp(ts.t[ts.i].text, "FOR") != 0 &&
        strcasecmp(ts.t[ts.i].text, "ON") != 0)
        snprintf(st->name, sizeof(st->name), "%s", ts.t[ts.i++].text);
    if (sch_word(&ts, "IF")) {
        if (create && !sch_word(&ts, "NOT")) goto done;
        if (!sch_word(&ts, "EXISTS")) goto done;
        st->if_flag = true;
    }
    if (drop && st->name[0] && ts.i == ts.n) { rc = 1; goto done; }
    if (drop && st->name[0] && sch_sym(&ts, ';') && ts.i == ts.n) { rc = 1; goto done; }

    if (!st->constraint) {
        if (sch_word(&ts, "FOR")) {
            if (ts.i < ts.n && ts.t[ts.i].sym == '(' && ts.i + 1 < ts.n && ts.t[ts.i + 1].sym == ')') {
                snprintf(msg, msglen, "SyntaxError: UnsupportedFeature: relationship property indexes are not supported");
                goto done;
            }
            if (!sch_node(&ts, var, st->label) || !sch_word(&ts, "ON") || !sch_sym(&ts, '(')) goto done;
            do { if (!sch_prop(&ts, var, st)) goto done; } while (sch_sym(&ts, ','));
            if (!sch_sym(&ts, ')')) goto done;
        } else if (sch_word(&ts, "ON")) {
            /* old form `ON :Label(p, q)` */
            if (!sch_sym(&ts, ':')) goto done;
            const char *l = sch_name(&ts);
            if (!l || !sch_sym(&ts, '(')) goto done;
            snprintf(st->label, sizeof(st->label), "%s", l);
            do {
                const char *p = sch_name(&ts);
                if (!p || st->nprops >= 8) goto done;
                snprintf(st->props[st->nprops++], 256, "%s", p);
            } while (sch_sym(&ts, ','));
            if (!sch_sym(&ts, ')')) goto done;
        } else {
            goto done;
        }
    } else {
        bool old = false;
        if (sch_word(&ts, "FOR")) old = false;
        else if (sch_word(&ts, "ON")) old = true;
        else goto done;
        if (ts.i < ts.n && ts.t[ts.i].sym == '(' && ts.i + 1 < ts.n && ts.t[ts.i + 1].sym == ')') {
            snprintf(msg, msglen, "SyntaxError: UnsupportedFeature: relationship constraints are not supported");
            goto done;
        }
        if (!sch_node(&ts, var, st->label)) goto done;
        if (!(old ? sch_word(&ts, "ASSERT") : sch_word(&ts, "REQUIRE"))) goto done;
        if (!sch_rule(&ts, var, st)) goto done;
    }
    sch_sym(&ts, ';');
    if (ts.i != ts.n) goto done;
    rc = 1;
done:
    if (rc < 0 && !msg[0])
        snprintf(msg, msglen, "SyntaxError: UnexpectedSyntax: invalid %s %s statement",
                 st->verb == SCH_DROP ? "DROP" : "CREATE", st->constraint ? "CONSTRAINT" : "INDEX");
    free(ts.t);
    return rc;
}

/* ---- storage ------------------------------------------------------------ */

static int sch_ensure_table(sqlite3 *db)
{
    return sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS " SCHEMA_TABLE " ("
        "  name TEXT PRIMARY KEY,"
        "  kind TEXT NOT NULL,"          /* index | constraint */
        "  type TEXT NOT NULL,"          /* RANGE | UNIQUE | NOT_NULL */
        "  label TEXT NOT NULL,"
        "  properties TEXT NOT NULL)",   /* JSON list */
        NULL, NULL, NULL) == SQLITE_OK ? 0 : -1;
}

static bool sch_table_exists(sqlite3 *db)
{
    sqlite3_stmt *st = NULL;
    bool yes = false;
    if (sqlite3_prepare_v2(db, "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = '" SCHEMA_TABLE "'",
                           -1, &st, NULL) == SQLITE_OK)
        yes = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_finalize(st);
    return yes;
}

static char *sch_props_json(const sch_stmt *s)
{
    size_t cap = 16;
    for (int i = 0; i < s->nprops; i++) cap += strlen(s->props[i]) * 2 + 4;
    char *j = malloc(cap), *o = j;
    if (!j) return NULL;
    *o++ = '[';
    for (int i = 0; i < s->nprops; i++) {
        if (i) *o++ = ',';
        *o++ = '"';
        for (const char *c = s->props[i]; *c; c++) { if (*c == '"' || *c == '\\') *o++ = '\\'; *o++ = *c; }
        *o++ = '"';
    }
    *o++ = ']'; *o = 0;
    return j;
}

/* The SQL key of a node's property `k` (`node_id` column of alias a):
 * a typed text; NULL when absent. An integer is its exact value, and a
 * whole-number float the same integer, so 1 = 1.0; a REAL key made ints
 * above 2^53 collide (GQLITE-T-0441). */
static char *sch_key_expr(const char *a, const char *k)
{
    char *ek = sqlite3_mprintf("%q", k);
    char *e = sqlite3_mprintf(
        "COALESCE("
        "(SELECT 't:' || p.value FROM node_props_text p WHERE p.node_id = %s.node_id AND p.key_id = (SELECT id FROM property_keys WHERE key = '%s')),"
        "(SELECT 'n:' || CAST(p.value AS INTEGER) FROM node_props_int p WHERE p.node_id = %s.node_id AND p.key_id = (SELECT id FROM property_keys WHERE key = '%s')),"
        "(SELECT 'n:' || CASE WHEN abs(p.value) < 9.0e18 AND p.value = CAST(p.value AS INTEGER) "
        "THEN CAST(CAST(p.value AS INTEGER) AS TEXT) ELSE CAST(p.value AS TEXT) END "
        "FROM node_props_real p WHERE p.node_id = %s.node_id AND p.key_id = (SELECT id FROM property_keys WHERE key = '%s')),"
        "(SELECT 'b:' || p.value FROM node_props_bool p WHERE p.node_id = %s.node_id AND p.key_id = (SELECT id FROM property_keys WHERE key = '%s')),"
        "(SELECT 'j:' || p.value FROM node_props_json p WHERE p.node_id = %s.node_id AND p.key_id = (SELECT id FROM property_keys WHERE key = '%s')))",
        a, ek, a, ek, a, ek, a, ek, a, ek);
    sqlite3_free(ek);
    return e;
}

/* NULL when the data meets the constraint, else the error (owned). */
static char *sch_check_one(sqlite3 *db, const char *name, const char *type, const char *label,
                           char props[][256], int nprops)
{
    if (!sch_table_exists(db)) { /* fall through: called also before the table is made */ }
    bool have_nodes = false;
    {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db, "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'node_labels'", -1, &st, NULL) == SQLITE_OK)
            have_nodes = sqlite3_step(st) == SQLITE_ROW;
        sqlite3_finalize(st);
    }
    if (!have_nodes || nprops == 0) return NULL;
    char *el = sqlite3_mprintf("%q", label);
    char *sql = NULL;
    if (strcmp(type, "NOT_NULL") == 0) {
        char *ke = sch_key_expr("l", props[0]);
        sql = sqlite3_mprintf("SELECT l.node_id FROM node_labels l WHERE l.label = '%s' AND %s IS NULL LIMIT 1", el, ke);
        sqlite3_free(ke);
    } else {
        /* all the properties present: one key, unit separator between */
        char *keys = sqlite3_mprintf("%s", "");
        char *present = sqlite3_mprintf("%s", "1");
        for (int i = 0; i < nprops; i++) {
            char *ke = sch_key_expr("l", props[i]);
            char *k2 = sqlite3_mprintf("%s%s(%s)", keys, i ? " || char(31) || " : "", ke);
            char *p2 = sqlite3_mprintf("%s AND (%s) IS NOT NULL", present, ke);
            sqlite3_free(keys); sqlite3_free(present); sqlite3_free(ke);
            keys = k2; present = p2;
        }
        sql = sqlite3_mprintf("SELECT k, n FROM (SELECT %s AS k, count(*) AS n FROM node_labels l "
                              "WHERE l.label = '%s' AND %s GROUP BY k) WHERE n > 1 LIMIT 1",
                              keys, el, present);
        sqlite3_free(keys); sqlite3_free(present);
    }
    sqlite3_free(el);
    char *err = NULL;
    sqlite3_stmt *st = NULL;
    int src = sql ? sqlite3_prepare_v2(db, sql, -1, &st, NULL) : SQLITE_NOMEM;
    if (src != SQLITE_OK) {
        /* a check that cannot run fails the write; it passed (GQLITE-T-0445) */
        char buf[700];
        snprintf(buf, sizeof(buf), "ConstraintVerificationFailed: could not check constraint %s: %s",
                 name, sql ? sqlite3_errmsg(db) : "out of memory");
        err = strdup(buf);
    } else {
        int step = sqlite3_step(st);
        if (step != SQLITE_ROW && step != SQLITE_DONE) {
            char buf[700];
            snprintf(buf, sizeof(buf), "ConstraintVerificationFailed: could not check constraint %s: %s",
                     name, sqlite3_errmsg(db));
            err = strdup(buf);
        }
        if (step == SQLITE_ROW) {
            char buf[700];
            if (strcmp(type, "NOT_NULL") == 0) {
                snprintf(buf, sizeof(buf),
                         "ConstraintVerificationFailed: ConstraintValidationFailed: node(%lld) with label `%s` must have the property `%s` (constraint %s)",
                         (long long)sqlite3_column_int64(st, 0), label, props[0], name);
            } else {
                const char *k = (const char *)sqlite3_column_text(st, 0);
                const char *v = k && strlen(k) > 2 ? k + 2 : "";
                snprintf(buf, sizeof(buf),
                         "ConstraintVerificationFailed: ConstraintValidationFailed: two nodes with label `%s` have the same value of `%s`%s (constraint %s): %.200s",
                         label, props[0], nprops > 1 ? " and the other key properties" : "", name, v);
            }
            err = strdup(buf);
        }
    }
    sqlite3_finalize(st);
    sqlite3_free(sql);
    return err;
}

char *schema_check_constraints(cypher_executor *executor)
{
    if (!executor || !executor->db || !sch_table_exists(executor->db)) return NULL;
    sqlite3_stmt *st = NULL;
    char *err = NULL;
    if (sqlite3_prepare_v2(executor->db,
            "SELECT name, type, label, properties FROM " SCHEMA_TABLE " WHERE kind = 'constraint'",
            -1, &st, NULL) != SQLITE_OK) {
        char buf[600];
        snprintf(buf, sizeof(buf), "ConstraintVerificationFailed: could not read the constraints: %s",
                 sqlite3_errmsg(executor->db));
        return strdup(buf);
    }
    while (!err && sqlite3_step(st) == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(st, 0);
        const char *type = (const char *)sqlite3_column_text(st, 1);
        const char *label = (const char *)sqlite3_column_text(st, 2);
        const char *pj = (const char *)sqlite3_column_text(st, 3);
        char props[8][256];
        int np = 0;
        sqlite3_stmt *js = NULL;
        if (sqlite3_prepare_v2(executor->db, "SELECT value FROM json_each(?1)", -1, &js, NULL) == SQLITE_OK) {
            sqlite3_bind_text(js, 1, pj ? pj : "[]", -1, SQLITE_TRANSIENT);
            while (np < 8 && sqlite3_step(js) == SQLITE_ROW)
                snprintf(props[np++], 256, "%s", (const char *)sqlite3_column_text(js, 0));
        }
        sqlite3_finalize(js);
        err = sch_check_one(executor->db, name ? name : "", type ? type : "", label ? label : "", props, np);
    }
    sqlite3_finalize(st);
    return err;
}

/* ---- the commands ------------------------------------------------------- */

static void sch_default_name(sch_stmt *s)
{
    if (s->name[0]) return;
    char b[256];
    snprintf(b, sizeof(b), "%s_%s", s->constraint ? "constraint" : "index", s->label);
    for (int i = 0; i < s->nprops; i++) {
        size_t n = strlen(b);
        snprintf(b + n, sizeof(b) - n, "_%s", s->props[i]);
    }
    if (s->constraint) {
        size_t n = strlen(b);
        snprintf(b + n, sizeof(b) - n, "_%s", strcmp(s->ctype, "UNIQUE") == 0 ? "unique" : "not_null");
    }
    snprintf(s->name, sizeof(s->name), "%s", b);
}

static int sch_create(cypher_executor *ex, sch_stmt *s, cypher_result *r)
{
    if (sch_ensure_table(ex->db) < 0) { set_result_error(r, "Failed to create the schema table"); return -1; }
    sch_default_name(s);
    char *pj = sch_props_json(s);
    const char *type = s->constraint ? s->ctype : "RANGE";
    const char *kind = s->constraint ? "constraint" : "index";
    /* same name, or the same rule under another name */
    sqlite3_stmt *st = NULL;
    int found = 0;   /* 1 same name, 2 equivalent rule */
    if (sqlite3_prepare_v2(ex->db,
            "SELECT CASE WHEN name = ?1 THEN 1 ELSE 2 END FROM " SCHEMA_TABLE
            " WHERE name = ?1 OR (kind = ?2 AND type = ?3 AND label = ?4 AND properties = ?5) ORDER BY 1 LIMIT 1",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, s->name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, kind, -1, SQLITE_STATIC);
        sqlite3_bind_text(st, 3, type, -1, SQLITE_STATIC);
        sqlite3_bind_text(st, 4, s->label, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 5, pj, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) found = sqlite3_column_int(st, 0);
    }
    sqlite3_finalize(st);
    if (found) {
        free(pj);
        if (s->if_flag) { r->success = true; return 0; }
        char msg[600];
        snprintf(msg, sizeof(msg), found == 1
                 ? "SchemaError: EquivalentSchemaRuleAlreadyExists: there is already an index or constraint named `%s`"
                 : "SchemaError: EquivalentSchemaRuleAlreadyExists: an equivalent %s already exists (%s)",
                 found == 1 ? s->name : kind, s->name);
        set_result_error(r, msg);
        return -1;
    }
    if (s->constraint) {
        char *err = sch_check_one(ex->db, s->name, s->ctype, s->label, s->props, s->nprops);
        if (err) {
            char msg[900];
            snprintf(msg, sizeof(msg), "%s; the data does not meet the new constraint", err);
            free(err); free(pj);
            set_result_error(r, msg);
            return -1;
        }
    }
    /* A store that fails (a read-only or busy database) is an error: the
     * schema object would not exist and never be enforced (GQLITE-T-0441). */
    int rc = sqlite3_prepare_v2(ex->db, "INSERT INTO " SCHEMA_TABLE " (name, kind, type, label, properties) VALUES (?, ?, ?, ?, ?)",
                                -1, &st, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(st, 1, s->name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, kind, -1, SQLITE_STATIC);
        sqlite3_bind_text(st, 3, type, -1, SQLITE_STATIC);
        sqlite3_bind_text(st, 4, s->label, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 5, pj, -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(st) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
    }
    sqlite3_finalize(st);
    free(pj);
    if (rc != SQLITE_OK) {
        char msg[600];
        snprintf(msg, sizeof(msg), "Failed to store the %s: %s", kind, sqlite3_errmsg(ex->db));
        set_result_error(r, msg);
        return -1;
    }
    r->success = true;
    return 0;
}

static int sch_drop(cypher_executor *ex, sch_stmt *s, cypher_result *r)
{
    int changed = 0;
    int drop_rc = SQLITE_OK;
    if (sch_table_exists(ex->db)) {
        sqlite3_stmt *st = NULL;
        const char *kind = s->constraint ? "constraint" : "index";
        if (s->name[0]) {
            if (sqlite3_prepare_v2(ex->db, "DELETE FROM " SCHEMA_TABLE " WHERE name = ?1 AND kind = ?2", -1, &st, NULL) == SQLITE_OK) {
                sqlite3_bind_text(st, 1, s->name, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(st, 2, kind, -1, SQLITE_STATIC);
                if (sqlite3_step(st) != SQLITE_DONE) drop_rc = SQLITE_ERROR;
            } else {
                drop_rc = SQLITE_ERROR;
            }
        } else {
            /* old form: by its definition */
            char *pj = sch_props_json(s);
            if (sqlite3_prepare_v2(ex->db, "DELETE FROM " SCHEMA_TABLE " WHERE kind = ?1 AND label = ?2 AND properties = ?3"
                                   " AND (?4 IS NULL OR type = ?4)", -1, &st, NULL) == SQLITE_OK) {
                sqlite3_bind_text(st, 1, kind, -1, SQLITE_STATIC);
                sqlite3_bind_text(st, 2, s->label, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(st, 3, pj, -1, SQLITE_TRANSIENT);
                if (s->constraint) sqlite3_bind_text(st, 4, s->ctype, -1, SQLITE_TRANSIENT);
                else sqlite3_bind_null(st, 4);
                if (sqlite3_step(st) != SQLITE_DONE) drop_rc = SQLITE_ERROR;
            } else {
                drop_rc = SQLITE_ERROR;
            }
            free(pj);
        }
        sqlite3_finalize(st);
        if (drop_rc != SQLITE_OK) {
            char msg[600];
            snprintf(msg, sizeof(msg), "Failed to drop the %s: %s", kind, sqlite3_errmsg(ex->db));
            set_result_error(r, msg);
            return -1;
        }
        changed = sqlite3_changes(ex->db);
    }
    if (!changed && !s->if_flag) {
        char msg[400];
        snprintf(msg, sizeof(msg), "SchemaError: %s: there is no %s %s",
                 s->constraint ? "ConstraintDropFailed" : "IndexDropFailed",
                 s->constraint ? "constraint" : "index", s->name[0] ? s->name : "with that definition");
        set_result_error(r, msg);
        return -1;
    }
    r->success = true;
    return 0;
}

static int sch_show(cypher_executor *ex, sch_stmt *s, cypher_result *r)
{
    static const char *cols[] = {"name", "type", "entityType", "labelsOrTypes", "properties"};
    r->column_names = calloc(5, sizeof(char *));
    if (!r->column_names) { set_result_error(r, "SHOW: out of memory"); return -1; }
    r->column_count = 5;
    for (int i = 0; i < 5; i++) r->column_names[i] = strdup(cols[i]);
    r->success = true;
    if (!sch_table_exists(ex->db)) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(ex->db,
            "SELECT name, CASE type WHEN 'UNIQUE' THEN 'UNIQUENESS' WHEN 'NOT_NULL' THEN 'NODE_PROPERTY_EXISTENCE' ELSE type END,"
            " json_array(label), properties FROM " SCHEMA_TABLE " WHERE kind = ?1 ORDER BY name",
            -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, s->constraint ? "constraint" : "index", -1, SQLITE_STATIC);
    int cap = 8;
    r->data = calloc((size_t)cap, sizeof(char **));
    r->data_types = calloc((size_t)cap, sizeof(int *));
    bool oom = !r->data || !r->data_types;
    while (!oom && sqlite3_step(st) == SQLITE_ROW) {
        if (r->row_count == cap) {
            char ***gd = realloc(r->data, (size_t)cap * 2 * sizeof(char **));
            if (gd) r->data = gd;
            int **gt = realloc(r->data_types, (size_t)cap * 2 * sizeof(int *));
            if (gt) r->data_types = gt;
            if (!gd || !gt) { oom = true; break; }
            cap *= 2;
        }
        char **row = calloc(5, sizeof(char *));
        int *ty = calloc(5, sizeof(int));
        if (!row || !ty) { free(row); free(ty); oom = true; break; }
        row[0] = strdup((const char *)sqlite3_column_text(st, 0)); ty[0] = GQL_COL_TYPE_STRING;
        row[1] = strdup((const char *)sqlite3_column_text(st, 1)); ty[1] = GQL_COL_TYPE_STRING;
        row[2] = strdup("NODE");                                   ty[2] = GQL_COL_TYPE_STRING;
        row[3] = strdup((const char *)sqlite3_column_text(st, 2)); ty[3] = GQL_COL_TYPE_JSON;
        row[4] = strdup((const char *)sqlite3_column_text(st, 3)); ty[4] = GQL_COL_TYPE_JSON;
        r->data[r->row_count] = row;
        r->data_types[r->row_count] = ty;
        r->row_count++;
    }
    sqlite3_finalize(st);
    if (oom) { set_result_error(r, "SHOW: out of memory"); return -1; }
    return 0;
}

int schema_command_check(const char *query, char *msg, size_t msglen)
{
    if (!query) return 0;
    const char *p = query;
    while (isspace((unsigned char)*p)) p++;
    if (strncasecmp(p, "CREATE", 6) != 0 && strncasecmp(p, "DROP", 4) != 0 && strncasecmp(p, "SHOW", 4) != 0)
        return 0;
    sch_stmt s;
    msg[0] = 0;
    return sch_parse(query, &s, msg, msglen);
}

cypher_result *schema_command_try(cypher_executor *executor, const char *query)
{
    if (!executor || !query) return NULL;
    /* quick: the first word */
    const char *p = query;
    while (isspace((unsigned char)*p)) p++;
    if (strncasecmp(p, "CREATE", 6) != 0 && strncasecmp(p, "DROP", 4) != 0 && strncasecmp(p, "SHOW", 4) != 0)
        return NULL;
    sch_stmt s;
    char msg[400] = "";
    int rc = sch_parse(query, &s, msg, sizeof(msg));
    if (rc == 0) return NULL;
    cypher_result *r = create_empty_result();
    if (!r) return NULL;
    if (rc < 0) { set_result_error_ex(r, msg, GQL_ERR_PARSE, 0, 0); return r; }
    switch (s.verb) {
        case SCH_CREATE: sch_create(executor, &s, r); break;
        case SCH_DROP:   sch_drop(executor, &s, r); break;
        case SCH_SHOW:   sch_show(executor, &s, r); break;
    }
    return r;
}
