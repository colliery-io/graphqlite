/*
 * Transform Internal Header
 * Shared declarations for transform module internals
 */

#ifndef TRANSFORM_INTERNAL_H
#define TRANSFORM_INTERNAL_H

#include "transform/cypher_transform.h"

/* Forward declarations for expression transformers */
int transform_expression(cypher_transform_context *ctx, ast_node *expr);
int transform_property_access(cypher_transform_context *ctx, cypher_property *prop);
int transform_label_expression(cypher_transform_context *ctx, cypher_label_expr *label_expr);
int transform_not_expression(cypher_transform_context *ctx, cypher_not_expr *not_expr);
int transform_null_check(cypher_transform_context *ctx, cypher_null_check *null_check);
int transform_binary_operation(cypher_transform_context *ctx, cypher_binary_op *binary_op);
int transform_function_call(cypher_transform_context *ctx, cypher_function_call *func_call);

/* Clause transformers */
int transform_return_clause(cypher_transform_context *ctx, cypher_return *ret);
int transform_with_clause(cypher_transform_context *ctx, cypher_with *with);
int transform_unwind_clause(cypher_transform_context *ctx, cypher_unwind *unwind);

/* Pending property joins for aggregation optimization */
void add_pending_prop_join(const char *join_sql);

#endif /* TRANSFORM_INTERNAL_H */
