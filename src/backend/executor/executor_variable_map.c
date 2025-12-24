/*
 * Variable Map Implementation
 * Maps Cypher variables to entity IDs (nodes and edges)
 */

#include <stdlib.h>
#include <string.h>

#include "executor/executor_internal.h"

/* Create variable map */
variable_map* create_variable_map(void)
{
    variable_map *map = calloc(1, sizeof(variable_map));
    if (!map) return NULL;

    map->capacity = 16;
    map->mappings = calloc(map->capacity, sizeof(variable_mapping));
    if (!map->mappings) {
        free(map);
        return NULL;
    }

    return map;
}

/* Free variable map */
void free_variable_map(variable_map *map)
{
    if (!map) return;

    for (int i = 0; i < map->count; i++) {
        free(map->mappings[i].variable);
    }
    free(map->mappings);
    free(map);
}

/* Get node ID for variable, return -1 if not found or not a node */
int get_variable_node_id(variable_map *map, const char *variable)
{
    if (!map || !variable) return -1;

    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->mappings[i].variable, variable) == 0) {
            if (map->mappings[i].type == VAR_MAP_TYPE_NODE) {
                return map->mappings[i].entity_id;
            }
            return -1; /* Variable exists but is not a node */
        }
    }
    return -1;
}

/* Get edge ID for variable, return -1 if not found or not an edge */
int get_variable_edge_id(variable_map *map, const char *variable)
{
    if (!map || !variable) return -1;

    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->mappings[i].variable, variable) == 0) {
            if (map->mappings[i].type == VAR_MAP_TYPE_EDGE) {
                return map->mappings[i].entity_id;
            }
            return -1; /* Variable exists but is not an edge */
        }
    }
    return -1;
}

/* Check if variable is an edge variable */
bool is_variable_edge(variable_map *map, const char *variable)
{
    if (!map || !variable) return false;

    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->mappings[i].variable, variable) == 0) {
            return map->mappings[i].type == VAR_MAP_TYPE_EDGE;
        }
    }
    return false;
}

/* Set variable to node ID mapping */
int set_variable_node_id(variable_map *map, const char *variable, int node_id)
{
    if (!map || !variable) return -1;

    /* Check if variable already exists */
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->mappings[i].variable, variable) == 0) {
            map->mappings[i].entity_id = node_id;
            map->mappings[i].type = VAR_MAP_TYPE_NODE;
            return 0;
        }
    }

    /* Add new mapping */
    if (map->count >= map->capacity) {
        /* Expand capacity */
        map->capacity *= 2;
        map->mappings = realloc(map->mappings, map->capacity * sizeof(variable_mapping));
        if (!map->mappings) return -1;
    }

    map->mappings[map->count].variable = strdup(variable);
    map->mappings[map->count].entity_id = node_id;
    map->mappings[map->count].type = VAR_MAP_TYPE_NODE;
    map->count++;

    return 0;
}

/* Set variable to edge ID mapping */
int set_variable_edge_id(variable_map *map, const char *variable, int edge_id)
{
    if (!map || !variable) return -1;

    /* Check if variable already exists */
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->mappings[i].variable, variable) == 0) {
            map->mappings[i].entity_id = edge_id;
            map->mappings[i].type = VAR_MAP_TYPE_EDGE;
            return 0;
        }
    }

    /* Add new mapping */
    if (map->count >= map->capacity) {
        /* Expand capacity */
        map->capacity *= 2;
        map->mappings = realloc(map->mappings, map->capacity * sizeof(variable_mapping));
        if (!map->mappings) return -1;
    }

    map->mappings[map->count].variable = strdup(variable);
    map->mappings[map->count].entity_id = edge_id;
    map->mappings[map->count].type = VAR_MAP_TYPE_EDGE;
    map->count++;

    return 0;
}
