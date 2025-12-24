#ifndef AGTYPE_H
#define AGTYPE_H

#include <stdint.h>
#include <stdbool.h>
#include "graphqlite_sqlite.h"

/* AGType value types - simplified from Apache AGE */
typedef enum agtype_value_type
{
    /* Scalar types */
    AGTV_NULL = 0x0,
    AGTV_STRING,
    AGTV_NUMERIC,
    AGTV_INTEGER,
    AGTV_FLOAT,
    AGTV_BOOL,
    AGTV_VERTEX,
    AGTV_EDGE,
    AGTV_PATH,
    /* Composite types */
    AGTV_ARRAY = 0x10,
    AGTV_OBJECT
} agtype_value_type;

/* AGType header constants */
#define AGT_HEADER_INTEGER 0x00000000
#define AGT_HEADER_FLOAT   0x00000001
#define AGT_HEADER_VERTEX  0x00000002
#define AGT_HEADER_EDGE    0x00000003
#define AGT_HEADER_PATH    0x00000004

/* Forward declarations */
typedef struct agtype_value agtype_value;
typedef struct agtype_pair agtype_pair;

/* Key/value pair for objects */
struct agtype_pair
{
    agtype_value *key;   /* Must be AGTV_STRING */
    agtype_value *value; /* Any type */
};

/* In-memory representation of agtype value */
struct agtype_value
{
    agtype_value_type type;
    
    union
    {
        int64_t int_value;
        double float_value;
        bool boolean;
        
        struct
        {
            int len;
            char *val;
        } string;
        
        struct
        {
            int64_t id;        /* Graph ID */
            char *label;       /* Node/Edge label */
            int num_pairs;     /* Number of properties */
            agtype_pair *pairs; /* Property key/value pairs */
        } entity;  /* For VERTEX and EDGE */
        
        struct
        {
            int64_t id;        /* Edge ID */
            char *label;       /* Edge label */
            int64_t start_id;  /* Source vertex ID */
            int64_t end_id;    /* Target vertex ID */
            int num_pairs;     /* Number of properties */
            agtype_pair *pairs; /* Property key/value pairs */
        } edge;
        
        struct
        {
            int num_elems;
            agtype_value *elems;
        } array;
        
        struct
        {
            int num_pairs;
            agtype_pair *pairs;
        } object;
    } val;
};

/* Convenience macros */
#define IS_A_AGTYPE_SCALAR(agtype_val) \
    ((agtype_val)->type >= AGTV_NULL && (agtype_val)->type < AGTV_ARRAY)

/* Function declarations */
agtype_value* agtype_value_create_null(void);
agtype_value* agtype_value_create_string(const char* str);
agtype_value* agtype_value_create_integer(int64_t val);
agtype_value* agtype_value_create_float(double val);
agtype_value* agtype_value_create_bool(bool val);
agtype_value* agtype_value_create_vertex(int64_t id, const char* label);
agtype_value* agtype_value_create_edge(int64_t id, const char* label, int64_t start_id, int64_t end_id);
agtype_value* agtype_value_create_vertex_with_properties(sqlite3 *db, int64_t id, const char* label);
agtype_value* agtype_value_create_edge_with_properties(sqlite3 *db, int64_t id, const char* label, int64_t start_id, int64_t end_id);
agtype_value* agtype_value_create_path(agtype_value **elements, int num_elements);
agtype_value* agtype_build_path(agtype_value **path_elements, int num_elements);

void agtype_value_free(agtype_value *val);
char* agtype_value_to_string(agtype_value *val);

#endif /* AGTYPE_H */