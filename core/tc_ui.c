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

/* Lifted from the web client's ds.css light theme so the consoles and the
 * browser look like the same program. Names match the CSS variables. */
#define PC_PAGE      0xffffff   /* --pc-page      the drawing page          */
#define PC_CHROME    0xc3cfe0   /* --pc-chrome    bar gradient, bottom      */
#define PC_CHROME_HI 0xe2e9f3   /* --pc-chrome-hi bar gradient, top         */
#define PC_LINE      0x94a3b8   /* --pc-line      bar underline             */
#define PC_INK       0x2b3340   /* --pc-ink                                 */
#define PC_INK_SOFT  0x64748b   /* --pc-ink-soft                            */
#define PC_INK_FAINT 0x94a3b8   /* --pc-ink-faint                           */
#define PC_SYS       0xf0f4fa   /* --pc-sys       system line background    */
#define PC_SYS_INK   0x5b6b82   /* --pc-sys-ink                             */
#define PC_PANEL     0xeef3fa   /* --pc-panel                               */
#define PC_DIVIDER   0xc2ccdb   /* --pc-divider   dashed rule between rows  */
#define PC_ACCENT    0x2f6fd0   /* --pc-accent                              */
#define PC_HEADING   0x33405a   /* --pc-heading   bar text                  */
#define PC_BEZEL     0x23252b   /* --bezel-color  around the screens        */
#define OK           0x4faa3c
#define BAD          0xc8352b

