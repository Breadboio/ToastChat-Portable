/* Writes the exact PNG bytes tc_send_canvas puts on the wire, so they can be
 * looked at rather than assumed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../core/tc_send.h"
#include "../core/tc_ui.h"

int main(void) {
    tc_canvas c; uint8_t *png; size_t len; int ow = 0, oh = 0, i;
    tc_rect cv = tc_ui_canvas_rect(1280, 720);
    tc_canvas_init(&c, 16000, 256);
    tc_canvas_begin(&c, 120, 80, TC_PALETTE[8], 8.0f); tc_canvas_to(&c, 120, 260); tc_canvas_end(&c);
    tc_canvas_begin(&c, 120, 170, TC_PALETTE[8], 8.0f); tc_canvas_to(&c, 230, 170); tc_canvas_end(&c);
    tc_canvas_begin(&c, 230, 80, TC_PALETTE[8], 8.0f); tc_canvas_to(&c, 230, 260); tc_canvas_end(&c);
    tc_canvas_begin(&c, 300, 80, TC_PALETTE[2], 8.0f); tc_canvas_to(&c, 300, 260); tc_canvas_end(&c);
    tc_canvas_begin(&c, 400, 300, TC_PALETTE[11], 4.0f);
    for (i = 0; i <= 200; i++) tc_canvas_to(&c, 400 + i * 3, (int)(340 + 60 * sin(i * 0.05)));
    tc_canvas_end(&c);

    png = tc_render_png(&c, cv.w, cv.h, 2, &len, &ow, &oh);
    if (!png) { puts("encode FAILED"); return 1; }
    printf("wire PNG: %dx%d, %zu bytes (limit 400KB)\n", ow, oh, len);
    { FILE *f = fopen("/src/build/wire.png", "wb"); fwrite(png, 1, len, f); fclose(f); }
    puts("wrote build/wire.png");
    tc_canvas_free(&c);
    return 0;
}
