/* GraphQLite - OpenCypher extension for SQLite */
#include <sqlite3ext.h> /* Do not use <sqlite3.h>! */
SQLITE_EXTENSION_INIT1

#include "graphqlite.h"
#include "ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Forward declaration for parser function
extern cypher_ast_node_t* parse_cypher_query(const char *query);

// ============================================================================
// Schema Management
// ============================================================================

static int create_schema(sqlite3 *db) {
    int rc = SQLITE_OK;
    char *err_msg = NULL;
    
    // Check if schema already exists (idempotent initialization)
    sqlite3_stmt *check_stmt;
    rc = sqlite3_prepare_v2(db, 
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name='nodes'",
        -1, &check_stmt, NULL);
    if (rc == SQLITE_OK && sqlite3_step(check_stmt) == SQLITE_ROW) {
        sqlite3_finalize(check_stmt);
        return GRAPHQLITE_OK;  // Schema already exists
    }
    sqlite3_finalize(check_stmt);
    
    // Disable foreign keys during schema creation
    sqlite3_exec(db, "PRAGMA foreign_keys = OFF", NULL, NULL, NULL);
    
    // Begin transaction for atomic schema creation
    rc = sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) sqlite3_free(err_msg);
        return GRAPHQLITE_ERROR;
    }
    
    // Schema SQL ordered by dependencies (parent tables first)
    const char *schema_sql[] = {
        // Core tables without foreign keys first
        "CREATE TABLE nodes ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT"
        ")",
        
        "CREATE TABLE property_keys ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  key TEXT UNIQUE NOT NULL"
        ")",
        
        // Tables with foreign keys after their dependencies
        "CREATE TABLE edges ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  source_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
        "  target_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
        "  type TEXT NOT NULL"
        ")",
        
        "CREATE TABLE node_labels ("
        "  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
        "  label TEXT NOT NULL,"
        "  PRIMARY KEY (node_id, label)"
        ")",
        
        // Node property tables (typed EAV)
        "CREATE TABLE node_props_int ("
        "  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
        "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
        "  value INTEGER NOT NULL,"
        "  PRIMARY KEY (node_id, key_id)"
        ")",
        
        "CREATE TABLE node_props_text ("
        "  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
        "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
        "  value TEXT NOT NULL,"
        "  PRIMARY KEY (node_id, key_id)"
        ")",
        
        "CREATE TABLE node_props_real ("
        "  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
        "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
        "  value REAL NOT NULL,"
        "  PRIMARY KEY (node_id, key_id)"
        ")",
        
        "CREATE TABLE node_props_bool ("
        "  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
        "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
        "  value INTEGER NOT NULL CHECK (value IN (0, 1)),"
        "  PRIMARY KEY (node_id, key_id)"
        ")",
        
        // Edge property tables (typed EAV)
        "CREATE TABLE edge_props_int ("
        "  edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,"
        "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
        "  value INTEGER NOT NULL,"
        "  PRIMARY KEY (edge_id, key_id)"
        ")",
        
        "CREATE TABLE edge_props_text ("
        "  edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,"
        "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
        "  value TEXT NOT NULL,"
        "  PRIMARY KEY (edge_id, key_id)"
        ")",
        
        "CREATE TABLE edge_props_real ("
        "  edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,"
        "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
        "  value REAL NOT NULL,"
        "  PRIMARY KEY (edge_id, key_id)"
        ")",
        
        "CREATE TABLE edge_props_bool ("
        "  edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,"
        "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
        "  value INTEGER NOT NULL CHECK (value IN (0, 1)),"
        "  PRIMARY KEY (edge_id, key_id)"
        ")",
        
        NULL  // Sentinel
    };
    
    // Create tables
    for (int i = 0; schema_sql[i] != NULL; i++) {
        rc = sqlite3_exec(db, schema_sql[i], NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            if (err_msg) {
                sqlite3_free(err_msg);
            }
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
            sqlite3_exec(db, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);
            return GRAPHQLITE_ERROR;
        }
    }
    
    // Performance indexes
    const char *index_sql[] = {
        // Core indexes for performance
        "CREATE INDEX idx_edges_source ON edges(source_id, type)",
        "CREATE INDEX idx_edges_target ON edges(target_id, type)",
        "CREATE INDEX idx_edges_type ON edges(type)",
        
        // Property indexes (property-first for efficient queries)
        "CREATE INDEX idx_node_props_int_key_value ON node_props_int(key_id, value, node_id)",
        "CREATE INDEX idx_node_props_text_key_value ON node_props_text(key_id, value, node_id)",
        "CREATE INDEX idx_node_props_real_key_value ON node_props_real(key_id, value, node_id)",
        "CREATE INDEX idx_node_props_bool_key_value ON node_props_bool(key_id, value, node_id)",
        
        "CREATE INDEX idx_edge_props_int_key_value ON edge_props_int(key_id, value, edge_id)",
        "CREATE INDEX idx_edge_props_text_key_value ON edge_props_text(key_id, value, edge_id)",
        "CREATE INDEX idx_edge_props_real_key_value ON edge_props_real(key_id, value, edge_id)",
        "CREATE INDEX idx_edge_props_bool_key_value ON edge_props_bool(key_id, value, edge_id)",
        
        // Label indexes
        "CREATE INDEX idx_node_labels_label ON node_labels(label, node_id)",
        
        // Property key index
        "CREATE INDEX idx_property_keys_key ON property_keys(key)",
        
        NULL  // Sentinel
    };
    
    // Create indexes
    for (int i = 0; index_sql[i] != NULL; i++) {
        rc = sqlite3_exec(db, index_sql[i], NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            if (err_msg) {
                sqlite3_free(err_msg);
            }
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
            sqlite3_exec(db, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);
            return GRAPHQLITE_ERROR;
        }
    }
    
    // Commit transaction
    rc = sqlite3_exec(db, "COMMIT", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) sqlite3_free(err_msg);
        sqlite3_exec(db, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);
        return GRAPHQLITE_ERROR;
    }
    
    // Re-enable foreign keys
    sqlite3_exec(db, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);
    
    return GRAPHQLITE_OK;
}

// ============================================================================
// Helper Functions for AST Property Extraction
// ============================================================================

// Extract property value and type from AST literal node
static int extract_property_from_ast(cypher_ast_node_t *value_node, const char **prop_value_str, 
                                     double *prop_value_num, int *prop_value_int, 
                                     graphqlite_value_type_t *prop_type) {
    if (!value_node || !prop_type) return 0;
    
    switch (value_node->type) {
        case AST_STRING_LITERAL:
            if (prop_value_str) {
                *prop_value_str = value_node->data.string_literal.value;
            }
            *prop_type = GRAPHQLITE_TYPE_TEXT;
            return 1;
            
        case AST_INTEGER_LITERAL:
            if (prop_value_int) {
                *prop_value_int = value_node->data.integer_literal.value;
            }
            *prop_type = GRAPHQLITE_TYPE_INTEGER;
            return 1;
            
        case AST_FLOAT_LITERAL:
            if (prop_value_num) {
                *prop_value_num = value_node->data.float_literal.value;
            }
            *prop_type = GRAPHQLITE_TYPE_FLOAT;
            return 1;
            
        case AST_BOOLEAN_LITERAL:
            if (prop_value_int) {
                *prop_value_int = value_node->data.boolean_literal.value;
            }
            *prop_type = GRAPHQLITE_TYPE_BOOLEAN;
            return 1;
            
        default:
            *prop_type = GRAPHQLITE_TYPE_NULL;
            return 0;
    }
}

// ============================================================================
// Helper Functions for Typed EAV Schema
// ============================================================================

