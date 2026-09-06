/* ToastChat for Nintendo Switch - touchscreen drawing client.
 * Everything visual is core/tc_ui.c, which the desktop target renders
 * identically, so layout is reviewed before it reaches hardware. */
#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../core/tc_ws.h"
#include "../../core/tc_ui.h"
#include "../../core/tc_draw.h"
#include "../../core/tc_send.h"

#ifndef TC_HOST
#define TC_HOST "192.168.1.167"
#endif
#ifndef TC_PORT
#define TC_PORT 3401
#endif
#ifndef TC_ROOM
#define TC_ROOM "C"
#endif

#define SW 1280
#define SH 720

void tc_switch_video_init(void);
void tc_switch_video_exit(void);
void tc_switch_input_init(void);

static int hit(tc_rect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

int main(int argc, char **argv) {
    uint8_t *screen;
    tc_canvas canvas;
    tc_ui ui;
    tc_ws ws;
    tc_rect cv;
    tc_pointer p, prev;
    int connected = 0, drawing = 0;
    char status[48];
    (void)argc; (void)argv;

    memset(&ui, 0, sizeof(ui));
    memset(&prev, 0, sizeof(prev));
    strcpy(status, "STARTING");

    screen = (uint8_t *)malloc((size_t)SW * SH * 4);
    if (!screen) return 1;
    tc_canvas_init(&canvas, 16000, 256);

    tc_switch_video_init();
    tc_switch_input_init();
    cv = tc_ui_canvas_rect(SW, SH);

    ui.room = '-'; ui.max = 16; ui.pal_index = 8; ui.pen_index = 3;
    ui.canvas = &canvas; ui.status = status;

    if (R_FAILED(socketInitializeDefault())) {
        strcpy(status, "NO SOCKETS");
    } else if (tc_ws_connect(&ws, TC_HOST, TC_PORT, "/ws") != 0) {
        strcpy(status, "CONNECT FAILED");
    } else {
        char m[128];
        connected = 1;
        snprintf(m, sizeof(m), "{\"t\":\"identify\",\"nick\":\"Switch\",\"color\":\"#2fa89a\"}");
        tc_ws_send_text(&ws, m, strlen(m));
        snprintf(m, sizeof(m), "{\"t\":\"join\",\"room\":\"%s\"}", TC_ROOM);
        tc_ws_send_text(&ws, m, strlen(m));
        ui.room = TC_ROOM[0];
        strcpy(status, "TOUCH TO DRAW");
    }
    ui.connected = connected;

    while (appletMainLoop() && !tc_input_quit()) {
        int i;
        tc_input_poll();
        tc_input_pointer(&p);

        /* ---- network ---- */
        if (connected) {
            int r = tc_ws_poll(&ws);
            if (r < 0) { connected = 0; ui.connected = 0; strcpy(status, "DISCONNECTED"); }
            else if (r == 1) {
                const char *m = (const char *)ws.msg;
                if (strstr(m, "\"t\":\"joined\"")) strcpy(status, "JOINED");
                else if (strstr(m, "\"t\":\"entry\"")) strcpy(status, "NEW DRAWING");
                else if (strstr(m, "\"t\":\"error\"")) strcpy(status, "SERVER SAID NO");
                ws.msg_len = 0;
            }
        }

        /* ---- pointer ---- */
        if (p.down && !prev.down) {                     /* press */
            if (hit(cv, p.x, p.y)) {
                tc_canvas_begin(&canvas, p.x - cv.x, p.y - cv.y,
                                TC_PALETTE[ui.pal_index], TC_PEN_RADIUS[ui.pen_index]);
                drawing = 1;
            } else {
                for (i = 0; i < 16; i++)
                    if (hit(tc_ui_swatch_rect(SW, SH, i), p.x, p.y)) ui.pal_index = i;
                for (i = 0; i < 6; i++)
                    if (hit(tc_ui_pen_rect(SW, SH, i), p.x, p.y)) ui.pen_index = i;
                if (hit(tc_ui_button_rect(SW, SH, 0), p.x, p.y)) tc_canvas_undo(&canvas);
                if (hit(tc_ui_button_rect(SW, SH, 1), p.x, p.y)) tc_canvas_clear(&canvas);
                if (hit(tc_ui_button_rect(SW, SH, 2), p.x, p.y)) {
                    if (!connected) strcpy(status, "NOT CONNECTED");
                    else if (tc_canvas_empty(&canvas)) strcpy(status, "NOTHING TO SEND");
                    else {
                        int rc = tc_send_canvas(&ws, &canvas, cv.w, cv.h, 2);
                        if (rc == 0)        { tc_canvas_clear(&canvas); strcpy(status, "SENT"); }
                        else if (rc == -10) strcpy(status, "TOO BIG");
                        else                strcpy(status, "SEND FAILED");
                    }
                }
            }
        } else if (p.down && drawing) {                 /* drag */
            tc_canvas_to(&canvas, p.x - cv.x, p.y - cv.y);
        } else if (!p.down && prev.down && drawing) {   /* release */
            tc_canvas_end(&canvas);
            drawing = 0;
        }
        prev = p;

        tc_ui_render(&ui, screen, SW, SH);
        tc_video_blit(TC_SCREEN_TOP, screen, SW, SH);
        tc_video_present();
    }

    if (connected) tc_ws_close(&ws);
    socketExit();
    tc_switch_video_exit();
    tc_canvas_free(&canvas);
    free(screen);
    return 0;
}
