#include "tc_imgdec.h"
#include "tc_base64.h"
#include "../platform/platform.h"
#include <string.h>

/* Console-friendly stb_image: PNG only, no stdio, allocations routed through
 * the platform so a tight target can account for them. */
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_MALLOC(sz)        tc_alloc(sz)
#define STBI_REALLOC(p, sz)    tc_realloc(p, sz)
#define STBI_FREE(p)           tc_free(p)
#define STB_IMAGE_IMPLEMENTATION
/* Third-party: not our warnings to fix, and we do not want them drowning ours. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "../third_party/stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/* Refuse anything absurd rather than trying and dying on a 3DS. */
#define TC_MAX_PIXELS (1600 * 1200)

int tc_img_thumb(const char *b64url, size_t len, uint8_t *out, int *ow, int *oh) {
    static const char PFX[] = "data:image/png;base64,";
    const size_t pfxlen = sizeof(PFX) - 1;
    uint8_t *png = NULL, *img = NULL;
    long pnglen;
    int w = 0, h = 0, comp = 0, tw, th, x, y, rc = -1;

    if (!b64url || len <= pfxlen || strncmp(b64url, PFX, pfxlen) != 0) return -1;
    b64url += pfxlen;
    len -= pfxlen;

    png = (uint8_t *)tc_alloc(len / 4 * 3 + 4);
    if (!png) return -1;
    pnglen = tc_b64_decode(b64url, len, png);
    if (pnglen <= 0) goto done;

    /* Check the dimensions before committing to a full decode. */
    if (!stbi_info_from_memory(png, (int)pnglen, &w, &h, &comp)) goto done;
    if (w <= 0 || h <= 0 || (long)w * h > TC_MAX_PIXELS) goto done;

    img = stbi_load_from_memory(png, (int)pnglen, &w, &h, &comp, 4);
    if (!img) goto done;

    /* Fit inside the thumbnail box, preserving aspect. */
    tw = TC_THUMB_W; th = (int)((long)h * TC_THUMB_W / w);
    if (th > TC_THUMB_H) { th = TC_THUMB_H; tw = (int)((long)w * TC_THUMB_H / h); }
    if (tw < 1) tw = 1;
    if (th < 1) th = 1;

    /* Box filter: average the source block each output pixel covers, so thin
     * strokes survive the shrink instead of being sampled away. */
    for (y = 0; y < th; y++) {
        int sy0 = (int)((long)y * h / th), sy1 = (int)((long)(y + 1) * h / th);
        if (sy1 <= sy0) sy1 = sy0 + 1;
        for (x = 0; x < tw; x++) {
            int sx0 = (int)((long)x * w / tw), sx1 = (int)((long)(x + 1) * w / tw);
            unsigned acc[4] = { 0, 0, 0, 0 };
            unsigned n = 0;
            int sx, sy;
            if (sx1 <= sx0) sx1 = sx0 + 1;
            for (sy = sy0; sy < sy1 && sy < h; sy++) {
                const uint8_t *row = img + ((size_t)sy * w) * 4;
                for (sx = sx0; sx < sx1 && sx < w; sx++) {
                    const uint8_t *p = row + (size_t)sx * 4;
                    acc[0] += p[0]; acc[1] += p[1]; acc[2] += p[2]; acc[3] += p[3];
                    n++;
                }
            }
            {
                uint8_t *d = out + ((size_t)y * TC_THUMB_W + x) * 4;
                if (!n) n = 1;
                d[0] = (uint8_t)(acc[0] / n); d[1] = (uint8_t)(acc[1] / n);
                d[2] = (uint8_t)(acc[2] / n); d[3] = (uint8_t)(acc[3] / n);
            }
        }
    }
    *ow = tw; *oh = th;
    rc = 0;
done:
    if (img) stbi_image_free(img);
    tc_free(png);
    return rc;
}
