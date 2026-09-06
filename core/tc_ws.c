/* RFC 6455 client. Plain ws:// only - see docs/PROTOCOL.md §7 for why. */
#include "tc_ws.h"
#include "tc_sha1.h"
#include "tc_base64.h"
#include <string.h>
#include <stdio.h>

/* Verified against the RFC 6455 §1.3 vector AND a live ws server.
   The common mistype moves the C to the end of the last group. */
static const char TC_WS_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

void tc_ws_accept_for(const char *key, char out[32]) {
    tc_sha1 c; uint8_t d[20];
    tc_sha1_init(&c);
    tc_sha1_update(&c, key, strlen(key));
    tc_sha1_update(&c, TC_WS_GUID, sizeof(TC_WS_GUID) - 1);
    tc_sha1_final(&c, d);
    tc_b64_encode(d, 20, out);
}

static int wr(tc_ws *w, const void *b, size_t n) {
    return w->tls ? tc_tls_send(w->tls, b, n) : tc_sock_send(w->sock, b, n);
}
static int rd(tc_ws *w, void *b, size_t n) {
    return w->tls ? tc_tls_recv(w->tls, b, n) : tc_sock_recv(w->sock, b, n);
}

/* HTTP header names are case-insensitive, and proxies do re-case them: Node's
 * `ws` sends "Sec-WebSocket-Accept" but nginx forwards "Sec-Websocket-Accept".
 * A case-sensitive search works against the dev server and fails against prod. */
static char *tc_stristr(char *hay, const char *needle) {
    size_t n = strlen(needle);
    if (!n) return hay;
    for (; *hay; hay++) {
        size_t i = 0;
        while (i < n) {
            char a = hay[i], b = needle[i];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) break;
            i++;
        }
        if (i == n) return hay;
    }
    return NULL;
}

/* memmem is a GNU extension; newlib (Switch/3DS/Wii) does not have it. */
static uint8_t *tc_find(uint8_t *h, size_t hn, const char *n, size_t nn) {
    size_t i;
    if (hn < nn) return NULL;
    for (i = 0; i + nn <= hn; i++)
        if (memcmp(h + i, n, nn) == 0) return h + i;
    return NULL;
}

static uint32_t xs(tc_ws *w) {           /* mask key only: not crypto */
    w->rng ^= w->rng << 13; w->rng ^= w->rng >> 17; w->rng ^= w->rng << 5;
    return w->rng;
}

static int rx_reserve(tc_ws *w, size_t extra) {
    if (w->rx_len + extra <= w->rx_cap) return 0;
    {
        size_t cap = w->rx_cap ? w->rx_cap : 4096;
        uint8_t *p;
        while (cap < w->rx_len + extra) cap *= 2;
        p = (uint8_t *)tc_realloc(w->rx, cap);
        if (!p) return -1;
        w->rx = p; w->rx_cap = cap;
    }
    return 0;
}

int tc_ws_connect(tc_ws *w, const char *host, int port, const char *path, int use_tls) {
    uint8_t nonce[16];
    char key[32], want[32], req[512];
    int i, n;
    size_t used = 0;

    memset(w, 0, sizeof(*w));
    w->rng = tc_millis() | 1u;
    for (i = 0; i < 16; i++) nonce[i] = (uint8_t)(xs(w) >> 11);
    tc_b64_encode(nonce, 16, key);
    tc_ws_accept_for(key, want);

    w->sock = tc_sock_open(host, port);
    if (!w->sock) return -1;
    if (use_tls) {
        w->tls = tc_tls_connect(w->sock, host);
        if (!w->tls) { tc_log("ws: tls failed: %s", tc_tls_error()); return -9; }
    }

    n = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\nHost: %s:%d\r\nUpgrade: websocket\r\n"
        "Connection: Upgrade\r\nSec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n", path, host, port, key);
    if (wr(w, req, (size_t)n) != n) return -2;

    /* Read until end of headers. */
    for (;;) {
        int r;
        if (rx_reserve(w, 1024) < 0) return -3;
        r = rd(w, w->rx + w->rx_len, 1024);
        if (r < 0) return -4;
        if (r > 0) w->rx_len += (size_t)r;
        if (w->rx_len >= 4) {
            uint8_t *e = tc_find(w->rx, w->rx_len, "\r\n\r\n", 4);
            if (e) { used = (size_t)(e - w->rx) + 4; break; }
        }
        if (w->rx_len > 8192) return -5;
    }
    {   /* validate status + accept */
        char head[8193];
        char *acc;
        memcpy(head, w->rx, used); head[used] = '\0';
        if (strncmp(head, "HTTP/1.1 101", 12) != 0 && strncmp(head, "HTTP/1.0 101", 12) != 0) {
            tc_log("ws: server did not upgrade: %.40s", head);
            return -6;
        }
        acc = tc_stristr(head, "sec-websocket-accept:");
        if (!acc) return -7;
        acc += 21; while (*acc == ' ') acc++;
        if (strncmp(acc, want, strlen(want)) != 0) {
            tc_log("ws: accept mismatch (got %.28s want %s)", acc, want);
            return -8;
        }
    }
    memmove(w->rx, w->rx + used, w->rx_len - used);
    w->rx_len -= used;
    w->open = 1;
    return 0;
}

