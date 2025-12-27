/*
 * json_builder.c
 *    Dynamic JSON string builder implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "executor/json_builder.h"

#define JBUF_DEFAULT_CAPACITY 256

static bool jbuf_ensure(json_builder *jb, size_t additional)
{
    if (!jb->data) return false;

    size_t needed = jb->len + additional + 1;
    if (needed <= jb->capacity) return true;

    size_t new_cap = jb->capacity * 2;
    while (new_cap < needed) new_cap *= 2;

    char *new_data = realloc(jb->data, new_cap);
    if (!new_data) return false;

    jb->data = new_data;
    jb->capacity = new_cap;
    return true;
}

void jbuf_init(json_builder *jb, size_t initial_capacity)
{
    if (!jb) return;

    jb->capacity = initial_capacity > 64 ? initial_capacity : JBUF_DEFAULT_CAPACITY;
    jb->data = malloc(jb->capacity);
    jb->len = 0;
    jb->item_count = 0;

    if (jb->data) {
        jb->data[0] = '\0';
    }
}

void jbuf_free(json_builder *jb)
{
    if (!jb) return;

    free(jb->data);
    jb->data = NULL;
    jb->len = 0;
    jb->capacity = 0;
    jb->item_count = 0;
}

void jbuf_start_array(json_builder *jb)
{
    if (!jb || !jbuf_ensure(jb, 1)) return;

    jb->data[jb->len++] = '[';
    jb->data[jb->len] = '\0';
    jb->item_count = 0;
}

void jbuf_end_array(json_builder *jb)
{
    if (!jb || !jbuf_ensure(jb, 1)) return;

    jb->data[jb->len++] = ']';
    jb->data[jb->len] = '\0';
}

void jbuf_start_object(json_builder *jb)
{
    if (!jb || !jbuf_ensure(jb, 1)) return;

    jb->data[jb->len++] = '{';
    jb->data[jb->len] = '\0';
    jb->item_count = 0;
}

void jbuf_end_object(json_builder *jb)
{
    if (!jb || !jbuf_ensure(jb, 1)) return;

    jb->data[jb->len++] = '}';
    jb->data[jb->len] = '\0';
}

void jbuf_append(json_builder *jb, const char *str)
{
    if (!jb || !str) return;

    size_t slen = strlen(str);
    if (!jbuf_ensure(jb, slen)) return;

    memcpy(jb->data + jb->len, str, slen + 1);
    jb->len += slen;
}

void jbuf_appendf(json_builder *jb, const char *fmt, ...)
{
    if (!jb || !fmt) return;

    va_list args, args_copy;
    va_start(args, fmt);
    va_copy(args_copy, args);

    int needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (needed < 0 || !jbuf_ensure(jb, needed)) {
        va_end(args_copy);
        return;
    }

    vsnprintf(jb->data + jb->len, needed + 1, fmt, args_copy);
    jb->len += needed;
    va_end(args_copy);
}

void jbuf_add_item(json_builder *jb, const char *fmt, ...)
{
    if (!jb || !fmt) return;

    /* Add comma separator if not first item */
    if (jb->item_count > 0) {
        if (!jbuf_ensure(jb, 1)) return;
        jb->data[jb->len++] = ',';
        jb->data[jb->len] = '\0';
    }

    va_list args, args_copy;
    va_start(args, fmt);
    va_copy(args_copy, args);

    int needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (needed < 0 || !jbuf_ensure(jb, needed)) {
        va_end(args_copy);
        return;
    }

    vsnprintf(jb->data + jb->len, needed + 1, fmt, args_copy);
    jb->len += needed;
    jb->item_count++;
    va_end(args_copy);
}

char *jbuf_take(json_builder *jb)
{
    if (!jb) return NULL;

    char *data = jb->data;
    jb->data = NULL;
    jb->len = 0;
    jb->capacity = 0;
    jb->item_count = 0;
    return data;
}

bool jbuf_ok(json_builder *jb)
{
    return jb && jb->data != NULL;
}
