#include "tc_tls.h"
#include <string.h>
#include <stdio.h>

static char g_err[128];
const char *tc_tls_error(void) { return g_err[0] ? g_err : "no error"; }

#ifndef TC_HAVE_TLS

int tc_tls_available(void) { return 0; }
tc_tls *tc_tls_connect(tc_sock *s, const char *h) {
    (void)s; (void)h;
    snprintf(g_err, sizeof(g_err), "built without TLS support");
    return NULL;
}
int  tc_tls_send(tc_tls *t, const void *b, size_t n) { (void)t; (void)b; (void)n; return -1; }
int  tc_tls_recv(tc_tls *t, void *b, size_t n)       { (void)t; (void)b; (void)n; return -1; }
void tc_tls_free(tc_tls *t) { (void)t; }

#else

#include "tc_cacert.h"
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/error.h>

struct tc_tls {
    mbedtls_ssl_context      ssl;
    mbedtls_ssl_config       conf;
    mbedtls_x509_crt         ca;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_entropy_context  entropy;
    tc_sock                 *sock;
};

int tc_tls_available(void) { return 1; }

/* mbedtls pulls entropy through the platform contract, so each console uses
 * its real hardware RNG rather than whatever newlib might guess at. */
static int entropy_from_platform(void *ctx, unsigned char *out, size_t len, size_t *olen) {
    (void)ctx;
    if (tc_random(out, len) != 0) return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    *olen = len;
    return 0;
}

/* Our sockets are non-blocking and report would-block as 0; mbedtls wants
 * MBEDTLS_ERR_SSL_WANT_READ/WRITE instead. */
/* MBEDTLS_ERR_NET_* live in net_sockets.h, which drags in POSIX socket code the
 * consoles do not have. Any negative return signals failure, so use an ssl.h
 * code and keep this file free of platform sockets. */
static int bio_send(void *ctx, const unsigned char *buf, size_t len) {
    int r = tc_sock_send((tc_sock *)ctx, buf, len);
    if (r < 0) return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    return r;
}
static int bio_recv(void *ctx, unsigned char *buf, size_t len) {
    int r = tc_sock_recv((tc_sock *)ctx, buf, len);
    if (r == 0) return MBEDTLS_ERR_SSL_WANT_READ;
    if (r < 0) return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    return r;
}

static void fail(tc_tls *t, const char *what, int code) {
    char b[96];
    mbedtls_strerror(code, b, sizeof(b));
    snprintf(g_err, sizeof(g_err), "%s: %s (-0x%04x)", what, b, (unsigned)-code);
    (void)t;
}

tc_tls *tc_tls_connect(tc_sock *sock, const char *hostname) {
    tc_tls *t = (tc_tls *)tc_alloc(sizeof(*t));
    int r;
    uint32_t spins = 0;
    if (!t) { snprintf(g_err, sizeof(g_err), "out of memory"); return NULL; }
    memset(t, 0, sizeof(*t));
    t->sock = sock;

    mbedtls_ssl_init(&t->ssl);
    mbedtls_ssl_config_init(&t->conf);
    mbedtls_x509_crt_init(&t->ca);
    mbedtls_ctr_drbg_init(&t->drbg);
    mbedtls_entropy_init(&t->entropy);

    mbedtls_entropy_add_source(&t->entropy, entropy_from_platform, NULL, 32,
                               MBEDTLS_ENTROPY_SOURCE_STRONG);
    r = mbedtls_ctr_drbg_seed(&t->drbg, mbedtls_entropy_func, &t->entropy,
                              (const unsigned char *)"toastchat", 9);
    if (r != 0) { fail(t, "drbg seed", r); goto bad; }

    r = mbedtls_x509_crt_parse(&t->ca, (const unsigned char *)tc_cacert_pem,
                               sizeof(tc_cacert_pem));
    if (r != 0) { fail(t, "parse CA", r); goto bad; }

    r = mbedtls_ssl_config_defaults(&t->conf, MBEDTLS_SSL_IS_CLIENT,
                                    MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT);
    if (r != 0) { fail(t, "config defaults", r); goto bad; }

    mbedtls_ssl_conf_authmode(&t->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&t->conf, &t->ca, NULL);
    mbedtls_ssl_conf_rng(&t->conf, mbedtls_ctr_drbg_random, &t->drbg);

    r = mbedtls_ssl_setup(&t->ssl, &t->conf);
    if (r != 0) { fail(t, "ssl setup", r); goto bad; }
    r = mbedtls_ssl_set_hostname(&t->ssl, hostname);   /* SNI + name check */
    if (r != 0) { fail(t, "set hostname", r); goto bad; }
    mbedtls_ssl_set_bio(&t->ssl, sock, bio_send, bio_recv, NULL);

    for (;;) {
        r = mbedtls_ssl_handshake(&t->ssl);
        if (r == 0) break;
        if (r != MBEDTLS_ERR_SSL_WANT_READ && r != MBEDTLS_ERR_SSL_WANT_WRITE) {
            fail(t, "handshake", r);
            goto bad;
        }
        if (++spins > 60000) { snprintf(g_err, sizeof(g_err), "handshake timed out"); goto bad; }
        tc_sleep_ms(1);
    }
    {
        uint32_t flags = mbedtls_ssl_get_verify_result(&t->ssl);
        if (flags != 0) {
            snprintf(g_err, sizeof(g_err), "certificate rejected (flags 0x%08lx)",
                     (unsigned long)flags);
            goto bad;
        }
    }
    g_err[0] = '\0';
    return t;
bad:
    tc_tls_free(t);
    return NULL;
}

int tc_tls_send(tc_tls *t, const void *buf, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        int r = mbedtls_ssl_write(&t->ssl, (const unsigned char *)buf + sent, n - sent);
        if (r > 0) { sent += (size_t)r; continue; }
        if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) {
            tc_sleep_ms(1); continue;
        }
        return -1;
    }
    return (int)n;
}

int tc_tls_recv(tc_tls *t, void *buf, size_t n) {
    int r = mbedtls_ssl_read(&t->ssl, (unsigned char *)buf, n);
    if (r > 0) return r;
    if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) return 0;
    if (r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) return -1;
    if (r == 0) return -1;
    return -1;
}

void tc_tls_free(tc_tls *t) {
    if (!t) return;
    mbedtls_ssl_free(&t->ssl);
    mbedtls_ssl_config_free(&t->conf);
    mbedtls_x509_crt_free(&t->ca);
    mbedtls_ctr_drbg_free(&t->drbg);
    mbedtls_entropy_free(&t->entropy);
    tc_free(t);
}

#endif /* TC_HAVE_TLS */
