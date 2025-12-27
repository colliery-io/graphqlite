/*
 * transform_func_dispatch.h
 *    Function dispatch table for Cypher function transformations
 *
 * Replaces the 280-line if-else chain with table-driven dispatch.
 */

#ifndef TRANSFORM_FUNC_DISPATCH_H
#define TRANSFORM_FUNC_DISPATCH_H

#include "transform/cypher_transform.h"
#include "parser/cypher_ast.h"

/*
 * Function handler signature.
 * All transform functions follow this pattern.
 */
typedef int (*transform_func_handler)(cypher_transform_context *ctx,
                                       cypher_function_call *func);

/*
 * Dispatch table entry.
 * Maps function name to its handler.
 */
typedef struct {
    const char *name;           /* Cypher function name (case-insensitive) */
    transform_func_handler handler;
} transform_func_entry;

/*
 * Look up a function handler by name.
 * Returns the handler function or NULL if not found.
 */
transform_func_handler lookup_function_handler(const char *function_name);

/*
 * Get the function dispatch table.
 * Returns pointer to the static dispatch table for introspection/testing.
 */
const transform_func_entry *get_function_dispatch_table(void);

/*
 * Get count of registered functions.
 */
int get_function_count(void);

#endif /* TRANSFORM_FUNC_DISPATCH_H */
