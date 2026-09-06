#ifndef TC_SHA1_H
#define TC_SHA1_H
#include <stdint.h>
#include <stddef.h>
typedef struct { uint32_t h[5]; uint64_t bits; uint8_t buf[64]; size_t n; } tc_sha1;
void tc_sha1_init(tc_sha1 *c);
void tc_sha1_update(tc_sha1 *c, const void *data, size_t len);
void tc_sha1_final(tc_sha1 *c, uint8_t out[20]);
#endif
