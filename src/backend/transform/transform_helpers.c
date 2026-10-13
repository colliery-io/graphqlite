/*
 * transform_helpers.c
 *    Shared helper functions for Cypher transformations
 *
 * This module consolidates common utility functions that were previously
 * duplicated across multiple transform_*.c files.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "transform/transform_helpers.h"
#include "parser/cypher_ast.h"

/* Render a Cypher name as a SQL identifier. A plain name ([A-Za-z_][A-Za-z0-9_]*)
 * that is not a reserved keyword stays raw; any other name is double-quoted,
 * with `"` doubled, so backtick names like `node id` or `a.b` work as column
 * names (GQLITE-T-0396). Returns the buffer passed in. */
const char *sql_ident(char *buf, size_t buflen, const char *name)
{
    static const char *kw[] = {
        "values", "order", "group", "by", "limit", "offset", "where",
        "select", "from", "join", "on", "as", "and", "or",
        "not", "in", "is", "null", "true", "false", "between", "like",
        "case", "when", "then", "else", "end", "union", "intersect",
        "except", "all", "any", "some", "exists", "table", "view",
        "create", "drop", "alter", "insert", "update", "delete",
        "having", "with", "distinct", "default", "primary", "key",
        "foreign", "references", "check", "constraint", "index",
        NULL
    };
    if (!buf || buflen == 0) return buf;
    if (!name) name = "";
    bool plain = (name[0] != '\0') &&
                 !(name[0] >= '0' && name[0] <= '9');
    for (const char *p = name; plain && *p; p++) {
        char c = *p;
        plain = (c == '_') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9');
    }
    if (plain) {
        size_t nlen = strlen(name);
        for (int i = 0; kw[i]; i++) {
            size_t klen = strlen(kw[i]);
            if (nlen != klen) continue;
            bool match = true;
            for (size_t j = 0; j < klen; j++) {
                char a = name[j];
                if (a >= 'A' && a <= 'Z') a += 32;
                if (a != kw[i][j]) { match = false; break; }
            }
            if (match) { plain = false; break; }
        }
    }
    if (plain) {
        snprintf(buf, buflen, "%s", name);
        return buf;
    }
    /* Quoted; leave room for the closing quote and the terminator. */
    size_t o = 0;
    if (buflen < 3) { buf[0] = '\0'; return buf; }
    buf[o++] = '"';
    for (const char *p = name; *p; p++) {
        size_t need = (*p == '"') ? 2 : 1;
        if (o + need + 2 > buflen) break;
        if (*p == '"') buf[o++] = '"';
        buf[o++] = *p;
    }
    buf[o++] = '"';
    buf[o] = '\0';
    return buf;
}

/* The column part of an SQL reference `<table>.<column>`, as SQL: a quoted
 * column ("a.b", "q""q") is found by its quotes, not by the last dot. Returns
 * a pointer into `ref`, or `ref` itself when it has no table part. */
const char *sql_ref_column(const char *ref)
{
    if (!ref) return ref;
    size_t len = strlen(ref);
    if (len >= 2 && ref[len - 1] == '"') {
        /* Walk back to the opening quote; "" inside is an escaped quote. */
        size_t i = len - 1;
        while (i > 0) {
            i--;
            if (ref[i] != '"') continue;
            if (i > 0 && ref[i - 1] == '"') { i--; continue; }
            return ref + i;
        }
        return ref;
    }
    const char *dot = strrchr(ref, '.');
    return dot ? dot + 1 : ref;
}

/*
 * Extract label string from a label AST node.
 * Label nodes are typically LITERAL nodes containing the label name.
 * Returns NULL if the node type is not recognized.
 */
const char *get_label_string(ast_node *label_node)
{
    if (!label_node || label_node->type != AST_NODE_LITERAL) {
        return NULL;
    }

    cypher_literal *lit = (cypher_literal *)label_node;
    if (lit->literal_type != LITERAL_STRING) {
        return NULL;
    }

    return lit->value.string;
}

/*
 * Check if a node pattern has any labels defined.
 * Returns true if the node has a non-empty labels list.
 */
bool has_labels(cypher_node_pattern *node)
{
    return node && node->labels && node->labels->count > 0;
}
