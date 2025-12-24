/*
 * AGType implementation - simplified from Apache AGE for SQLite
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "executor/agtype.h"
#include "parser/cypher_debug.h"

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

/* Parse a JSON string like {"id":1,"labels":["L"],"properties":{"k":"v"}} into a vertex agtype */
agtype_value* agtype_value_from_vertex_json(sqlite3 *db, const char *json)
{
    if (!json || json[0] != '{') return NULL;

    /* Use SQLite's JSON functions to extract values */
    sqlite3_stmt *stmt;
    int64_t id = 0;
    char *label = NULL;
    agtype_pair *pairs = NULL;
    int num_pairs = 0;

    /* Extract id */
    const char *id_sql = "SELECT json_extract(?, '$.id')";
    if (sqlite3_prepare_v2(db, id_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, json, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            id = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    /* Extract first label from labels array */
    const char *label_sql = "SELECT json_extract(?, '$.labels[0]')";
    if (sqlite3_prepare_v2(db, label_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, json, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *label_text = (const char*)sqlite3_column_text(stmt, 0);
            if (label_text) {
                label = strdup(label_text);
            }
        }
        sqlite3_finalize(stmt);
    }

    /* Extract properties as key/value pairs */
    const char *props_sql = "SELECT key, value FROM json_each(json_extract(?, '$.properties'))";
    if (sqlite3_prepare_v2(db, props_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, json, -1, SQLITE_STATIC);

        /* Count pairs first */
        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) count++;
        sqlite3_reset(stmt);

        if (count > 0) {
            pairs = malloc(count * sizeof(agtype_pair));
            num_pairs = 0;

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *key = (const char*)sqlite3_column_text(stmt, 0);
                const char *val_text = (const char*)sqlite3_column_text(stmt, 1);
                int val_type = sqlite3_column_type(stmt, 1);

                pairs[num_pairs].key = agtype_value_create_string(key);

                if (val_type == SQLITE_INTEGER) {
                    pairs[num_pairs].value = agtype_value_create_integer(sqlite3_column_int64(stmt, 1));
                } else if (val_type == SQLITE_FLOAT) {
                    pairs[num_pairs].value = agtype_value_create_float(sqlite3_column_double(stmt, 1));
                } else if (val_type == SQLITE_NULL) {
                    pairs[num_pairs].value = agtype_value_create_null();
                } else {
                    pairs[num_pairs].value = agtype_value_create_string(val_text);
                }
                num_pairs++;
            }
        }
        sqlite3_finalize(stmt);
    }

    /* Create the vertex value */
    agtype_value *val = agtype_value_create_vertex(id, label);
    if (val && num_pairs > 0) {
        val->val.entity.pairs = pairs;
        val->val.entity.num_pairs = num_pairs;
    } else if (pairs) {
        /* Clean up if vertex creation failed */
        for (int i = 0; i < num_pairs; i++) {
            agtype_value_free(pairs[i].key);
            agtype_value_free(pairs[i].value);
        }
        free(pairs);
    }

    free(label);
    return val;
}

/* Parse a JSON string like {"id":1,"type":"T","startNodeId":1,"endNodeId":2,"properties":{}} into an edge agtype */
agtype_value* agtype_value_from_edge_json(sqlite3 *db, const char *json)
{
    if (!json || json[0] != '{') return NULL;

    sqlite3_stmt *stmt;
    int64_t id = 0, start_id = 0, end_id = 0;
    char *type = NULL;
    agtype_pair *pairs = NULL;
    int num_pairs = 0;

    /* Extract id, type, startNodeId, endNodeId */
    const char *edge_sql = "SELECT json_extract(?, '$.id'), json_extract(?, '$.type'), "
                          "json_extract(?, '$.startNodeId'), json_extract(?, '$.endNodeId')";
    if (sqlite3_prepare_v2(db, edge_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, json, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, json, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, json, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, json, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            id = sqlite3_column_int64(stmt, 0);
            const char *type_text = (const char*)sqlite3_column_text(stmt, 1);
            if (type_text) type = strdup(type_text);
            start_id = sqlite3_column_int64(stmt, 2);
            end_id = sqlite3_column_int64(stmt, 3);
        }
        sqlite3_finalize(stmt);
    }

    /* Extract properties */
    const char *props_sql = "SELECT key, value FROM json_each(json_extract(?, '$.properties'))";
    if (sqlite3_prepare_v2(db, props_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, json, -1, SQLITE_STATIC);

        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) count++;
        sqlite3_reset(stmt);

        if (count > 0) {
            pairs = malloc(count * sizeof(agtype_pair));
            num_pairs = 0;

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *key = (const char*)sqlite3_column_text(stmt, 0);
                int val_type = sqlite3_column_type(stmt, 1);

                pairs[num_pairs].key = agtype_value_create_string(key);

                if (val_type == SQLITE_INTEGER) {
                    pairs[num_pairs].value = agtype_value_create_integer(sqlite3_column_int64(stmt, 1));
                } else if (val_type == SQLITE_FLOAT) {
                    pairs[num_pairs].value = agtype_value_create_float(sqlite3_column_double(stmt, 1));
                } else if (val_type == SQLITE_NULL) {
                    pairs[num_pairs].value = agtype_value_create_null();
                } else {
                    pairs[num_pairs].value = agtype_value_create_string((const char*)sqlite3_column_text(stmt, 1));
                }
                num_pairs++;
            }
        }
        sqlite3_finalize(stmt);
    }

    agtype_value *val = agtype_value_create_edge(id, type, start_id, end_id);
    if (val && num_pairs > 0) {
        val->val.edge.pairs = pairs;
        val->val.edge.num_pairs = num_pairs;
    } else if (pairs) {
        for (int i = 0; i < num_pairs; i++) {
            agtype_value_free(pairs[i].key);
            agtype_value_free(pairs[i].value);
        }
        free(pairs);
    }

    free(type);
    return val;
}