// Get or create property key ID (property key interning)
static int64_t get_or_create_property_key_id(sqlite3 *db, const char *key) {
    if (!key) return -1;
    
    // First, try to find existing key
    const char *select_sql = "SELECT id FROM property_keys WHERE key = ?";
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_prepare_v2(db, select_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;
    
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    
    int64_t key_id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        key_id = sqlite3_column_int64(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    
    if (key_id != -1) {
        return key_id;  // Found existing key
    }
    
    // Key doesn't exist, create it
    const char *insert_sql = "INSERT INTO property_keys (key) VALUES (?)";
    rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;
    
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) return -1;
    
    return sqlite3_last_insert_rowid(db);
}

// Insert property based on type
static int insert_node_property(sqlite3 *db, int64_t node_id, int64_t key_id, 
                               const char *value, graphqlite_value_type_t type) {
    const char *table_name;
    switch (type) {
        case GRAPHQLITE_TYPE_INTEGER:
            table_name = "node_props_int";
            break;
        case GRAPHQLITE_TYPE_FLOAT:
            table_name = "node_props_real";
            break;
        case GRAPHQLITE_TYPE_TEXT:
            table_name = "node_props_text";
            break;
        case GRAPHQLITE_TYPE_BOOLEAN:
            table_name = "node_props_bool";
            break;
        default:
            return GRAPHQLITE_INVALID;
    }
    
    char sql[256];
    snprintf(sql, sizeof(sql), 
        "INSERT INTO %s (node_id, key_id, value) VALUES (?, ?, ?)", table_name);
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return GRAPHQLITE_ERROR;
    
    sqlite3_bind_int64(stmt, 1, node_id);
    sqlite3_bind_int64(stmt, 2, key_id);
    
    switch (type) {
        case GRAPHQLITE_TYPE_INTEGER: {
            int64_t int_val = atoll(value);
            sqlite3_bind_int64(stmt, 3, int_val);
            break;
        }
        case GRAPHQLITE_TYPE_FLOAT: {
            double float_val = atof(value);
            sqlite3_bind_double(stmt, 3, float_val);
            break;
        }
        case GRAPHQLITE_TYPE_TEXT:
            sqlite3_bind_text(stmt, 3, value, -1, SQLITE_STATIC);
            break;
        case GRAPHQLITE_TYPE_BOOLEAN: {
            int bool_val = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) ? 1 : 0;
            sqlite3_bind_int(stmt, 3, bool_val);
            break;
        }
        default:
            sqlite3_finalize(stmt);
            return GRAPHQLITE_INVALID;
    }
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? GRAPHQLITE_OK : GRAPHQLITE_ERROR;
}

// Insert edge property based on type
static int insert_edge_property(sqlite3 *db, int64_t edge_id, int64_t key_id, 
                               const char *value, graphqlite_value_type_t type) {
    const char *table_name;
    switch (type) {
        case GRAPHQLITE_TYPE_INTEGER:
            table_name = "edge_props_int";
            break;
        case GRAPHQLITE_TYPE_FLOAT:
            table_name = "edge_props_real";
            break;
        case GRAPHQLITE_TYPE_TEXT:
            table_name = "edge_props_text";
            break;
        case GRAPHQLITE_TYPE_BOOLEAN:
            table_name = "edge_props_bool";
            break;
        default:
            return GRAPHQLITE_INVALID;
    }
    
    char sql[256];
    snprintf(sql, sizeof(sql), 
        "INSERT INTO %s (edge_id, key_id, value) VALUES (?, ?, ?)", table_name);
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return GRAPHQLITE_ERROR;
    
    sqlite3_bind_int64(stmt, 1, edge_id);
    sqlite3_bind_int64(stmt, 2, key_id);
    
    switch (type) {
        case GRAPHQLITE_TYPE_INTEGER: {
            int64_t int_val = atoll(value);
            sqlite3_bind_int64(stmt, 3, int_val);
            break;
        }
        case GRAPHQLITE_TYPE_FLOAT: {
            double float_val = atof(value);
            sqlite3_bind_double(stmt, 3, float_val);
            break;
        }
        case GRAPHQLITE_TYPE_TEXT:
            sqlite3_bind_text(stmt, 3, value, -1, SQLITE_STATIC);
            break;
        case GRAPHQLITE_TYPE_BOOLEAN: {
            int bool_val = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) ? 1 : 0;
            sqlite3_bind_int(stmt, 3, bool_val);
            break;
        }
        default:
            sqlite3_finalize(stmt);
            return GRAPHQLITE_INVALID;
    }
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? GRAPHQLITE_OK : GRAPHQLITE_ERROR;
}

// ============================================================================
// Expression Evaluation Engine
// ============================================================================

// Evaluation result type
typedef struct {
    graphqlite_value_type_t type;
    union {
        int64_t integer;
        double float_val;
        char *text;
        int boolean;
    } data;
} eval_result_t;

// Variable binding for expression evaluation
typedef struct {
    char *variable_name;    // e.g., "n", "r"
    int64_t node_id;        // ID if it's a node
    int64_t edge_id;        // ID if it's an edge
    int is_edge;            // 1 if edge, 0 if node
} variable_binding_t;

// Create evaluation result
static eval_result_t* create_eval_result(graphqlite_value_type_t type) {
    eval_result_t *result = malloc(sizeof(eval_result_t));
    if (!result) return NULL;
    result->type = type;
    return result;
}

// Free evaluation result
static void free_eval_result(eval_result_t *result) {
    if (!result) return;
    if (result->type == GRAPHQLITE_TYPE_TEXT && result->data.text) {
        free(result->data.text);
    }
    free(result);
}

