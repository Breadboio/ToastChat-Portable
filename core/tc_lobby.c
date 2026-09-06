#include "tc_lobby.h"
#include "tc_font.h"
#include <string.h>

#define PC_PAGE      0xffffff
#define PC_CHROME    0xc3cfe0
#define PC_CHROME_HI 0xe2e9f3
#define PC_LINE      0x94a3b8
#define PC_INK_SOFT  0x64748b
#define PC_PANEL     0xeef3fa
#define PC_ACCENT    0x2f6fd0
#define PC_HEADING   0x33405a
#define PC_CARD_2    0xeaf0f8
#define OKC          0x4faa3c
#define BADC         0xc8352b

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
static uint32_t mix(uint32_t a, uint32_t bb, int n, int d) {
    int i; uint32_t o = 0;
    for (i = 16; i >= 0; i -= 8) {
        int ca = (int)((a >> i) & 0xFF), cb = (int)((bb >> i) & 0xFF);
        o |= (uint32_t)(ca + (cb - ca) * n / (d ? d : 1)) << i;
    }
    return o;
}
static void vgrad(uint8_t *b, int W, int H, int x, int y, int w, int h,
                  uint32_t t, uint32_t bo) {
    int j;
    for (j = 0; j < h; j++) fill(b, W, H, x, y + j, w, 1, mix(t, bo, j, h - 1));
}
static void rrect(uint8_t *b, int W, int H, int x, int y, int w, int h, int r, uint32_t c) {
    int i, j;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    for (j = 0; j < h; j++) for (i = 0; i < w; i++) {
        int dx = 0, dy = 0;
        if (i < r) dx = r - i; else if (i >= w - r) dx = i - (w - r - 1);
        if (j < r) dy = r - j; else if (j >= h - r) dy = j - (h - r - 1);
        if (dx && dy && dx * dx + dy * dy > r * r) continue;
        px(b, W, H, x + i, y + j, c);
    }
}
static void rframe(uint8_t *b, int W, int H, int x, int y, int w, int h, int r, uint32_t c) {
    int i;
    for (i = r; i < w - r; i++) { px(b,W,H,x+i,y,c); px(b,W,H,x+i,y+h-1,c); }
    for (i = r; i < h - r; i++) { px(b,W,H,x,y+i,c); px(b,W,H,x+w-1,y+i,c); }
    /* Corners are left to the rounded fill underneath. */
}
static void text(uint8_t *b, int W, int H, int x, int y, const char *s, uint32_t c, int sc) {
    int cx = x;
    for (; *s; s++) {
        int ch = (unsigned char)*s, row, col;
        if (ch < TC_FONT_FIRST || ch > TC_FONT_LAST) ch = '?';
        for (row = 0; row < TC_FONT_H; row++) {
            uint8_t bits = tc_font[ch - TC_FONT_FIRST][row];
            for (col = 0; col < 8; col++) if (bits & (0x80 >> col)) {
                int sx, sy;
                for (sy = 0; sy < sc; sy++) for (sx = 0; sx < sc; sx++)
                    px(b, W, H, cx + col * sc + sx, y + row * sc + sy, c);
            }
        }
        cx += 8 * sc;
    }
}

/* 2x2 on a 3DS bottom screen, one row across everywhere else. */
static void grid(int W, int H, int *cols, int *cw, int *chh, int *ox, int *oy, int *bar) {
    int top = (W < 400) ? 46 : (W < 900 ? 70 : 120);
    int pad = (W < 400) ? 10 : (W < 900 ? 20 : 40);
    *bar = (W < 400) ? 26 : (W < 900 ? 30 : 44);
    if (W < 400) {
        *cols = 2;
        *cw = (W - pad * 3) / 2;
        *chh = (H - top - pad * 3) / 2;
    } else {
        *cols = 4;
        *cw = (W - pad * 5) / 4;
        *chh = H - top - pad * 2;
        if (*chh > *cw) *chh = *cw;
    }
    *ox = pad;
    *oy = top;
}

tc_rect tc_lobby_card_rect(int W, int H, int i) {
    int cols, cw, ch, ox, oy, bar, pad = (W < 400) ? 10 : (W < 900 ? 20 : 40);
    tc_rect r;
    grid(W, H, &cols, &cw, &ch, &ox, &oy, &bar);
    r.x = ox + (i % cols) * (cw + pad);
    r.y = oy + (i / cols) * (ch + pad);
    r.w = cw; r.h = ch;
    return r;
}

int tc_lobby_room_at(int W, int H, int x, int y, const int counts[4], int max) {
    int i;
    for (i = 0; i < 4; i++) {
        tc_rect r = tc_lobby_card_rect(W, H, i);
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
            return (counts[i] >= max) ? -1 : i;      /* full rooms not pickable */
    }
    return -1;
}

static void num(char *out, int n, int m) {
    int k = 0;
    if (n >= 10) out[k++] = (char)('0' + n / 10);
    out[k++] = (char)('0' + n % 10);
    out[k++] = '/';
    if (m >= 10) out[k++] = (char)('0' + m / 10);
    out[k++] = (char)('0' + m % 10);
    out[k] = '\0';
}

