/*
 * transform_func_path.h
 *    Declarations for functions implemented in src/backend/transform/transform_func_path.c.
 *    Split out from transform_functions.h (I-0041 C5).
 */

#ifndef TRANSFORM_FUNC_PATH_H
#define TRANSFORM_FUNC_PATH_H

#include "transform/cypher_transform.h"
#include "parser/cypher_ast.h"

int transform_path_length_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_path_nodes_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_path_relationships_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_startnode_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_endnode_function(cypher_transform_context *ctx, cypher_function_call *func);
/* GQLITE-T-0181: emit only the endpoint node id of startNode(r)/endNode(r)
 * (for property access and id()); the functions above emit the node object. */
int transform_endpoint_node_id(cypher_transform_context *ctx, cypher_function_call *func);

#endif /* TRANSFORM_FUNC_PATH_H */
