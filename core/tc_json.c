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
        else if (*p == '}' || *p == ']') {
            depth--;
            /* Stop at the end of the object we were handed. Without this the
             * scan runs on into whatever follows in the buffer, so asking a
             * system log entry for "png" happily returns the NEXT entry's
             * image. Elements of an array are not NUL-terminated slices. */
            if (depth <= 0) return NULL;
        }
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

int tc_json_top_str(const char *doc, const char *key, char *out, size_t outn) {
    const char *v = tc_json_top(doc, key);
    size_t o = 0;
    if (!v || *v != '"' || outn == 0) { if (outn) out[0] = '\0'; return -1; }
    v++;
    while (*v && *v != '"' && o + 1 < outn) {
        if (*v == '\\' && v[1]) {
            v++;
            switch (*v) {
                case 'n': out[o++] = '\n'; break;
                case 't': out[o++] = '\t'; break;
                case 'r': out[o++] = '\r'; break;
                case 'u': /* not needed for nicks; emit '?' and skip */
                    out[o++] = '?';
                    if (v[1] && v[2] && v[3] && v[4]) v += 4;
                    break;
                default: out[o++] = *v; break;
            }
            v++;
            continue;
        }
        out[o++] = *v++;
    }
    out[o] = '\0';
    return (int)o;
}

const char *tc_json_top_raw(const char *doc, const char *key, size_t *len) {
    const char *v = tc_json_top(doc, key);
    const char *p;
    if (!v || *v != '"') return NULL;
    p = v + 1;
    while (*p) {
        if (*p == '\\' && p[1]) { p += 2; continue; }
        if (*p == '"') break;
        p++;
    }
    *len = (size_t)(p - (v + 1));
    return v + 1;
}

const char *tc_json_array_first(const char *arr) {
    const char *p = arr;
    if (!p || *p != '[') return NULL;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return (*p == ']' || *p == '\0') ? NULL : p;
}

const char *tc_json_array_next(const char *elem) {
    int depth = 0;
    const char *p = elem;
    if (!p) return NULL;
    for (;;) {
        if (!*p) return NULL;
        if (*p == '"') { p = skip_string(p); continue; }
        if (*p == '[' || *p == '{') { depth++; p++; continue; }
        if (*p == ']' || *p == '}') {
            if (depth == 0) return NULL;      /* end of the containing array */
            depth--; p++; continue;
        }
        if (*p == ',' && depth == 0) {
            p++;
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
            return *p ? p : NULL;
        }
        p++;
    }
}
