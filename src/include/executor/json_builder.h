/*
 * json_builder.h
 *    Dynamic JSON string builder for graph algorithm results
 *
 * Provides a simple, safe way to build JSON arrays and objects
 * with automatic memory management and capacity growth.
 */

#ifndef JSON_BUILDER_H
#define JSON_BUILDER_H

#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char *data;
    size_t len;
    size_t capacity;
    int item_count;  /* Track items for comma handling */
} json_builder;

/* Initialize a JSON builder with initial capacity */
void jbuf_init(json_builder *jb, size_t initial_capacity);

/* Free all memory (use jbuf_take to get result without freeing) */
void jbuf_free(json_builder *jb);

/* Start/end JSON array */
void jbuf_start_array(json_builder *jb);
void jbuf_end_array(json_builder *jb);

/* Start/end JSON object */
void jbuf_start_object(json_builder *jb);
void jbuf_end_object(json_builder *jb);

/* Add a raw string (no quoting) */
void jbuf_append(json_builder *jb, const char *str);

/* Add formatted content */
void jbuf_appendf(json_builder *jb, const char *fmt, ...);

/* Add array/object item with automatic comma handling */
void jbuf_add_item(json_builder *jb, const char *fmt, ...);

/* Take ownership of the built string (caller must free) */
char *jbuf_take(json_builder *jb);

/* Check if builder is valid (no allocation failures) */
bool jbuf_ok(json_builder *jb);

#endif /* JSON_BUILDER_H */
