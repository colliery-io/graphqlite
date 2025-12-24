/*
 * transform_functions.h
 *    Declarations for Cypher function transformations
 *
 * This header declares functions extracted from transform_return.c
 * for better code organization and maintainability.
 */

#ifndef TRANSFORM_FUNCTIONS_H
#define TRANSFORM_FUNCTIONS_H

#include "transform/cypher_transform.h"
#include "parser/cypher_ast.h"

/*
 * String functions - transform_func_string.c
 */
int transform_string_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_substring_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_replace_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_split_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_leftright_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_pattern_match_function(cypher_transform_context *ctx, cypher_function_call *func);

/*
 * Math functions - transform_func_math.c
 */
int transform_math_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_round_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_noarg_function(cypher_transform_context *ctx, cypher_function_call *func);

/*
 * Entity introspection - transform_func_entity.c
 */
int transform_id_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_labels_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_properties_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_keys_function(cypher_transform_context *ctx, cypher_function_call *func);

/*
 * Path functions - transform_func_path.c
 */
int transform_path_length_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_path_nodes_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_path_relationships_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_startnode_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_endnode_function(cypher_transform_context *ctx, cypher_function_call *func);

/*
 * List and utility functions - transform_func_list.c
 */
int transform_coalesce_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_tostring_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_type_conversion_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_list_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_range_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_collect_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_timestamp_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_randomuuid_function(cypher_transform_context *ctx, cypher_function_call *func);

/*
 * Aggregate functions - transform_func_aggregate.c
 */
int transform_count_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_aggregate_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_aggregate_with_property(cypher_transform_context *ctx, cypher_function_call *func, cypher_property *prop);
int transform_type_function(cypher_transform_context *ctx, cypher_function_call *func);

/*
 * Predicate expressions - transform_expr_predicate.c
 */
int transform_exists_expression(cypher_transform_context *ctx, cypher_exists_expr *exists_expr);
int transform_list_predicate(cypher_transform_context *ctx, cypher_list_predicate *pred);
int transform_reduce_expr(cypher_transform_context *ctx, cypher_reduce_expr *reduce);

/*
 * Graph algorithms - transform_func_graph.c
 */
int transform_pagerank_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_top_pagerank_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_personalized_pagerank_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_label_propagation_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_community_of_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_community_members_function(cypher_transform_context *ctx, cypher_function_call *func);
int transform_community_count_function(cypher_transform_context *ctx, cypher_function_call *func);

#endif /* TRANSFORM_FUNCTIONS_H */
