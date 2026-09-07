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

static int W = 1280, H = 720;

static void render_one(const char *out, int w, int h);

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    render_one("/src/build/ui_switch.png", 1280, 720);
    render_one("/src/build/ui_wii.png", 640, 480);
    /* Android renders 640 wide with the height taken from the panel aspect;
     * 1422 is a 1080x2400 phone, 1024 is a 1600x2560 tablet. */
    render_one("/src/build/ui_android_phone.png", 640, 1422);
    render_one("/src/build/ui_android_tablet.png", 640, 1024);
    return 0;
}

static void render_one(const char *out, int w, int h) {
    W = w; H = h;
    {
    uint8_t *buf = (uint8_t *)calloc((size_t)W * H * 4, 1);
    (void)0;
    tc_canvas cv;
    tc_ui ui;
    tc_rect cr;
    int i;

    memset(&ui, 0, sizeof(ui));
    tc_canvas_init(&cv, 8000, 128);
    cr = tc_ui_canvas_rect(W, H);

    /* Something drawn on the canvas, in canvas-local coordinates. */
    {
        float sx = cr.w / 1280.0f, sy = cr.h / 436.0f;
        tc_canvas_begin(&cv, (int)(120*sx), (int)(90*sy), TC_PALETTE[8], 3.0f);
        for (i = 0; i <= 300; i++)
            tc_canvas_to(&cv, (int)((120 + i * 3)*sx), (int)((150 + 70 * sin(i * 0.04))*sy));
        tc_canvas_end(&cv);
        tc_canvas_begin(&cv, (int)(1060*sx), (int)(70*sy), TC_PALETTE[2], 8.0f);
        tc_canvas_to(&cv, (int)(1160*sx), (int)(200*sy)); tc_canvas_end(&cv);
        tc_canvas_begin(&cv, (int)(1160*sx), (int)(70*sy), TC_PALETTE[2], 8.0f);
        tc_canvas_to(&cv, (int)(1060*sx), (int)(200*sy)); tc_canvas_end(&cv);
        for (i = 0; i < 5; i++) {
            tc_canvas_begin(&cv, (int)((900 + i * 26)*sx), (int)(250*sy), TC_PALETTE[11], 2.0f);
            tc_canvas_end(&cv);
        }
    }

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
        strcpy(ui.log[0].nick, "Breadboi");
        ui.log[0].w = 120; ui.log[0].h = 90; ui.log[0].stride = 120;
        ui.log[0].rgba = thumb; ui.log[0].color = TC_PALETTE[8];
        ui.log[1].is_sys = 1; strcpy(ui.log[1].text, "Switch entered Room C.");
        strcpy(ui.log[2].nick, "Switch");
        ui.log[2].w = 120; ui.log[2].h = 90; ui.log[2].stride = 120;
        ui.log[2].rgba = thumb; ui.log[2].color = TC_PALETTE[11];
        ui.nlog = 3;
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
        stbi_write_png(out, W, H, 3, flat, W * 3);
        free(flat);
    }
    printf("%s  %dx%d  canvas %dx%d at y=%d\n", out, W, H, cr.w, cr.h, cr.y);
    tc_canvas_free(&cv); free(buf);
    }
}
