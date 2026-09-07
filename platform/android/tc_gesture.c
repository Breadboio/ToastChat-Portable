#include "tc_gesture.h"
#include "../../core/tc_ui.h"
#include <string.h>

void tc_gesture_init(tc_gesture *g, int surf_w, int surf_h, int lw, int lh) {
    tc_rect cv = tc_ui_canvas_rect(lw, lh);
    memset(g, 0, sizeof(*g));
    /* Guard the divisors: a zero-sized surface has been reported on some
     * launch paths, and a divide by it would take the whole app down. */
    g->surf_w = surf_w > 0 ? surf_w : 1;
    g->surf_h = surf_h > 0 ? surf_h : 1;
    g->logical_w = lw;
    g->logical_h = lh;
    g->canvas_y = cv.y;
}

void tc_gesture_event(tc_gesture *g, int action, float sx, float sy) {
    /* Touch arrives in SURFACE px. The window buffer is the logical size and
     * the compositor scales it, so the two spaces differ by exactly this
     * ratio. Skipping it is invisible on a 1:1 emulator and completely wrong
     * on a real phone. */
    /* Round, do not truncate. Truncation biases every coordinate down by up
     * to a logical pixel, which at this downscale is ~1.7 surface px of
     * systematic error - enough to lose a row off the end of a drag. */
    int x = (int)(sx * (float)g->logical_w / (float)g->surf_w + 0.5f);
    int y = (int)(sy * (float)g->logical_h / (float)g->surf_h + 0.5f);

    if (x < 0) x = 0; else if (x >= g->logical_w) x = g->logical_w - 1;
    if (y < 0) y = 0; else if (y >= g->logical_h) y = g->logical_h - 1;

    if (action == TC_G_DOWN) {
        if (y < g->canvas_y) {
            /* Started in the bar/log band: this is a scroll, and the pointer
             * never sees it. That is also what stops a flick from landing on
             * a toolbar button when it crosses one. */
            g->scrolling = 1; g->accum = 0; g->last_y = y;
            g->ptr.down = 0;
        } else {
            g->scrolling = 0;
            g->ptr.x = x; g->ptr.y = y; g->ptr.down = 1;
        }
    } else if (action == TC_G_MOVE) {
        if (g->scrolling) {
            g->accum += y - g->last_y;
            g->last_y = y;
            /* Dragging DOWN reveals older rows, which is scroll -1. */
            while (g->accum >= TC_SCROLL_STEP) { g->accum -= TC_SCROLL_STEP; g->ticks--; }
            while (g->accum <= -TC_SCROLL_STEP) { g->accum += TC_SCROLL_STEP; g->ticks++; }
        } else if (g->ptr.down) {
            g->ptr.x = x; g->ptr.y = y;
        }
    } else if (action == TC_G_UP) {
        /* x/y are deliberately kept: the stroke model reads the position on
         * the release edge. */
        g->ptr.down = 0;
        g->scrolling = 0;
    }
}

int tc_gesture_scroll(tc_gesture *g) {
    if (g->ticks > 0) { g->ticks--; return  1; }
    if (g->ticks < 0) { g->ticks++; return -1; }
    return 0;
}
