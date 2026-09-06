/* ToastChat Switch - network probe build.
 * Answers the one question an emulator cannot: does a real console reach the
 * server and complete an RFC 6455 handshake? Prints the live room feed. */
#include <switch.h>
#include <stdio.h>
#include <string.h>
#include "../../core/tc_ws.h"

#ifndef TC_HOST
#define TC_HOST "192.168.1.167"
#endif
#ifndef TC_PORT
#define TC_PORT 3401
#endif

int main(int argc, char **argv) {
    tc_ws w;
    char buf[256];
    int rc, joined = 0;
    PadState pad;
    (void)argc; (void)argv;

    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    printf("\x1b[2J\x1b[1;1HToastChat - Switch probe\n");
    printf("target ws://%s:%d/ws\n\n", TC_HOST, TC_PORT);
    consoleUpdate(NULL);

    if (R_FAILED(socketInitializeDefault())) {
        printf("socketInitializeDefault FAILED\n");
    } else {
        printf("sockets up. connecting...\n"); consoleUpdate(NULL);
        rc = tc_ws_connect(&w, TC_HOST, TC_PORT, "/ws");
        if (rc != 0) {
            printf("handshake FAILED rc=%d\n", rc);
        } else {
            printf("handshake OK (101 + accept verified)\n\n");
            snprintf(buf, sizeof(buf), "{\"t\":\"identify\",\"nick\":\"Switch\",\"color\":\"#2fa89a\"}");
            tc_ws_send_text(&w, buf, strlen(buf));
            snprintf(buf, sizeof(buf), "{\"t\":\"join\",\"room\":\"C\"}");
            tc_ws_send_text(&w, buf, strlen(buf));
            joined = 1;
        }
        consoleUpdate(NULL);
    }

    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
        if (joined) {
            int p = tc_ws_poll(&w);
            if (p < 0) { printf("socket closed\n"); joined = 0; }
            else if (p == 1) {
                printf("<- %.70s\n", (char *)w.msg);
                w.msg_len = 0;
            }
        }
        consoleUpdate(NULL);
        svcSleepThread(16000000ULL);
    }

    if (joined) tc_ws_close(&w);
    socketExit();
    consoleExit(NULL);
    return 0;
}
