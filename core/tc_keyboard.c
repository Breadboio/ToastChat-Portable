#include "tc_keyboard.h"
#include "tc_ui.h"
#include "tc_font.h"
#include <string.h>

/* 40 cells: A-Z, 0-9, space, hyphen, backspace, OK. */
static const char KEYS[TC_KB_ROWS][TC_KB_COLS + 1] = {
    "ABCDEFGHIJ",
    "KLMNOPQRST",
    "UVWXYZ0123",
    "456789 -\b\n"
};

/* Same light PictoChat palette as tc_ui.c, from the web client's ds.css. */
#define BG      0xeef3fa   /* --pc-panel   */
#define PANEL   0xffffff   /* --pc-card    */
#define EDGE    0xc2ccdb   /* --pc-divider */
#define INK     0x2b3340   /* --pc-ink     */
#define DIM     0x64748b   /* --pc-ink-soft*/
#define OK      0x2f6fd0   /* --pc-accent  */
#define PAPER   0xffffff
#define CHROME    0xc3cfe0
#define CHROME_HI 0xe2e9f3
#define HEADING   0x33405a
#define LINE      0x94a3b8

static void px(uint8_t *b, int W, int H, int x, int y, uint32_t c) {
    uint8_t *p;
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    p = b + ((size_t)y * W + x) * 4;
    p[0] = (uint8_t)(c >> 16); p[1] = (uint8_t)(c >> 8); p[2] = (uint8_t)c; p[3] = 255;
}
static void fill(uint8_t *b, int W, int H, int x, int y, int w, int h, uint32_t c) {
    int i, j;
    for (j = y; j < y + h; j++) for (i = x; i < x + w; i++) px(b, W, H, i, j, c);
}
static void frame(uint8_t *b, int W, int H, int x, int y, int w, int h, uint32_t c) {
    fill(b, W, H, x, y, w, 1, c); fill(b, W, H, x, y + h - 1, w, 1, c);
    fill(b, W, H, x, y, 1, h, c); fill(b, W, H, x + w - 1, y, 1, h, c);
}
static void text(uint8_t *b, int W, int H, int x, int y, const char *s, uint32_t c, int sc) {
    int cx = x;
    for (; *s; s++) {
        int ch = (unsigned char)*s, row, col;
        if (ch < TC_FONT_FIRST || ch > TC_FONT_LAST) ch = '?';
        for (row = 0; row < TC_FONT_H; row++) {
            uint8_t bits = tc_font[ch - TC_FONT_FIRST][row];
            for (col = 0; col < 8; col++)
                if (bits & (0x80 >> col)) {
                    int sx, sy;
                    for (sy = 0; sy < sc; sy++)
                        for (sx = 0; sx < sc; sx++)
                            px(b, W, H, cx + col * sc + sx, y + row * sc + sy, c);
                }
        }
        cx += 8 * sc;
    }
}

/* One source of truth for the grid, used by both render and hit test. */
static void grid(int W, int H, int *ox, int *oy, int *cw, int *ch, int *sc) {
    int pad = W < 400 ? 6 : (W < 900 ? 14 : 28);
    int top = W < 400 ? 74 : (W < 900 ? 150 : 240);
    *cw = (W - pad * 2) / TC_KB_COLS;
    *ch = (H - top - pad) / TC_KB_ROWS;
    if (*ch > *cw) *ch = *cw;
    *ox = (W - *cw * TC_KB_COLS) / 2;
    *oy = top;
    *sc = (*cw >= 44) ? 3 : (*cw >= 26 ? 2 : 1);
}