// Look up property value for a variable
static eval_result_t* lookup_property_value(sqlite3 *db, variable_binding_t *binding, const char *property) {
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

// Compare two evaluation results
static int compare_eval_results(eval_result_t *left, eval_result_t *right, ast_operator_t op) {
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

// Forward declaration for recursive evaluation
static eval_result_t* evaluate_expression(sqlite3 *db, cypher_ast_node_t *expr, variable_binding_t *bindings, int binding_count);

// Evaluate expression recursively
static eval_result_t* evaluate_expression(sqlite3 *db, cypher_ast_node_t *expr, variable_binding_t *bindings, int binding_count) {
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

// ============================================================================
// Query Execution - Forward Declarations
// ============================================================================

static graphqlite_result_t* execute_create_node(sqlite3 *db, cypher_ast_node_t *node_pattern);
static graphqlite_result_t* execute_create_relationship(sqlite3 *db, cypher_ast_node_t *rel_pattern);
static graphqlite_result_t* execute_match_node(sqlite3 *db, cypher_ast_node_t *node_pattern, cypher_ast_node_t *return_stmt);
static graphqlite_result_t* execute_match_relationship(sqlite3 *db, cypher_ast_node_t *rel_pattern, cypher_ast_node_t *return_stmt);
static graphqlite_result_t* execute_match_node_with_where(sqlite3 *db, cypher_ast_node_t *node_pattern, cypher_ast_node_t *where_clause, cypher_ast_node_t *return_stmt);
static graphqlite_result_t* execute_match_relationship_with_where(sqlite3 *db, cypher_ast_node_t *rel_pattern, cypher_ast_node_t *where_clause, cypher_ast_node_t *return_stmt);
static char* serialize_node_entity(sqlite3 *db, int64_t node_id);
static char* serialize_relationship_entity(sqlite3 *db, int64_t edge_id);

// ============================================================================
// Query Execution
// ============================================================================

static graphqlite_result_t* execute_create_statement(sqlite3 *db, cypher_ast_node_t *ast) {
    graphqlite_result_t *result = graphqlite_result_create();
    if (!result) return NULL;
    
    // Extract pattern from CREATE statement (can be node or relationship)
    cypher_ast_node_t *pattern = ast->data.create_stmt.node_pattern;
    if (!pattern) {
        graphqlite_result_set_error(result, "Missing pattern in CREATE statement");
        return result;
    }
    
    if (pattern->type == AST_NODE_PATTERN) {
        // Handle node creation (existing logic)
        return execute_create_node(db, pattern);
    } else if (pattern->type == AST_RELATIONSHIP_PATTERN) {
        // Handle relationship creation (new logic)
        return execute_create_relationship(db, pattern);
    } else {
        graphqlite_result_set_error(result, "Invalid pattern in CREATE statement");
        return result;
    }
}

// Helper function to create a single node and return its ID
static int64_t create_node_with_properties(sqlite3 *db, cypher_ast_node_t *node_pattern, graphqlite_result_t *result) {
    // Extract label
    const char *label = NULL;
    if (node_pattern->data.node_pattern.label && node_pattern->data.node_pattern.label->type == AST_LABEL) {
        label = node_pattern->data.node_pattern.label->data.label.name;
    }
    
    if (!label) {
        graphqlite_result_set_error(result, "Node must have a label");
        return -1;
    }
    
    // Step 1: Insert node into nodes table
    const char *insert_node_sql = "INSERT INTO nodes DEFAULT VALUES";
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_prepare_v2(db, insert_node_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        graphqlite_result_set_error(result, "Failed to prepare node INSERT statement");
        return -1;
    }
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        graphqlite_result_set_error(result, "Failed to insert node");
        return -1;
    }
    
    int64_t node_id = sqlite3_last_insert_rowid(db);
    
    // Step 2: Insert label into node_labels table
    const char *insert_label_sql = "INSERT INTO node_labels (node_id, label) VALUES (?, ?)";
    rc = sqlite3_prepare_v2(db, insert_label_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        graphqlite_result_set_error(result, "Failed to prepare label INSERT statement");
        return -1;
    }
    
    sqlite3_bind_int64(stmt, 1, node_id);
    sqlite3_bind_text(stmt, 2, label, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        graphqlite_result_set_error(result, "Failed to insert node label");
        return -1;
    }
    
    // Step 3: Insert properties (if any) into appropriate typed tables
    if (node_pattern->data.node_pattern.properties) {
        cypher_ast_node_t *props = node_pattern->data.node_pattern.properties;
        
        // Properties should always be a list now
        if (props->type == AST_PROPERTY_LIST) {
            for (int i = 0; i < props->data.property_list.count; i++) {
                cypher_ast_node_t *prop = props->data.property_list.properties[i];
                const char *prop_key = prop->data.property.key;
                const char *prop_value_str = NULL;
                double prop_value_num = 0.0;
                int prop_value_int = 0;
                graphqlite_value_type_t prop_type = GRAPHQLITE_TYPE_NULL;
                
                // Extract property value and type from AST
                int extracted = extract_property_from_ast(prop->data.property.value, 
                                                         &prop_value_str, &prop_value_num, 
                                                         &prop_value_int, &prop_type);
                
                if (prop_key && extracted && prop_type != GRAPHQLITE_TYPE_NULL) {
                    // Get or create property key ID
                    int64_t key_id = get_or_create_property_key_id(db, prop_key);
                    if (key_id == -1) {
                        graphqlite_result_set_error(result, "Failed to get property key ID");
                        return -1;
                    }
                    
                    // Insert property with correct value based on type
                    const char *value_to_insert = NULL;
                    char int_str[32], float_str[32];
                    
                    switch (prop_type) {
                        case GRAPHQLITE_TYPE_TEXT:
                            value_to_insert = prop_value_str;
                            break;
                        case GRAPHQLITE_TYPE_INTEGER:
                            snprintf(int_str, sizeof(int_str), "%d", prop_value_int);
                            value_to_insert = int_str;
                            break;
                        case GRAPHQLITE_TYPE_FLOAT:
                            snprintf(float_str, sizeof(float_str), "%.15g", prop_value_num);
                            value_to_insert = float_str;
                            break;
                        case GRAPHQLITE_TYPE_BOOLEAN:
                            snprintf(int_str, sizeof(int_str), "%d", prop_value_int);
                            value_to_insert = int_str;
                            break;
                        default:
                            break;
                    }
                    
                    if (value_to_insert) {
                        int prop_result = insert_node_property(db, node_id, key_id, value_to_insert, prop_type);
                        if (prop_result != GRAPHQLITE_OK) {
                            graphqlite_result_set_error(result, "Failed to insert node property");
                            return -1;
                        }
                    }
                }
            }
        }
    }
    
    return node_id;
}

static graphqlite_result_t* execute_create_node(sqlite3 *db, cypher_ast_node_t *node_pattern) {
    graphqlite_result_t *result = graphqlite_result_create();
    if (!result) return NULL;
    
    int64_t node_id = create_node_with_properties(db, node_pattern, result);
    if (node_id == -1) {
        return result;  // Error already set in result
    }
    
    // Return success result (no data for CREATE)
    result->result_code = GRAPHQLITE_OK;
    return result;
}

static graphqlite_result_t* execute_create_relationship(sqlite3 *db, cypher_ast_node_t *rel_pattern) {
    graphqlite_result_t *result = graphqlite_result_create();
    if (!result) return NULL;
    
    // Extract nodes and edge from relationship pattern
    cypher_ast_node_t *left_node = rel_pattern->data.relationship_pattern.left_node;
    cypher_ast_node_t *edge = rel_pattern->data.relationship_pattern.edge;
    cypher_ast_node_t *right_node = rel_pattern->data.relationship_pattern.right_node;
    int direction = rel_pattern->data.relationship_pattern.direction;
    
    if (!left_node || !edge || !right_node) {
        graphqlite_result_set_error(result, "Invalid relationship pattern");
        return result;
    }
    
    // Create left node
    int64_t left_id = create_node_with_properties(db, left_node, result);
    if (left_id == -1) {
        return result;  // Error already set
    }
    
    // Create right node  
    int64_t right_id = create_node_with_properties(db, right_node, result);
    if (right_id == -1) {
        return result;  // Error already set
    }
    
    // Extract edge type
    const char *edge_type = NULL;
    if (edge->data.edge_pattern.label && edge->data.edge_pattern.label->type == AST_LABEL) {
        edge_type = edge->data.edge_pattern.label->data.label.name;
    }
    
    if (!edge_type) {
        graphqlite_result_set_error(result, "Edge must have a type");
        return result;
    }
    
    // Determine source and target based on direction
    int64_t source_id = (direction == -1) ? right_id : left_id;
    int64_t target_id = (direction == -1) ? left_id : right_id;
    
    // Insert edge into edges table
    const char *insert_edge_sql = "INSERT INTO edges (source_id, target_id, type) VALUES (?, ?, ?)";
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_prepare_v2(db, insert_edge_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        graphqlite_result_set_error(result, "Failed to prepare edge INSERT statement");
        return result;
    }
    
    sqlite3_bind_int64(stmt, 1, source_id);
    sqlite3_bind_int64(stmt, 2, target_id);
    sqlite3_bind_text(stmt, 3, edge_type, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        graphqlite_result_set_error(result, "Failed to insert edge");
        return result;
    }
    
    int64_t edge_id = sqlite3_last_insert_rowid(db);
    
    // Insert edge properties (if any)
    if (edge->data.edge_pattern.properties) {
        cypher_ast_node_t *props = edge->data.edge_pattern.properties;
        
        if (props->type == AST_PROPERTY_LIST) {
            for (int i = 0; i < props->data.property_list.count; i++) {
                cypher_ast_node_t *prop = props->data.property_list.properties[i];
                const char *prop_key = prop->data.property.key;
                const char *prop_value_str = NULL;
                double prop_value_num = 0.0;
                int prop_value_int = 0;
                graphqlite_value_type_t prop_type = GRAPHQLITE_TYPE_NULL;
                
                // Extract property value and type from AST
                int extracted = extract_property_from_ast(prop->data.property.value, 
                                                         &prop_value_str, &prop_value_num, 
                                                         &prop_value_int, &prop_type);
                
                if (prop_key && extracted && prop_type != GRAPHQLITE_TYPE_NULL) {
                    // Get or create property key ID
                    int64_t key_id = get_or_create_property_key_id(db, prop_key);
                    if (key_id == -1) {
                        graphqlite_result_set_error(result, "Failed to get property key ID");
                        return result;
                    }
                    
                    // Insert edge property with correct value based on type
                    const char *value_to_insert = NULL;
                    char int_str[32], float_str[32];
                    
                    switch (prop_type) {
                        case GRAPHQLITE_TYPE_TEXT:
                            value_to_insert = prop_value_str;
                            break;
                        case GRAPHQLITE_TYPE_INTEGER:
                            snprintf(int_str, sizeof(int_str), "%d", prop_value_int);
                            value_to_insert = int_str;
                            break;
                        case GRAPHQLITE_TYPE_FLOAT:
                            snprintf(float_str, sizeof(float_str), "%.15g", prop_value_num);
                            value_to_insert = float_str;
                            break;
                        case GRAPHQLITE_TYPE_BOOLEAN:
                            snprintf(int_str, sizeof(int_str), "%d", prop_value_int);
                            value_to_insert = int_str;
                            break;
                        default:
                            break;
                    }
                    
                    if (value_to_insert) {
                        int prop_result = insert_edge_property(db, edge_id, key_id, value_to_insert, prop_type);
                        if (prop_result != GRAPHQLITE_OK) {
                            graphqlite_result_set_error(result, "Failed to insert edge property");
                            return result;
                        }
                    }
                }
            }
        }
    }
    
    // Return success result (no data for CREATE)
    result->result_code = GRAPHQLITE_OK;
    return result;
}

static graphqlite_result_t* execute_match_statement(sqlite3 *db, cypher_ast_node_t *match_stmt, cypher_ast_node_t *return_stmt) {
    graphqlite_result_t *result = graphqlite_result_create();
    if (!result) return NULL;
    
    // Extract pattern from MATCH statement (can be node or relationship)
    cypher_ast_node_t *pattern = match_stmt->data.match_stmt.node_pattern;
    cypher_ast_node_t *where_clause = match_stmt->data.match_stmt.where_clause;
    
    if (!pattern) {
        graphqlite_result_set_error(result, "Missing pattern in MATCH statement");
        return result;
    }
    
    graphqlite_result_free(result);  // Free the temporary result
    
    if (pattern->type == AST_NODE_PATTERN) {
        // Handle node matching (existing logic)
        return execute_match_node_with_where(db, pattern, where_clause, return_stmt);
    } else if (pattern->type == AST_RELATIONSHIP_PATTERN) {
        // Handle relationship matching (new logic)
        return execute_match_relationship_with_where(db, pattern, where_clause, return_stmt);
    } else {
        result = graphqlite_result_create();
        if (result) {
            graphqlite_result_set_error(result, "Invalid pattern in MATCH statement");
        }
        return result;
    }
}

static graphqlite_result_t* execute_match_node(sqlite3 *db, cypher_ast_node_t *node_pattern, cypher_ast_node_t *return_stmt) {
    graphqlite_result_t *result = graphqlite_result_create();
    if (!result) return NULL;
    
    // Build SELECT query based on pattern
    const char *label = NULL;
    const char *prop_key = NULL;
    const char *prop_value_str = NULL;
    double prop_value_num = 0.0;
    int prop_value_int = 0;
    graphqlite_value_type_t prop_type = GRAPHQLITE_TYPE_NULL;
    
    if (node_pattern->data.node_pattern.label && node_pattern->data.node_pattern.label->type == AST_LABEL) {
        label = node_pattern->data.node_pattern.label->data.label.name;
    }
    
    if (node_pattern->data.node_pattern.properties) {
        cypher_ast_node_t *props = node_pattern->data.node_pattern.properties;
        
        // Properties should always be a list now
        if (props->type == AST_PROPERTY_LIST && props->data.property_list.count > 0) {
            // For MATCH, we only use the first property for now
            // TODO: Support multiple property filters in MATCH
            cypher_ast_node_t *prop = props->data.property_list.properties[0];
            prop_key = prop->data.property.key;
            extract_property_from_ast(prop->data.property.value, 
                                     &prop_value_str, &prop_value_num, 
                                     &prop_value_int, &prop_type);
        }
    }
    
    // Build query using new schema with JOINs
    char query[1024];
    const char *prop_table = NULL;
    
    // Determine which property table to query based on type
    switch (prop_type) {
        case GRAPHQLITE_TYPE_TEXT:
            prop_table = "node_props_text";
            break;
        case GRAPHQLITE_TYPE_INTEGER:
            prop_table = "node_props_int";
            break;
        case GRAPHQLITE_TYPE_FLOAT:
            prop_table = "node_props_real";
            break;
        case GRAPHQLITE_TYPE_BOOLEAN:
            prop_table = "node_props_bool";
            break;
        default:
            prop_table = NULL;
            break;
    }
    
    if (prop_key && prop_type != GRAPHQLITE_TYPE_NULL && prop_table) {
        // Match by label AND property
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT n.id, nl.label, pk.key, %s.value "
            "FROM nodes n "
            "JOIN node_labels nl ON n.id = nl.node_id "
            "JOIN %s ON n.id = %s.node_id "
            "JOIN property_keys pk ON %s.key_id = pk.id "
            "WHERE nl.label = ? AND pk.key = ? AND %s.value = ?",
            prop_table, prop_table, prop_table, prop_table, prop_table);
    } else if (label) {
        // Match by label only
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT n.id, nl.label, 'NULL' as key, 'NULL' as value "
            "FROM nodes n "
            "JOIN node_labels nl ON n.id = nl.node_id "
            "WHERE nl.label = ?");
    } else {
        // Match all nodes
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT n.id, nl.label, 'NULL' as key, 'NULL' as value "
            "FROM nodes n "
            "JOIN node_labels nl ON n.id = nl.node_id");
    }
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        graphqlite_result_set_error(result, "Failed to prepare SELECT statement");
        return result;
    }
    
    // Bind parameters
    int param_idx = 1;
    if (label) {
        sqlite3_bind_text(stmt, param_idx++, label, -1, SQLITE_STATIC);
    }
    if (prop_key && prop_type != GRAPHQLITE_TYPE_NULL && prop_table) {
        sqlite3_bind_text(stmt, param_idx++, prop_key, -1, SQLITE_STATIC);
        
        // Bind value based on type
        switch (prop_type) {
            case GRAPHQLITE_TYPE_TEXT:
                sqlite3_bind_text(stmt, param_idx++, prop_value_str, -1, SQLITE_STATIC);
                break;
            case GRAPHQLITE_TYPE_INTEGER:
            case GRAPHQLITE_TYPE_BOOLEAN:
                sqlite3_bind_int(stmt, param_idx++, prop_value_int);
                break;
            case GRAPHQLITE_TYPE_FLOAT:
                sqlite3_bind_double(stmt, param_idx++, prop_value_num);
                break;
            default:
                break;
        }
    }
    
    // Parse which variable to return from RETURN statement
    const char *return_variable = "node"; // default
    if (return_stmt && return_stmt->type == AST_RETURN_STATEMENT) {
        cypher_ast_node_t *var = return_stmt->data.return_stmt.variable;
        if (var && var->type == AST_VARIABLE) {
            return_variable = var->data.variable.name;
        }
    }
    
    // Set up single result column for entity
    graphqlite_result_add_column(result, return_variable, GRAPHQLITE_TYPE_TEXT);
    
    // Fetch results
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        graphqlite_result_add_row(result);
        size_t row_idx = result->row_count - 1;
        
        // Serialize complete node entity
        graphqlite_value_t value;
        value.type = GRAPHQLITE_TYPE_TEXT;
        int64_t node_id = sqlite3_column_int64(stmt, 0);
        value.data.text = serialize_node_entity(db, node_id);
        
        graphqlite_result_set_value(result, row_idx, 0, &value);
    }
    
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        graphqlite_result_set_error(result, "Failed to execute SELECT statement");
        return result;
    }
    
    result->result_code = GRAPHQLITE_OK;
    return result;
}

