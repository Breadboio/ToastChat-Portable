/* Round-trips the Wii UI through YUY2 and back, so the colour conversion can
 * be looked at instead of discovered on a television. */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"
#include "../core/tc_yuv.h"
#include "../core/tc_ui.h"
#include "../core/tc_draw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define W 640
#define H 480

static int clamp(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

int main(void) {
    uint8_t *rgba = (uint8_t *)calloc((size_t)W * H * 4, 1);
    uint8_t *out  = (uint8_t *)malloc((size_t)W * H * 3);
    tc_canvas cv; tc_ui ui; tc_rect cr; int x, y, i;
    double err = 0; long n = 0;

    memset(&ui, 0, sizeof(ui));
    tc_canvas_init(&cv, 8000, 128);
    cr = tc_ui_canvas_rect(W, H);
    tc_canvas_begin(&cv, 60, 60, TC_PALETTE[8], 4.0f);
    for (i = 0; i <= 160; i++) tc_canvas_to(&cv, 60 + i * 3, (int)(120 + 50 * sin(i * 0.06)));
    tc_canvas_end(&cv);
    tc_canvas_begin(&cv, 520, 40, TC_PALETTE[2], 8.0f); tc_canvas_to(&cv, 600, 180); tc_canvas_end(&cv);
    ui.connected = 1; ui.room = 'C'; ui.people = 2; ui.max = 16;
    ui.status = "POINT + HOLD A"; ui.pal_index = 8; ui.pen_index = 3; ui.canvas = &cv;
    strcpy(ui.log[0].nick, "Breadboi"); ui.nlog = 1;
    tc_ui_render(&ui, rgba, W, H);

    /* forward to YUY2, then back to RGB the way a TV decodes it */
    for (y = 0; y < H; y++) {
        for (x = 0; x + 1 < W; x += 2) {
            const uint8_t *a = rgba + ((size_t)y * W + x) * 4;
            const uint8_t *b = a + 4;
            uint32_t w = tc_rgb_to_yuy2(a[0], a[1], a[2], b[0], b[1], b[2]);
            int y1 = (w >> 24) & 0xFF, cb = (w >> 16) & 0xFF;
            int y2 = (w >> 8) & 0xFF,  crv = w & 0xFF;
            int k;
            for (k = 0; k < 2; k++) {
                int Y = k ? y2 : y1;
                int R = clamp((int)(Y + 1.402 * (crv - 128)));
                int G = clamp((int)(Y - 0.344136 * (cb - 128) - 0.714136 * (crv - 128)));
                int B = clamp((int)(Y + 1.772 * (cb - 128)));
                uint8_t *o = out + ((size_t)y * W + x + k) * 3;
                o[0] = (uint8_t)R; o[1] = (uint8_t)G; o[2] = (uint8_t)B;
                {
                    const uint8_t *s = rgba + ((size_t)y * W + x + k) * 4;
                    err += (double)((R - s[0]) * (R - s[0]) + (G - s[1]) * (G - s[1]) + (B - s[2]) * (B - s[2]));
                    n += 3;
                }
            }
        }
    }
    printf("YUY2 round-trip RMS error: %.2f / 255 (chroma subsampling makes 0 impossible)\n",
           sqrt(err / n));
    stbi_write_png("/src/build/wii_yuv_roundtrip.png", W, H, 3, out, W * 3);
    puts("wrote build/wii_yuv_roundtrip.png");
    tc_canvas_free(&cv); free(rgba); free(out);
    return 0;
}
