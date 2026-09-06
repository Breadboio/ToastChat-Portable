/* Renders the stroke rasteriser to a PNG so a human can look at it before any
 * of this reaches a console. Exercises the cases most likely to be wrong:
 * single-point dots, thin vs thick, diagonals, curves, and off-canvas clipping. */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"
#include "../core/tc_draw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define W 640
#define H 480

/* DS favourite-colour palette, as the server sends it. */
static const uint32_t PAL[16] = {
    0x8c8f94,0x7a4c2a,0xc8352b,0xe77aa6,0xe0762a,0xe0b52a,0xa8cc37,0x4faa3c,
    0x2f7d34,0x2fa89a,0x3fbbd6,0x2f6fd0,0x2a3f9e,0x7a49c4,0xb04ec4,0xd6459b
};

int main(void) {
    tc_canvas c;
    uint8_t *buf = (uint8_t *)calloc((size_t)W * H * 4, 1);
    int i;

    if (tc_canvas_init(&c, 8000, 128) != 0) { puts("canvas init failed"); return 1; }

    /* 1. dots of every pen size, top-left */
    for (i = 0; i < 6; i++) {
        static const float R[6] = { 0.5f, 1.0f, 2.0f, 3.0f, 5.0f, 8.0f };
        tc_canvas_begin(&c, 40 + i * 45, 45, PAL[8], R[i]);
        tc_canvas_end(&c);
    }
    /* 2. straight horizontal/vertical/diagonal at each width */
    for (i = 0; i < 6; i++) {
        static const float R[6] = { 0.5f, 1.0f, 2.0f, 3.0f, 5.0f, 8.0f };
        int y = 100 + i * 22;
        tc_canvas_begin(&c, 30, y, PAL[2], R[i]);
        tc_canvas_to(&c, 250, y);
        tc_canvas_end(&c);
        tc_canvas_begin(&c, 280, 95, PAL[11], R[i]);
        tc_canvas_to(&c, 280 + i * 30, 95 + 120);
        tc_canvas_end(&c);
    }
    /* 3. a smooth curve, the normal case for a finger */
    tc_canvas_begin(&c, 40, 300, PAL[9], 3.0f);
    for (i = 0; i <= 220; i++)
        tc_canvas_to(&c, 40 + i * 2, (int)(340.0 + 60.0 * sin(i * 0.055)));
    tc_canvas_end(&c);

    /* 4. clipping: a stroke that runs well off every edge */
    tc_canvas_begin(&c, -60, 430, PAL[14], 6.0f);
    tc_canvas_to(&c, W + 60, 455);
    tc_canvas_end(&c);
    tc_canvas_begin(&c, 500, -40, PAL[5], 4.0f);
    tc_canvas_to(&c, 560, H + 40);
    tc_canvas_end(&c);

    /* 5. undo must remove exactly one stroke */
    tc_canvas_begin(&c, 300, 300, PAL[3], 9.0f);
    tc_canvas_to(&c, 600, 300);
    tc_canvas_end(&c);
    tc_canvas_undo(&c);

    printf("strokes=%u points=%u  (undo left the last one out)\n", c.nstrokes, c.npts);
    tc_canvas_render(&c, buf, W, H);

    /* Composite onto white so the PNG is viewable; the wire format keeps alpha. */
    {
        uint8_t *flat = (uint8_t *)malloc((size_t)W * H * 3);
        long k;
        for (k = 0; k < (long)W * H; k++) {
            float a = buf[k*4+3] / 255.0f;
            flat[k*3+0] = (uint8_t)(buf[k*4+0] * a + 255 * (1 - a));
            flat[k*3+1] = (uint8_t)(buf[k*4+1] * a + 255 * (1 - a));
            flat[k*3+2] = (uint8_t)(buf[k*4+2] * a + 255 * (1 - a));
        }
        stbi_write_png("/src/build/draw_test.png", W, H, 3, flat, W * 3);
        free(flat);
    }
    stbi_write_png("/src/build/draw_test_rgba.png", W, H, 4, buf, W * 4);
    puts("wrote build/draw_test.png");
    tc_canvas_free(&c);
    free(buf);
    return 0;
}