/* Create a path agtype value from array of vertex/edge values */
agtype_value* agtype_value_create_path(agtype_value **elements, int num_elements)
{
    CYPHER_DEBUG("agtype_value_create_path: Called with %d elements", num_elements);
    if (!elements || num_elements < 1) return NULL;
    
    /* Validate path structure: must start with vertex, alternate edge/vertex */
    CYPHER_DEBUG("agtype_value_create_path: First element type: %d", elements[0] ? elements[0]->type : -1);
    if (num_elements % 2 == 0 || elements[0]->type != AGTV_VERTEX) {
        CYPHER_DEBUG("agtype_value_create_path: Invalid path structure - odd elements: %s, starts with vertex: %s",
                     num_elements % 2 == 1 ? "yes" : "no",
                     elements[0] && elements[0]->type == AGTV_VERTEX ? "yes" : "no");
        return NULL; /* Invalid path structure */
    }
    
    for (int i = 1; i < num_elements; i++) {
        if (i % 2 == 1) {
            /* Odd positions should be edges */
            if (elements[i]->type != AGTV_EDGE) return NULL;
        } else {
            /* Even positions should be vertices */
            if (elements[i]->type != AGTV_VERTEX) return NULL;
        }
    }
    
    CYPHER_DEBUG("agtype_value_create_path: Path validation passed, allocating memory");
    
    agtype_value *val = malloc(sizeof(agtype_value));
    if (!val) {
        CYPHER_DEBUG("agtype_value_create_path: Failed to allocate path value");
        return NULL;
    }
    
    val->type = AGTV_PATH;
    val->val.array.num_elems = num_elements;
    
    CYPHER_DEBUG("agtype_value_create_path: Allocating array for %d elements, size = %zu", 
                 num_elements, num_elements * sizeof(agtype_value));
    
    val->val.array.elems = malloc(num_elements * sizeof(agtype_value));
    if (!val->val.array.elems) {
        CYPHER_DEBUG("agtype_value_create_path: Failed to allocate elements array");
        free(val);
        return NULL;
    }
    
    /* Deep copy the elements */
    for (int i = 0; i < num_elements; i++) {
        if (!elements[i]) {
            free(val->val.array.elems);
            free(val);
            return NULL;
        }
        
        /* Copy the structure */
        val->val.array.elems[i] = *elements[i];
        
        /* Deep copy strings and other allocated data */
        if (elements[i]->type == AGTV_VERTEX) {
            /* Deep copy vertex label string */
            if (elements[i]->val.entity.label) {
                val->val.array.elems[i].val.entity.label = strdup(elements[i]->val.entity.label);
            }
            /* TODO: Deep copy properties if needed */
        } else if (elements[i]->type == AGTV_EDGE) {
            /* Deep copy edge label string */
            if (elements[i]->val.edge.label) {
                val->val.array.elems[i].val.edge.label = strdup(elements[i]->val.edge.label);
            }
            /* TODO: Deep copy properties if needed */
        }
    }
    
    return val;
}

/* Build a path from collected vertex and edge values during query execution */
agtype_value* agtype_build_path(agtype_value **path_elements, int num_elements)
{
    if (!path_elements || num_elements < 1) {
        return agtype_value_create_null();
    }
    
    return agtype_value_create_path(path_elements, num_elements);
}

