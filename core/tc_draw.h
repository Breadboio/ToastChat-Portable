#ifndef TC_DRAW_H
#define TC_DRAW_H
#include <stdint.h>

/* Strokes, not pixels: undo is free and the send path rasterises once.
 * Fixed capacity so a 16MB Dreamcast can reason about the cost up front. */
typedef struct { int16_t x, y; } tc_pt;
typedef struct { uint32_t rgb; float radius; uint16_t first, count; } tc_stroke;

typedef struct {
    tc_pt     *pts;    uint16_t npts,    cap_pts;
    tc_stroke *strokes; uint16_t nstrokes, cap_strokes;
    int        active;              /* a stroke is currently being drawn */
} tc_canvas;

int  tc_canvas_init(tc_canvas *c, uint16_t max_pts, uint16_t max_strokes);
void tc_canvas_free(tc_canvas *c);

void tc_canvas_begin(tc_canvas *c, int x, int y, uint32_t rgb, float radius);
void tc_canvas_to(tc_canvas *c, int x, int y);
void tc_canvas_end(tc_canvas *c);
void tc_canvas_undo(tc_canvas *c);
void tc_canvas_clear(tc_canvas *c);
int  tc_canvas_empty(const tc_canvas *c);

/* Renders anti-aliased, source-over, onto whatever is already in `rgba`.
 * Caller clears first if it wants a fresh page. */
void tc_canvas_render(const tc_canvas *c, uint8_t *rgba, int w, int h);
#endif
