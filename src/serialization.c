/* GraphQLite Serialization Module */
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "serialization.h"

char* serialize_node_entity(sqlite3 *db, int64_t node_id) {
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

char* serialize_relationship_entity(sqlite3 *db, int64_t edge_id) {
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