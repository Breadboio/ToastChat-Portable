/* Drives the Android port's touch handling on x86. There is no phone and no
 * emulator on this box (no /dev/kvm, no /dev/binder), so this is how the one
 * piece of real logic in the port gets exercised before it ships.
 *
 * Inputs are deliberately REALISTIC: a drag is many small moves with fractional
 * surface coordinates, the way a digitiser actually reports, not two endpoints.
 */
#include "../platform/android/tc_gesture.h"
#include "../core/tc_ui.h"
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { \
    printf("FAIL %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

/* A 1080x2400 phone, which is what the logical size is derived from. */
#define SW 1080
#define SH 2400
#define LW 640
#define LH 1422

static void drag(tc_gesture *g, float x0, float y0, float x1, float y1, int steps) {
    int i;
    tc_gesture_event(g, TC_G_DOWN, x0, y0);
    for (i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        tc_gesture_event(g, TC_G_MOVE, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
    }
    tc_gesture_event(g, TC_G_UP, x1, y1);
}

static int drain(tc_gesture *g) {   /* net scroll, in rows */
    int n = 0, s;
    while ((s = tc_gesture_scroll(g)) != 0) n += s;
    return n;
}

int main(void) {
    tc_gesture g;
    tc_rect cv   = tc_ui_canvas_rect(LW, LH);
    tc_rect send = tc_ui_button_rect(LW, LH, 2);
    tc_layout L  = tc_ui_layout(LW, LH);

    printf("layout: tall=%d compact=%d bar=%d log=%d tool=%d canvas y=%d h=%d\n",
           L.tall, L.compact, L.bar_h, L.log_h, L.tool_h, cv.y, cv.h);
    CHECK(L.tall == 1, "a 640x1422 phone must take the tall branch");
    CHECK(cv.h > 0 && cv.y > 0, "canvas must be non-empty");

    /* --- 1. surface -> logical mapping ---------------------------------- */
    tc_gesture_init(&g, SW, SH, LW, LH);
    tc_gesture_event(&g, TC_G_DOWN, SW / 2.0f, SH / 2.0f);
    CHECK(g.ptr.down == 1, "centre of the screen is canvas, so pointer is down");
    CHECK(g.ptr.x >= LW/2 - 2 && g.ptr.x <= LW/2 + 2, "x centre mapped to %d, want ~%d", g.ptr.x, LW/2);
    CHECK(g.ptr.y >= LH/2 - 2 && g.ptr.y <= LH/2 + 2, "y centre mapped to %d, want ~%d", g.ptr.y, LH/2);
    tc_gesture_event(&g, TC_G_UP, SW / 2.0f, SH / 2.0f);

    /* A raw pass-through (no scaling) would land here instead - the bug this
     * whole mapping exists to prevent. Prove the two differ. */
    CHECK(g.ptr.x != SW / 2, "unscaled coords must not be what we report");

    /* --- 2. tapping SEND actually hits SEND ----------------------------- */
    {
        float tx = (send.x + send.w / 2.0f) * (float)SW / (float)LW;
        float ty = (send.y + send.h / 2.0f) * (float)SH / (float)LH;
        tc_gesture_init(&g, SW, SH, LW, LH);
        tc_gesture_event(&g, TC_G_DOWN, tx, ty);
        CHECK(g.ptr.down == 1, "SEND tap must register as a pointer press");
        CHECK(g.ptr.x >= send.x && g.ptr.x < send.x + send.w &&
              g.ptr.y >= send.y && g.ptr.y < send.y + send.h,
              "SEND tap landed at (%d,%d), rect is %d,%d %dx%d",
              g.ptr.x, g.ptr.y, send.x, send.y, send.w, send.h);
        tc_gesture_event(&g, TC_G_UP, tx, ty);
    }

    /* --- 3. the log band must not poke the UI ---------------------------- */
    tc_gesture_init(&g, SW, SH, LW, LH);
    tc_gesture_event(&g, TC_G_DOWN, SW / 2.0f, 100.0f);   /* well inside the log */
    CHECK(g.ptr.down == 0, "a touch in the log band must not press the pointer");
    CHECK(g.scrolling == 1, "a touch in the log band starts a scroll");
    tc_gesture_event(&g, TC_G_UP, SW / 2.0f, 100.0f);
    CHECK(g.scrolling == 0, "release ends the scroll gesture");

    /* --- 4. a realistic flick scrolls the right way and the right amount - */
    {
        /* Drag DOWN by 240 logical px = 5 rows of older history. */
        float y0 = 120.0f * (float)SH / (float)LH;
        float y1 = (120.0f + 240.0f) * (float)SH / (float)LH;
        int rows;
        tc_gesture_init(&g, SW, SH, LW, LH);
        drag(&g, SW / 2.0f, y0, SW / 2.0f, y1, 40);
        rows = drain(&g);
        CHECK(rows == -5, "240px drag down should be -5 rows (older), got %d", rows);
        CHECK(g.ptr.down == 0, "a scroll must never leave the pointer pressed");

        /* And the opposite direction. */
        tc_gesture_init(&g, SW, SH, LW, LH);
        drag(&g, SW / 2.0f, y1, SW / 2.0f, y0, 40);
        rows = drain(&g);
        CHECK(rows == 5, "240px drag up should be +5 rows (newer), got %d", rows);
    }

    /* --- 5. a stroke on the canvas ------------------------------------- */
    {
        float y0 = (cv.y + 40.0f) * (float)SH / (float)LH;
        float y1 = (cv.y + 300.0f) * (float)SH / (float)LH;
        int moved = 0, i;
        tc_gesture_init(&g, SW, SH, LW, LH);
        tc_gesture_event(&g, TC_G_DOWN, 200.0f, y0);
        CHECK(g.ptr.down == 1, "stroke must press");
        for (i = 1; i <= 30; i++) {
            int px = g.ptr.x, py = g.ptr.y;
            float t = (float)i / 30.0f;
            tc_gesture_event(&g, TC_G_MOVE, 200.0f + 400.0f * t, y0 + (y1 - y0) * t);
            if (g.ptr.x != px || g.ptr.y != py) moved++;
            CHECK(g.ptr.down == 1, "pointer must stay down through the stroke");
        }
        CHECK(moved > 20, "the stroke should report movement, moved %d of 30", moved);
        CHECK(drain(&g) == 0, "a canvas stroke must not generate scroll");
        {
            int lx = g.ptr.x, ly = g.ptr.y;
            tc_gesture_event(&g, TC_G_UP, 600.0f, y1);
            CHECK(g.ptr.down == 0, "release must clear down");
            CHECK(g.ptr.x == lx && g.ptr.y == ly,
                  "release must keep the last position (%d,%d) not (%d,%d)",
                  lx, ly, g.ptr.x, g.ptr.y);
        }
    }

    /* --- 6. a scroll that crosses the toolbar stays a scroll ------------- */
    {
        float y0 = 100.0f * (float)SH / (float)LH;
        float y1 = (float)(LH - 20) * (float)SH / (float)LH;   /* over SEND */
        tc_gesture_init(&g, SW, SH, LW, LH);
        drag(&g, SW / 2.0f, y0, SW / 2.0f, y1, 60);
        CHECK(g.ptr.down == 0, "a flick ending on the toolbar must not press a button");
    }

    /* --- 7. nothing escapes the buffer ---------------------------------- */
    tc_gesture_init(&g, SW, SH, LW, LH);
    tc_gesture_event(&g, TC_G_DOWN, -50.0f, (float)SH + 999.0f);
    CHECK(g.ptr.x >= 0 && g.ptr.x < LW, "x clamped, got %d", g.ptr.x);
    CHECK(g.ptr.y >= 0 && g.ptr.y < LH, "y clamped, got %d", g.ptr.y);

    /* --- 8. a zero-sized surface must not divide by zero ----------------- */
    tc_gesture_init(&g, 0, 0, LW, LH);
    tc_gesture_event(&g, TC_G_DOWN, 10.0f, 10.0f);
    CHECK(g.ptr.x >= 0 && g.ptr.x < LW, "degenerate surface survived");

    /* --- 9. ticks drain one row at a time -------------------------------- */
    tc_gesture_init(&g, SW, SH, LW, LH);
    g.ticks = 3;
    CHECK(tc_gesture_scroll(&g) == 1 && tc_gesture_scroll(&g) == 1 &&
          tc_gesture_scroll(&g) == 1 && tc_gesture_scroll(&g) == 0,
          "scroll must hand back one row per call, then stop");

    printf(fails ? "\n%d CHECK(s) FAILED\n" : "\nall gesture checks passed\n", fails);
    return fails ? 1 : 0;
}
