/* Portable UI compositor: writes the whole ToastChat screen into an RGBA
 * buffer. No platform calls, so the desktop target renders the identical
 * pixels a console will - which is how this gets reviewed before it ships. */
#include "tc_ui.h"
#include "tc_font.h"
#include <string.h>

const uint32_t TC_PALETTE[16] = {
    0x8c8f94,0x7a4c2a,0xc8352b,0xe77aa6,0xe0762a,0xe0b52a,0xa8cc37,0x4faa3c,
    0x2f7d34,0x2fa89a,0x3fbbd6,0x2f6fd0,0x2a3f9e,0x7a49c4,0xb04ec4,0xd6459b
};
const float TC_PEN_RADIUS[6] = { 0.5f, 1.0f, 2.0f, 3.0f, 5.0f, 8.0f };

#define BG      0x11151c
#define PANEL   0x1b212b
#define EDGE    0x2c3542
#define INK     0xd8dee9
#define DIM     0x7c8797
#define OK      0x4faa3c
#define BAD     0xc8352b
#define PAPER   0xf4f1e8

static void px_set(uint8_t *b, int W, int H, int x, int y, uint32_t c) {
    uint8_t *p;
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    p = b + ((size_t)y * W + x) * 4;
    p[0] = (uint8_t)(c >> 16); p[1] = (uint8_t)(c >> 8); p[2] = (uint8_t)c; p[3] = 255;
}
static void fill(uint8_t *b, int W, int H, int x, int y, int w, int h, uint32_t c) {
    int i, j;
    for (j = y; j < y + h; j++) for (i = x; i < x + w; i++) px_set(b, W, H, i, j, c);
}
static void frame(uint8_t *b, int W, int H, int x, int y, int w, int h, uint32_t c) {
    fill(b, W, H, x, y, w, 1, c); fill(b, W, H, x, y + h - 1, w, 1, c);
    fill(b, W, H, x, y, 1, h, c); fill(b, W, H, x + w - 1, y, 1, h, c);
}
static void text(uint8_t *b, int W, int H, int x, int y, const char *s, uint32_t c, int scale) {
    int cx = x;
    for (; *s; s++) {
        int ch = (unsigned char)*s, row, col;
        if (ch < TC_FONT_FIRST || ch > TC_FONT_LAST) ch = '?';
        for (row = 0; row < TC_FONT_H; row++) {
            uint8_t bits = tc_font[ch - TC_FONT_FIRST][row];
            for (col = 0; col < 8; col++) {
                if (bits & (0x80 >> col)) {
                    int sx, sy;
                    for (sy = 0; sy < scale; sy++)
                        for (sx = 0; sx < scale; sx++)
                            px_set(b, W, H, cx + col * scale + sx, y + row * scale + sy, c);
                }
            }
        }
        cx += 8 * scale;
    }
    return;
}

tc_layout tc_ui_layout(int W, int H) {
    tc_layout L;
    memset(&L, 0, sizeof(L));
    L.compact = (W < 900);
    L.dual = (W < 400);
    if (L.dual) {
        /* 3DS bottom screen, 320x240: no status bar and no log here - those
         * live on the top screen - so the whole height is canvas + toolbar. */
        L.bar_h = 0; L.log_h = 0; L.tool_h = 60;
        L.sw_size = 16; L.sw_pitch = 18; L.sw_x = 6; L.sw_cols = 8;
        L.sw_y = H - L.tool_h + 4;
        L.pen_size = 14; L.pen_pitch = 16; L.pen_x = 158; L.pen_y = L.sw_y;
        L.btn_w = 48; L.btn_h = 18; L.btn_pitch = 52;
        L.btn_x = 158; L.btn_y = L.sw_y + 22;
        L.thumb_w = 100; L.thumb_h = 58; L.thumb_gap = 8;
    } else if (!L.compact) {
        L.bar_h = 40; L.log_h = 160; L.tool_h = 84;
        L.sw_size = 34; L.sw_pitch = 39; L.sw_x = 16; L.sw_cols = 16;
        L.sw_y = H - L.tool_h + 12;
        L.pen_size = 34; L.pen_pitch = 39;
        L.pen_x = L.sw_x + 16 * L.sw_pitch + 25; L.pen_y = L.sw_y;
        L.btn_w = 110; L.btn_h = 40; L.btn_pitch = 118;
        L.btn_x = W - 16 - (3 * 110 + 2 * 8); L.btn_y = L.sw_y;
        L.thumb_w = 180; L.thumb_h = 118; L.thumb_gap = 12;
    } else {
        /* 640x480: swatches wrap to two rows of eight and the buttons sit under
         * the pen row. Sixteen swatches in one row would want 624 of 640px. */
        L.bar_h = 24; L.log_h = 88; L.tool_h = 96;
        L.sw_size = 30; L.sw_pitch = 34; L.sw_x = 12; L.sw_cols = 8;
        L.sw_y = H - L.tool_h + 8;
        L.pen_size = 28; L.pen_pitch = 32; L.pen_x = 292; L.pen_y = L.sw_y;
        L.btn_w = 70; L.btn_h = 28; L.btn_pitch = 76;
        L.btn_x = 292; L.btn_y = L.sw_y + 34;
        L.thumb_w = 100; L.thumb_h = 58; L.thumb_gap = 10;
    }
    return L;
}

