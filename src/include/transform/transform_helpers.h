/*
 * transform_helpers.h
 *    Shared helper functions for Cypher transformations
 *
 * This module consolidates common utility functions that were previously
 * duplicated across multiple transform_*.c files.
 */

#ifndef TRANSFORM_HELPERS_H
#define TRANSFORM_HELPERS_H

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

#endif /* TRANSFORM_HELPERS_H */
