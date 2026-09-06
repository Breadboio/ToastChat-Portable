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

#define BAR_H   40
#define LOG_H   160
#define TOOL_H  84

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
static int text(uint8_t *b, int W, int H, int x, int y, const char *s, uint32_t c, int scale) {
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
    return cx - x;
}

tc_rect tc_ui_canvas_rect(int W, int H) {
    tc_rect r; r.x = 0; r.y = BAR_H + LOG_H; r.w = W; r.h = H - BAR_H - LOG_H - TOOL_H;
    return r;
}
tc_rect tc_ui_swatch_rect(int W, int H, int i) {
    tc_rect r; r.w = 34; r.h = 40;
    r.x = 16 + i * 39; r.y = H - TOOL_H + 12;
    (void)W; return r;
}
tc_rect tc_ui_pen_rect(int W, int H, int i) {
    tc_rect r; r.w = 34; r.h = 40;
    r.x = 16 + 16 * 39 + 25 + i * 39; r.y = H - TOOL_H + 12;
    (void)W; return r;
}
tc_rect tc_ui_button_rect(int W, int H, int i) {
    tc_rect r; int bw = 110, gap = 8, total = 3 * bw + 2 * gap;
    r.w = bw; r.h = 40; r.y = H - TOOL_H + 12;
    r.x = W - 16 - total + i * (bw + gap);
    return r;
}

static void draw_thumb(uint8_t *b, int W, int H, const tc_ui_entry *e, int x, int y, int tw, int th) {
    int i, j;
    fill(b, W, H, x, y, tw, th, PAPER);
    frame(b, W, H, x, y, tw, th, EDGE);
    if (e->rgba && e->w > 0 && e->h > 0) {
        for (j = 0; j < th - 2; j++) {
            int sy = j * e->h / (th - 2);
            for (i = 0; i < tw - 2; i++) {
                int sx = i * e->w / (tw - 2);
                const uint8_t *s = e->rgba + ((size_t)sy * e->w + sx) * 4;
                float a = s[3] / 255.0f;
                uint32_t c = ((uint32_t)(s[0] * a + 0xf4 * (1 - a)) << 16) |
                             ((uint32_t)(s[1] * a + 0xf1 * (1 - a)) << 8)  |
                              (uint32_t)(s[2] * a + 0xe8 * (1 - a));
                px_set(b, W, H, x + 1 + i, y + 1 + j, c);
            }
        }
    } else {
        text(b, W, H, x + tw / 2 - 24, y + th / 2 - 8, "...", DIM, 2);
    }
    text(b, W, H, x + 3, y + th + 4, e->nick, DIM, 1);
}

void tc_ui_render(const tc_ui *ui, uint8_t *rgba, int W, int H) {
    tc_rect cv = tc_ui_canvas_rect(W, H);
    char buf[64];
    int i;

    fill(rgba, W, H, 0, 0, W, H, BG);

    /* --- status bar --- */
    fill(rgba, W, H, 0, 0, W, BAR_H, PANEL);
    fill(rgba, W, H, 0, BAR_H - 1, W, 1, EDGE);
    text(rgba, W, H, 16, 12, "TOASTCHAT", INK, 1);
    fill(rgba, W, H, 108, 12, 8, 16, ui->connected ? OK : BAD);
    text(rgba, W, H, 124, 12, ui->connected ? "ONLINE" : "OFFLINE",
         ui->connected ? OK : BAD, 1);
    if (ui->room >= 'A' && ui->room <= 'D') {
        buf[0] = 'R'; buf[1] = 'O'; buf[2] = 'O'; buf[3] = 'M'; buf[4] = ' ';
        buf[5] = ui->room; buf[6] = '\0';
        text(rgba, W, H, 240, 12, buf, INK, 1);
        {   /* people/max without stdio */
            int n = ui->people, m = ui->max, k = 0;
            if (n >= 10) buf[k++] = (char)('0' + n / 10);
            buf[k++] = (char)('0' + n % 10); buf[k++] = '/';
            if (m >= 10) buf[k++] = (char)('0' + m / 10);
            buf[k++] = (char)('0' + m % 10); buf[k] = '\0';
            text(rgba, W, H, 330, 12, buf, DIM, 1);
        }
    } else {
        text(rgba, W, H, 240, 12, "LOBBY", DIM, 1);
    }
    if (ui->status) text(rgba, W, H, W - 16 - (int)strlen(ui->status) * 8, 12, ui->status, DIM, 1);

    /* --- log strip --- */
    fill(rgba, W, H, 0, BAR_H, W, LOG_H, BG);
    {
        int tw = 180, th = 118, x = 16;
        for (i = 0; i < ui->nlog && i < TC_UI_LOG_MAX; i++) {
            draw_thumb(rgba, W, H, &ui->log[i], x, BAR_H + 12, tw, th);
            x += tw + 12;
        }
        if (ui->nlog == 0)
            text(rgba, W, H, 16, BAR_H + 60, "NOTHING DRAWN YET", DIM, 1);
    }
    fill(rgba, W, H, 0, BAR_H + LOG_H - 1, W, 1, EDGE);

    /* --- canvas --- */
    fill(rgba, W, H, cv.x, cv.y, cv.w, cv.h, PAPER);
    if (ui->canvas) {
        /* strokes are stored in canvas space; blit with the region offset */
        uint8_t *sub = rgba + ((size_t)cv.y * W) * 4;
        tc_canvas_render(ui->canvas, sub, W, cv.h);
    }

    /* --- toolbar --- */
    fill(rgba, W, H, 0, H - TOOL_H, W, TOOL_H, PANEL);
    fill(rgba, W, H, 0, H - TOOL_H, W, 1, EDGE);
    for (i = 0; i < 16; i++) {
        tc_rect r = tc_ui_swatch_rect(W, H, i);
        fill(rgba, W, H, r.x, r.y, r.w, r.h, TC_PALETTE[i]);
        if (i == ui->pal_index) {
            frame(rgba, W, H, r.x - 3, r.y - 3, r.w + 6, r.h + 6, INK);
            frame(rgba, W, H, r.x - 2, r.y - 2, r.w + 4, r.h + 4, INK);
        } else {
            frame(rgba, W, H, r.x, r.y, r.w, r.h, EDGE);
        }
    }
    for (i = 0; i < 6; i++) {
        tc_rect r = tc_ui_pen_rect(W, H, i);
        int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
        int rad = (int)TC_PEN_RADIUS[i], dx, dy;
        fill(rgba, W, H, r.x, r.y, r.w, r.h, BG);
        frame(rgba, W, H, r.x, r.y, r.w, r.h, i == ui->pen_index ? INK : EDGE);
        for (dy = -rad; dy <= rad; dy++)
            for (dx = -rad; dx <= rad; dx++)
                if (dx * dx + dy * dy <= rad * rad) px_set(rgba, W, H, cx + dx, cy + dy, INK);
        if (rad == 0) px_set(rgba, W, H, cx, cy, INK);
    }
    {
        static const char *L[3] = { "UNDO", "CLEAR", "SEND" };
        for (i = 0; i < 3; i++) {
            tc_rect r = tc_ui_button_rect(W, H, i);
            uint32_t c = (i == 2) ? OK : EDGE;
            fill(rgba, W, H, r.x, r.y, r.w, r.h, c);
            frame(rgba, W, H, r.x, r.y, r.w, r.h, INK);
            text(rgba, W, H, r.x + r.w / 2 - (int)strlen(L[i]) * 4, r.y + 12, L[i], INK, 1);
        }
    }
}
