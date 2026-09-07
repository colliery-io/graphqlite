/*
 * Executor Helper Functions
 * Common utilities used across executor modules
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "executor/executor_internal.h"
#include "executor/json_builder.h"
#include "transform/transform_helpers.h"
#include "parser/cypher_ast.h"

/* Encode one Unicode code point as UTF-8. Writes at most 4 bytes. */
static int utf8_encode(unsigned int cp, char *out)
{
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

/* Read exactly four hex digits. Returns -1 (leaving *out untouched) if any of
 * them is not a hex digit, which includes running into the terminating NUL. */
static int hex4(const char *s, unsigned int *out)
{
    unsigned int v = 0;
    for (int i = 0; i < 4; i++) {
        char c = s[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned int)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned int)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned int)(c - 'A' + 10);
        else return -1;
    }
    *out = v;
    return 0;
}

/*
 * Decode one JSON escape sequence. `*pp` points at the character *after* the
 * backslash; on return it points past the sequence. Writes the decoded bytes
 * to `out` (at most 4) and returns how many were written.
 *
 * `\uXXXX` is decoded to UTF-8, including surrogate pairs, because
 * json.dumps() escapes every non-ASCII character that way by default: without
 * this, "café" arrived as the literal text "cafu00e9" and any control
 * character in a parameter was mangled. A lone surrogate becomes U+FFFD and an
 * unknown escape is copied through as its own character.
 */
int gql_json_decode_escape(const char **pp, char *out)
{
    const char *p = *pp;
    switch (*p) {
        case 'n':  *pp = p + 1; out[0] = '\n'; return 1;
        case 't':  *pp = p + 1; out[0] = '\t'; return 1;
        case 'r':  *pp = p + 1; out[0] = '\r'; return 1;
        case 'b':  *pp = p + 1; out[0] = '\b'; return 1;
        case 'f':  *pp = p + 1; out[0] = '\f'; return 1;
        case '/':  *pp = p + 1; out[0] = '/';  return 1;
        case '"':  *pp = p + 1; out[0] = '"';  return 1;
        case '\\': *pp = p + 1; out[0] = '\\'; return 1;
        case 'u': {
            unsigned int cp;
            if (hex4(p + 1, &cp) < 0) break;  /* malformed: copy literally */
            const char *after = p + 5;
            if (cp >= 0xD800 && cp <= 0xDBFF && after[0] == '\\' && after[1] == 'u') {
                unsigned int lo;
                if (hex4(after + 2, &lo) == 0 && lo >= 0xDC00 && lo <= 0xDFFF) {
                    cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
                    after += 6;
                }
            }
            if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0xFFFD;  /* unpaired surrogate */
            *pp = after;
            return utf8_encode(cp, out);
        }
        default: break;
    }
    *pp = p + 1;
    out[0] = *p;
    return 1;
}