/* Free an agtype value */
void agtype_value_free(agtype_value *val)
{
    if (!val) return;
    
    CYPHER_DEBUG("agtype_value_free: Freeing agtype value %p of type %d", (void*)val, val->type);
    
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
        case AGTV_PATH:
            CYPHER_DEBUG("agtype_value_free: Freeing PATH with %d elements", val->val.array.num_elems);
            if (val->val.array.elems) {
                for (int i = 0; i < val->val.array.num_elems; i++) {
                    CYPHER_DEBUG("agtype_value_free: Freeing path element %d of type %d", i, val->val.array.elems[i].type);
                    /* Free allocated strings in each element */
                    if (val->val.array.elems[i].type == AGTV_VERTEX) {
                        free(val->val.array.elems[i].val.entity.label);
                        if (val->val.array.elems[i].val.entity.pairs) {
                            /* TODO: Free properties properly */
                            free(val->val.array.elems[i].val.entity.pairs);
                        }
                    } else if (val->val.array.elems[i].type == AGTV_EDGE) {
                        free(val->val.array.elems[i].val.edge.label);
                        if (val->val.array.elems[i].val.edge.pairs) {
                            /* TODO: Free properties properly */
                            free(val->val.array.elems[i].val.edge.pairs);
                        }
                    }
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
            
        case AGTV_STRING: {
            /* Quote the string with proper JSON escaping */
            const char *src = val->val.string.val;
            int src_len = val->val.string.len;

            /* Count how many characters need escaping */
            int escape_count = 0;
            for (int i = 0; i < src_len; i++) {
                unsigned char c = (unsigned char)src[i];
                if (c == '"' || c == '\\' || c < 32) {
                    escape_count++;
                }
            }

            /* Allocate: original + escapes + quotes + null */
            result = malloc(src_len + escape_count + 3);
            if (result) {
                char *dst = result;
                *dst++ = '"';
                for (int i = 0; i < src_len; i++) {
                    unsigned char c = (unsigned char)src[i];
                    if (c == '"') {
                        *dst++ = '\\';
                        *dst++ = '"';
                    } else if (c == '\\') {
                        *dst++ = '\\';
                        *dst++ = '\\';
                    } else if (c == '\n') {
                        *dst++ = '\\';
                        *dst++ = 'n';
                    } else if (c == '\r') {
                        *dst++ = '\\';
                        *dst++ = 'r';
                    } else if (c == '\t') {
                        *dst++ = '\\';
                        *dst++ = 't';
                    } else if (c < 32) {
                        /* Skip other control characters */
                        *dst++ = ' ';
                    } else {
                        *dst++ = c;
                    }
                }
                *dst++ = '"';
                *dst = '\0';
            }
            break;
        }
            
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
            /* OpenCypher format: {"id": 123, "labels": ["Person"], "properties": {"name": "Alice"}} */
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
                /* Use "labels" as array per OpenCypher spec */
                if (val->val.entity.label && strlen(val->val.entity.label) > 0) {
                    snprintf(result, base_size,
                        "{\"id\": %lld, \"labels\": [\"%s\"], \"properties\": {",
                        (long long)val->val.entity.id,
                        val->val.entity.label);
                } else {
                    snprintf(result, base_size,
                        "{\"id\": %lld, \"labels\": [], \"properties\": {",
                        (long long)val->val.entity.id);
                }

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

                strcat(result, "}}");
            }
            break;
        }
        
        case AGTV_EDGE: {
            /* OpenCypher format: {"id": 123, "type": "KNOWS", "startNode": 456, "endNode": 789, "properties": {...}} */
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
                /* Use "type" instead of "label", and "startNode"/"endNode" per OpenCypher */
                snprintf(result, base_size,
                    "{\"id\": %lld, \"type\": \"%s\", \"startNode\": %lld, \"endNode\": %lld, \"properties\": {",
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

                strcat(result, "}}");
            }
            break;
        }
        
        case AGTV_PATH: {
            /* Path as JSON array of alternating vertices and edges */
            int base_size = 50;

            /* Calculate size needed for all elements */
            for (int i = 0; i < val->val.array.num_elems; i++) {
                char *elem_str = agtype_value_to_string(&val->val.array.elems[i]);
                if (elem_str) {
                    base_size += strlen(elem_str) + 2; /* element + comma/space */
                    free(elem_str);
                }
            }

            result = malloc(base_size);
            if (result) {
                strcpy(result, "[");

                for (int i = 0; i < val->val.array.num_elems; i++) {
                    if (i > 0) strcat(result, ", ");

                    char *elem_str = agtype_value_to_string(&val->val.array.elems[i]);
                    if (elem_str) {
                        strcat(result, elem_str);
                        free(elem_str);
                    }
                }

                strcat(result, "]");
            }
            break;
        }
        
        default:
            result = strdup("undefined");
            break;
    }
    
    return result ? result : strdup("null");
}