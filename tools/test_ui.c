/* Renders the exact 1280x720 Switch screen on the desktop, so the layout can be
 * reviewed before it goes near hardware. */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"
#include "../core/tc_ui.h"
#include "../core/tc_draw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define W 1280
#define H 720

int main(void) {
    uint8_t *buf = (uint8_t *)calloc((size_t)W * H * 4, 1);
    tc_canvas cv;
    tc_ui ui;
    tc_rect cr;
    int i;

    memset(&ui, 0, sizeof(ui));
    tc_canvas_init(&cv, 8000, 128);
    cr = tc_ui_canvas_rect(W, H);

    /* Something drawn on the canvas, in canvas-local coordinates. */
    tc_canvas_begin(&cv, 120, 90, TC_PALETTE[8], 3.0f);
    for (i = 0; i <= 300; i++)
        tc_canvas_to(&cv, 120 + i * 3, (int)(150 + 70 * sin(i * 0.04)));
    tc_canvas_end(&cv);
    tc_canvas_begin(&cv, 1060, 70, TC_PALETTE[2], 8.0f);
    tc_canvas_to(&cv, 1160, 200);
    tc_canvas_end(&cv);
    tc_canvas_begin(&cv, 1160, 70, TC_PALETTE[2], 8.0f);
    tc_canvas_to(&cv, 1060, 200);
    tc_canvas_end(&cv);
    for (i = 0; i < 5; i++) { tc_canvas_begin(&cv, 900 + i * 26, 250, TC_PALETTE[11], 2.0f); tc_canvas_end(&cv); }

    /* A couple of thumbnails: one real, one still loading. */
    {
        static uint8_t thumb[120 * 90 * 4];
        tc_canvas t; int k;
        tc_canvas_init(&t, 2000, 16);
        tc_canvas_begin(&t, 20, 70, TC_PALETTE[12], 3.0f);
        for (k = 0; k <= 80; k++) tc_canvas_to(&t, 20 + k, (int)(70 - 40 * sin(k * 0.08)));
        tc_canvas_end(&t);
        memset(thumb, 0, sizeof(thumb));
        tc_canvas_render(&t, thumb, 120, 90);
        strcpy(ui.log[0].nick, "Breadboi"); ui.log[0].w = 120; ui.log[0].h = 90; ui.log[0].rgba = thumb;
        strcpy(ui.log[1].nick, "Switch");   ui.log[1].w = 0;   ui.log[1].h = 0;  ui.log[1].rgba = NULL;
        ui.nlog = 2;
        tc_canvas_free(&t);
    }

    ui.connected = 1; ui.room = 'C'; ui.people = 3; ui.max = 16;
    ui.status = "TOUCH TO DRAW";
    ui.pal_index = 8; ui.pen_index = 3; ui.canvas = &cv;

    tc_ui_render(&ui, buf, W, H);

    {   /* flatten to RGB for viewing */
        uint8_t *flat = (uint8_t *)malloc((size_t)W * H * 3);
        long k;
        for (k = 0; k < (long)W * H; k++) {
            flat[k*3+0] = buf[k*4+0]; flat[k*3+1] = buf[k*4+1]; flat[k*3+2] = buf[k*4+2];
        }
        stbi_write_png("/src/build/ui_switch.png", W, H, 3, flat, W * 3);
        free(flat);
    }
    printf("canvas region: x=%d y=%d w=%d h=%d\n", cr.x, cr.y, cr.w, cr.h);
    puts("wrote build/ui_switch.png");
    tc_canvas_free(&cv); free(buf);
    return 0;
}
