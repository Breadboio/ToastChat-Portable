#ifndef TC_UI_H
#define TC_UI_H
#include <stdint.h>
#include "tc_draw.h"

#define TC_UI_LOG_MAX 16   /* scrollback depth; also the image-pool size */

typedef struct {
    char           nick[12];
    uint32_t       color;        /* the sender's DS colour, for the name chip */
    int            is_sys;       /* system line: render as an italic notice   */
    char           text[40];     /* system line text                          */
    int            w, h;         /* pixels actually used inside the buffer   */
    int            stride;       /* buffer row width; >= w (thumbnails are
                                  * decoded into a fixed-size box)           */
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
    int          scroll;      /* rows skipped from the newest end; 0 = bottom */
} tc_ui;

/* Screen regions, so input hit-testing and drawing agree on one source. */
typedef struct { int x, y, w, h; } tc_rect;

/* One layout computed from the screen size. Wide = Switch/desktop (1280x720),
 * compact = Wii/Dreamcast (640x480), where 16 swatches in a row do not fit. */
typedef struct {
    int compact, dual;
    int bar_h, log_h, tool_h;
    int sw_size, sw_pitch, sw_x, sw_y, sw_cols;
    int pen_size, pen_pitch, pen_x, pen_y;
    int btn_w, btn_h, btn_x, btn_y, btn_pitch;
    int thumb_w, thumb_h, thumb_gap;
} tc_layout;
tc_layout tc_ui_layout(int W, int H);
tc_rect tc_ui_canvas_rect(int W, int H);
tc_rect tc_ui_swatch_rect(int W, int H, int i);
tc_rect tc_ui_pen_rect(int W, int H, int i);
tc_rect tc_ui_button_rect(int W, int H, int i);   /* 0=UNDO 1=CLEAR 2=SEND */
tc_rect tc_ui_room_rect(int W, int H);            /* tap to cycle A-B-C-D    */

/* How many rows the log can show, so the app can clamp scrolling. */
int     tc_ui_visible_rows(const tc_ui *ui, int W, int H);

void tc_ui_render(const tc_ui *ui, uint8_t *rgba, int W, int H);

/* Dual-screen platforms (3DS) draw the two halves separately: the top screen
 * carries status + the log, the bottom screen carries canvas + toolbar.
 * A layout with W < 400 is automatically the bottom half of a dual-screen
 * device, so tc_ui_canvas_rect()/swatch/pen/button already return
 * bottom-screen coordinates and hit-testing needs no special case. */
void tc_ui_render_top(const tc_ui *ui, uint8_t *rgba, int W, int H);
void tc_ui_render_bottom(const tc_ui *ui, uint8_t *rgba, int W, int H);
extern const uint32_t TC_PALETTE[16];
extern const float    TC_PEN_RADIUS[6];
#endif
