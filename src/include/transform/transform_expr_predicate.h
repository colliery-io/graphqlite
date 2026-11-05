/*
 * transform_expr_predicate.h
 *    Declarations for predicate/list-comprehension expressions
 *    (EXISTS{}, list predicates ALL/ANY/NONE/SINGLE, reduce()).
 *    Split out from transform_functions.h (I-0041 C5).
 */

#ifndef TRANSFORM_EXPR_PREDICATE_H
#define TRANSFORM_EXPR_PREDICATE_H

#include "transform/cypher_transform.h"
#include "parser/cypher_ast.h"

int transform_exists_expression(cypher_transform_context *ctx, cypher_exists_expr *exists_expr);
/* COUNT { ... }: the parser's __gql_count_subquery(EXISTS { query }) (GQLITE-T-0485). */
int transform_count_subquery(cypher_transform_context *ctx, cypher_function_call *fc);
int transform_list_predicate(cypher_transform_context *ctx, cypher_list_predicate *pred);
int transform_reduce_expr(cypher_transform_context *ctx, cypher_reduce_expr *reduce);

#endif /* TRANSFORM_EXPR_PREDICATE_H */