/* kept for the parts still referring to the old names */
#define BG      PC_PAGE
#define PANEL   PC_CHROME
#define EDGE    PC_DIVIDER
#define INK     PC_INK
#define DIM     PC_INK_SOFT
#define PAPER   PC_PAGE

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
static uint32_t mix(uint32_t a, uint32_t b, int num, int den) {
    int i;
    uint32_t out = 0;
    for (i = 16; i >= 0; i -= 8) {
        int ca = (int)((a >> i) & 0xFF), cb = (int)((b >> i) & 0xFF);
        out |= (uint32_t)(ca + (cb - ca) * num / (den ? den : 1)) << i;
    }
    return out;
}
/* Chrome bars in the web client are a vertical gradient; so are these. */
static void vgrad(uint8_t *b, int W, int H, int x, int y, int w, int h,
                  uint32_t top, uint32_t bot) {
    int j;
    for (j = 0; j < h; j++)
        fill(b, W, H, x, y + j, w, 1, mix(top, bot, j, h - 1));
}
/* Rounded rectangle - the name chips have a 4px radius in ds.css. */
static void round_rect(uint8_t *b, int W, int H, int x, int y, int w, int h,
                       int r, uint32_t c) {
    int i, j;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            int dx = 0, dy = 0;
            if (i < r)          dx = r - i;
            else if (i >= w - r) dx = i - (w - r - 1);
            if (j < r)          dy = r - j;
            else if (j >= h - r) dy = j - (h - r - 1);
            if (dx && dy && dx * dx + dy * dy > r * r) continue;
            px_set(b, W, H, x + i, y + j, c);
        }
    }
}
static void dashed(uint8_t *b, int W, int H, int x, int y, int w, uint32_t c) {
    int i;
    for (i = 0; i < w; i++) if ((i & 3) < 2) px_set(b, W, H, x + i, y, c);
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
    /* A portrait phone. Every console is landscape - the Wii is the tallest at
     * H = 0.75W - so nothing existing can reach this branch. */
    L.tall = (!L.dual && H * 5 >= W * 6);
    /* An 8x16 glyph at scale 1 is ~27 device px once a 640-wide logical
     * screen is upscaled to a 1080-wide panel - legible, but far smaller in
     * proportion than the same font on a Wii. Phones get double. */
    L.text_scale = L.tall ? 2 : 1;
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
        L.bar_h = 40; L.log_h = 248; L.tool_h = 84;
        L.sw_size = 34; L.sw_pitch = 39; L.sw_x = 16; L.sw_cols = 16;
        L.sw_y = H - L.tool_h + 12;
        L.pen_size = 34; L.pen_pitch = 39;
        L.pen_x = L.sw_x + 16 * L.sw_pitch + 25; L.pen_y = L.sw_y;
        L.btn_w = 110; L.btn_h = 40; L.btn_pitch = 118;
        L.btn_x = W - 16 - (3 * 110 + 2 * 8); L.btn_y = L.sw_y;
        L.thumb_w = 180; L.thumb_h = 118; L.thumb_gap = 12;
    } else if (L.tall) {
        /* Portrait phone. The toolbar is the 640x480 one unchanged - it is
         * tuned for a 640-wide screen and the phone target renders at 640 - but
         * the log and canvas SPLIT the leftover height instead of the log
         * taking a fixed 152px. On a 640x1387 phone the fixed slice would have
         * given the log 11% of the screen. */
        int avail;
        /* Everything here is sized for scale-2 text: the bar holds a 32px
         * glyph, CLEAR needs 5*16=80px of label, and the toolbar is two rows
         * deep. The leftover height at the bottom is deliberate - it keeps the
         * SEND button clear of the gesture-navigation strip. */
        L.bar_h = 44; L.tool_h = 116;
        avail = H - L.bar_h - L.tool_h;
        if (avail < 120) avail = 120;
        L.log_h = avail * 2 / 5;                 /* log 40%, canvas 60% */
        L.sw_size = 30; L.sw_pitch = 34; L.sw_x = 12; L.sw_cols = 8;
        L.sw_y = H - L.tool_h + 10;
        L.pen_size = 28; L.pen_pitch = 32; L.pen_x = 292; L.pen_y = L.sw_y;
        L.btn_w = 88; L.btn_h = 38; L.btn_pitch = 96;
        L.btn_x = 292; L.btn_y = L.sw_y + 34;
        L.thumb_w = 108; L.thumb_h = 62; L.thumb_gap = 10;
    } else {
        /* 640x480: swatches wrap to two rows of eight and the buttons sit under
         * the pen row. Sixteen swatches in one row would want 624 of 640px. */
        L.bar_h = 24; L.log_h = 152; L.tool_h = 96;
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
tc_rect tc_ui_room_rect(int W, int H) {
    tc_layout L = tc_ui_layout(W, H);
    tc_rect r;
    if (L.dual) {
        r.x = L.pen_x + 6 * L.pen_pitch + 6; r.y = L.pen_y;
        r.w = W - r.x - 6; r.h = 14;
    } else if (L.tall) {
        r.x = L.pen_x + 6 * L.pen_pitch + 8; r.y = L.pen_y;
        r.w = 76; r.h = 30;                    /* "RM C" is 4*16 = 64px wide */
    } else if (L.compact) {
        r.x = L.pen_x + 6 * L.pen_pitch + 10; r.y = L.pen_y;
        r.w = 66; r.h = 28;
    } else {
        r.x = L.pen_x + 6 * L.pen_pitch + 20; r.y = L.pen_y;
        r.w = 96; r.h = 40;
    }
    return r;
}

tc_rect tc_ui_button_rect(int W, int H, int i) {
    tc_layout L = tc_ui_layout(W, H);
    tc_rect r;
    r.w = L.btn_w; r.h = L.btn_h;
    r.x = L.btn_x + i * L.btn_pitch; r.y = L.btn_y;
    return r;
}

/* One log row, PictoChat-style: a coloured name chip, the drawing beside it,
 * a dashed rule underneath. System lines get the pale notice treatment. */
/* tall is checked first: it implies compact. A scale-2 glyph is 32px, so
 * the old 18px system row could not contain one. */
#define TC_ROW_MSG(L) ((L).tall ? 72 : ((L).compact ? ((L).dual ? 62 : 64) : 112))
#define TC_ROW_SYS(L) ((L).tall ? 38 : ((L).compact ? 18 : 24))

int tc_ui_visible_rows(const tc_ui *ui, int W, int H) {
    tc_layout L = tc_ui_layout(W, H);
    int used = 0, n = 0, i;
    for (i = ui->scroll; i < ui->nlog && i < TC_UI_LOG_MAX; i++) {
        int rh = ui->log[i].is_sys ? TC_ROW_SYS(L) : TC_ROW_MSG(L);
        if (used + rh > L.log_h) break;
        used += rh;
        n++;
    }
    return n;
}

static void draw_row(uint8_t *b, int W, int H, const tc_ui_entry *e,
                     int x, int y, int w, int h, tc_layout L) {
    int compact = L.compact;
    int ts = L.text_scale;
    /* The chip holds the server's 10-character nick limit, so it has to grow
     * with the glyphs: 10 * 8 * scale + padding. */
    int chip_w = 10 * 8 * ts + 8;
    int chip_h = (compact ? 13 : 17) + (ts - 1) * 20;
    int pad = compact ? 4 : 6;

    if (e->is_sys) {
        fill(b, W, H, x, y, w, h, PC_SYS);
        text(b, W, H, x + pad + 2, y + (h - 16 * ts) / 2, e->text, PC_SYS_INK, ts);
        fill(b, W, H, x, y + h - 1, w, 1, PC_DIVIDER);
        return;
    }

    fill(b, W, H, x, y, w, h, PC_PAGE);
    round_rect(b, W, H, x + pad, y + pad, chip_w, chip_h, 3,
               e->color ? e->color : 0x8c8f94);
    {
        int maxch = (chip_w - 6) / (8 * ts);
        char n[12];
        int i = 0;
        while (e->nick[i] && i < maxch && i < 11) { n[i] = e->nick[i]; i++; }
        n[i] = '\0';
        text(b, W, H, x + pad + 3, y + pad + (chip_h - 16 * ts) / 2 + 1, n, 0xffffff, ts);
    }

    if (e->rgba && e->w > 0 && e->h > 0) {
        int ax = x + pad + chip_w + pad;
        int aw = w - (ax - x) - pad, ah = h - pad * 2;
        int dw = aw, dh = (int)((long)e->h * aw / e->w);
        int ox, oy, i, j;
        if (dh > ah) { dh = ah; dw = (int)((long)e->w * ah / e->h); }
        if (dw < 1) dw = 1;
        if (dh < 1) dh = 1;
        ox = ax;                       /* left-aligned, as the web client does */
        oy = y + (h - dh) / 2;
        for (j = 0; j < dh; j++) {
            int sy = j * e->h / dh;
            for (i = 0; i < dw; i++) {
                int sx = i * e->w / dw;
                const uint8_t *sp = e->rgba + ((size_t)sy * e->stride + sx) * 4;
                /* Integer composite over white. This runs for every pixel of
                 * every visible thumbnail, every frame - floats here cost
                 * about a third of the frame rate on a 3DS. */
                unsigned a = sp[3], ia = 255u - a;
                uint32_t c = (uint32_t)((sp[0] * a + 255u * ia) / 255u) << 16 |
                             (uint32_t)((sp[1] * a + 255u * ia) / 255u) << 8  |
                             (uint32_t)((sp[2] * a + 255u * ia) / 255u);
                px_set(b, W, H, ox + i, oy + j, c);
            }
        }
    }
    dashed(b, W, H, x, y + h - 1, w, PC_DIVIDER);
}

static void render_bar_and_log(const tc_ui *ui, uint8_t *rgba, int W, int H,
                               tc_layout L, int pad) {
    char buf[64];
    int i, ty, msg_h, sys_h, top, bottom, y;

    /* --- chrome bar --- */
    {
    int ts = L.text_scale;
    int gw = 8 * ts;                 /* one glyph's advance */
    int rx, dot_y, occ_dx;
    vgrad(rgba, W, H, 0, 0, W, L.bar_h, PC_CHROME_HI, PC_CHROME);
    fill(rgba, W, H, 0, L.bar_h - 1, W, 1, PC_LINE);
    ty = (L.bar_h - 16 * ts) / 2;
    if (ty < 0) ty = 0;
    dot_y = ty + (16 * ts - 9) / 2;
    /* 68px at scale 1, which is what this was before the scale existed. */
    occ_dx = 6 * gw + 20 * ts;

    if (L.tall) {
        /* At scale 2 the wordmark alone is 9*16 = 144px and the bar still has
         * to carry room, occupancy and status. Drop it - the app's own name is
         * the least informative thing up there, and it is on the launcher icon. */
        round_rect(rgba, W, H, pad, dot_y, 9, 9, 2, ui->connected ? OK : BAD);
        rx = pad + 9 + gw;
    } else if (W >= 360) {
        text(rgba, W, H, pad, ty, "TOASTCHAT", PC_HEADING, ts);
        round_rect(rgba, W, H, pad + 84, ty + 3, 9, 9, 2, ui->connected ? OK : BAD);
        text(rgba, W, H, pad + 100, ty, ui->connected ? "ONLINE" : "OFFLINE",
             ui->connected ? OK : BAD, ts);
        rx = pad + 200;
    } else {
        round_rect(rgba, W, H, pad, ty + 3, 9, 9, 2, ui->connected ? OK : BAD);
        rx = pad + 16;
    }
    {
        if (ui->room >= 'A' && ui->room <= 'D') {
            int k = 0;
            memcpy(buf, "ROOM ", 5); buf[5] = ui->room; buf[6] = '\0';
            text(rgba, W, H, rx, ty, buf, PC_HEADING, ts);
            if (ui->people >= 10) buf[k++] = (char)('0' + ui->people / 10);
            buf[k++] = (char)('0' + ui->people % 10); buf[k++] = '/';
            if (ui->max >= 10) buf[k++] = (char)('0' + ui->max / 10);
            buf[k++] = (char)('0' + ui->max % 10); buf[k] = '\0';
            text(rgba, W, H, rx + occ_dx, ty, buf, PC_INK_SOFT, ts);
        } else {
            text(rgba, W, H, rx, ty, "LOBBY", PC_INK_SOFT, ts);
        }
    }
    if (ui->status) {
        int sx = W - pad - (int)strlen(ui->status) * gw;
        /* Only draw the status if it cannot collide with the room block. */
        int min_sx = L.tall ? (rx + occ_dx + 5 * gw + 8) : (pad + 300);
        if (sx > min_sx || W < 360) text(rgba, W, H, sx, ty, ui->status, PC_INK_SOFT, ts);
    }
    }

    /* --- log: newest at the bottom, older scrolling up off the top --- */
    top = L.bar_h;
    bottom = L.bar_h + L.log_h;
    fill(rgba, W, H, 0, top, W, L.log_h, PC_PAGE);
    msg_h = TC_ROW_MSG(L);
    sys_h = TC_ROW_SYS(L);

    y = bottom;
    for (i = ui->scroll; i < ui->nlog && i < TC_UI_LOG_MAX; i++) {
        int rh = ui->log[i].is_sys ? sys_h : msg_h;
        y -= rh;
        if (y < top) break;
        draw_row(rgba, W, H, &ui->log[i], 0, y, W, rh, L);
    }
    if (ui->nlog == 0)
        text(rgba, W, H, pad, top + L.log_h / 2 - 8 * L.text_scale,
             "NOTHING DRAWN YET", PC_INK_FAINT, L.text_scale);

    /* Scrollbar: only when there is more than fits, so it does not nag. */
    {
        int shown = tc_ui_visible_rows(ui, W, H);
        if (ui->nlog > shown && shown > 0) {
            int track_h = L.log_h - 8;
            int bar_h = track_h * shown / ui->nlog;
            int maxs = ui->nlog - shown;
            int off = maxs > 0 ? (track_h - bar_h) * (maxs - ui->scroll) / maxs : 0;
            if (bar_h < 10) bar_h = 10;
            /* The bar alone says it; a text hint here overlapped the top row. */
            fill(rgba, W, H, W - 5, top + 4, 3, track_h, PC_PANEL);
            round_rect(rgba, W, H, W - 6, top + 4 + off, 5, bar_h, 2, PC_INK_FAINT);
        }
    }
    fill(rgba, W, H, 0, bottom - 1, W, 1, PC_LINE);
}

static void render_canvas_and_tools(const tc_ui *ui, uint8_t *rgba, int W, int H,
                                    tc_layout L) {
    tc_rect cv;
    int i;

    cv.x = 0; cv.y = L.bar_h + L.log_h;
    cv.w = W; cv.h = H - L.bar_h - L.log_h - L.tool_h;

    fill(rgba, W, H, cv.x, cv.y, cv.w, cv.h, PC_PAGE);
    if (ui->canvas) {
        uint8_t *sub = rgba + ((size_t)cv.y * W) * 4;
        tc_canvas_render(ui->canvas, sub, W, cv.h);
    }

    vgrad(rgba, W, H, 0, H - L.tool_h, W, L.tool_h, PC_CHROME_HI, PC_CHROME);
    fill(rgba, W, H, 0, H - L.tool_h, W, 1, PC_LINE);

    for (i = 0; i < 16; i++) {
        tc_rect r = tc_ui_swatch_rect(W, H, i);
        round_rect(rgba, W, H, r.x, r.y, r.w, r.h, 3, TC_PALETTE[i]);
        if (i == ui->pal_index) {
            int g = L.dual ? 1 : 2;
            round_rect(rgba, W, H, r.x - g - 1, r.y - g - 1,
                       r.w + 2 * g + 2, r.h + 2 * g + 2, 4, PC_ACCENT);
            round_rect(rgba, W, H, r.x, r.y, r.w, r.h, 3, TC_PALETTE[i]);
        } else {
            frame(rgba, W, H, r.x, r.y, r.w, r.h, 0x000000 | 0x6b7280);
        }
    }
    for (i = 0; i < 6; i++) {
        tc_rect r = tc_ui_pen_rect(W, H, i);
        int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
        int rad = (int)TC_PEN_RADIUS[i], dx, dy;
        if (L.dual && rad > 4) rad = 4;
        round_rect(rgba, W, H, r.x, r.y, r.w, r.h, 3,
                   i == ui->pen_index ? PC_PAGE : PC_PANEL);
        frame(rgba, W, H, r.x, r.y, r.w, r.h,
              i == ui->pen_index ? PC_ACCENT : PC_DIVIDER);
        for (dy = -rad; dy <= rad; dy++)
            for (dx = -rad; dx <= rad; dx++)
                if (dx * dx + dy * dy <= rad * rad) px_set(rgba, W, H, cx + dx, cy + dy, PC_INK);
        if (rad == 0) px_set(rgba, W, H, cx, cy, PC_INK);
    }
    {
        tc_rect r = tc_ui_room_rect(W, H);
        char lab[8];
        int lw;
        lab[0] = 'R'; lab[1] = 'M'; lab[2] = ' ';
        lab[3] = (ui->room >= 'A' && ui->room <= 'D') ? ui->room : '-';
        lab[4] = '\0';
        if (L.dual) { lab[0] = lab[3]; lab[1] = '\0'; }
        lw = (int)strlen(lab) * 8 * L.text_scale;
        round_rect(rgba, W, H, r.x, r.y, r.w, r.h, 3, PC_PAGE);
        frame(rgba, W, H, r.x, r.y, r.w, r.h, PC_LINE);
        text(rgba, W, H, r.x + (r.w - lw) / 2, r.y + (r.h - 16 * L.text_scale) / 2,
             lab, PC_ACCENT, L.text_scale);
    }
    {
        static const char *Lb[3] = { "UNDO", "CLEAR", "SEND" };
        static const char *Ls[3] = { "UN", "CL", "GO" };
        for (i = 0; i < 3; i++) {
            tc_rect r = tc_ui_button_rect(W, H, i);
            const char *lab = L.dual ? Ls[i] : Lb[i];
            uint32_t bgc = (i == 2) ? PC_ACCENT : PC_PAGE;
            uint32_t ink = (i == 2) ? 0xffffff : PC_INK;
            round_rect(rgba, W, H, r.x, r.y, r.w, r.h, 4, bgc);
            frame(rgba, W, H, r.x, r.y, r.w, r.h, (i == 2) ? PC_ACCENT : PC_LINE);
            text(rgba, W, H, r.x + r.w / 2 - (int)strlen(lab) * 4 * L.text_scale,
                 r.y + (r.h - 16 * L.text_scale) / 2, lab, ink, L.text_scale);
        }
    }
}

void tc_ui_render_top(const tc_ui *ui, uint8_t *rgba, int W, int H) {
    tc_layout L = tc_ui_layout(W, H);
    L.bar_h = 24; L.log_h = H - 24;
    L.thumb_w = 100; L.thumb_h = 58; L.thumb_gap = 8;
    fill(rgba, W, H, 0, 0, W, H, PC_PAGE);
    render_bar_and_log(ui, rgba, W, H, L, 8);
}

void tc_ui_render_bottom(const tc_ui *ui, uint8_t *rgba, int W, int H) {
    tc_layout L = tc_ui_layout(W, H);
    fill(rgba, W, H, 0, 0, W, H, PC_PAGE);
    render_canvas_and_tools(ui, rgba, W, H, L);
}

void tc_ui_render(const tc_ui *ui, uint8_t *rgba, int W, int H) {
    tc_layout L = tc_ui_layout(W, H);
    fill(rgba, W, H, 0, 0, W, H, PC_PAGE);
    render_bar_and_log(ui, rgba, W, H, L, L.compact ? 12 : 16);
    render_canvas_and_tools(ui, rgba, W, H, L);
}
