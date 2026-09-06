#ifndef TC_PNG_H
#define TC_PNG_H
#include <stdint.h>
#include <stddef.h>
/* Encodes RGBA to PNG in memory. Caller tc_free()s the result. NULL on failure. */
uint8_t *tc_png_encode(const uint8_t *rgba, int w, int h, size_t *out_len);
#endif
