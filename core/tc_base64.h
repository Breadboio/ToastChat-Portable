#ifndef TC_BASE64_H
#define TC_BASE64_H
#include <stddef.h>
#include <stdint.h>
/* out must hold ((n+2)/3)*4 + 1 bytes. Returns written length (excl NUL). */
size_t tc_b64_encode(const uint8_t *in, size_t n, char *out);
/* Decodes in place-able; out must hold (n/4)*3 bytes. Returns bytes, or -1. */
long   tc_b64_decode(const char *in, size_t n, uint8_t *out);
#endif
