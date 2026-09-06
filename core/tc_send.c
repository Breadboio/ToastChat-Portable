#include "tc_send.h"
#include "tc_png.h"
#include "tc_base64.h"
#include "../platform/platform.h"
#include <string.h>
#include <stdio.h>

#define TC_MAX_SEND (400 * 1024)

uint8_t *tc_render_png(const tc_canvas *c, int w, int h, int shrink,
                       size_t *out_len, int *out_w, int *out_h) {
    uint8_t *full = NULL, *small = NULL, *png = NULL;
    int sw, sh;

    if (shrink < 1) shrink = 1;
    sw = w / shrink; sh = h / shrink;
    if (sw < 1 || sh < 1) return NULL;

    full = (uint8_t *)tc_alloc((size_t)w * h * 4);
    if (!full) goto enc_done;
    memset(full, 0, (size_t)w * h * 4);
    tc_canvas_render(c, full, w, h);

    if (shrink == 1) { small = full; }
    else {
        int x, y, dx, dy;
        small = (uint8_t *)tc_alloc((size_t)sw * sh * 4);
        if (!small) goto enc_done;
        for (y = 0; y < sh; y++) {
            for (x = 0; x < sw; x++) {
                unsigned acc[4] = { 0, 0, 0, 0 };
                int n = 0;
                for (dy = 0; dy < shrink; dy++) {
                    for (dx = 0; dx < shrink; dx++) {
                        const uint8_t *s = full + (((size_t)(y * shrink + dy) * w) + (x * shrink + dx)) * 4;
                        acc[0] += s[0]; acc[1] += s[1]; acc[2] += s[2]; acc[3] += s[3];
                        n++;
                    }
                }
                {
                    uint8_t *d = small + ((size_t)y * sw + x) * 4;
                    d[0] = (uint8_t)(acc[0] / n); d[1] = (uint8_t)(acc[1] / n);
                    d[2] = (uint8_t)(acc[2] / n); d[3] = (uint8_t)(acc[3] / n);
                }
            }
        }
    }

    png = tc_png_encode(small, sw, sh, out_len);
    *out_w = sw; *out_h = sh;
enc_done:
    if (small != full) tc_free(small);
    tc_free(full);
    return png;
}

int tc_send_canvas(tc_ws *ws, const tc_canvas *c, int w, int h, int shrink) {
    uint8_t *png;
    char *b64 = NULL, *json = NULL;
    size_t plen = 0, blen, jcap;
    int sw = 0, sh = 0, rc = -1;

    png = tc_render_png(c, w, h, shrink, &plen, &sw, &sh);
    if (!png) return -1;

    b64 = (char *)tc_alloc(((plen + 2) / 3) * 4 + 4);
    if (!b64) goto done;
    blen = tc_b64_encode(png, plen, b64);

    jcap = blen + 160;
    if (jcap > TC_MAX_SEND) { rc = -10; goto done; }
    json = (char *)tc_alloc(jcap);
    if (!json) goto done;
    {
        int n = snprintf(json, jcap,
            "{\"t\":\"msg\",\"png\":\"data:image/png;base64,%s\",\"w\":%d,\"h\":%d}",
            b64, sw, sh);
        if (n <= 0 || (size_t)n >= jcap) goto done;
        rc = tc_ws_send_text(ws, json, (size_t)n);
    }
done:
    tc_free(png); tc_free(b64); tc_free(json);
    return rc;
}
