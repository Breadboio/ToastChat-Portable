/* Proves wss:// end to end: TLS handshake with our own bundled trust anchors,
 * certificate verification, then the RFC 6455 upgrade.
 *
 * Deliberately stops after `hello`. It never sends `join`, so it can be pointed
 * at the LIVE server without entering a room anyone can see. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../core/tc_ws.h"
#include "../core/tc_tls.h"

int main(int argc, char **argv) {
    const char *host = argc > 1 ? argv[1] : "breadtoasting.com";
    int port = argc > 2 ? atoi(argv[2]) : 443;
    const char *path = argc > 3 ? argv[3] : "/toastchat/ws";
    int use_tls = argc > 4 ? atoi(argv[4]) : 1;
    tc_ws w;
    int rc, spins = 0;

    printf("TLS compiled in : %s\n", tc_tls_available() ? "yes" : "NO");
    printf("connecting      : %s://%s:%d%s\n", use_tls ? "wss" : "ws", host, port, path);

    rc = tc_ws_connect(&w, host, port, path, use_tls);
    if (rc != 0) {
        printf("FAILED rc=%d (%s)\n", rc, tc_tls_error());
        return 1;
    }
    if (use_tls) puts("TLS handshake   : OK (certificate verified against bundled anchors)");
    puts("websocket       : 101 Switching Protocols, accept verified");

    while (spins++ < 500) {
        int p = tc_ws_poll(&w);
        if (p < 0) { puts("socket closed"); break; }
        if (p == 1) {
            char *c = strstr((char *)w.msg, "\"t\":\"hello\"");
            if (c) {
                char *rooms = strstr((char *)w.msg, "\"rooms\"");
                printf("server hello    : %.60s\n", rooms ? rooms : (char *)w.msg);
                printf("\nPUBLIC SERVER REACHED OVER %s (did not join any room)\n", use_tls ? "TLS" : "PLAIN ws://");
                tc_ws_close(&w);
                return 0;
            }
            w.msg_len = 0;
        }
        tc_sleep_ms(10);
    }
    tc_ws_close(&w);
    puts("no hello received");
    return 1;
}
