/*
 * transform_helpers.c
 *    Shared helper functions for Cypher transformations
 *
 * This module consolidates common utility functions that were previously
 * duplicated across multiple transform_*.c files.
 */

#include <stdlib.h>

#include "transform/transform_helpers.h"
#include "parser/cypher_ast.h"

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