int tc_ws_send_text(tc_ws *w, const char *s, size_t n) {
    uint8_t hdr[14]; size_t h = 0; uint8_t mask[4]; size_t i;
    uint8_t *body;
    if (!w->open) return -1;
    hdr[h++] = 0x81;                                  /* FIN + text */
    if (n < 126)        hdr[h++] = (uint8_t)(0x80 | n);
    else if (n <= 0xFFFF) { hdr[h++] = 0x80 | 126;
        hdr[h++] = (uint8_t)(n >> 8); hdr[h++] = (uint8_t)n; }
    else { int k; hdr[h++] = 0x80 | 127;
        for (k = 7; k >= 0; k--) hdr[h++] = (uint8_t)((uint64_t)n >> (k * 8)); }
    for (i = 0; i < 4; i++) { mask[i] = (uint8_t)(xs(w) >> 13); hdr[h++] = mask[i]; }
    if (wr(w, hdr, h) != (int)h) return -2;

    body = (uint8_t *)tc_alloc(n ? n : 1);
    if (!body) return -3;
    for (i = 0; i < n; i++) body[i] = (uint8_t)s[i] ^ mask[i & 3];
    { int r = wr(w, body, n); tc_free(body);
      if (r != (int)n) return -4; }
    return 0;
}

static int send_pong(tc_ws *w, const uint8_t *p, size_t n) {
    uint8_t hdr[10]; size_t h = 0, i; uint8_t mask[4]; uint8_t *b;
    hdr[h++] = 0x8A;
    hdr[h++] = (uint8_t)(0x80 | (n < 126 ? n : 125));
    if (n > 125) n = 125;
    for (i = 0; i < 4; i++) { mask[i] = (uint8_t)(xs(w) >> 13); hdr[h++] = mask[i]; }
    if (wr(w, hdr, h) != (int)h) return -1;
    b = (uint8_t *)tc_alloc(n ? n : 1);
    if (!b) return -1;
    for (i = 0; i < n; i++) b[i] = p[i] ^ mask[i & 3];
    { int r = wr(w, b, n); tc_free(b); return r == (int)n ? 0 : -1; }
}

int tc_ws_poll(tc_ws *w) {
    int r;
    if (!w->open) return -1;
    if (rx_reserve(w, 4096) < 0) return -1;
    r = rd(w, w->rx + w->rx_len, 4096);
    if (r < 0) { w->open = 0; return -1; }
    w->rx_len += (size_t)r;

    for (;;) {
        uint64_t len; size_t off = 2; int op, fin;
        if (w->rx_len < 2) return 0;
        fin = (w->rx[0] & 0x80) != 0; op = w->rx[0] & 0x0F;
        len = (uint64_t)(w->rx[1] & 0x7F);
        if (w->rx[1] & 0x80) return -1;             /* server must not mask */
        if (len == 126) {
            if (w->rx_len < 4) return 0;
            len = ((uint64_t)w->rx[2] << 8) | w->rx[3]; off = 4;
        } else if (len == 127) {
            int k; if (w->rx_len < 10) return 0;
            len = 0; for (k = 0; k < 8; k++) len = (len << 8) | w->rx[2 + k];
            off = 10;
        }
        if (w->rx_len < off + len) return 0;        /* wait for the rest */

        if (op == 0x8) { w->open = 0; return -1; }
        if (op == 0x9) { send_pong(w, w->rx + off, (size_t)len); }
        else if (op == 0x1 || op == 0x2 || op == 0x0) {
            size_t need = w->msg_len + (size_t)len + 1;
            if (need > w->msg_cap) {
                size_t cap = w->msg_cap ? w->msg_cap : 8192;
                uint8_t *p;
                while (cap < need) cap *= 2;
                p = (uint8_t *)tc_realloc(w->msg, cap);
                if (!p) return -1;
                w->msg = p; w->msg_cap = cap;
            }
            memcpy(w->msg + w->msg_len, w->rx + off, (size_t)len);
            w->msg_len += (size_t)len;
        }
        memmove(w->rx, w->rx + off + len, w->rx_len - off - (size_t)len);
        w->rx_len -= off + (size_t)len;

        if (fin && w->msg_len && (op == 0x1 || op == 0x2 || op == 0x0)) {
            w->msg[w->msg_len] = '\0';
            return 1;
        }
    }
}

void tc_ws_close(tc_ws *w) {
    if (w->tls) tc_tls_free(w->tls);
    if (w->sock) tc_sock_close(w->sock);
    tc_free(w->rx); tc_free(w->msg);
    memset(w, 0, sizeof(*w));
}
