/* Renders both 3DS screens at native size for review. */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"
#include "../core/tc_ui.h"
#include "../core/tc_draw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void dump(const char *path, const uint8_t *rgba, int w, int h) {
    uint8_t *flat = (uint8_t *)malloc((size_t)w * h * 3);
    long k;
    for (k = 0; k < (long)w * h; k++) {
        flat[k*3+0] = rgba[k*4+0]; flat[k*3+1] = rgba[k*4+1]; flat[k*3+2] = rgba[k*4+2];
    }
    stbi_write_png(path, w, h, 3, flat, w * 3);
    free(flat);
}

int main(void) {
    uint8_t *top = (uint8_t *)calloc(400 * 240 * 4, 1);
    uint8_t *bot = (uint8_t *)calloc(320 * 240 * 4, 1);
    tc_canvas cv; tc_ui ui; tc_rect cr; int i;
    static uint8_t thumb[100 * 58 * 4];

    memset(&ui, 0, sizeof(ui));
    tc_canvas_init(&cv, 8000, 128);
    cr = tc_ui_canvas_rect(320, 240);
    tc_canvas_begin(&cv, 30, 40, TC_PALETTE[8], 3.0f);
    for (i = 0; i <= 120; i++) tc_canvas_to(&cv, 30 + i * 2, (int)(70 + 30 * sin(i * 0.07)));
    tc_canvas_end(&cv);
    tc_canvas_begin(&cv, 250, 30, TC_PALETTE[2], 5.0f); tc_canvas_to(&cv, 300, 110); tc_canvas_end(&cv);

    {   /* a thumbnail for the top screen */
        tc_canvas t; int k;
        tc_canvas_init(&t, 2000, 16);
        tc_canvas_begin(&t, 15, 40, TC_PALETTE[12], 3.0f);
        for (k = 0; k <= 70; k++) tc_canvas_to(&t, 15 + k, (int)(40 - 22 * sin(k * 0.09)));
        tc_canvas_end(&t);
        memset(thumb, 0, sizeof(thumb));
        tc_canvas_render(&t, thumb, 100, 58);
        strcpy(ui.log[0].nick, "Breadboi"); ui.log[0].w = 100; ui.log[0].h = 58; ui.log[0].rgba = thumb;
        strcpy(ui.log[1].nick, "Wii");      ui.nlog = 2;
        tc_canvas_free(&t);
    }
    ui.connected = 1; ui.room = 'C'; ui.people = 3; ui.max = 16;
    ui.status = "DRAW"; ui.pal_index = 8; ui.pen_index = 3; ui.canvas = &cv;

    tc_ui_render_top(&ui, top, 400, 240);
    tc_ui_render_bottom(&ui, bot, 320, 240);
    dump("/src/build/ui_3ds_top.png", top, 400, 240);
    dump("/src/build/ui_3ds_bottom.png", bot, 320, 240);
    printf("bottom canvas: %dx%d at y=%d (toolbar takes the rest)\n", cr.w, cr.h, cr.y);
    puts("wrote build/ui_3ds_top.png + ui_3ds_bottom.png");
    tc_canvas_free(&cv); free(top); free(bot);
    return 0;
}