/* Helper to lookup a parameter value from JSON */
int get_param_value(const char *params_json, const char *param_name,
                    property_type *out_type, property_value *out_value)
{
    if (!params_json || !param_name || !out_type || !out_value) {
        return -1;
    }

    /* Skip whitespace */
    const char *p = params_json;
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

    if (*p != '{') return -1;
    p++;

    while (*p) {
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        if (*p == '}') break;
        if (*p == ',') { p++; continue; }

        /* Parse key */
        if (*p != '"') return -1;
        p++;
        const char *key_start = p;
        while (*p && *p != '"') p++;
        if (!*p) return -1;
        size_t key_len = p - key_start;
        p++;

        /* Check if this is our parameter */
        bool is_match = (strlen(param_name) == key_len && strncmp(param_name, key_start, key_len) == 0);

        while (*p && *p != ':') p++;
        if (!*p) return -1;
        p++;
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

        if (is_match) {
            /* Parse the value */
            if (*p == '"') {
                /* String value — scan to find length, then allocate */
                p++;
                const char *str_start = p;
                size_t raw_len = 0;
                const char *scan = p;
                while (*scan && *scan != '"') {
                    if (*scan == '\\' && *(scan+1)) scan++;
                    scan++;
                    raw_len++;
                }
                char *buf = malloc(raw_len + 1);
                if (!buf) return -1;
                size_t i = 0;
                while (*p && *p != '"') {
                    if (*p == '\\' && *(p+1)) {
                        p++;
                        i += (size_t)gql_json_decode_escape(&p, buf + i);
                    } else {
                        buf[i++] = *p++;
                    }
                }
                buf[i] = '\0';
                if (*p) p++;  /* Skip closing quote */
                out_value->as_str = buf;
                out_value->as_str_len = i;
                *out_type = PROP_TYPE_TEXT;
                return 0;
            } else if (*p == 't') {
                *out_type = PROP_TYPE_BOOLEAN;
                out_value->as_bool = 1;
                return 0;
            } else if (*p == 'f') {
                *out_type = PROP_TYPE_BOOLEAN;
                out_value->as_bool = 0;
                return 0;
            } else if (*p == 'n') {
                return -2;  /* null - special return code */
            } else if (*p == '-' || (*p >= '0' && *p <= '9')) {
                const char *num_start = p;
                bool is_float = false;
                if (*p == '-') p++;
                while (*p >= '0' && *p <= '9') p++;
                if (*p == '.') { is_float = true; p++; while (*p >= '0' && *p <= '9') p++; }
                if (*p == 'e' || *p == 'E') { is_float = true; p++; if (*p == '+' || *p == '-') p++; while (*p >= '0' && *p <= '9') p++; }

                if (is_float) {
                    *out_type = PROP_TYPE_REAL;
                    out_value->as_real = strtod(num_start, NULL);
                } else {
                    *out_type = PROP_TYPE_INTEGER;
                    out_value->as_int = strtoll(num_start, NULL, 10);
                }
                return 0;
            } else if (*p == '[' || *p == '{') {
                /* Array or object value - store as JSON text */
                const char *json_start = p;
                int depth = 1;
                p++;
                while (*p && depth > 0) {
                    if (*p == '[' || *p == '{') depth++;
                    else if (*p == ']' || *p == '}') depth--;
                    else if (*p == '"') {
                        p++;
                        while (*p && *p != '"') {
                            if (*p == '\\' && *(p+1)) p++;
                            p++;
                        }
                    }
                    if (*p) p++;
                }
                size_t json_len = p - json_start;
                char *buf = malloc(json_len + 1);
                if (!buf) return -1;
                memcpy(buf, json_start, json_len);
                buf[json_len] = '\0';
                out_value->as_str = buf;
                out_value->as_str_len = json_len;
                *out_type = PROP_TYPE_JSON;
                return 0;
            }
            return -1;
        } else {
            /* Skip this value */
            if (*p == '"') {
                p++;
                while (*p && *p != '"') { if (*p == '\\' && *(p+1)) p++; p++; }
                if (*p) p++;
            } else if (*p == '[' || *p == '{') {
                int depth = 1;
                p++;
                while (*p && depth > 0) {
                    if (*p == '[' || *p == '{') depth++;
                    else if (*p == ']' || *p == '}') depth--;
                    else if (*p == '"') {
                        p++;
                        while (*p && *p != '"') {
                            if (*p == '\\' && *(p+1)) p++;
                            p++;
                        }
                    }
                    if (*p) p++;
                }
            } else if (*p == 't' || *p == 'f') {
                while (*p && *p != ',' && *p != '}') p++;
            } else if (*p == 'n') {
                p += 4;
            } else {
                while (*p && *p != ',' && *p != '}') p++;
            }
        }
    }

    return -1;  /* Parameter not found */
}

