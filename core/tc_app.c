#include "tc_app.h"
#include "tc_ws.h"
#include "tc_ui.h"
#include "tc_draw.h"
#include "tc_send.h"
#include "../platform/platform.h"
#include <string.h>
#include <stdio.h>

static int hit(tc_rect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

int tc_app_run(int W, int H, const char *host, int port, const char *room,
               const char *nick, const char *hint) {
    uint8_t *screen, *screen_top = NULL;
    int topw = 0, toph = 0, dual;
    tc_canvas canvas;
    tc_ui ui;
    tc_ws ws;
    tc_rect cv;
    tc_pointer p, prev;
    int connected = 0, drawing = 0;
    char status[48];
    char m[160];

    memset(&ui, 0, sizeof(ui));
    memset(&prev, 0, sizeof(prev));
    memset(&ws, 0, sizeof(ws));

    dual = (W < 400);
    screen = (uint8_t *)tc_alloc((size_t)W * H * 4);
    if (!screen) return 1;
    if (dual) {
        tc_video_size(TC_SCREEN_TOP, &topw, &toph);
        screen_top = (uint8_t *)tc_alloc((size_t)topw * toph * 4);
        if (!screen_top) { tc_free(screen); return 1; }
    }
    if (tc_canvas_init(&canvas, 16000, 256) != 0) { tc_free(screen); return 1; }

    cv = tc_ui_canvas_rect(W, H);
    ui.room = '-'; ui.max = 16; ui.pal_index = 8; ui.pen_index = 3;
    ui.canvas = &canvas; ui.status = status;
    strcpy(status, "CONNECTING");

    if (tc_ws_connect(&ws, host, port, "/ws") != 0) {
        strcpy(status, "CONNECT FAILED");
    } else {
        connected = 1;
        snprintf(m, sizeof(m), "{\"t\":\"identify\",\"nick\":\"%s\",\"color\":\"#2fa89a\"}", nick);
        tc_ws_send_text(&ws, m, strlen(m));
        snprintf(m, sizeof(m), "{\"t\":\"join\",\"room\":\"%s\"}", room);
        tc_ws_send_text(&ws, m, strlen(m));
        ui.room = room[0];
        strncpy(status, hint, sizeof(status) - 1);
        status[sizeof(status) - 1] = '\0';
    }
    ui.connected = connected;

    while (!tc_input_quit()) {
        int i;
        tc_input_poll();
        tc_input_pointer(&p);

        if (connected) {
            int r = tc_ws_poll(&ws);
            if (r < 0) { connected = 0; ui.connected = 0; strcpy(status, "DISCONNECTED"); }
            else if (r == 1) {
                const char *s = (const char *)ws.msg;
                if (strstr(s, "\"t\":\"joined\""))     strcpy(status, "JOINED");
                else if (strstr(s, "\"t\":\"entry\"")) strcpy(status, "NEW DRAWING");
                else if (strstr(s, "\"t\":\"error\"")) strcpy(status, "SERVER SAID NO");
                ws.msg_len = 0;
            }
        }

        if (p.down && !prev.down) {
            if (hit(cv, p.x, p.y)) {
                tc_canvas_begin(&canvas, p.x - cv.x, p.y - cv.y,
                                TC_PALETTE[ui.pal_index], TC_PEN_RADIUS[ui.pen_index]);
                drawing = 1;
            } else {
                for (i = 0; i < 16; i++)
                    if (hit(tc_ui_swatch_rect(W, H, i), p.x, p.y)) ui.pal_index = i;
                for (i = 0; i < 6; i++)
                    if (hit(tc_ui_pen_rect(W, H, i), p.x, p.y)) ui.pen_index = i;
                if (hit(tc_ui_button_rect(W, H, 0), p.x, p.y)) tc_canvas_undo(&canvas);
                if (hit(tc_ui_button_rect(W, H, 1), p.x, p.y)) tc_canvas_clear(&canvas);
                if (hit(tc_ui_button_rect(W, H, 2), p.x, p.y)) {
                    if (!connected)                  strcpy(status, "NOT CONNECTED");
                    else if (tc_canvas_empty(&canvas)) strcpy(status, "NOTHING TO SEND");
                    else {
                        /* Shrink harder on small screens: the canvas is already
                         * small and the ws payload cliff is unforgiving. */
                        int rc = tc_send_canvas(&ws, &canvas, cv.w, cv.h, W < 900 ? 1 : 2);
                        if (rc == 0)        { tc_canvas_clear(&canvas); strcpy(status, "SENT"); }
                        else if (rc == -10) strcpy(status, "TOO BIG");
                        else                strcpy(status, "SEND FAILED");
                    }
                }
            }
        } else if (p.down && drawing) {
            tc_canvas_to(&canvas, p.x - cv.x, p.y - cv.y);
        } else if (!p.down && prev.down && drawing) {
            tc_canvas_end(&canvas);
            drawing = 0;
        }
        prev = p;

        if (dual) {
            tc_ui_render_bottom(&ui, screen, W, H);
            tc_ui_render_top(&ui, screen_top, topw, toph);
            tc_video_blit(TC_SCREEN_TOP, screen_top, topw, toph);
            tc_video_blit(TC_SCREEN_BOTTOM, screen, W, H);
        } else {
            tc_ui_render(&ui, screen, W, H);
            tc_video_blit(TC_SCREEN_TOP, screen, W, H);
        }
        tc_video_present();
    }

    if (connected) tc_ws_close(&ws);
    tc_canvas_free(&canvas);
    tc_free(screen);
    tc_free(screen_top);
    return 0;
}
