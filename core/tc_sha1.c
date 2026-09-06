/* SHA-1, only ever used for the RFC 6455 handshake accept value. */
#include "tc_sha1.h"
#include <string.h>

#define ROL(v,b) (((v) << (b)) | ((v) >> (32 - (b))))

static void tc_sha1_block(tc_sha1 *c, const uint8_t *p) {
    uint32_t w[80], a, b, d, e, f, k, t;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) |
               ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    for (i = 16; i < 80; i++)
        w[i] = ROL(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);

    a = c->h[0]; b = c->h[1]; d = c->h[2]; e = c->h[3]; f = c->h[4];
    for (i = 0; i < 80; i++) {
        if      (i < 20) { k = 0x5A827999; t = (b & d) | (~b & e); }
        else if (i < 40) { k = 0x6ED9EBA1; t = b ^ d ^ e; }
        else if (i < 60) { k = 0x8F1BBCDC; t = (b & d) | (b & e) | (d & e); }
        else             { k = 0xCA62C1D6; t = b ^ d ^ e; }
        t = ROL(a, 5) + t + f + k + w[i];
        f = e; e = d; d = ROL(b, 30); b = a; a = t;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += d; c->h[3] += e; c->h[4] += f;
}

void tc_sha1_init(tc_sha1 *c) {
    c->h[0] = 0x67452301; c->h[1] = 0xEFCDAB89; c->h[2] = 0x98BADCFE;
    c->h[3] = 0x10325476; c->h[4] = 0xC3D2E1F0;
    c->bits = 0; c->n = 0;
}

void tc_sha1_update(tc_sha1 *c, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    c->bits += (uint64_t)len * 8;
    while (len--) {
        c->buf[c->n++] = *p++;
        if (c->n == 64) { tc_sha1_block(c, c->buf); c->n = 0; }
    }
}

void tc_sha1_final(tc_sha1 *c, uint8_t out[20]) {
    uint64_t bits = c->bits;
    int i;
    tc_sha1_update(c, "\x80", 1);
    while (c->n != 56) tc_sha1_update(c, "\0", 1);
    for (i = 7; i >= 0; i--) { uint8_t b = (uint8_t)(bits >> (i * 8)); tc_sha1_update(c, &b, 1); }
    for (i = 0; i < 5; i++) {
        out[i*4]   = (uint8_t)(c->h[i] >> 24); out[i*4+1] = (uint8_t)(c->h[i] >> 16);
        out[i*4+2] = (uint8_t)(c->h[i] >> 8);  out[i*4+3] = (uint8_t)(c->h[i]);
    }
}
