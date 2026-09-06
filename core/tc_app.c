#include "tc_app.h"
#include "tc_ws.h"
#include "tc_ui.h"
#include "tc_draw.h"
#include "tc_send.h"
#include "tc_json.h"
#include "tc_imgdec.h"
#include "tc_recv.h"
#include "tc_keyboard.h"
#include "tc_lobby.h"
#include "../platform/platform.h"
#include <string.h>
#include <stdio.h>

/* {"rooms":{"A":0,"B":2,...}} -> counts[4]. tc_json_top stops at the end of the
 * object it is handed, so the nested object can be scanned directly. */
static void parse_counts(const char *msg, int counts[4]) {
    const char *rooms = tc_json_top(msg, "rooms");
    int i;
    if (!rooms) return;
    for (i = 0; i < 4; i++) {
        char key[2];
        key[0] = (char)('A' + i); key[1] = '\0';
        counts[i] = tc_json_top_int(rooms, key, counts[i]);
    }
}

static int hit(tc_rect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

#define TC_NICK_LIMIT 10   /* server truncates past this */
#define TC_BUFFERS     2   /* 3DS/Wii/Switch all double-buffer */

int tc_app_run(int W, int H, const char *host, int port, int use_tls,
               const char *nick, const char *hint) {
    /* Behind unified-nginx the app is mounted at /toastchat/ and the proxy
     * strips the prefix, so the browser (and we) must ask for
     * /toastchat/ws. Straight to the container it is just /ws. */
    const char *path = use_tls ? "/toastchat/ws" : "/ws";
    uint8_t *screen, *screen_top = NULL, *thumbs = NULL;
    int topw = 0, toph = 0, dual;
    tc_canvas canvas;
    tc_ui ui;
    tc_ws ws;
    tc_rect cv;
    tc_pointer p, prev;
    int connected = 0, drawing = 0;
    /* Frames still owed a redraw. Consoles are double-buffered, so ONE draw
     * after a change updates only one of the two buffers; the other still holds
     * the previous frame. Anything that then swaps shows them alternately -
     * which is exactly the "top screen flickers until I draw again" report.
     * Drawing TC_BUFFERS times after every change makes both buffers current,
     * so it cannot matter what any platform's present() does. */
    int redraw = TC_BUFFERS;
    uint32_t last_scroll = 0;
    int counts[4];
    int in_lobby = 1;            /* pick a room before joining one */
    char status[48];
    char m[160];
    char nickbuf[TC_NICK_LIMIT + 1];

    memset(&ui, 0, sizeof(ui));
    memset(&prev, 0, sizeof(prev));
    memset(&ws, 0, sizeof(ws));
    counts[0] = counts[1] = counts[2] = counts[3] = 0;

    /* Fixed thumbnail pool: newest TC_UI_LOG_MAX drawings, nothing else kept.
     * Bounded by construction, so a busy room cannot grow our footprint. */
    thumbs = (uint8_t *)tc_alloc(TC_POOL_BYTES);
    if (!thumbs) return 1;

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

    /* --- pick a name before connecting ---
     * TC_AUTONAME skips this and accepts the default. It exists so the headless
     * emulator smoke test can still exercise connect/receive, which needs no
     * keyboard; it is never set for a release build. */
#ifdef TC_AUTONAME
    strncpy(nickbuf, nick, TC_NICK_LIMIT);
    nickbuf[TC_NICK_LIMIT] = '\0';
#else
    {
        char chosen[TC_NICK_LIMIT + 1];
        tc_pointer kp, kprev;
        int done = 0;
        memset(&kprev, 0, sizeof(kprev));
        strncpy(chosen, nick, TC_NICK_LIMIT);
        chosen[TC_NICK_LIMIT] = '\0';
        while (!done && !tc_input_quit()) {
            tc_input_poll();
            tc_input_pointer(&kp);
            if (kp.down && !kprev.down) {
                char k = tc_keyboard_key_at(W, H, kp.x, kp.y);
                if (k) done = tc_keyboard_apply(k, chosen, sizeof(chosen));
            }
            kprev = kp;
            tc_keyboard_render(screen, W, H, chosen, TC_NICK_LIMIT);
            if (dual) {
                tc_keyboard_render_top(screen_top, topw, toph, chosen);
                tc_video_blit(TC_SCREEN_TOP, screen_top, topw, toph);
                tc_video_blit(TC_SCREEN_BOTTOM, screen, W, H);
            } else {
                tc_video_blit(TC_SCREEN_TOP, screen, W, H);
            }
            tc_video_present();
        }
        if (*chosen) { strncpy(nickbuf, chosen, TC_NICK_LIMIT); nickbuf[TC_NICK_LIMIT] = '\0'; }
        else         { strncpy(nickbuf, nick, TC_NICK_LIMIT); nickbuf[TC_NICK_LIMIT] = '\0'; }
    }
#endif
    if (tc_input_quit()) goto cleanup;
    strcpy(status, "CONNECTING");

    if (tc_ws_connect(&ws, host, port, path, use_tls) != 0) {
        strcpy(status, "CONNECT FAILED");
    } else {
        connected = 1;
        snprintf(m, sizeof(m), "{\"t\":\"identify\",\"nick\":\"%s\",\"color\":\"#2fa89a\"}", nickbuf);
        tc_ws_send_text(&ws, m, strlen(m));
        /* No auto-join: the lobby picks the room. `room` is only the default
         * highlighted choice. */
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
            if (r < 0) { connected = 0; ui.connected = 0; strcpy(status, "DISCONNECTED"); redraw = TC_BUFFERS; }
            else if (r == 1) {
                const char *s = (const char *)ws.msg;
                const char *users;
                if (strstr(s, "\"t\":\"hello\"")) {
                    int mx = tc_json_top_int(s, "max", 0);
                    if (mx > 0) ui.max = mx;
                    parse_counts(s, counts);
                } else if (strstr(s, "\"t\":\"counts\"")) {
                    parse_counts(s, counts);
                } else if (strstr(s, "\"t\":\"joined\"")) {
                    strcpy(status, "JOINED");
                    users = tc_json_top(s, "users");
                    if (users) { int n = tc_json_array_len(users); if (n >= 0) ui.people = n; }
                    /* Replay the room's backlog oldest-first so the newest
                     * ends up at the front of the list. */
                    tc_recv_replay(&ui, thumbs, s);
                    ui.scroll = 0;
                } else if (strstr(s, "\"t\":\"wiped\"")) {
                    ui.nlog = 0;
                } else if (strstr(s, "\"t\":\"roster\"")) {
                    users = tc_json_top(s, "users");
                    if (users) { int n = tc_json_array_len(users); if (n >= 0) ui.people = n; }
                } else if (strstr(s, "\"t\":\"entry\"")) {
                    const char *obj = tc_json_top(s, "entry");
                    if (obj) {
                        tc_recv_push(&ui, thumbs, obj);
                        /* If the reader has scrolled back, hold their place
                         * rather than yanking the view to the newest row. */
                        if (ui.scroll > 0 && ui.scroll < TC_UI_LOG_MAX - 1) ui.scroll++;
                        else strcpy(status, "NEW DRAWING");
                    }
                } else if (strstr(s, "\"t\":\"error\"")) {
                    strcpy(status, "SERVER SAID NO");
                }
                ws.msg_len = 0;
                redraw = TC_BUFFERS;
            }
        }

        {
            int sc = tc_input_scroll();
            uint32_t now = tc_millis();
            if (sc && now - last_scroll > 110) {
                int shown = tc_ui_visible_rows(&ui, W, H);
                int maxs = ui.nlog - shown;
                int want = ui.scroll + (sc < 0 ? 1 : -1);   /* up = older */
                if (maxs < 0) maxs = 0;
                if (want < 0) want = 0;
                if (want > maxs) want = maxs;
                if (want != ui.scroll) { ui.scroll = want; redraw = TC_BUFFERS; }
                last_scroll = now;
            }
        }
        if (p.down || prev.down) redraw = TC_BUFFERS;
        if (in_lobby) {
            if (p.down && !prev.down) {
                int pick = tc_lobby_room_at(W, H, p.x, p.y, counts, ui.max);
                if (pick >= 0 && connected) {
                    char jm[64];
                    ui.room = (char)('A' + pick);
                    ui.nlog = 0; ui.scroll = 0; ui.people = 0;
                    snprintf(jm, sizeof(jm), "{\"t\":\"join\",\"room\":\"%c\"}", ui.room);
                    tc_ws_send_text(&ws, jm, strlen(jm));
                    strncpy(status, hint, sizeof(status) - 1);
                    status[sizeof(status) - 1] = '\0';
                    in_lobby = 0;
                    tc_canvas_clear(&canvas);
                }
            }
            prev = p;
            if (redraw <= 0) { tc_video_wait(); continue; }
            redraw--;
            if (dual) {
                tc_lobby_render(screen, W, H, counts, ui.max, nickbuf, connected);
                tc_lobby_render_top(screen_top, topw, toph, counts, ui.max);
                tc_video_blit(TC_SCREEN_TOP, screen_top, topw, toph);
                tc_video_blit(TC_SCREEN_BOTTOM, screen, W, H);
            } else {
                tc_lobby_render(screen, W, H, counts, ui.max, nickbuf, connected);
                tc_video_blit(TC_SCREEN_TOP, screen, W, H);
            }
            tc_video_present();
            continue;
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
                if (hit(tc_ui_room_rect(W, H), p.x, p.y) && connected) {
                    /* back to the picker rather than cycling blind - you can
                     * see who is where before committing */
                    const char *lv = "{\"t\":\"leave\"}";
                    tc_ws_send_text(&ws, lv, strlen(lv));
                    ui.room = '-'; ui.nlog = 0; ui.scroll = 0; ui.people = 0;
                    in_lobby = 1;
                    strcpy(status, "PICK A ROOM");
                }
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

        /* The screen is static most frames - drawing every one costs about two
         * thirds of the frame rate on a 3DS for no visible benefit. */
        /* Nothing owed: wait WITHOUT presenting. Presenting here would swap to
         * the other buffer, which is the same bug from the other direction. */
        if (redraw <= 0) { tc_video_wait(); continue; }
        redraw--;
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

cleanup:
    if (connected) tc_ws_close(&ws);
    tc_canvas_free(&canvas);
    tc_free(screen);
    tc_free(screen_top);
    tc_free(thumbs);
    return 0;
}