void tc_lobby_render(uint8_t *rgba, int W, int H, const int counts[4], int max,
                     const char *nick, int connected) {
    int cols, cw, ch, ox, oy, barh, i;
    int hdr = (W < 400) ? 24 : (W < 900 ? 30 : 40);
    int tsc = (W < 400) ? 1 : 2;
    int lsc = (W < 400) ? 3 : (W < 900 ? 4 : 6);

    fill(rgba, W, H, 0, 0, W, H, PC_PANEL);
    vgrad(rgba, W, H, 0, 0, W, hdr, PC_CHROME_HI, PC_CHROME);
    fill(rgba, W, H, 0, hdr - 1, W, 1, PC_LINE);
    text(rgba, W, H, 10, (hdr - 16 * tsc) / 2, "PICK A ROOM", PC_HEADING, tsc);
    {
        int nl = (int)strlen(nick) * 8;
        int cx = W - 8 - (nl + 10);
        if (cx < 100) { cx = 100; }
        rrect(rgba, W, H, cx, (hdr - 18) / 2, nl + 10, 18, 3,
              connected ? OKC : BADC);
        text(rgba, W, H, cx + 5, (hdr - 16) / 2, nick, 0xffffff, 1);
    }

    grid(W, H, &cols, &cw, &ch, &ox, &oy, &barh);
    for (i = 0; i < 4; i++) {
        tc_rect r = tc_lobby_card_rect(W, H, i);
        int full = counts[i] >= max;
        uint32_t face_a = full ? PC_CARD_2 : PC_PAGE;
        uint32_t face_b = full ? PC_PANEL  : PC_CARD_2;
        uint32_t ink    = full ? PC_INK_SOFT : PC_HEADING;
        char n[8];
        char letter[2];
        int j;

        for (j = 0; j < r.h; j++)
            fill(rgba, W, H, r.x, r.y + j, r.w, 1, mix(face_a, face_b, j, r.h - 1));
        rrect(rgba, W, H, r.x, r.y, r.w, 7, 7, mix(face_a, face_b, 0, r.h - 1));
        rframe(rgba, W, H, r.x, r.y, r.w, r.h, 7, PC_LINE);
        /* the inset bottom edge ds.css draws with a box-shadow */
        fill(rgba, W, H, r.x + 1, r.y + r.h - 3, r.w - 2, 2, mix(face_b, PC_LINE, 1, 3));

        letter[0] = (char)('A' + i); letter[1] = '\0';
        text(rgba, W, H, r.x + (r.w - 8 * lsc) / 2, r.y + r.h / 2 - 16 * lsc / 2 - barh / 2,
             letter, ink, lsc);
        if (full) strcpy(n, "FULL");
        else      num(n, counts[i], max);
        text(rgba, W, H, r.x + (r.w - (int)strlen(n) * 8) / 2,
             r.y + r.h / 2 + 16 * lsc / 2 - barh / 2 + 2, n,
             full ? BADC : PC_INK_SOFT, 1);

        {   /* occupancy bar */
            int bw = r.w - 20, bx = r.x + 10, by = r.y + r.h - 14;
            int fillw = max > 0 ? bw * counts[i] / max : 0;
            fill(rgba, W, H, bx, by, bw, 6, PC_CARD_2);
            fill(rgba, W, H, bx, by, bw, 1, PC_LINE);
            if (fillw > 0) fill(rgba, W, H, bx, by, fillw, 6, full ? BADC : PC_ACCENT);
        }
    }
}

void tc_lobby_render_top(uint8_t *rgba, int W, int H, const int counts[4], int max) {
    int sc = (W < 500) ? 2 : 3;
    char n[8];
    fill(rgba, W, H, 0, 0, W, H, PC_PAGE);
    text(rgba, W, H, (W - 9 * 8 * sc) / 2, H / 3 - 20, "TOASTCHAT", PC_HEADING, sc);
    text(rgba, W, H, (W - 22 * 8) / 2, H / 3 + 20, "CHOOSE A ROOM BELOW", PC_INK_SOFT, 1);
    {
        int bx = (W - 240) / 2, by = H / 2 + 30, j;
        for (j = 0; j < 4; j++) {
            char lab[2];
            int bw = 50, x = bx + j * 60;
            int fw = max > 0 ? bw * counts[j] / max : 0;
            lab[0] = (char)('A' + j); lab[1] = '\0';
            text(rgba, W, H, x + 21, by - 20, lab, PC_HEADING, 1);
            fill(rgba, W, H, x, by, bw, 8, PC_CARD_2);
            fill(rgba, W, H, x, by, bw, 1, PC_LINE);
            if (fw > 0) fill(rgba, W, H, x, by, fw, 8, PC_ACCENT);
            num(n, counts[j], max);
            text(rgba, W, H, x + (bw - (int)strlen(n) * 8) / 2, by + 12, n, PC_INK_SOFT, 1);
        }
    }
}
