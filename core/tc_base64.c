#include "tc_base64.h"
static const char E[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t tc_b64_encode(const uint8_t *in, size_t n, char *out) {
    size_t i = 0, o = 0;
    while (i + 2 < n) {
        uint32_t v = ((uint32_t)in[i] << 16) | ((uint32_t)in[i+1] << 8) | in[i+2];
        out[o++] = E[(v >> 18) & 63]; out[o++] = E[(v >> 12) & 63];
        out[o++] = E[(v >> 6) & 63];  out[o++] = E[v & 63];
        i += 3;
    }
    if (i + 1 == n) {
        uint32_t v = (uint32_t)in[i] << 16;
        out[o++] = E[(v >> 18) & 63]; out[o++] = E[(v >> 12) & 63];
        out[o++] = '='; out[o++] = '=';
    } else if (i + 2 == n) {
        uint32_t v = ((uint32_t)in[i] << 16) | ((uint32_t)in[i+1] << 8);
        out[o++] = E[(v >> 18) & 63]; out[o++] = E[(v >> 12) & 63];
        out[o++] = E[(v >> 6) & 63];  out[o++] = '=';
    }
    out[o] = '\0';
    return o;
}

static int dv(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

long tc_b64_decode(const char *in, size_t n, uint8_t *out) {
    uint32_t acc = 0; int bits = 0; long o = 0; size_t i;
    for (i = 0; i < n; i++) {
        int v;
        if (in[i] == '=' ) break;
        if (in[i] == '\n' || in[i] == '\r' || in[i] == ' ') continue;
        v = dv(in[i]);
        if (v < 0) return -1;
        acc = (acc << 6) | (uint32_t)v; bits += 6;
        if (bits >= 8) { bits -= 8; out[o++] = (uint8_t)(acc >> bits); }
    }
    return o;
}
