#ifndef GRAPHQLITE_EXPRESSION_H
#define GRAPHQLITE_EXPRESSION_H

#include <sqlite3.h>
#include "graphqlite.h"
#include "ast.h"

/**
 * Expression Evaluation Engine
 * 
 * This module handles the evaluation of WHERE clause expressions and other
 * OpenCypher expressions in GraphQLite. It provides a complete evaluation
 * engine for property comparisons, logical operations, and value lookups.
 * 
 * Features:
 * - Recursive expression evaluation
 * - Property value lookups for nodes and edges
 * - Type-safe comparisons (integer, float, text, boolean)
 * - Logical operations (AND, OR, NOT)
 * - NULL handling and IS NULL/IS NOT NULL expressions
 */

/**
 * Evaluation result type for expression values.
 */
typedef struct {
    graphqlite_value_type_t type;
    union {
        int64_t integer;
        double float_val;
        char *text;
        int boolean;
    } data;
} eval_result_t;

/**
 * Variable binding for expression evaluation.
 * Links variable names to actual entity IDs in the database.
 */
typedef struct {
    char *variable_name;    // e.g., "n", "r"
    int64_t node_id;        // ID if it's a node
    int64_t edge_id;        // ID if it's an edge
    int is_edge;            // 1 if edge, 0 if node
} variable_binding_t;

/**
 * Create an evaluation result of the specified type.
 * 
 * @param type The type of evaluation result to create
 * @return Allocated eval_result_t, or NULL on failure
 */
eval_result_t* create_eval_result(graphqlite_value_type_t type);

/**
 * Free an evaluation result and its associated memory.
 * 
 * @param result Evaluation result to free (can be NULL)
 */
void free_eval_result(eval_result_t *result);

/**
 * Look up property value for a variable binding.
 * 
 * This function searches the appropriate property tables (node or edge)
 * for the specified property and returns its typed value.
 * 
 * @param db SQLite database handle
 * @param binding Variable binding containing entity ID and type
 * @param property Property name to look up
 * @return eval_result_t with property value, or NULL if not found
 */
eval_result_t* lookup_property_value(sqlite3 *db, variable_binding_t *binding, const char *property);

/**
 * Compare two evaluation results using the specified operator.
 * 
 * Performs type-safe comparisons between evaluation results.
 * Handles NULL values and type mismatches appropriately.
 * 
 * @param left Left operand for comparison
 * @param right Right operand for comparison
 * @param op Comparison operator (EQ, NEQ, LT, GT, LE, GE)
 * @return 1 if comparison is true, 0 if false
 */
int compare_eval_results(eval_result_t *left, eval_result_t *right, ast_operator_t op);

/**
 * Evaluate an expression recursively.
 * 
 * This is the main entry point for expression evaluation. It handles
 * all OpenCypher expression types including literals, property access,
 * binary operations, unary operations, and NULL checks.
 * 
 * @param db SQLite database handle
 * @param expr AST node representing the expression to evaluate
 * @param bindings Array of variable bindings for the current context
 * @param binding_count Number of bindings in the array
 * @return eval_result_t with expression result, or NULL on error
 */
eval_result_t* evaluate_expression(sqlite3 *db, cypher_ast_node_t *expr, variable_binding_t *bindings, int binding_count);

#endif // GRAPHQLITE_EXPRESSION_H