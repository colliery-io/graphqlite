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

#include "transform/transform_helpers.h"
#include "parser/cypher_ast.h"

/* Quote a Cypher variable name if it conflicts with a SQLite reserved
 * keyword. Returns the buffer passed in, populated with either the raw
 * name or `"name"`. */
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
    bool reserved = false;
    if (name) {
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
            if (match) { reserved = true; break; }
        }
    }
    if (reserved) snprintf(buf, buflen, "\"%s\"", name);
    else          snprintf(buf, buflen, "%s", name ? name : "");
    return buf;
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
