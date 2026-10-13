/*
 * transform_helpers.h
 *    Shared helper functions for Cypher transformations
 *
 * This module consolidates common utility functions that were previously
 * duplicated across multiple transform_*.c files.
 */

#ifndef TRANSFORM_HELPERS_H
#define TRANSFORM_HELPERS_H

#include <stddef.h>

#include "parser/cypher_ast.h"

/*
 * Extract label string from a label AST node.
 * Label nodes are typically LITERAL nodes containing the label name.
 * Returns NULL if the node type is not recognized.
 */
const char *get_label_string(ast_node *label_node);

/*
 * Check if a node pattern has any labels defined.
 * Returns true if the node has a non-empty labels list.
 */
bool has_labels(cypher_node_pattern *node);

/* Render `name` as a SQL identifier: raw when it is a plain name and not a
 * reserved keyword, else "name" with `"` doubled. Returns `buf`. */
const char *sql_ident(char *buf, size_t buflen, const char *name);

/* The column part of an SQL reference `<table>.<column>`, quotes kept. */
const char *sql_ref_column(const char *ref);

/* True when the expression is boolean by its shape (comparison, logic,
 * predicate, boolean literal). */
bool ast_is_boolean_expr(const ast_node *expr);

/* Shortest round-trip text of a double; a whole number keeps ".0". */
const char *gql_format_double(char *buf, size_t buflen, double v);

/* True if `expr` evaluates to a Cypher boolean (comparison, AND/OR/NOT/XOR,
 * IS NULL, quantifier, EXISTS, boolean literal). Used to wrap projections in
 * _gql_bool_str so the result renders as a JSON boolean. Defined in
 * transform_return.c. */
bool ast_yields_boolean(ast_node *expr);

#endif /* TRANSFORM_HELPERS_H */
