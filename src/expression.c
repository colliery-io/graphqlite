/* GraphQLite Expression Evaluation Engine */
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "expression.h"
#include "property.h"
#include "graphqlite.h"
#include "ast.h"

eval_result_t* create_eval_result(graphqlite_value_type_t type) {
    eval_result_t *result = malloc(sizeof(eval_result_t));
    if (!result) return NULL;
    result->type = type;
    return result;
}

void free_eval_result(eval_result_t *result) {
    if (!result) return;
    if (result->type == GRAPHQLITE_TYPE_TEXT && result->data.text) {
        free(result->data.text);
    }
    free(result);
}

eval_result_t* lookup_property_value(sqlite3 *db, variable_binding_t *binding, const char *property) {
    if (!binding || !property) return NULL;
    
    // Get property key ID
    int64_t key_id = get_or_create_property_key_id(db, property);
    if (key_id == -1) return NULL;
    
    // Try each type table
    const char *table_prefixes[] = {"node_props", "edge_props"};
    const char *table_types[] = {"int", "real", "text", "bool"};
    const char *id_column = binding->is_edge ? "edge_id" : "node_id";
    int64_t entity_id = binding->is_edge ? binding->edge_id : binding->node_id;
    
    for (int type_idx = 0; type_idx < 4; type_idx++) {
        char table_name[64];
        snprintf(table_name, sizeof(table_name), "%s_%s", 
                binding->is_edge ? table_prefixes[1] : table_prefixes[0], 
                table_types[type_idx]);
        
        char sql[256];
        snprintf(sql, sizeof(sql), "SELECT value FROM %s WHERE %s = ? AND key_id = ?", 
                table_name, id_column);
        
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, entity_id);
            sqlite3_bind_int64(stmt, 2, key_id);
            
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                eval_result_t *result = create_eval_result(
                    type_idx == 0 ? GRAPHQLITE_TYPE_INTEGER :
                    type_idx == 1 ? GRAPHQLITE_TYPE_FLOAT :
                    type_idx == 2 ? GRAPHQLITE_TYPE_TEXT :
                    GRAPHQLITE_TYPE_BOOLEAN
                );
                
                if (result) {
                    switch (type_idx) {
                        case 0: // int
                            result->data.integer = sqlite3_column_int64(stmt, 0);
                            break;
                        case 1: // real
                            result->data.float_val = sqlite3_column_double(stmt, 0);
                            break;
                        case 2: // text
                            result->data.text = strdup((const char*)sqlite3_column_text(stmt, 0));
                            break;
                        case 3: // bool
                            result->data.boolean = sqlite3_column_int(stmt, 0);
                            break;
                    }
                }
                
                sqlite3_finalize(stmt);
                return result;
            }
            sqlite3_finalize(stmt);
        }
    }
    
    return NULL; // Property not found
}

int compare_eval_results(eval_result_t *left, eval_result_t *right, ast_operator_t op) {
    if (!left || !right) return 0;
    
    // Handle NULL comparisons
    if (left->type == GRAPHQLITE_TYPE_NULL || right->type == GRAPHQLITE_TYPE_NULL) {
        if (op == AST_OP_EQ) return (left->type == GRAPHQLITE_TYPE_NULL && right->type == GRAPHQLITE_TYPE_NULL);
        if (op == AST_OP_NEQ) return !(left->type == GRAPHQLITE_TYPE_NULL && right->type == GRAPHQLITE_TYPE_NULL);
        return 0; // Other comparisons with NULL return false
    }
    
    // Type mismatch
    if (left->type != right->type) return 0;
    
    switch (left->type) {
        case GRAPHQLITE_TYPE_INTEGER: {
            int64_t l = left->data.integer;
            int64_t r = right->data.integer;
            switch (op) {
                case AST_OP_EQ: return l == r;
                case AST_OP_NEQ: return l != r;
                case AST_OP_LT: return l < r;
                case AST_OP_GT: return l > r;
                case AST_OP_LE: return l <= r;
                case AST_OP_GE: return l >= r;
                default: return 0;
            }
        }
        
        case GRAPHQLITE_TYPE_FLOAT: {
            double l = left->data.float_val;
            double r = right->data.float_val;
            switch (op) {
                case AST_OP_EQ: return l == r;
                case AST_OP_NEQ: return l != r;
                case AST_OP_LT: return l < r;
                case AST_OP_GT: return l > r;
                case AST_OP_LE: return l <= r;
                case AST_OP_GE: return l >= r;
                default: return 0;
            }
        }
        
        case GRAPHQLITE_TYPE_TEXT: {
            const char *l = left->data.text ? left->data.text : "";
            const char *r = right->data.text ? right->data.text : "";
            int cmp = strcmp(l, r);
            switch (op) {
                case AST_OP_EQ: return cmp == 0;
                case AST_OP_NEQ: return cmp != 0;
                case AST_OP_LT: return cmp < 0;
                case AST_OP_GT: return cmp > 0;
                case AST_OP_LE: return cmp <= 0;
                case AST_OP_GE: return cmp >= 0;
                default: return 0;
            }
        }
        
        case GRAPHQLITE_TYPE_BOOLEAN: {
            int l = left->data.boolean;
            int r = right->data.boolean;
            switch (op) {
                case AST_OP_EQ: return l == r;
                case AST_OP_NEQ: return l != r;
                case AST_OP_LT: return l < r;  // false < true
                case AST_OP_GT: return l > r;
                case AST_OP_LE: return l <= r;
                case AST_OP_GE: return l >= r;
                default: return 0;
            }
        }
        
        default:
            return 0;
    }
}