void tc_keyboard_render(uint8_t *rgba, int W, int H, const char *nick, int nick_max) {
    int ox, oy, cw, ch, sc, r, c;
    int pad = W < 400 ? 6 : (W < 900 ? 14 : 28);
    int tsc = W < 400 ? 1 : 2;

    fill(rgba, W, H, 0, 0, W, H, BG);
    /* chrome header, matching the main screen */
    {
        int bh = 16 * tsc + 8;
        int j;
        for (j = 0; j < bh; j++) {
            int r0 = (int)((CHROME_HI >> 16) & 0xFF), r1 = (int)((CHROME >> 16) & 0xFF);
            int g0 = (int)((CHROME_HI >> 8) & 0xFF),  g1 = (int)((CHROME >> 8) & 0xFF);
            int b0 = (int)(CHROME_HI & 0xFF),         b1 = (int)(CHROME & 0xFF);
            uint32_t c = (uint32_t)(r0 + (r1 - r0) * j / (bh - 1)) << 16 |
                         (uint32_t)(g0 + (g1 - g0) * j / (bh - 1)) << 8  |
                         (uint32_t)(b0 + (b1 - b0) * j / (bh - 1));
            fill(rgba, W, H, 0, j, W, 1, c);
        }
        fill(rgba, W, H, 0, bh - 1, W, 1, LINE);
        text(rgba, W, H, pad, (bh - 16 * tsc) / 2, "PICK A NAME", HEADING, tsc);
    }

    /* the name box */
    {
        int bh = 20 * tsc, by = 16 * tsc + 8 + pad;
        fill(rgba, W, H, pad, by, W - pad * 2, bh, PAPER);
        frame(rgba, W, H, pad, by, W - pad * 2, bh, EDGE);
        text(rgba, W, H, pad + 8, by + (bh - 16 * tsc) / 2, *nick ? nick : "", INK, tsc);
        /* caret */
        fill(rgba, W, H, pad + 8 + (int)strlen(nick) * 8 * tsc, by + 4, 2 * tsc, bh - 8, DIM);
        {
            char n[16];
            int len = (int)strlen(nick);
            n[0] = (char)('0' + len / 10); n[1] = (char)('0' + len % 10);
            n[2] = '/';
            n[3] = (char)('0' + nick_max / 10); n[4] = (char)('0' + nick_max % 10);
            n[5] = '\0';
            text(rgba, W, H, W - pad - 40 * tsc / 2 - 8, by + bh + 4, n[0] == '0' ? n + 1 : n, DIM, 1);
        }
    }

    grid(W, H, &ox, &oy, &cw, &ch, &sc);
    for (r = 0; r < TC_KB_ROWS; r++) {
        for (c = 0; c < TC_KB_COLS; c++) {
            int x = ox + c * cw, y = oy + r * ch;
            char k = KEYS[r][c];
            const char *label;
            char one[2];
            uint32_t bgc = PANEL, ink = INK;
            if (k == '\n') { bgc = OK; ink = 0xffffff; label = "OK"; }
            else if (k == '\b') { label = "<-"; }
            else if (k == ' ') { label = "SP"; }
            else { one[0] = k; one[1] = '\0'; label = one; }
            fill(rgba, W, H, x + 1, y + 1, cw - 2, ch - 2, bgc);
            frame(rgba, W, H, x + 1, y + 1, cw - 2, ch - 2, EDGE);
            {
                int lw = (int)strlen(label) * 8 * sc;
                text(rgba, W, H, x + (cw - lw) / 2, y + (ch - 16 * sc) / 2, label, ink, sc);
            }
        }
    }
}

void tc_keyboard_render_top(uint8_t *rgba, int W, int H, const char *nick) {
    int sc = W < 500 ? 2 : 3;
    int cx = (W - 9 * 8 * sc) / 2;
    fill(rgba, W, H, 0, 0, W, H, PAPER);
    text(rgba, W, H, cx, H / 2 - 40, "TOASTCHAT", HEADING, sc);
    text(rgba, W, H, (W - 21 * 8) / 2, H / 2 + 4, "TAP LETTERS BELOW,", DIM, 1);
    text(rgba, W, H, (W - 21 * 8) / 2, H / 2 + 22, "THEN OK TO CONNECT.", DIM, 1);
    if (*nick) {
        int nw = (int)strlen(nick) * 8 * 2;
        text(rgba, W, H, (W - nw) / 2, H / 2 + 52, nick, OK, 2);
    }
}

char tc_keyboard_key_at(int W, int H, int x, int y) {
    int ox, oy, cw, ch, sc, c, r;
    grid(W, H, &ox, &oy, &cw, &ch, &sc);
    if (x < ox || y < oy) return 0;
    c = (x - ox) / cw;
    r = (y - oy) / ch;
    if (c < 0 || c >= TC_KB_COLS || r < 0 || r >= TC_KB_ROWS) return 0;
    return KEYS[r][c];
}

int tc_keyboard_apply(char key, char *nick, size_t nick_max) {
    size_t len = strlen(nick);
    if (key == '\n') return 1;
    if (key == '\b') { if (len) nick[len - 1] = '\0'; return 0; }
    if (key == 0) return 0;
    if (len + 1 < nick_max) { nick[len] = key; nick[len + 1] = '\0'; }
    return 0;
}