static graphqlite_result_t* execute_match_node_with_where(sqlite3 *db, cypher_ast_node_t *node_pattern, cypher_ast_node_t *where_clause, cypher_ast_node_t *return_stmt) {
    // If no WHERE clause, fall back to original function
    if (!where_clause) {
        return execute_match_node(db, node_pattern, return_stmt);
    }
    
    graphqlite_result_t *result = graphqlite_result_create();
    if (!result) return NULL;
    
    // Build SELECT query based on pattern (same as original function)
    const char *label = NULL;
    const char *prop_key = NULL;
    const char *prop_value_str = NULL;
    double prop_value_num = 0.0;
    int prop_value_int = 0;
    graphqlite_value_type_t prop_type = GRAPHQLITE_TYPE_NULL;
    
    if (node_pattern->data.node_pattern.label && node_pattern->data.node_pattern.label->type == AST_LABEL) {
        label = node_pattern->data.node_pattern.label->data.label.name;
    }
    
    if (node_pattern->data.node_pattern.properties) {
        cypher_ast_node_t *props = node_pattern->data.node_pattern.properties;
        
        if (props->type == AST_PROPERTY_LIST && props->data.property_list.count > 0) {
            cypher_ast_node_t *prop = props->data.property_list.properties[0];
            prop_key = prop->data.property.key;
            extract_property_from_ast(prop->data.property.value, 
                                     &prop_value_str, &prop_value_num, 
                                     &prop_value_int, &prop_type);
        }
    }
    
    // Build query using same logic as original function
    char query[1024];
    const char *prop_table = NULL;
    
    switch (prop_type) {
        case GRAPHQLITE_TYPE_TEXT:
            prop_table = "node_props_text";
            break;
        case GRAPHQLITE_TYPE_INTEGER:
            prop_table = "node_props_int";
            break;
        case GRAPHQLITE_TYPE_FLOAT:
            prop_table = "node_props_real";
            break;
        case GRAPHQLITE_TYPE_BOOLEAN:
            prop_table = "node_props_bool";
            break;
        default:
            prop_table = NULL;
            break;
    }
    
    if (prop_key && prop_type != GRAPHQLITE_TYPE_NULL && prop_table) {
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT n.id, nl.label, pk.key, %s.value "
            "FROM nodes n "
            "JOIN node_labels nl ON n.id = nl.node_id "
            "JOIN %s ON n.id = %s.node_id "
            "JOIN property_keys pk ON %s.key_id = pk.id "
            "WHERE nl.label = ? AND pk.key = ? AND %s.value = ?",
            prop_table, prop_table, prop_table, prop_table, prop_table);
    } else if (label) {
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT n.id, nl.label, 'NULL' as key, 'NULL' as value "
            "FROM nodes n "
            "JOIN node_labels nl ON n.id = nl.node_id "
            "WHERE nl.label = ?");
    } else {
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT n.id, nl.label, 'NULL' as key, 'NULL' as value "
            "FROM nodes n "
            "JOIN node_labels nl ON n.id = nl.node_id");
    }
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        graphqlite_result_set_error(result, "Failed to prepare SELECT statement");
        return result;
    }
    
    // Bind parameters (same as original function)
    int param_idx = 1;
    if (label) {
        sqlite3_bind_text(stmt, param_idx++, label, -1, SQLITE_STATIC);
    }
    if (prop_key && prop_type != GRAPHQLITE_TYPE_NULL && prop_table) {
        sqlite3_bind_text(stmt, param_idx++, prop_key, -1, SQLITE_STATIC);
        
        switch (prop_type) {
            case GRAPHQLITE_TYPE_TEXT:
                sqlite3_bind_text(stmt, param_idx++, prop_value_str, -1, SQLITE_STATIC);
                break;
            case GRAPHQLITE_TYPE_INTEGER:
            case GRAPHQLITE_TYPE_BOOLEAN:
                sqlite3_bind_int(stmt, param_idx++, prop_value_int);
                break;
            case GRAPHQLITE_TYPE_FLOAT:
                sqlite3_bind_double(stmt, param_idx++, prop_value_num);
                break;
            default:
                break;
        }
    }
    
    // Parse return variable
    const char *return_variable = "node"; // default
    const char *node_variable = "n"; // default node variable
    if (return_stmt && return_stmt->type == AST_RETURN_STATEMENT) {
        cypher_ast_node_t *var = return_stmt->data.return_stmt.variable;
        if (var && var->type == AST_VARIABLE) {
            return_variable = var->data.variable.name;
        }
    }
    
    // Extract node variable from pattern for binding
    if (node_pattern->data.node_pattern.variable && node_pattern->data.node_pattern.variable->type == AST_VARIABLE) {
        node_variable = node_pattern->data.node_pattern.variable->data.variable.name;
    }
    
    // Set up result column
    graphqlite_result_add_column(result, return_variable, GRAPHQLITE_TYPE_TEXT);
    
    // Fetch and filter results with WHERE clause
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int64_t node_id = sqlite3_column_int64(stmt, 0);
        
        // Create variable binding for WHERE evaluation
        variable_binding_t binding;
        binding.variable_name = (char*)node_variable;
        binding.node_id = node_id;
        binding.edge_id = -1;
        binding.is_edge = 0;
        
        // Evaluate WHERE clause
        eval_result_t *where_result = evaluate_expression(db, where_clause->data.where_clause.expression, &binding, 1);
        
        // Check if WHERE condition is satisfied
        int include_result = 0;
        if (where_result && where_result->type == GRAPHQLITE_TYPE_BOOLEAN && where_result->data.boolean) {
            include_result = 1;
        }
        
        free_eval_result(where_result);
        
        if (include_result) {
            graphqlite_result_add_row(result);
            size_t row_idx = result->row_count - 1;
            
            // Serialize complete node entity
            graphqlite_value_t value;
            value.type = GRAPHQLITE_TYPE_TEXT;
            value.data.text = serialize_node_entity(db, node_id);
            
            graphqlite_result_set_value(result, row_idx, 0, &value);
        }
    }
    
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        graphqlite_result_set_error(result, "Failed to execute SELECT statement");
        return result;
    }
    
    result->result_code = GRAPHQLITE_OK;
    return result;
}

