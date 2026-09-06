/* End-to-end: connect, join, draw, encode, send - then confirm the server has
 * it. Exercises the identical core the .nro runs. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "../core/tc_ws.h"
#include "../core/tc_ui.h"
#include "../core/tc_draw.h"
#include "../core/tc_send.h"

int main(int argc, char **argv) {
    const char *host = argc > 1 ? argv[1] : "127.0.0.1";
    int port = argc > 2 ? atoi(argv[2]) : 3401;
    tc_ws ws; tc_canvas c; char m[128]; int i, spins = 0, sent = 0, echoed = 0;
    tc_rect cv = tc_ui_canvas_rect(1280, 720);

    if (tc_ws_connect(&ws, host, port, "/ws") != 0) { puts("connect FAILED"); return 1; }
    puts("connected");
    snprintf(m, sizeof(m), "{\"t\":\"identify\",\"nick\":\"Switch\",\"color\":\"#2fa89a\"}");
    tc_ws_send_text(&ws, m, strlen(m));
    snprintf(m, sizeof(m), "{\"t\":\"join\",\"room\":\"C\"}");
    tc_ws_send_text(&ws, m, strlen(m));

    tc_canvas_init(&c, 16000, 256);
    /* write "HI" as strokes, plus a wave, in canvas-local coords */
    tc_canvas_begin(&c, 120, 80, TC_PALETTE[8], 8.0f); tc_canvas_to(&c, 120, 260); tc_canvas_end(&c);
    tc_canvas_begin(&c, 120, 170, TC_PALETTE[8], 8.0f); tc_canvas_to(&c, 230, 170); tc_canvas_end(&c);
    tc_canvas_begin(&c, 230, 80, TC_PALETTE[8], 8.0f); tc_canvas_to(&c, 230, 260); tc_canvas_end(&c);
    tc_canvas_begin(&c, 300, 80, TC_PALETTE[2], 8.0f); tc_canvas_to(&c, 300, 260); tc_canvas_end(&c);
    tc_canvas_begin(&c, 400, 300, TC_PALETTE[11], 4.0f);
    for (i = 0; i <= 200; i++) tc_canvas_to(&c, 400 + i * 3, (int)(340 + 60 * sin(i * 0.05)));
    tc_canvas_end(&c);

    while (spins++ < 400) {
        int r = tc_ws_poll(&ws);
        if (r < 0) { puts("socket closed"); break; }
        if (r == 1) {
            if (strstr((char *)ws.msg, "\"t\":\"joined\"") && !sent) {
                int rc = tc_send_canvas(&ws, &c, cv.w, cv.h, 2);
                printf("tc_send_canvas -> %d (%s)\n", rc, rc == 0 ? "sent" : "FAILED");
                if (rc != 0) return 1;
                sent = 1;
            } else if (sent && strstr((char *)ws.msg, "\"t\":\"entry\"") &&
                       strstr((char *)ws.msg, "\"nick\":\"Switch\"")) {
                char *w = strstr((char *)ws.msg, "\"w\":");
                printf("server echoed our drawing back: %.*s\n", 24, w ? w : "?");
                echoed = 1; break;
            }
            ws.msg_len = 0;
        }
        tc_sleep_ms(10);
    }
    tc_canvas_free(&c); tc_ws_close(&ws);
    printf("%s\n", echoed ? "ROUND TRIP OK" : "no echo seen");
    return echoed ? 0 : 1;
}
