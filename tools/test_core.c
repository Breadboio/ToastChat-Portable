/* Self-test + live protocol test. Runs on the POSIX shim; the console ports
 * reuse every line of core/ that this exercises. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../core/tc_sha1.h"
#include "../core/tc_base64.h"
#include "../core/tc_ws.h"

static int fails = 0;
static void ck(const char *what, int ok, const char *got, const char *want) {
    printf("  [%s] %-34s", ok ? "PASS" : "FAIL", what);
    if (!ok) { printf("  got=%s want=%s", got ? got : "?", want ? want : "?"); fails++; }
    printf("\n");
}

static void hex(const uint8_t *d, int n, char *o) {
    int i; for (i = 0; i < n; i++) sprintf(o + i * 2, "%02x", d[i]); o[n*2] = 0;
}

int main(int argc, char **argv) {
    const char *host = argc > 1 ? argv[1] : "127.0.0.1";
    int port = argc > 2 ? atoi(argv[2]) : 3401;
    const char *room = argc > 3 ? argv[3] : "C";
    int use_tls = argc > 4 ? atoi(argv[4]) : 0;
    const char *path = use_tls ? "/toastchat/ws" : "/ws";

    puts("== unit: SHA-1 ==");
    {   /* FIPS 180-1 vectors */
        tc_sha1 c; uint8_t d[20]; char h[41];
        tc_sha1_init(&c); tc_sha1_update(&c, "abc", 3); tc_sha1_final(&c, d); hex(d, 20, h);
        ck("sha1(\"abc\")", !strcmp(h, "a9993e364706816aba3e25717850c26c9cd0d89d"),
           h, "a9993e364706816aba3e25717850c26c9cd0d89d");
        tc_sha1_init(&c); tc_sha1_update(&c, "", 0); tc_sha1_final(&c, d); hex(d, 20, h);
        ck("sha1(\"\")", !strcmp(h, "da39a3ee5e6b4b0d3255bfef95601890afd80709"),
           h, "da39a3ee5e6b4b0d3255bfef95601890afd80709");
        tc_sha1_init(&c);
        tc_sha1_update(&c, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56);
        tc_sha1_final(&c, d); hex(d, 20, h);
        ck("sha1(56-byte, spans blocks)", !strcmp(h, "84983e441c3bd26ebaae4aa1f95129e5e54670f1"),
           h, "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    }
    puts("== unit: base64 ==");
    {
        char o[64]; long n; uint8_t back[64];
        tc_b64_encode((const uint8_t *)"any carnal pleasure.", 20, o);
        ck("encode (no pad)", !strcmp(o, "YW55IGNhcm5hbCBwbGVhc3VyZS4="), o, "YW55IGNhcm5hbCBwbGVhc3VyZS4=");
        tc_b64_encode((const uint8_t *)"a", 1, o);
        ck("encode (2 pad)", !strcmp(o, "YQ=="), o, "YQ==");
        tc_b64_encode((const uint8_t *)"ab", 2, o);
        ck("encode (1 pad)", !strcmp(o, "YWI="), o, "YWI=");
        n = tc_b64_decode("YW55IGNhcm5hbCBwbGVhc3VyZS4=", 28, back); back[n] = 0;
        ck("decode round-trip", n == 20 && !strcmp((char *)back, "any carnal pleasure."),
           (char *)back, "any carnal pleasure.");
    }
    puts("== unit: RFC 6455 accept ==");
    {
        char a[32];
        tc_ws_accept_for("dGhlIHNhbXBsZSBub25jZQ==", a);
        ck("RFC 6455 1.3 vector", !strcmp(a, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="),
           a, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    }

    printf("\n== live: ws://%s:%d/ws room %s ==\n", host, port, room);
    {
        tc_ws w; int r, got_hello = 0, got_joined = 0, spins = 0;
        char buf[256];
        r = tc_ws_connect(&w, host, port, path, use_tls);
        ck("handshake (101 + accept match)", r == 0, r == 0 ? "ok" : "err", "ok");
        if (r != 0) { printf("\nconnect failed rc=%d\n", r); return 1; }

        snprintf(buf, sizeof(buf), "{\"t\":\"identify\",\"nick\":\"Switch\",\"color\":\"#2fa89a\"}");
        ck("send identify (masked frame)", tc_ws_send_text(&w, buf, strlen(buf)) == 0, NULL, NULL);
        snprintf(buf, sizeof(buf), "{\"t\":\"join\",\"room\":\"%s\"}", room);
        ck("send join", tc_ws_send_text(&w, buf, strlen(buf)) == 0, NULL, NULL);

        while (spins++ < 600) {
            int p = tc_ws_poll(&w);
            if (p < 0) { puts("  socket closed"); break; }
            if (p == 1) {
                if (strstr((char *)w.msg, "\"t\":\"hello\""))  got_hello = 1;
                if (strstr((char *)w.msg, "\"t\":\"joined\"")) got_joined = 1;
                printf("  <- %.90s%s\n", (char *)w.msg, w.msg_len > 90 ? "..." : "");
                w.msg_len = 0;
                if (got_hello && got_joined) break;
            }
            tc_sleep_ms(10);
        }
        ck("received hello", got_hello, NULL, NULL);
        ck("received joined", got_joined, NULL, NULL);
        tc_ws_close(&w);
    }
    printf("\n%s (%d failure%s)\n", fails ? "FAILED" : "ALL PASS", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
