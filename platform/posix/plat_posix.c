/* POSIX shim - the desktop/CI target. Proves the core before it meets hardware.
 * Deliberately the same five groups a console port must provide. */
#define _POSIX_C_SOURCE 200809L
#include "../platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

struct tc_sock { int fd; };

tc_sock *tc_sock_open(const char *host, int port) {
    struct addrinfo hints, *res = NULL, *it;
    char portstr[16];
    tc_sock *s;
    int fd = -1, one = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;            /* consoles are v4-only in practice */
    hints.ai_socktype = SOCK_STREAM;
    snprintf(portstr, sizeof(portstr), "%d", port);
    if (getaddrinfo(host, portstr, &hints, &res) != 0) return NULL;
    for (it = res; it; it = it->ai_next) {
        fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, it->ai_addr, it->ai_addrlen) == 0) break;
        close(fd); fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) return NULL;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    s = (tc_sock *)malloc(sizeof(*s));
    if (!s) { close(fd); return NULL; }
    s->fd = fd;
    return s;
}

int tc_sock_send(tc_sock *s, const void *b, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        ssize_t r = send(s->fd, (const char *)b + sent, n - sent, 0);
        if (r > 0) { sent += (size_t)r; continue; }
        if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { tc_sleep_ms(1); continue; }
        return -1;
    }
    return (int)sent;
}

int tc_sock_recv(tc_sock *s, void *b, size_t n) {
    ssize_t r = recv(s->fd, b, n, 0);
    if (r > 0) return (int)r;
    if (r == 0) return -1;                                   /* peer closed */
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;    /* nothing yet */
    return -1;
}

void tc_sock_close(tc_sock *s) { if (s) { close(s->fd); free(s); } }

void tc_video_wait(void) { tc_sleep_ms(16); }
int  tc_input_scroll(void) { return 0; }

uint32_t tc_millis(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}
void tc_sleep_ms(uint32_t ms) {
    struct timespec ts; ts.tv_sec = ms / 1000; ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}
int tc_random(void *buf, size_t n) {
    FILE *f = fopen("/dev/urandom", "rb");
    size_t got;
    if (!f) return -1;
    got = fread(buf, 1, n, f);
    fclose(f);
    return got == n ? 0 : -1;
}
void *tc_alloc(size_t n) { return malloc(n); }
void *tc_realloc(void *p, size_t n) { return realloc(p, n); }
void  tc_free(void *p) { free(p); }
void  tc_log(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap);
}
