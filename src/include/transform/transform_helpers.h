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

/* Quote `name` as a SQL identifier when it conflicts with a reserved
 * keyword. Returns `buf`. Always populates `buf` (raw name or "name"). */
const char *sql_ident(char *buf, size_t buflen, const char *name);

#endif /* TRANSFORM_HELPERS_H */
