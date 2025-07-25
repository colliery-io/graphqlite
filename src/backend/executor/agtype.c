/*
 * AGType implementation - simplified from Apache AGE for SQLite
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "executor/agtype.h"

/* Forward declarations */
static int load_node_properties(sqlite3 *db, int64_t node_id, agtype_pair **pairs, int *num_pairs);
static int load_edge_properties(sqlite3 *db, int64_t edge_id, agtype_pair **pairs, int *num_pairs);

/* Create a NULL agtype value */
agtype_value* agtype_value_create_null(void)
{
    agtype_value *val = malloc(sizeof(agtype_value));
    if (!val) return NULL;
    
    val->type = AGTV_NULL;
    return val;
}

/* Create a string agtype value */
agtype_value* agtype_value_create_string(const char* str)
{
    if (!str) return agtype_value_create_null();
    
    agtype_value *val = malloc(sizeof(agtype_value));
    if (!val) return NULL;
    
    val->type = AGTV_STRING;
    val->val.string.len = strlen(str);
    val->val.string.val = strdup(str);
    if (!val->val.string.val) {
        free(val);
        return NULL;
    }
    
    return val;
}

/* Create an integer agtype value */
agtype_value* agtype_value_create_integer(int64_t int_val)
{
    agtype_value *val = malloc(sizeof(agtype_value));
    if (!val) return NULL;
    
    val->type = AGTV_INTEGER;
    val->val.int_value = int_val;
    return val;
}

/* Create a float agtype value */
agtype_value* agtype_value_create_float(double float_val)
{
    agtype_value *val = malloc(sizeof(agtype_value));
    if (!val) return NULL;
    
    val->type = AGTV_FLOAT;
    val->val.float_value = float_val;
    return val;
}

/* Create a boolean agtype value */
agtype_value* agtype_value_create_bool(bool bool_val)
{
    agtype_value *val = malloc(sizeof(agtype_value));
    if (!val) return NULL;
    
    val->type = AGTV_BOOL;
    val->val.boolean = bool_val;
    return val;
}

/* Create a vertex agtype value */
agtype_value* agtype_value_create_vertex(int64_t id, const char* label)
{
    agtype_value *val = malloc(sizeof(agtype_value));
    if (!val) return NULL;
    
    val->type = AGTV_VERTEX;
    val->val.entity.id = id;
    val->val.entity.label = label ? strdup(label) : NULL;
    val->val.entity.num_pairs = 0;
    val->val.entity.pairs = NULL;
    
    return val;
}

/* Create an edge agtype value */
agtype_value* agtype_value_create_edge(int64_t id, const char* label, int64_t start_id, int64_t end_id)
{
    agtype_value *val = malloc(sizeof(agtype_value));
    if (!val) return NULL;
    
    val->type = AGTV_EDGE;
    val->val.edge.id = id;
    val->val.edge.label = label ? strdup(label) : NULL;
    val->val.edge.start_id = start_id;
    val->val.edge.end_id = end_id;
    val->val.edge.num_pairs = 0;
    val->val.edge.pairs = NULL;
    
    return val;
}

/* Create a vertex agtype value with properties loaded from database */
agtype_value* agtype_value_create_vertex_with_properties(sqlite3 *db, int64_t id, const char* label)
{
    agtype_value *val = agtype_value_create_vertex(id, label);
    if (!val || !db) return val;
    
    /* Load properties from EAV schema */
    agtype_pair *pairs = NULL;
    int num_pairs = 0;
    
    if (load_node_properties(db, id, &pairs, &num_pairs) == 0 && num_pairs > 0) {
        val->val.entity.pairs = pairs;
        val->val.entity.num_pairs = num_pairs;
    }
    
    return val;
}