/* Helper to bind parameters from JSON to a prepared statement */
int bind_params_from_json(sqlite3_stmt *stmt, const char *params_json)
{
    if (!params_json || !stmt) {
        return 0;  /* No params to bind */
    }

    /* Skip whitespace */
    const char *p = params_json;
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

    if (*p != '{') {
        return -1;  /* Not a JSON object */
    }
    p++;

    while (*p) {
        /* Skip whitespace */
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

        if (*p == '}') break;  /* End of object */
        if (*p == ',') { p++; continue; }  /* Next key-value pair */

        /* Parse key (must be quoted string) */
        if (*p != '"') return -1;
        p++;
        const char *key_start = p;
        while (*p && *p != '"') p++;
        if (!*p) return -1;
        size_t key_len = p - key_start;
        p++;  /* Skip closing quote */

        /* Build parameter name with : prefix */
        char param_name[256];
        if (key_len >= sizeof(param_name) - 1) return -1;
        param_name[0] = ':';
        memcpy(param_name + 1, key_start, key_len);
        param_name[key_len + 1] = '\0';

        /* Skip to colon */
        while (*p && *p != ':') p++;
        if (!*p) return -1;
        p++;  /* Skip colon */

        /* Skip whitespace before value */
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

        /* Find parameter index */
        int idx = sqlite3_bind_parameter_index(stmt, param_name);
        if (idx == 0) {
            /* Parameter not used in query, skip value */
            if (*p == '"') {
                p++;
                while (*p && *p != '"') { if (*p == '\\' && *(p+1)) p++; p++; }
                if (*p) p++;
            } else if (*p == '[' || *p == '{') {
                int depth = 1;
                p++;
                while (*p && depth > 0) {
                    if (*p == '[' || *p == '{') depth++;
                    else if (*p == ']' || *p == '}') depth--;
                    else if (*p == '"') {
                        p++;
                        while (*p && *p != '"') {
                            if (*p == '\\' && *(p+1)) p++;
                            p++;
                        }
                    }
                    if (*p) p++;
                }
            } else if (*p == 't' || *p == 'f') {
                while (*p && *p != ',' && *p != '}') p++;
            } else if (*p == 'n') {
                p += 4;  /* null */
            } else {
                while (*p && *p != ',' && *p != '}') p++;
            }
            continue;
        }

        /* Parse and bind value */
        if (*p == '"') {
            /* String value */
            p++;
            char *unescaped = malloc(strlen(p) + 1);
            if (!unescaped) return -1;
            char *out = unescaped;
            while (*p && *p != '"') {
                if (*p == '\\' && *(p+1)) {
                    p++;
                    out += gql_json_decode_escape(&p, out);
                } else {
                    *out++ = *p++;
                }
            }
            *out = '\0';
            if (*p) p++;  /* Skip closing quote */
            sqlite3_bind_text(stmt, idx, unescaped, -1, SQLITE_TRANSIENT);
            free(unescaped);
        } else if (*p == 't') {
            /* true */
            sqlite3_bind_int(stmt, idx, 1);
            p += 4;
        } else if (*p == 'f') {
            /* false */
            sqlite3_bind_int(stmt, idx, 0);
            p += 5;
        } else if (*p == 'n') {
            /* null */
            sqlite3_bind_null(stmt, idx);
            p += 4;
        } else if (*p == '-' || (*p >= '0' && *p <= '9')) {
            /* Number */
            const char *num_start = p;
            bool is_float = false;
            if (*p == '-') p++;
            while (*p >= '0' && *p <= '9') p++;
            if (*p == '.') { is_float = true; p++; while (*p >= '0' && *p <= '9') p++; }
            if (*p == 'e' || *p == 'E') { is_float = true; p++; if (*p == '+' || *p == '-') p++; while (*p >= '0' && *p <= '9') p++; }

            if (is_float) {
                double val = strtod(num_start, NULL);
                sqlite3_bind_double(stmt, idx, val);
            } else {
                long long val = strtoll(num_start, NULL, 10);
                sqlite3_bind_int64(stmt, idx, val);
            }
        } else if (*p == '[' || *p == '{') {
            /* Array or object value - bind as JSON text */
            const char *json_start = p;
            int depth = 1;
            p++;
            while (*p && depth > 0) {
                if (*p == '[' || *p == '{') depth++;
                else if (*p == ']' || *p == '}') depth--;
                else if (*p == '"') {
                    p++;
                    while (*p && *p != '"') {
                        if (*p == '\\' && *(p+1)) p++;
                        p++;
                    }
                }
                if (*p) p++;
            }
            size_t json_len = p - json_start;
            char *json_val = malloc(json_len + 1);
            if (!json_val) return -1;
            memcpy(json_val, json_start, json_len);
            json_val[json_len] = '\0';
            sqlite3_bind_text(stmt, idx, json_val, -1, SQLITE_TRANSIENT);
            free(json_val);
        } else {
            return -1;  /* Unknown value type */
        }
    }

    return 0;
}

