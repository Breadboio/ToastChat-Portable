#ifndef TC_UI_H
#define TC_UI_H
#include <stdint.h>
#include "tc_draw.h"

#define TC_UI_LOG_MAX 6

typedef struct {
    char           nick[12];
    int            w, h;
    const uint8_t *rgba;         /* decoded thumbnail, or NULL for a placeholder */
} tc_ui_entry;

typedef struct {
    int          connected;
    char         room;           /* 'A'..'D', or '-' for lobby */
    int          people, max;
    const char  *status;
    int          pal_index;      /* selected colour 0..15 */
    int          pen_index;      /* selected width 0..5   */
    const tc_canvas *canvas;
    tc_ui_entry  log[TC_UI_LOG_MAX];
    int          nlog;
} tc_ui;

/* Screen regions, so input hit-testing and drawing agree on one source. */
typedef struct { int x, y, w, h; } tc_rect;
tc_rect tc_ui_canvas_rect(int W, int H);
tc_rect tc_ui_swatch_rect(int W, int H, int i);
tc_rect tc_ui_pen_rect(int W, int H, int i);
tc_rect tc_ui_button_rect(int W, int H, int i);   /* 0=UNDO 1=CLEAR 2=SEND */

void tc_ui_render(const tc_ui *ui, uint8_t *rgba, int W, int H);
extern const uint32_t TC_PALETTE[16];
extern const float    TC_PEN_RADIUS[6];
#endif