tc_rect tc_ui_canvas_rect(int W, int H) {
    tc_layout L = tc_ui_layout(W, H);
    tc_rect r;
    r.x = 0; r.y = L.bar_h + L.log_h;
    r.w = W; r.h = H - L.bar_h - L.log_h - L.tool_h;
    return r;
}
tc_rect tc_ui_swatch_rect(int W, int H, int i) {
    tc_layout L = tc_ui_layout(W, H);
    tc_rect r;
    r.w = L.sw_size; r.h = L.dual ? 16 : (L.compact ? 30 : 40);
    r.x = L.sw_x + (i % L.sw_cols) * L.sw_pitch;
    r.y = L.sw_y + (i / L.sw_cols) * L.sw_pitch;
    return r;
}
tc_rect tc_ui_pen_rect(int W, int H, int i) {
    tc_layout L = tc_ui_layout(W, H);
    tc_rect r;
    r.w = L.pen_size; r.h = L.dual ? 14 : (L.compact ? 28 : 40);
    r.x = L.pen_x + i * L.pen_pitch; r.y = L.pen_y;
    return r;
}
tc_rect tc_ui_button_rect(int W, int H, int i) {
    tc_layout L = tc_ui_layout(W, H);
    tc_rect r;
    r.w = L.btn_w; r.h = L.btn_h;
    r.x = L.btn_x + i * L.btn_pitch; r.y = L.btn_y;
    return r;
}

static void draw_thumb(uint8_t *b, int W, int H, const tc_ui_entry *e,
                       int x, int y, int tw, int th) {
    int i, j;
    fill(b, W, H, x, y, tw, th, PAPER);
    frame(b, W, H, x, y, tw, th, EDGE);
    if (e->rgba && e->w > 0 && e->h > 0) {
        /* Fit inside the box preserving aspect, centred - drawings are wide
         * strips and stretching them to a 3:2 box looks wrong. */
        int bw = tw - 2, bh = th - 2;
        int dw = bw, dh = (int)((long)e->h * bw / e->w);
        int ox, oy;
        if (dh > bh) { dh = bh; dw = (int)((long)e->w * bh / e->h); }
        if (dw < 1) dw = 1;
        if (dh < 1) dh = 1;
        ox = x + 1 + (bw - dw) / 2;
        oy = y + 1 + (bh - dh) / 2;
        for (j = 0; j < dh; j++) {
            int sy = j * e->h / dh;
            for (i = 0; i < dw; i++) {
                int sx = i * e->w / dw;
                const uint8_t *s = e->rgba + ((size_t)sy * e->stride + sx) * 4;
                float a = s[3] / 255.0f;
                uint32_t c = ((uint32_t)(s[0] * a + 0xf4 * (1 - a)) << 16) |
                             ((uint32_t)(s[1] * a + 0xf1 * (1 - a)) << 8)  |
                              (uint32_t)(s[2] * a + 0xe8 * (1 - a));
                px_set(b, W, H, ox + i, oy + j, c);
            }
        }
    } else {
        text(b, W, H, x + tw / 2 - 12, y + th / 2 - 8, "...", DIM, 1);
    }
    text(b, W, H, x + 2, y + th + 2, e->nick, DIM, 1);
}

static void render_bar_and_log(const tc_ui *ui, uint8_t *rgba, int W, int H,
                               tc_layout L, int pad) {
    char buf[64];
    int i, ty;

    fill(rgba, W, H, 0, 0, W, L.bar_h, PANEL);
    fill(rgba, W, H, 0, L.bar_h - 1, W, 1, EDGE);
    ty = (L.bar_h - 16) / 2;
    if (ty < 0) ty = 0;

    if (W >= 360) {
        text(rgba, W, H, pad, ty, "TOASTCHAT", INK, 1);
        fill(rgba, W, H, pad + 84, ty, 8, 16, ui->connected ? OK : BAD);
        text(rgba, W, H, pad + 100, ty, ui->connected ? "ONLINE" : "OFFLINE",
             ui->connected ? OK : BAD, 1);
    } else {
        fill(rgba, W, H, pad, ty, 8, 16, ui->connected ? OK : BAD);
    }
    {
        int rx = (W >= 360) ? pad + 200 : pad + 16;
        if (ui->room >= 'A' && ui->room <= 'D') {
            int k = 0;
            memcpy(buf, "ROOM ", 5); buf[5] = ui->room; buf[6] = '\0';
            text(rgba, W, H, rx, ty, buf, INK, 1);
            if (ui->people >= 10) buf[k++] = (char)('0' + ui->people / 10);
            buf[k++] = (char)('0' + ui->people % 10); buf[k++] = '/';
            if (ui->max >= 10) buf[k++] = (char)('0' + ui->max / 10);
            buf[k++] = (char)('0' + ui->max % 10); buf[k] = '\0';
            text(rgba, W, H, rx + 68, ty, buf, DIM, 1);
        } else {
            text(rgba, W, H, rx, ty, "LOBBY", DIM, 1);
        }
    }
    if (ui->status) {
        int sx = W - pad - (int)strlen(ui->status) * 8;
        if (sx > pad + 300 || W < 360) text(rgba, W, H, sx, ty, ui->status, DIM, 1);
    }

    {
        int x = pad, shown = 0;
        for (i = 0; i < ui->nlog && i < TC_UI_LOG_MAX; i++) {
            if (x + L.thumb_w > W - pad) break;
            draw_thumb(rgba, W, H, &ui->log[i], x, L.bar_h + 8, L.thumb_w, L.thumb_h);
            x += L.thumb_w + L.thumb_gap;
            shown++;
        }
        if (shown == 0)
            text(rgba, W, H, pad, L.bar_h + L.log_h / 2 - 8, "NOTHING DRAWN YET", DIM, 1);
    }
    if (L.log_h > 0) fill(rgba, W, H, 0, L.bar_h + L.log_h - 1, W, 1, EDGE);
}