eval_result_t* evaluate_expression(sqlite3 *db, cypher_ast_node_t *expr, variable_binding_t *bindings, int binding_count) {
    if (!expr) return NULL;
    
    switch (expr->type) {
        case AST_IDENTIFIER: {
            // Look up variable in bindings - for now, just return a placeholder
            // TODO: Implement variable resolution
            return NULL;
        }
        
        case AST_PROPERTY_ACCESS: {
            // Find the variable in bindings
            const char *var_name = expr->data.property_access.variable;
            const char *prop_name = expr->data.property_access.property;
            
            for (int i = 0; i < binding_count; i++) {
                if (strcmp(bindings[i].variable_name, var_name) == 0) {
                    return lookup_property_value(db, &bindings[i], prop_name);
                }
            }
            return NULL;
        }
        
        case AST_STRING_LITERAL: {
            eval_result_t *result = create_eval_result(GRAPHQLITE_TYPE_TEXT);
            if (result) {
                result->data.text = strdup(expr->data.string_literal.value);
            }
            return result;
        }
        
        case AST_INTEGER_LITERAL: {
            eval_result_t *result = create_eval_result(GRAPHQLITE_TYPE_INTEGER);
            if (result) {
                result->data.integer = expr->data.integer_literal.value;
            }
            return result;
        }
        
        case AST_FLOAT_LITERAL: {
            eval_result_t *result = create_eval_result(GRAPHQLITE_TYPE_FLOAT);
            if (result) {
                result->data.float_val = expr->data.float_literal.value;
            }
            return result;
        }
        
        case AST_BOOLEAN_LITERAL: {
            eval_result_t *result = create_eval_result(GRAPHQLITE_TYPE_BOOLEAN);
            if (result) {
                result->data.boolean = expr->data.boolean_literal.value;
            }
            return result;
        }
        
        case AST_BINARY_EXPR: {
            eval_result_t *left = evaluate_expression(db, expr->data.binary_expr.left, bindings, binding_count);
            eval_result_t *right = evaluate_expression(db, expr->data.binary_expr.right, bindings, binding_count);
            
            eval_result_t *result = create_eval_result(GRAPHQLITE_TYPE_BOOLEAN);
            if (result) {
                switch (expr->data.binary_expr.op) {
                    case AST_OP_AND:
                        result->data.boolean = (left && left->type == GRAPHQLITE_TYPE_BOOLEAN && left->data.boolean) &&
                                             (right && right->type == GRAPHQLITE_TYPE_BOOLEAN && right->data.boolean);
                        break;
                    case AST_OP_OR:
                        result->data.boolean = (left && left->type == GRAPHQLITE_TYPE_BOOLEAN && left->data.boolean) ||
                                             (right && right->type == GRAPHQLITE_TYPE_BOOLEAN && right->data.boolean);
                        break;
                    default:
                        // Comparison operators
                        result->data.boolean = compare_eval_results(left, right, expr->data.binary_expr.op);
                        break;
                }
            }
            
            free_eval_result(left);
            free_eval_result(right);
            return result;
        }
        
        case AST_UNARY_EXPR: {
            if (expr->data.unary_expr.op == AST_OP_NOT) {
                eval_result_t *operand = evaluate_expression(db, expr->data.unary_expr.operand, bindings, binding_count);
                eval_result_t *result = create_eval_result(GRAPHQLITE_TYPE_BOOLEAN);
                if (result) {
                    result->data.boolean = !(operand && operand->type == GRAPHQLITE_TYPE_BOOLEAN && operand->data.boolean);
                }
                free_eval_result(operand);
                return result;
            }
            return NULL;
        }
        
        case AST_IS_NULL_EXPR: {
            eval_result_t *operand = evaluate_expression(db, expr->data.is_null_expr.expression, bindings, binding_count);
            eval_result_t *result = create_eval_result(GRAPHQLITE_TYPE_BOOLEAN);
            if (result) {
                int is_null = (operand == NULL || operand->type == GRAPHQLITE_TYPE_NULL);
                result->data.boolean = expr->data.is_null_expr.is_null ? is_null : !is_null;
            }
            free_eval_result(operand);
            return result;
        }
        
        default:
            return NULL;
    }
}