#ifndef TC_JSON_H
#define TC_JSON_H
#include <stddef.h>

/* A depth-aware scanner for the handful of fields the client needs.
 *
 * Deliberately NOT jsmn, even though it is vendored: jsmn tokenises the whole
 * document, and a `joined` message carries up to 40 base64 PNGs - several
 * megabytes and tens of thousands of tokens, on machines with 16-64MB of RAM.
 * This walks the text once and allocates nothing. jsmn is still the right tool
 * for the receive path, where the entries actually have to be pulled apart. */

/* Value of a key in the TOP-LEVEL object only, so nested keys of the same name
 * (a `nick` inside a log entry, say) cannot shadow it. NULL if absent. */
const char *tc_json_top(const char *doc, const char *key);

/* Elements in the array `arr` points at (which must start at '['). -1 if not. */
int tc_json_array_len(const char *arr);

/* Convenience: top-level integer, or `dflt`. */
int tc_json_top_int(const char *doc, const char *key, int dflt);

/* Copy a top-level string value out, unescaping \" and \\. Always NUL
 * terminates. Returns the length written, or -1 if the key is missing. */
int tc_json_top_str(const char *doc, const char *key, char *out, size_t outn);

/* Raw pointer into a top-level string value, WITHOUT unescaping or copying -
 * for the multi-megabyte base64 payloads, which contain no escapes. */
const char *tc_json_top_raw(const char *doc, const char *key, size_t *len);

/* Iterate array elements. `first` takes the '[' and returns the first element
 * (NULL if empty); `next` takes an element and returns the one after it. Both
 * return a pointer suitable for passing straight back into tc_json_top(). */
const char *tc_json_array_first(const char *arr);
const char *tc_json_array_next(const char *elem);
#endif