// Helper function to serialize a complete node entity
static char* serialize_node_entity(sqlite3 *db, int64_t node_id) {
    char *result = malloc(4096);
    if (!result) return NULL;
    
    // Get node labels
    sqlite3_stmt *label_stmt;
    const char *label_query = "SELECT label FROM node_labels WHERE node_id = ?";
    if (sqlite3_prepare_v2(db, label_query, -1, &label_stmt, NULL) != SQLITE_OK) {
        free(result);
        return NULL;
    }
    sqlite3_bind_int64(label_stmt, 1, node_id);
    
    char labels_str[512] = "[";
    int first_label = 1;
    while (sqlite3_step(label_stmt) == SQLITE_ROW) {
        if (!first_label) strcat(labels_str, ", ");
        strcat(labels_str, "\"");
        strcat(labels_str, (char*)sqlite3_column_text(label_stmt, 0));
        strcat(labels_str, "\"");
        first_label = 0;
    }
    strcat(labels_str, "]");
    sqlite3_finalize(label_stmt);
    
    // Get node properties
    char properties_str[2048] = "{";
    int first_prop = 1;
    
    // Text properties
    const char *text_query = "SELECT pk.key, npt.value FROM node_props_text npt "
                             "JOIN property_keys pk ON npt.key_id = pk.id "
                             "WHERE npt.node_id = ?";
    sqlite3_stmt *text_stmt;
    if (sqlite3_prepare_v2(db, text_query, -1, &text_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(text_stmt, 1, node_id);
        while (sqlite3_step(text_stmt) == SQLITE_ROW) {
            if (!first_prop) strcat(properties_str, ", ");
            strcat(properties_str, "\"");
            strcat(properties_str, (char*)sqlite3_column_text(text_stmt, 0));
            strcat(properties_str, "\": \"");
            strcat(properties_str, (char*)sqlite3_column_text(text_stmt, 1));
            strcat(properties_str, "\"");
            first_prop = 0;
        }
        sqlite3_finalize(text_stmt);
    }
    
    // Integer properties
    const char *int_query = "SELECT pk.key, npi.value FROM node_props_int npi "
                            "JOIN property_keys pk ON npi.key_id = pk.id "
                            "WHERE npi.node_id = ?";
    sqlite3_stmt *int_stmt;
    if (sqlite3_prepare_v2(db, int_query, -1, &int_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(int_stmt, 1, node_id);
        while (sqlite3_step(int_stmt) == SQLITE_ROW) {
            if (!first_prop) strcat(properties_str, ", ");
            char prop_str[256];
            snprintf(prop_str, sizeof(prop_str), "\"%s\": %lld", 
                    (char*)sqlite3_column_text(int_stmt, 0),
                    sqlite3_column_int64(int_stmt, 1));
            strcat(properties_str, prop_str);
            first_prop = 0;
        }
        sqlite3_finalize(int_stmt);
    }
    
    // Float properties
    const char *float_query = "SELECT pk.key, npr.value FROM node_props_real npr "
                              "JOIN property_keys pk ON npr.key_id = pk.id "
                              "WHERE npr.node_id = ?";
    sqlite3_stmt *float_stmt;
    if (sqlite3_prepare_v2(db, float_query, -1, &float_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(float_stmt, 1, node_id);
        while (sqlite3_step(float_stmt) == SQLITE_ROW) {
            if (!first_prop) strcat(properties_str, ", ");
            char prop_str[256];
            snprintf(prop_str, sizeof(prop_str), "\"%s\": %g", 
                    (char*)sqlite3_column_text(float_stmt, 0),
                    sqlite3_column_double(float_stmt, 1));
            strcat(properties_str, prop_str);
            first_prop = 0;
        }
        sqlite3_finalize(float_stmt);
    }
    
    // Boolean properties
    const char *bool_query = "SELECT pk.key, npb.value FROM node_props_bool npb "
                             "JOIN property_keys pk ON npb.key_id = pk.id "
                             "WHERE npb.node_id = ?";
    sqlite3_stmt *bool_stmt;
    if (sqlite3_prepare_v2(db, bool_query, -1, &bool_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(bool_stmt, 1, node_id);
        while (sqlite3_step(bool_stmt) == SQLITE_ROW) {
            if (!first_prop) strcat(properties_str, ", ");
            char prop_str[256];
            snprintf(prop_str, sizeof(prop_str), "\"%s\": %s", 
                    (char*)sqlite3_column_text(bool_stmt, 0),
                    sqlite3_column_int(bool_stmt, 1) ? "true" : "false");
            strcat(properties_str, prop_str);
            first_prop = 0;
        }
        sqlite3_finalize(bool_stmt);
    }
    
    strcat(properties_str, "}");
    
    // Build complete node JSON
    snprintf(result, 4096, "{\"identity\": %lld, \"labels\": %s, \"properties\": %s}", 
             node_id, labels_str, properties_str);
    
    return result;
}

// Helper function to serialize a complete relationship entity
static char* serialize_relationship_entity(sqlite3 *db, int64_t edge_id) {
    char *result = malloc(2048);
    if (!result) return NULL;
    
    // Get edge basic info
    sqlite3_stmt *edge_stmt;
    const char *edge_query = "SELECT type, source_id, target_id FROM edges WHERE id = ?";
    if (sqlite3_prepare_v2(db, edge_query, -1, &edge_stmt, NULL) != SQLITE_OK) {
        free(result);
        return NULL;
    }
    sqlite3_bind_int64(edge_stmt, 1, edge_id);
    
    if (sqlite3_step(edge_stmt) != SQLITE_ROW) {
        sqlite3_finalize(edge_stmt);
        free(result);
        return NULL;
    }
    
    const char *type_text = (char*)sqlite3_column_text(edge_stmt, 0);
    char type[256];
    strncpy(type, type_text ? type_text : "", sizeof(type) - 1);
    type[sizeof(type) - 1] = '\0';
    
    int64_t start_id = sqlite3_column_int64(edge_stmt, 1);
    int64_t end_id = sqlite3_column_int64(edge_stmt, 2);
    sqlite3_finalize(edge_stmt);
    
    // Get edge properties (simplified for now - just text properties)
    char properties_str[1024] = "{";
    const char *prop_query = "SELECT pk.key, ept.value FROM edge_props_text ept "
                             "JOIN property_keys pk ON ept.key_id = pk.id "
                             "WHERE ept.edge_id = ?";
    sqlite3_stmt *prop_stmt;
    int first_prop = 1;
    if (sqlite3_prepare_v2(db, prop_query, -1, &prop_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(prop_stmt, 1, edge_id);
        while (sqlite3_step(prop_stmt) == SQLITE_ROW) {
            if (!first_prop) strcat(properties_str, ", ");
            strcat(properties_str, "\"");
            strcat(properties_str, (char*)sqlite3_column_text(prop_stmt, 0));
            strcat(properties_str, "\": \"");
            strcat(properties_str, (char*)sqlite3_column_text(prop_stmt, 1));
            strcat(properties_str, "\"");
            first_prop = 0;
        }
        sqlite3_finalize(prop_stmt);
    }
    strcat(properties_str, "}");
    
    // Build complete relationship JSON
    snprintf(result, 2048, "{\"identity\": %lld, \"type\": \"%s\", \"start\": %lld, \"end\": %lld, \"properties\": %s}", 
             edge_id, type, start_id, end_id, properties_str);
    
    return result;
}

static graphqlite_result_t* execute_match_relationship(sqlite3 *db, cypher_ast_node_t *rel_pattern, cypher_ast_node_t *return_stmt) {
    graphqlite_result_t *result = graphqlite_result_create();
    if (!result) return NULL;
    
    // Extract nodes and edge from relationship pattern
    cypher_ast_node_t *left_node = rel_pattern->data.relationship_pattern.left_node;
    cypher_ast_node_t *edge = rel_pattern->data.relationship_pattern.edge;
    cypher_ast_node_t *right_node = rel_pattern->data.relationship_pattern.right_node;
    int direction = rel_pattern->data.relationship_pattern.direction;
    
    if (!left_node || !edge || !right_node) {
        graphqlite_result_set_error(result, "Invalid relationship pattern");
        return result;
    }
    
    // Extract patterns for matching
    const char *left_label = NULL;
    const char *right_label = NULL;
    const char *edge_type = NULL;
    
    // Get left node label
    if (left_node->data.node_pattern.label && left_node->data.node_pattern.label->type == AST_LABEL) {
        left_label = left_node->data.node_pattern.label->data.label.name;
    }
    
    // Get right node label  
    if (right_node->data.node_pattern.label && right_node->data.node_pattern.label->type == AST_LABEL) {
        right_label = right_node->data.node_pattern.label->data.label.name;
    }
    
    // Get edge type
    if (edge->data.edge_pattern.label && edge->data.edge_pattern.label->type == AST_LABEL) {
        edge_type = edge->data.edge_pattern.label->data.label.name;
    }
    
    // Build query to match relationships
    char query[2048];
    if (direction == 1) {  // Right direction ->
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT "
            "  ln.id as left_id, ll.label as left_label, "
            "  e.id as edge_id, e.type as edge_type, "
            "  rn.id as right_id, rl.label as right_label "
            "FROM edges e "
            "JOIN nodes ln ON e.source_id = ln.id "
            "JOIN nodes rn ON e.target_id = rn.id "
            "JOIN node_labels ll ON ln.id = ll.node_id "
            "JOIN node_labels rl ON rn.id = rl.node_id "
            "WHERE 1=1 %s %s %s",
            left_label ? "AND ll.label = ?" : "",
            edge_type ? "AND e.type = ?" : "",
            right_label ? "AND rl.label = ?" : "");
    } else if (direction == -1) {  // Left direction <-
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT "
            "  ln.id as left_id, ll.label as left_label, "
            "  e.id as edge_id, e.type as edge_type, "
            "  rn.id as right_id, rl.label as right_label "
            "FROM edges e "
            "JOIN nodes ln ON e.target_id = ln.id "  // Reversed
            "JOIN nodes rn ON e.source_id = rn.id "  // Reversed
            "JOIN node_labels ll ON ln.id = ll.node_id "
            "JOIN node_labels rl ON rn.id = rl.node_id "
            "WHERE 1=1 %s %s %s",
            left_label ? "AND ll.label = ?" : "",
            edge_type ? "AND e.type = ?" : "",
            right_label ? "AND rl.label = ?" : "");
    } else {  // Undirected (direction == 0)
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT "
            "  ln.id as left_id, ll.label as left_label, "
            "  e.id as edge_id, e.type as edge_type, "
            "  rn.id as right_id, rl.label as right_label "
            "FROM edges e "
            "JOIN nodes ln ON (e.source_id = ln.id OR e.target_id = ln.id) "
            "JOIN nodes rn ON (e.source_id = rn.id OR e.target_id = rn.id) "
            "JOIN node_labels ll ON ln.id = ll.node_id "
            "JOIN node_labels rl ON rn.id = rl.node_id "
            "WHERE ln.id != rn.id %s %s %s",
            left_label ? "AND ll.label = ?" : "",
            edge_type ? "AND e.type = ?" : "",
            right_label ? "AND rl.label = ?" : "");
    }
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        graphqlite_result_set_error(result, "Failed to prepare relationship SELECT statement");
        return result;
    }
    
    // Bind parameters
    int param_idx = 1;
    if (left_label) {
        sqlite3_bind_text(stmt, param_idx++, left_label, -1, SQLITE_STATIC);
    }
    if (edge_type) {
        sqlite3_bind_text(stmt, param_idx++, edge_type, -1, SQLITE_STATIC);
    }
    if (right_label) {
        sqlite3_bind_text(stmt, param_idx++, right_label, -1, SQLITE_STATIC);
    }
    
    // TODO: Handle edge properties in WHERE clause
    
    // Parse which variable to return from RETURN statement
    const char *return_variable = NULL;
    if (return_stmt && return_stmt->type == AST_RETURN_STATEMENT) {
        cypher_ast_node_t *var = return_stmt->data.return_stmt.variable;
        if (var && var->type == AST_VARIABLE) {
            return_variable = var->data.variable.name;
        }
    }
    
    if (!return_variable) {
        graphqlite_result_set_error(result, "Cannot determine which variable to return");
        sqlite3_finalize(stmt);
        return result;
    }
    
    // Extract variable names from relationship pattern to map RETURN variable
    const char *left_var = NULL, *edge_var = NULL, *right_var = NULL;
    
    if (left_node && left_node->data.node_pattern.variable) {
        left_var = left_node->data.node_pattern.variable->data.variable.name;
    }
    if (edge && edge->data.edge_pattern.variable) {
        edge_var = edge->data.edge_pattern.variable->data.variable.name;
    }
    if (right_node && right_node->data.node_pattern.variable) {
        right_var = right_node->data.node_pattern.variable->data.variable.name;
    }
    
    // Determine which part of the pattern to return
    enum return_type { RETURN_LEFT_NODE, RETURN_EDGE, RETURN_RIGHT_NODE } return_type;
    if (left_var && strcmp(return_variable, left_var) == 0) {
        return_type = RETURN_LEFT_NODE;
    } else if (edge_var && strcmp(return_variable, edge_var) == 0) {
        return_type = RETURN_EDGE;
    } else if (right_var && strcmp(return_variable, right_var) == 0) {
        return_type = RETURN_RIGHT_NODE;
    } else {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Variable '%s' not found in pattern", return_variable);
        graphqlite_result_set_error(result, error_msg);
        sqlite3_finalize(stmt);
        return result;
    }
    
    // Set up single result column for entity
    graphqlite_result_add_column(result, return_variable, GRAPHQLITE_TYPE_TEXT);
    
    // Fetch results
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        graphqlite_result_add_row(result);
        size_t row_idx = result->row_count - 1;
        
        // Get entity ID based on what we're returning and serialize complete entity
        graphqlite_value_t value;
        value.type = GRAPHQLITE_TYPE_TEXT;
        
        if (return_type == RETURN_LEFT_NODE) {
            int64_t node_id = sqlite3_column_int64(stmt, 0);
            value.data.text = serialize_node_entity(db, node_id);
        } else if (return_type == RETURN_EDGE) {
            int64_t edge_id = sqlite3_column_int64(stmt, 2);
            value.data.text = serialize_relationship_entity(db, edge_id);
        } else { // RETURN_RIGHT_NODE
            int64_t node_id = sqlite3_column_int64(stmt, 4);
            value.data.text = serialize_node_entity(db, node_id);
        }
        
        graphqlite_result_set_value(result, row_idx, 0, &value);
    }
    
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        graphqlite_result_set_error(result, "Failed to execute relationship SELECT statement");
        return result;
    }
    
    result->result_code = GRAPHQLITE_OK;
    return result;
}

