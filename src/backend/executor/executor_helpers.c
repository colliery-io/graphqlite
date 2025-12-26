/*
 * Executor Helper Functions
 * Common utilities used across executor modules
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "executor/executor_internal.h"
#include "transform/transform_helpers.h"
#include "parser/cypher_ast.h"

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
            const char *val_start = p;
            char *unescaped = malloc(strlen(p) + 1);
            char *out = unescaped;
            while (*p && *p != '"') {
                if (*p == '\\' && *(p+1)) {
                    p++;
                    switch (*p) {
                        case 'n': *out++ = '\n'; break;
                        case 't': *out++ = '\t'; break;
                        case 'r': *out++ = '\r'; break;
                        case '\\': *out++ = '\\'; break;
                        case '"': *out++ = '"'; break;
                        default: *out++ = *p; break;
                    }
                    p++;
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
        } else {
            return -1;  /* Unknown value type */
        }
    }

    return 0;
}