/* Create an edge agtype value with properties loaded from database */
agtype_value* agtype_value_create_edge_with_properties(sqlite3 *db, int64_t id, const char* label, int64_t start_id, int64_t end_id)
{
    agtype_value *val = agtype_value_create_edge(id, label, start_id, end_id);
    if (!val || !db) return val;
    
    /* Load properties from EAV schema */
    agtype_pair *pairs = NULL;
    int num_pairs = 0;
    
    if (load_edge_properties(db, id, &pairs, &num_pairs) == 0 && num_pairs > 0) {
        val->val.edge.pairs = pairs;
        val->val.edge.num_pairs = num_pairs;
    }
    
    return val;
}

/* Free an agtype value */
void agtype_value_free(agtype_value *val)
{
    if (!val) return;
    
    switch (val->type) {
        case AGTV_STRING:
            free(val->val.string.val);
            break;
        case AGTV_VERTEX:
            free(val->val.entity.label);
            if (val->val.entity.pairs) {
                for (int i = 0; i < val->val.entity.num_pairs; i++) {
                    agtype_value_free(val->val.entity.pairs[i].key);
                    agtype_value_free(val->val.entity.pairs[i].value);
                }
                free(val->val.entity.pairs);
            }
            break;
        case AGTV_EDGE:
            free(val->val.edge.label);
            if (val->val.edge.pairs) {
                for (int i = 0; i < val->val.edge.num_pairs; i++) {
                    agtype_value_free(val->val.edge.pairs[i].key);
                    agtype_value_free(val->val.edge.pairs[i].value);
                }
                free(val->val.edge.pairs);
            }
            break;
        case AGTV_ARRAY:
            if (val->val.array.elems) {
                for (int i = 0; i < val->val.array.num_elems; i++) {
                    agtype_value_free(&val->val.array.elems[i]);
                }
                free(val->val.array.elems);
            }
            break;
        case AGTV_OBJECT:
            if (val->val.object.pairs) {
                for (int i = 0; i < val->val.object.num_pairs; i++) {
                    agtype_value_free(val->val.object.pairs[i].key);
                    agtype_value_free(val->val.object.pairs[i].value);
                }
                free(val->val.object.pairs);
            }
            break;
        default:
            /* Scalar types need no special cleanup */
            break;
    }
    
    free(val);
}