/* Append a JSON-escaped string value (with quotes) to a json_builder */
static void jbuf_add_json_string(json_builder *jb, const char *str)
{
    jbuf_append(jb, "\"");
    if (str) {
        for (const char *p = str; *p; p++) {
            switch (*p) {
                case '"':  jbuf_append(jb, "\\\""); break;
                case '\\': jbuf_append(jb, "\\\\"); break;
                case '\n': jbuf_append(jb, "\\n"); break;
                case '\r': jbuf_append(jb, "\\r"); break;
                case '\t': jbuf_append(jb, "\\t"); break;
                default:   jbuf_appendf(jb, "%c", *p); break;
            }
        }
    }
    jbuf_append(jb, "\"");
}

/* Recursively serialize an AST expression node to JSON in a json_builder */
static void serialize_ast_node(json_builder *jb, ast_node *expr)
{
    if (!expr) {
        jbuf_append(jb, "null");
        return;
    }

    switch (expr->type) {
        case AST_NODE_MAP: {
            cypher_map *map = (cypher_map*)expr;
            jbuf_append(jb, "{");
            if (map->pairs) {
                for (int i = 0; i < map->pairs->count; i++) {
                    if (i > 0) jbuf_append(jb, ",");
                    cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[i];
                    jbuf_add_json_string(jb, pair->key);
                    jbuf_append(jb, ":");
                    serialize_ast_node(jb, pair->value);
                }
            }
            jbuf_append(jb, "}");
            break;
        }
        case AST_NODE_LIST: {
            cypher_list *list = (cypher_list*)expr;
            jbuf_append(jb, "[");
            if (list->items) {
                for (int i = 0; i < list->items->count; i++) {
                    if (i > 0) jbuf_append(jb, ",");
                    serialize_ast_node(jb, list->items->items[i]);
                }
            }
            jbuf_append(jb, "]");
            break;
        }
        case AST_NODE_LITERAL: {
            cypher_literal *lit = (cypher_literal*)expr;
            switch (lit->literal_type) {
                case LITERAL_STRING:
                    jbuf_add_json_string(jb, lit->value.string);
                    break;
                case LITERAL_INTEGER:
                    jbuf_appendf(jb, "%lld", (long long)lit->value.integer);
                    break;
                case LITERAL_DECIMAL:
                    jbuf_appendf(jb, "%g", lit->value.decimal);
                    break;
                case LITERAL_BOOLEAN:
                    jbuf_append(jb, lit->value.boolean ? "true" : "false");
                    break;
                case LITERAL_NULL:
                    jbuf_append(jb, "null");
                    break;
            }
            break;
        }
        default:
            /* Unsupported expression type in JSON context — emit null */
            jbuf_append(jb, "null");
            break;
    }
}

/* Serialize an AST map or list node to a JSON string.
 * Returns a malloc'd string that the caller must free, or NULL on error. */
char* serialize_ast_to_json(ast_node *expr)
{
    if (!expr) return NULL;
    if (expr->type != AST_NODE_MAP && expr->type != AST_NODE_LIST) return NULL;

    json_builder jb;
    jbuf_init(&jb, 256);
    if (!jbuf_ok(&jb)) return NULL;

    serialize_ast_node(&jb, expr);

    return jbuf_take(&jb);
}