static graphqlite_result_t* execute_match_relationship_with_where(sqlite3 *db, cypher_ast_node_t *rel_pattern, cypher_ast_node_t *where_clause, cypher_ast_node_t *return_stmt) {
    // If no WHERE clause, fall back to original function
    if (!where_clause) {
        return execute_match_relationship(db, rel_pattern, return_stmt);
    }
    
    graphqlite_result_t *result = graphqlite_result_create();
    if (!result) return NULL;
    
    // Extract nodes and edge from relationship pattern (same as original)
    cypher_ast_node_t *left_node = rel_pattern->data.relationship_pattern.left_node;
    cypher_ast_node_t *edge = rel_pattern->data.relationship_pattern.edge;
    cypher_ast_node_t *right_node = rel_pattern->data.relationship_pattern.right_node;
    int direction = rel_pattern->data.relationship_pattern.direction;
    
    if (!left_node || !edge || !right_node) {
        graphqlite_result_set_error(result, "Invalid relationship pattern");
        return result;
    }
    
    // Extract patterns for matching (same as original)
    const char *left_label = NULL;
    const char *right_label = NULL;
    const char *edge_type = NULL;
    
    if (left_node->data.node_pattern.label && left_node->data.node_pattern.label->type == AST_LABEL) {
        left_label = left_node->data.node_pattern.label->data.label.name;
    }
    
    if (right_node->data.node_pattern.label && right_node->data.node_pattern.label->type == AST_LABEL) {
        right_label = right_node->data.node_pattern.label->data.label.name;
    }
    
    if (edge->data.edge_pattern.label && edge->data.edge_pattern.label->type == AST_LABEL) {
        edge_type = edge->data.edge_pattern.label->data.label.name;
    }
    
    // Build query to match relationships (same as original)
    char query[2048];
    if (direction == 1) {  // Right direction ->
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT "
            "  ln.id as left_id, ll.label as left_label, "
            "  e.id as edge_id, e.type as edge_type, "
            "  rn.id as right_id, rl.label as right_label "
            "FROM edges e "
            "JOIN nodes ln ON e.source_id = ln.id "
            "JOIN nodes rn ON e.target_id = rn.id "
            "JOIN node_labels ll ON ln.id = ll.node_id "
            "JOIN node_labels rl ON rn.id = rl.node_id "
            "WHERE 1=1 %s %s %s",
            left_label ? "AND ll.label = ?" : "",
            edge_type ? "AND e.type = ?" : "",
            right_label ? "AND rl.label = ?" : "");
    } else if (direction == -1) {  // Left direction <-
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT "
            "  ln.id as left_id, ll.label as left_label, "
            "  e.id as edge_id, e.type as edge_type, "
            "  rn.id as right_id, rl.label as right_label "
            "FROM edges e "
            "JOIN nodes ln ON e.target_id = ln.id "  // Reversed
            "JOIN nodes rn ON e.source_id = rn.id "  // Reversed
            "JOIN node_labels ll ON ln.id = ll.node_id "
            "JOIN node_labels rl ON rn.id = rl.node_id "
            "WHERE 1=1 %s %s %s",
            left_label ? "AND ll.label = ?" : "",
            edge_type ? "AND e.type = ?" : "",
            right_label ? "AND rl.label = ?" : "");
    } else {  // Undirected (direction == 0)
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT "
            "  ln.id as left_id, ll.label as left_label, "
            "  e.id as edge_id, e.type as edge_type, "
            "  rn.id as right_id, rl.label as right_label "
            "FROM edges e "
            "JOIN nodes ln ON (e.source_id = ln.id OR e.target_id = ln.id) "
            "JOIN nodes rn ON (e.source_id = rn.id OR e.target_id = rn.id) "
            "JOIN node_labels ll ON ln.id = ll.node_id "
            "JOIN node_labels rl ON rn.id = rl.node_id "
            "WHERE ln.id != rn.id %s %s %s",
            left_label ? "AND ll.label = ?" : "",
            edge_type ? "AND e.type = ?" : "",
            right_label ? "AND rl.label = ?" : "");
    }
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        graphqlite_result_set_error(result, "Failed to prepare relationship SELECT statement");
        return result;
    }
    
    // Bind parameters (same as original)
    int param_idx = 1;
    if (left_label) {
        sqlite3_bind_text(stmt, param_idx++, left_label, -1, SQLITE_STATIC);
    }
    if (edge_type) {
        sqlite3_bind_text(stmt, param_idx++, edge_type, -1, SQLITE_STATIC);
    }
    if (right_label) {
        sqlite3_bind_text(stmt, param_idx++, right_label, -1, SQLITE_STATIC);
    }
    
    // Parse which variable to return
    const char *return_variable = NULL;
    if (return_stmt && return_stmt->type == AST_RETURN_STATEMENT) {
        cypher_ast_node_t *var = return_stmt->data.return_stmt.variable;
        if (var && var->type == AST_VARIABLE) {
            return_variable = var->data.variable.name;
        }
    }
    
    if (!return_variable) {
        graphqlite_result_set_error(result, "Cannot determine which variable to return");
        sqlite3_finalize(stmt);
        return result;
    }
    
    // Extract variable names from relationship pattern
    const char *left_var = NULL, *edge_var = NULL, *right_var = NULL;
    
    if (left_node && left_node->data.node_pattern.variable) {
        left_var = left_node->data.node_pattern.variable->data.variable.name;
    }
    if (edge && edge->data.edge_pattern.variable) {
        edge_var = edge->data.edge_pattern.variable->data.variable.name;
    }
    if (right_node && right_node->data.node_pattern.variable) {
        right_var = right_node->data.node_pattern.variable->data.variable.name;
    }
    
    // Determine which part of the pattern to return
    enum return_type { RETURN_LEFT_NODE, RETURN_EDGE, RETURN_RIGHT_NODE } return_type;
    if (left_var && strcmp(return_variable, left_var) == 0) {
        return_type = RETURN_LEFT_NODE;
    } else if (edge_var && strcmp(return_variable, edge_var) == 0) {
        return_type = RETURN_EDGE;
    } else if (right_var && strcmp(return_variable, right_var) == 0) {
        return_type = RETURN_RIGHT_NODE;
    } else {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Variable '%s' not found in pattern", return_variable);
        graphqlite_result_set_error(result, error_msg);
        sqlite3_finalize(stmt);
        return result;
    }
    
    // Set up result column
    graphqlite_result_add_column(result, return_variable, GRAPHQLITE_TYPE_TEXT);
    
    // Fetch and filter results with WHERE clause
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int64_t left_id = sqlite3_column_int64(stmt, 0);
        int64_t edge_id = sqlite3_column_int64(stmt, 2);
        int64_t right_id = sqlite3_column_int64(stmt, 4);
        
        // Create variable bindings for WHERE evaluation
        variable_binding_t bindings[3];
        int binding_count = 0;
        
        if (left_var) {
            bindings[binding_count].variable_name = (char*)left_var;
            bindings[binding_count].node_id = left_id;
            bindings[binding_count].edge_id = -1;
            bindings[binding_count].is_edge = 0;
            binding_count++;
        }
        
        if (edge_var) {
            bindings[binding_count].variable_name = (char*)edge_var;
            bindings[binding_count].node_id = -1;
            bindings[binding_count].edge_id = edge_id;
            bindings[binding_count].is_edge = 1;
            binding_count++;
        }
        
        if (right_var) {
            bindings[binding_count].variable_name = (char*)right_var;
            bindings[binding_count].node_id = right_id;
            bindings[binding_count].edge_id = -1;
            bindings[binding_count].is_edge = 0;
            binding_count++;
        }
        
        // Evaluate WHERE clause
        eval_result_t *where_result = evaluate_expression(db, where_clause->data.where_clause.expression, bindings, binding_count);
        
        // Check if WHERE condition is satisfied
        int include_result = 0;
        if (where_result && where_result->type == GRAPHQLITE_TYPE_BOOLEAN && where_result->data.boolean) {
            include_result = 1;
        }
        
        free_eval_result(where_result);
        
        if (include_result) {
            graphqlite_result_add_row(result);
            size_t row_idx = result->row_count - 1;
            
            // Get entity ID based on what we're returning and serialize complete entity
            graphqlite_value_t value;
            value.type = GRAPHQLITE_TYPE_TEXT;
            
            if (return_type == RETURN_LEFT_NODE) {
                value.data.text = serialize_node_entity(db, left_id);
            } else if (return_type == RETURN_EDGE) {
                value.data.text = serialize_relationship_entity(db, edge_id);
            } else { // RETURN_RIGHT_NODE
                value.data.text = serialize_node_entity(db, right_id);
            }
            
            graphqlite_result_set_value(result, row_idx, 0, &value);
        }
    }
    
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        graphqlite_result_set_error(result, "Failed to execute relationship SELECT statement");
        return result;
    }
    
    result->result_code = GRAPHQLITE_OK;
    return result;
}

