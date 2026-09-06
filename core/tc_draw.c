#include "tc_draw.h"
#include "../platform/platform.h"
#include <string.h>
#include <math.h>

int tc_canvas_init(tc_canvas *c, uint16_t max_pts, uint16_t max_strokes) {
    memset(c, 0, sizeof(*c));
    c->pts = (tc_pt *)tc_alloc((size_t)max_pts * sizeof(tc_pt));
    c->strokes = (tc_stroke *)tc_alloc((size_t)max_strokes * sizeof(tc_stroke));
    if (!c->pts || !c->strokes) { tc_canvas_free(c); return -1; }
    c->cap_pts = max_pts; c->cap_strokes = max_strokes;
    return 0;
}
void tc_canvas_free(tc_canvas *c) {
    tc_free(c->pts); tc_free(c->strokes); memset(c, 0, sizeof(*c));
}
void tc_canvas_clear(tc_canvas *c) { c->npts = 0; c->nstrokes = 0; c->active = 0; }
int  tc_canvas_empty(const tc_canvas *c) { return c->nstrokes == 0; }

void tc_canvas_begin(tc_canvas *c, int x, int y, uint32_t rgb, float radius) {
    if (c->nstrokes >= c->cap_strokes || c->npts >= c->cap_pts) return;
    c->strokes[c->nstrokes].rgb = rgb;
    c->strokes[c->nstrokes].radius = radius;
    c->strokes[c->nstrokes].first = c->npts;
    c->strokes[c->nstrokes].count = 1;
    c->pts[c->npts].x = (int16_t)x; c->pts[c->npts].y = (int16_t)y;
    c->npts++; c->nstrokes++; c->active = 1;
}
void tc_canvas_to(tc_canvas *c, int x, int y) {
    tc_stroke *s;
    if (!c->active || c->npts >= c->cap_pts) return;
    s = &c->strokes[c->nstrokes - 1];
    /* Drop duplicate samples: a resistive/capacitive screen repeats a lot. */
    if (s->count && c->pts[c->npts - 1].x == (int16_t)x && c->pts[c->npts - 1].y == (int16_t)y) return;
    c->pts[c->npts].x = (int16_t)x; c->pts[c->npts].y = (int16_t)y;
    c->npts++; s->count++;
}
void tc_canvas_end(tc_canvas *c) { c->active = 0; }
void tc_canvas_undo(tc_canvas *c) {
    if (!c->nstrokes) return;
    c->nstrokes--;
    c->npts = c->strokes[c->nstrokes].first;
    c->active = 0;
}

/* squared distance from p to segment ab */
static float seg_d2(float px, float py, float ax, float ay, float bx, float by) {
    float vx = bx - ax, vy = by - ay, wx = px - ax, wy = py - ay;
    float c1 = vx * wx + vy * wy, c2, t, dx, dy;
    if (c1 <= 0.0f) return wx * wx + wy * wy;
    c2 = vx * vx + vy * vy;
    if (c2 <= c1) { dx = px - bx; dy = py - by; return dx * dx + dy * dy; }
    t = c1 / c2;
    dx = px - (ax + t * vx); dy = py - (ay + t * vy);
    return dx * dx + dy * dy;
}

static void blend(uint8_t *p, uint32_t rgb, float a) {
    float ia;
    if (a <= 0.0f) return;
    if (a > 1.0f) a = 1.0f;
    ia = 1.0f - a;
    p[0] = (uint8_t)(((rgb >> 16) & 0xFF) * a + p[0] * ia);
    p[1] = (uint8_t)(((rgb >> 8) & 0xFF) * a + p[1] * ia);
    p[2] = (uint8_t)((rgb & 0xFF) * a + p[2] * ia);
    p[3] = (uint8_t)(255.0f * a + p[3] * ia);
}

void tc_canvas_render(const tc_canvas *c, uint8_t *rgba, int w, int h) {
    uint16_t si;
    for (si = 0; si < c->nstrokes; si++) {
        const tc_stroke *s = &c->strokes[si];
        float r = s->radius, r2;
        uint16_t k;
        if (r < 0.5f) r = 0.5f;
        r2 = r + 0.7071f;                       /* AA reach: half a pixel diag */
        /* A 1-point stroke is a dot: render it as a zero-length segment. */
        for (k = 0; k + 1 < s->count || (s->count == 1 && k == 0); k++) {
            const tc_pt *a = &c->pts[s->first + k];
            const tc_pt *b = (s->count == 1) ? a : &c->pts[s->first + k + 1];
            int x0 = (int)((a->x < b->x ? a->x : b->x) - r2 - 1);
            int y0 = (int)((a->y < b->y ? a->y : b->y) - r2 - 1);
            int x1 = (int)((a->x > b->x ? a->x : b->x) + r2 + 1);
            int y1 = (int)((a->y > b->y ? a->y : b->y) + r2 + 1);
            int px, py;
            if (x0 < 0) x0 = 0;
            if (y0 < 0) y0 = 0;
            if (x1 > w - 1) x1 = w - 1;
            if (y1 > h - 1) y1 = h - 1;
            for (py = y0; py <= y1; py++) {
                for (px = x0; px <= x1; px++) {
                    float d2 = seg_d2((float)px + 0.5f, (float)py + 0.5f,
                                      (float)a->x, (float)a->y, (float)b->x, (float)b->y);
                    float d = d2 > 0.0f ? sqrtf(d2) : 0.0f;
                    float cov = r + 0.5f - d;
                    if (cov > 0.0f) blend(rgba + ((size_t)py * w + px) * 4, s->rgb, cov);
                }
            }
            if (s->count == 1) break;
        }
    }
}
