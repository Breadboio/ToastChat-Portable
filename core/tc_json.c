#include "tc_json.h"
#include <string.h>

/* Advance past a JSON string starting at the opening quote. */
static const char *skip_string(const char *p) {
    p++;                                   /* opening quote */
    while (*p) {
        if (*p == '\\' && p[1]) { p += 2; continue; }
        if (*p == '"') return p + 1;
        p++;
    }
    return p;
}

const char *tc_json_top(const char *doc, const char *key) {
    int depth = 0;
    size_t klen = strlen(key);
    const char *p = doc;

    while (*p) {
        if (*p == '"') {
            const char *start = p;
            const char *end = skip_string(p);
            /* A key is a string at depth 1 followed by a colon. */
            if (depth == 1) {
                const char *q = end;
                while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
                if (*q == ':') {
                    size_t len = (size_t)(end - start) - 2;
                    if (len == klen && strncmp(start + 1, key, klen) == 0) {
                        q++;
                        while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
                        return q;
                    }
                    p = q + 1;             /* skip the colon, value handled below */
                    continue;
                }
            }
            p = end;
            continue;
        }
        if (*p == '{' || *p == '[') depth++;
        else if (*p == '}' || *p == ']') { depth--; if (depth < 0) return NULL; }
        p++;
    }
    return NULL;
}

int tc_json_array_len(const char *arr) {
    int depth = 0, count = 0, seen_value = 0;
    const char *p = arr;
    if (!p || *p != '[') return -1;

    for (;;) {
        if (!*p) return -1;
        if (*p == '"') {
            if (depth == 1) seen_value = 1;
            p = skip_string(p);
            continue;
        }
        if (*p == '[' || *p == '{') {
            depth++;
            if (depth == 2) seen_value = 1;
            p++;
            continue;
        }
        if (*p == ']' || *p == '}') {
            depth--;
            if (depth == 0) return seen_value ? count + 1 : 0;
            p++;
            continue;
        }
        if (*p == ',' && depth == 1) { count++; p++; continue; }
        if (depth == 1 && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') seen_value = 1;
        p++;
    }
}

int tc_json_top_int(const char *doc, const char *key, int dflt) {
    const char *v = tc_json_top(doc, key);
    int sign = 1, n = 0, any = 0;
    if (!v) return dflt;
    if (*v == '-') { sign = -1; v++; }
    while (*v >= '0' && *v <= '9') { n = n * 10 + (*v - '0'); v++; any = 1; }
    return any ? sign * n : dflt;
}