static graphqlite_result_t* execute_query(sqlite3 *db, cypher_ast_node_t *ast) {
    if (!ast) {
        graphqlite_result_t *result = graphqlite_result_create();
        if (result) {
            graphqlite_result_set_error(result, "Invalid AST");
        }
        return result;
    }
    
    switch (ast->type) {
        case AST_CREATE_STATEMENT:
            return execute_create_statement(db, ast);
            
        case AST_COMPOUND_STATEMENT: {
            // MATCH + RETURN combination
            cypher_ast_node_t *match_stmt = ast->data.compound_stmt.match_stmt;
            cypher_ast_node_t *return_stmt = ast->data.compound_stmt.return_stmt;
            return execute_match_statement(db, match_stmt, return_stmt);
        }
        
        default: {
            graphqlite_result_t *result = graphqlite_result_create();
            if (result) {
                graphqlite_result_set_error(result, "Unsupported statement type");
            }
            return result;
        }
    }
}

// ============================================================================
// SQLite Function Implementation
// ============================================================================

void graphqlite_cypher_func(sqlite3_context *context, int argc, sqlite3_value **argv) {
    if (argc != 1) {
        sqlite3_result_error(context, "cypher() function requires exactly one argument", -1);
        return;
    }
    
    const char *query = (const char*)sqlite3_value_text(argv[0]);
    if (!query) {
        sqlite3_result_error(context, "Query string cannot be NULL", -1);
        return;
    }
    
    // Get database connection
    sqlite3 *db = sqlite3_context_db_handle(context);
    
    // Parse the query
    cypher_ast_node_t *ast = parse_cypher_query(query);
    if (!ast) {
        sqlite3_result_error(context, "Failed to parse OpenCypher query", -1);
        return;
    }
    
    // Execute the query
    graphqlite_result_t *result = execute_query(db, ast);
    
    // Clean up AST
    ast_free(ast);
    
    if (!result) {
        sqlite3_result_error(context, "Query execution failed", -1);
        return;
    }
    
    if (result->result_code != GRAPHQLITE_OK) {
        sqlite3_result_error(context, result->error_message ? result->error_message : "Unknown error", -1);
        graphqlite_result_free(result);
        return;
    }
    
    // Return actual result data
    if (result->row_count > 0 && result->column_count > 0) {
        // For single value results, return the value directly
        if (result->column_count == 1) {
            graphqlite_value_t *value = &result->rows[0].values[0];
            
            switch (value->type) {
                case GRAPHQLITE_TYPE_INTEGER:
                    sqlite3_result_int64(context, value->data.integer);
                    break;
                case GRAPHQLITE_TYPE_TEXT:
                    if (value->data.text) {
                        sqlite3_result_text(context, value->data.text, -1, SQLITE_TRANSIENT);
                    } else {
                        sqlite3_result_null(context);
                    }
                    break;
                case GRAPHQLITE_TYPE_FLOAT:
                    sqlite3_result_double(context, value->data.float_val);
                    break;
                case GRAPHQLITE_TYPE_BOOLEAN:
                    sqlite3_result_int(context, value->data.boolean ? 1 : 0);
                    break;
                default:
                    sqlite3_result_null(context);
                    break;
            }
        } else {
            // For multiple columns, return JSON-like format
            char result_str[2048] = "{";
            
            for (size_t col = 0; col < result->column_count; col++) {
                char col_str[512];
                graphqlite_value_t *value = &result->rows[0].values[col];
                
                if (col > 0) {
                    strncat(result_str, ", ", sizeof(result_str) - strlen(result_str) - 1);
                }
                
                switch (value->type) {
                    case GRAPHQLITE_TYPE_INTEGER:
                        snprintf(col_str, sizeof(col_str), "\"%s\": %lld", 
                                result->columns[col].name, (long long)value->data.integer);
                        break;
                    case GRAPHQLITE_TYPE_TEXT:
                        snprintf(col_str, sizeof(col_str), "\"%s\": \"%s\"", 
                                result->columns[col].name, 
                                value->data.text ? value->data.text : "null");
                        break;
                    case GRAPHQLITE_TYPE_FLOAT:
                        snprintf(col_str, sizeof(col_str), "\"%s\": %g", 
                                result->columns[col].name, value->data.float_val);
                        break;
                    case GRAPHQLITE_TYPE_BOOLEAN:
                        snprintf(col_str, sizeof(col_str), "\"%s\": %s", 
                                result->columns[col].name, value->data.boolean ? "true" : "false");
                        break;
                    default:
                        snprintf(col_str, sizeof(col_str), "\"%s\": null", result->columns[col].name);
                        break;
                }
                
                strncat(result_str, col_str, sizeof(result_str) - strlen(result_str) - 1);
            }
            
            strncat(result_str, "}", sizeof(result_str) - strlen(result_str) - 1);
            sqlite3_result_text(context, result_str, -1, SQLITE_TRANSIENT);
        }
    } else {
        // For successful operations with no result data (like CREATE without RETURN)
        if (result->result_code == GRAPHQLITE_OK) {
            sqlite3_result_text(context, "Query executed successfully", -1, SQLITE_STATIC);
        } else {
            sqlite3_result_null(context);
        }
    }
    
    graphqlite_result_free(result);
}

// ============================================================================
// Extension Entry Point
// ============================================================================

// Simple test function - using direct calls like working simple_test.c
static void simple_test_func(sqlite3_context *context, int argc, sqlite3_value **argv) {
    (void)argc;
    (void)argv;
    sqlite3_result_text(context, "GraphQLite extension loaded successfully!", -1, SQLITE_STATIC);
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_graphqlite_init(
  sqlite3 *db,
  char **pzErrMsg,
  const sqlite3_api_routines *pApi
){
  int rc = SQLITE_OK;
  SQLITE_EXTENSION_INIT2(pApi);
  
  /* Register the graphqlite_test function */
  sqlite3_create_function(db, "graphqlite_test", 0, SQLITE_UTF8, 0,
                         simple_test_func, 0, 0);
  
  /* Register the main cypher() function */
  sqlite3_create_function(db, "cypher", 1, SQLITE_UTF8, 0,
                         graphqlite_cypher_func, 0, 0);
  
  /* Create schema during initialization */
  create_schema(db);
  
  return rc;
}