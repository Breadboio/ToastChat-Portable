/* The single translation unit that owns stb_image_write. No stdio: consoles
 * have no filesystem we want here, and it keeps the .nro smaller. */
#include "tc_png.h"
#include "../platform/platform.h"
#include <string.h>

#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"

typedef struct { uint8_t *buf; size_t len, cap; } membuf;

static void sink(void *ctx, void *data, int size) {
    membuf *m = (membuf *)ctx;
    if (m->len + (size_t)size > m->cap) {
        size_t cap = m->cap ? m->cap : 16384;
        uint8_t *p;
        while (cap < m->len + (size_t)size) cap *= 2;
        p = (uint8_t *)tc_realloc(m->buf, cap);
        if (!p) { tc_free(m->buf); m->buf = NULL; m->cap = m->len = 0; return; }
        m->buf = p; m->cap = cap;
    }
    if (!m->buf) return;
    memcpy(m->buf + m->len, data, (size_t)size);
    m->len += (size_t)size;
}

uint8_t *tc_png_encode(const uint8_t *rgba, int w, int h, size_t *out_len) {
    membuf m; memset(&m, 0, sizeof(m));
    if (!stbi_write_png_to_func(sink, &m, w, h, 4, rgba, w * 4)) {
        tc_free(m.buf); return NULL;
    }
    *out_len = m.len;
    return m.buf;
}
