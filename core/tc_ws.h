#ifndef TC_WS_H
#define TC_WS_H
#include <stddef.h>
#include <stdint.h>
#include "../platform/platform.h"
#include "tc_tls.h"

typedef struct {
    tc_sock *sock;
    tc_tls  *tls;                          /* NULL = plain ws://, else wss:// */
    uint8_t *rx;  size_t rx_len, rx_cap;   /* raw bytes off the socket   */
    uint8_t *msg; size_t msg_len, msg_cap; /* reassembled message payload*/
    uint32_t rng;
    int      open;
} tc_ws;

/* Blocking handshake. 0 on success, negative on failure. */
int  tc_ws_connect(tc_ws *w, const char *host, int port, const char *path, int use_tls);
/* Non-blocking pump. 1 = a complete text message is in w->msg/msg_len. */
int  tc_ws_poll(tc_ws *w);
int  tc_ws_send_text(tc_ws *w, const char *s, size_t n);
void tc_ws_close(tc_ws *w);

/* Exposed for the self-test: RFC 6455 accept computation. */
void tc_ws_accept_for(const char *key, char out[32]);
#endif