static void render_canvas_and_tools(const tc_ui *ui, uint8_t *rgba, int W, int H,
                                    tc_layout L) {
    tc_rect cv;
    int i;

    cv.x = 0; cv.y = L.bar_h + L.log_h;
    cv.w = W; cv.h = H - L.bar_h - L.log_h - L.tool_h;

    fill(rgba, W, H, cv.x, cv.y, cv.w, cv.h, PAPER);
    if (ui->canvas) {
        uint8_t *sub = rgba + ((size_t)cv.y * W) * 4;
        tc_canvas_render(ui->canvas, sub, W, cv.h);
    }

    fill(rgba, W, H, 0, H - L.tool_h, W, L.tool_h, PANEL);
    fill(rgba, W, H, 0, H - L.tool_h, W, 1, EDGE);
    for (i = 0; i < 16; i++) {
        tc_rect r = tc_ui_swatch_rect(W, H, i);
        fill(rgba, W, H, r.x, r.y, r.w, r.h, TC_PALETTE[i]);
        if (i == ui->pal_index) {
            int g = L.dual ? 1 : 2;
            frame(rgba, W, H, r.x - g - 1, r.y - g - 1, r.w + 2 * g + 2, r.h + 2 * g + 2, INK);
            frame(rgba, W, H, r.x - g, r.y - g, r.w + 2 * g, r.h + 2 * g, INK);
        } else {
            frame(rgba, W, H, r.x, r.y, r.w, r.h, EDGE);
        }
    }
    for (i = 0; i < 6; i++) {
        tc_rect r = tc_ui_pen_rect(W, H, i);
        int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
        int rad = (int)TC_PEN_RADIUS[i], dx, dy;
        if (L.dual && rad > 4) rad = 4;
        fill(rgba, W, H, r.x, r.y, r.w, r.h, BG);
        frame(rgba, W, H, r.x, r.y, r.w, r.h, i == ui->pen_index ? INK : EDGE);
        for (dy = -rad; dy <= rad; dy++)
            for (dx = -rad; dx <= rad; dx++)
                if (dx * dx + dy * dy <= rad * rad) px_set(rgba, W, H, cx + dx, cy + dy, INK);
        if (rad == 0) px_set(rgba, W, H, cx, cy, INK);
    }
    {
        static const char *Lb[3] = { "UNDO", "CLEAR", "SEND" };
        static const char *Ls[3] = { "UN", "CL", "GO" };
        for (i = 0; i < 3; i++) {
            tc_rect r = tc_ui_button_rect(W, H, i);
            const char *lab = L.dual ? Ls[i] : Lb[i];
            fill(rgba, W, H, r.x, r.y, r.w, r.h, (i == 2) ? OK : EDGE);
            frame(rgba, W, H, r.x, r.y, r.w, r.h, INK);
            text(rgba, W, H, r.x + r.w / 2 - (int)strlen(lab) * 4,
                 r.y + (r.h - 16) / 2, lab, INK, 1);
        }
    }
}

void tc_ui_render_top(const tc_ui *ui, uint8_t *rgba, int W, int H) {
    tc_layout L = tc_ui_layout(W, H);
    L.bar_h = 24; L.log_h = H - 24;
    L.thumb_w = 100; L.thumb_h = 58; L.thumb_gap = 8;
    fill(rgba, W, H, 0, 0, W, H, BG);
    render_bar_and_log(ui, rgba, W, H, L, 8);
}

void tc_ui_render_bottom(const tc_ui *ui, uint8_t *rgba, int W, int H) {
    tc_layout L = tc_ui_layout(W, H);
    fill(rgba, W, H, 0, 0, W, H, BG);
    render_canvas_and_tools(ui, rgba, W, H, L);
}

void tc_ui_render(const tc_ui *ui, uint8_t *rgba, int W, int H) {
    tc_layout L = tc_ui_layout(W, H);
    fill(rgba, W, H, 0, 0, W, H, BG);
    render_bar_and_log(ui, rgba, W, H, L, L.compact ? 12 : 16);
    render_canvas_and_tools(ui, rgba, W, H, L);
}