/* Load properties for a node from EAV schema */
static int load_node_properties(sqlite3 *db, int64_t node_id, agtype_pair **pairs, int *num_pairs)
{
    if (!db || !pairs || !num_pairs) return -1;
    
    *pairs = NULL;
    *num_pairs = 0;
    
    /* Count total properties across all type tables */
    const char *count_sql = 
        "SELECT COUNT(*) FROM ("
        "  SELECT pk.key FROM node_props_text npt "
        "  JOIN property_keys pk ON npt.key_id = pk.id WHERE npt.node_id = ? "
        "  UNION ALL "
        "  SELECT pk.key FROM node_props_int npi "
        "  JOIN property_keys pk ON npi.key_id = pk.id WHERE npi.node_id = ? "
        "  UNION ALL "
        "  SELECT pk.key FROM node_props_real npr "
        "  JOIN property_keys pk ON npr.key_id = pk.id WHERE npr.node_id = ? "
        "  UNION ALL "
        "  SELECT pk.key FROM node_props_bool npb "
        "  JOIN property_keys pk ON npb.key_id = pk.id WHERE npb.node_id = ?"
        ")";
    
    sqlite3_stmt *count_stmt;
    int total_props = 0;
    
    if (sqlite3_prepare_v2(db, count_sql, -1, &count_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(count_stmt, 1, node_id);
        sqlite3_bind_int64(count_stmt, 2, node_id);
        sqlite3_bind_int64(count_stmt, 3, node_id);
        sqlite3_bind_int64(count_stmt, 4, node_id);
        
        if (sqlite3_step(count_stmt) == SQLITE_ROW) {
            total_props = sqlite3_column_int(count_stmt, 0);
        }
        sqlite3_finalize(count_stmt);
    }
    
    if (total_props == 0) {
        return 0; /* No properties */
    }
    
    /* Allocate property array */
    *pairs = malloc(total_props * sizeof(agtype_pair));
    if (!*pairs) return -1;
    
    /* Load properties from all type tables */
    const char *props_sql = 
        "SELECT pk.key, npt.value, 'text' as type FROM node_props_text npt "
        "JOIN property_keys pk ON npt.key_id = pk.id WHERE npt.node_id = ? "
        "UNION ALL "
        "SELECT pk.key, CAST(npi.value AS TEXT), 'int' as type FROM node_props_int npi "
        "JOIN property_keys pk ON npi.key_id = pk.id WHERE npi.node_id = ? "
        "UNION ALL "
        "SELECT pk.key, CAST(npr.value AS TEXT), 'real' as type FROM node_props_real npr "
        "JOIN property_keys pk ON npr.key_id = pk.id WHERE npr.node_id = ? "
        "UNION ALL "
        "SELECT pk.key, CASE npb.value WHEN 1 THEN 'true' ELSE 'false' END, 'bool' as type FROM node_props_bool npb "
        "JOIN property_keys pk ON npb.key_id = pk.id WHERE npb.node_id = ?";
    
    sqlite3_stmt *props_stmt;
    int prop_index = 0;
    
    if (sqlite3_prepare_v2(db, props_sql, -1, &props_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(props_stmt, 1, node_id);
        sqlite3_bind_int64(props_stmt, 2, node_id);
        sqlite3_bind_int64(props_stmt, 3, node_id);
        sqlite3_bind_int64(props_stmt, 4, node_id);
        
        while (sqlite3_step(props_stmt) == SQLITE_ROW && prop_index < total_props) {
            const char *key = (const char*)sqlite3_column_text(props_stmt, 0);
            const char *value = (const char*)sqlite3_column_text(props_stmt, 1);
            const char *type = (const char*)sqlite3_column_text(props_stmt, 2);
            
            if (key && value && type) {
                (*pairs)[prop_index].key = agtype_value_create_string(key);
                
                /* Create appropriate agtype value based on type */
                if (strcmp(type, "int") == 0) {
                    (*pairs)[prop_index].value = agtype_value_create_integer(atoll(value));
                } else if (strcmp(type, "real") == 0) {
                    (*pairs)[prop_index].value = agtype_value_create_float(atof(value));
                } else if (strcmp(type, "bool") == 0) {
                    (*pairs)[prop_index].value = agtype_value_create_bool(strcmp(value, "true") == 0);
                } else {
                    (*pairs)[prop_index].value = agtype_value_create_string(value);
                }
                
                prop_index++;
            }
        }
        sqlite3_finalize(props_stmt);
    }
    
    *num_pairs = prop_index;
    return 0;
}

/* Load properties for an edge from EAV schema */
static int load_edge_properties(sqlite3 *db, int64_t edge_id, agtype_pair **pairs, int *num_pairs)
{
    if (!db || !pairs || !num_pairs) return -1;
    
    *pairs = NULL;
    *num_pairs = 0;
    
    /* Count total properties across all type tables */
    const char *count_sql = 
        "SELECT COUNT(*) FROM ("
        "  SELECT pk.key FROM edge_props_text ept "
        "  JOIN property_keys pk ON ept.key_id = pk.id WHERE ept.edge_id = ? "
        "  UNION ALL "
        "  SELECT pk.key FROM edge_props_int epi "
        "  JOIN property_keys pk ON epi.key_id = pk.id WHERE epi.edge_id = ? "
        "  UNION ALL "
        "  SELECT pk.key FROM edge_props_real epr "
        "  JOIN property_keys pk ON epr.key_id = pk.id WHERE epr.edge_id = ? "
        "  UNION ALL "
        "  SELECT pk.key FROM edge_props_bool epb "
        "  JOIN property_keys pk ON epb.key_id = pk.id WHERE epb.edge_id = ?"
        ")";
    
    sqlite3_stmt *count_stmt;
    int total_props = 0;
    
    if (sqlite3_prepare_v2(db, count_sql, -1, &count_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(count_stmt, 1, edge_id);
        sqlite3_bind_int64(count_stmt, 2, edge_id);
        sqlite3_bind_int64(count_stmt, 3, edge_id);
        sqlite3_bind_int64(count_stmt, 4, edge_id);
        
        if (sqlite3_step(count_stmt) == SQLITE_ROW) {
            total_props = sqlite3_column_int(count_stmt, 0);
        }
        sqlite3_finalize(count_stmt);
    }
    
    if (total_props == 0) {
        return 0; /* No properties */
    }
    
    /* Allocate property array */
    *pairs = malloc(total_props * sizeof(agtype_pair));
    if (!*pairs) return -1;
    
    /* Load properties from all type tables */
    const char *props_sql = 
        "SELECT pk.key, ept.value, 'text' as type FROM edge_props_text ept "
        "JOIN property_keys pk ON ept.key_id = pk.id WHERE ept.edge_id = ? "
        "UNION ALL "
        "SELECT pk.key, CAST(epi.value AS TEXT), 'int' as type FROM edge_props_int epi "
        "JOIN property_keys pk ON epi.key_id = pk.id WHERE epi.edge_id = ? "
        "UNION ALL "
        "SELECT pk.key, CAST(epr.value AS TEXT), 'real' as type FROM edge_props_real epr "
        "JOIN property_keys pk ON epr.key_id = pk.id WHERE epr.edge_id = ? "
        "UNION ALL "
        "SELECT pk.key, CASE epb.value WHEN 1 THEN 'true' ELSE 'false' END, 'bool' as type FROM edge_props_bool epb "
        "JOIN property_keys pk ON epb.key_id = pk.id WHERE epb.edge_id = ?";
    
    sqlite3_stmt *props_stmt;
    int prop_index = 0;
    
    if (sqlite3_prepare_v2(db, props_sql, -1, &props_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(props_stmt, 1, edge_id);
        sqlite3_bind_int64(props_stmt, 2, edge_id);
        sqlite3_bind_int64(props_stmt, 3, edge_id);
        sqlite3_bind_int64(props_stmt, 4, edge_id);
        
        while (sqlite3_step(props_stmt) == SQLITE_ROW && prop_index < total_props) {
            const char *key = (const char*)sqlite3_column_text(props_stmt, 0);
            const char *value = (const char*)sqlite3_column_text(props_stmt, 1);
            const char *type = (const char*)sqlite3_column_text(props_stmt, 2);
            
            if (key && value && type) {
                (*pairs)[prop_index].key = agtype_value_create_string(key);
                
                /* Create appropriate agtype value based on type */
                if (strcmp(type, "int") == 0) {
                    (*pairs)[prop_index].value = agtype_value_create_integer(atoll(value));
                } else if (strcmp(type, "real") == 0) {
                    (*pairs)[prop_index].value = agtype_value_create_float(atof(value));
                } else if (strcmp(type, "bool") == 0) {
                    (*pairs)[prop_index].value = agtype_value_create_bool(strcmp(value, "true") == 0);
                } else {
                    (*pairs)[prop_index].value = agtype_value_create_string(value);
                }
                
                prop_index++;
            }
        }
        sqlite3_finalize(props_stmt);
    }
    
    *num_pairs = prop_index;
    return 0;
}

/* Convert agtype value to AGE-compatible string representation */
char* agtype_value_to_string(agtype_value *val)
{
    if (!val) return strdup("null");
    
    char *result = NULL;
    
    switch (val->type) {
        case AGTV_NULL:
            result = strdup("null");
            break;
            
        case AGTV_STRING:
            /* Quote the string */
            result = malloc(val->val.string.len + 3);
            if (result) {
                snprintf(result, val->val.string.len + 3, "\"%s\"", val->val.string.val);
            }
            break;
            
        case AGTV_INTEGER: {
            result = malloc(32);
            if (result) {
                snprintf(result, 32, "%lld", (long long)val->val.int_value);
            }
            break;
        }
        
        case AGTV_FLOAT: {
            result = malloc(32);
            if (result) {
                snprintf(result, 32, "%.10g", val->val.float_value);
            }
            break;
        }
        
        case AGTV_BOOL:
            result = strdup(val->val.boolean ? "true" : "false");
            break;
            
        case AGTV_VERTEX: {
            /* Format: {"id": 123, "label": "Person", "properties": {"name": "Alice", "age": 30}}::vertex */
            int base_size = 200;
            if (val->val.entity.label) {
                base_size += strlen(val->val.entity.label);
            }
            
            /* Calculate size needed for properties */
            for (int i = 0; i < val->val.entity.num_pairs; i++) {
                if (val->val.entity.pairs[i].key && val->val.entity.pairs[i].value) {
                    char *key_str = agtype_value_to_string(val->val.entity.pairs[i].key);
                    char *value_str = agtype_value_to_string(val->val.entity.pairs[i].value);
                    if (key_str && value_str) {
                        base_size += strlen(key_str) + strlen(value_str) + 10; /* quotes, colon, comma */
                    }
                    free(key_str);
                    free(value_str);
                }
            }
            
            result = malloc(base_size);
            if (result) {
                snprintf(result, base_size, 
                    "{\"id\": %lld, \"label\": \"%s\", \"properties\": {",
                    (long long)val->val.entity.id,
                    val->val.entity.label ? val->val.entity.label : "");
                
                /* Add properties */
                for (int i = 0; i < val->val.entity.num_pairs; i++) {
                    if (val->val.entity.pairs[i].key && val->val.entity.pairs[i].value) {
                        if (i > 0) strcat(result, ", ");
                        
                        char *key_str = agtype_value_to_string(val->val.entity.pairs[i].key);
                        char *value_str = agtype_value_to_string(val->val.entity.pairs[i].value);
                        if (key_str && value_str) {
                            strcat(result, key_str);
                            strcat(result, ": ");
                            strcat(result, value_str);
                        }
                        free(key_str);
                        free(value_str);
                    }
                }
                
                strcat(result, "}}::vertex");
            }
            break;
        }
        
        case AGTV_EDGE: {
            /* Format: {"id": 123, "label": "KNOWS", "start_id": 456, "end_id": 789, "properties": {"since": 2020}}::edge */
            int base_size = 250;
            if (val->val.edge.label) {
                base_size += strlen(val->val.edge.label);
            }
            
            /* Calculate size needed for properties */
            for (int i = 0; i < val->val.edge.num_pairs; i++) {
                if (val->val.edge.pairs[i].key && val->val.edge.pairs[i].value) {
                    char *key_str = agtype_value_to_string(val->val.edge.pairs[i].key);
                    char *value_str = agtype_value_to_string(val->val.edge.pairs[i].value);
                    if (key_str && value_str) {
                        base_size += strlen(key_str) + strlen(value_str) + 10; /* quotes, colon, comma */
                    }
                    free(key_str);
                    free(value_str);
                }
            }
            
            result = malloc(base_size);
            if (result) {
                snprintf(result, base_size,
                    "{\"id\": %lld, \"label\": \"%s\", \"start_id\": %lld, \"end_id\": %lld, \"properties\": {",
                    (long long)val->val.edge.id,
                    val->val.edge.label ? val->val.edge.label : "",
                    (long long)val->val.edge.start_id,
                    (long long)val->val.edge.end_id);
                
                /* Add properties */
                for (int i = 0; i < val->val.edge.num_pairs; i++) {
                    if (val->val.edge.pairs[i].key && val->val.edge.pairs[i].value) {
                        if (i > 0) strcat(result, ", ");
                        
                        char *key_str = agtype_value_to_string(val->val.edge.pairs[i].key);
                        char *value_str = agtype_value_to_string(val->val.edge.pairs[i].value);
                        if (key_str && value_str) {
                            strcat(result, key_str);
                            strcat(result, ": ");
                            strcat(result, value_str);
                        }
                        free(key_str);
                        free(value_str);
                    }
                }
                
                strcat(result, "}}::edge");
            }
            break;
        }
        
        default:
            result = strdup("undefined");
            break;
    }
    
    return result ? result : strdup("null");
}