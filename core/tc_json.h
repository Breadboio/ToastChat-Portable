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
#endif
