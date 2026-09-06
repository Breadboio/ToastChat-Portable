/* Switch shim (libnx). Sockets + time + alloc + log; video/input land with the
 * framebuffer pass. Deliberately the same five groups as every other port. */
#include "../platform.h"
#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
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
    hints.ai_family   = AF_INET;          /* libnx is v4 in practice */
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
        int r = (int)send(s->fd, (const char *)b + sent, n - sent, 0);
        if (r > 0) { sent += (size_t)r; continue; }
        if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { tc_sleep_ms(1); continue; }
        return -1;
    }
    return (int)sent;
}

int tc_sock_recv(tc_sock *s, void *b, size_t n) {
    int r = (int)recv(s->fd, b, n, 0);
    if (r > 0) return r;
    if (r == 0) return -1;
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
    return -1;
}

void tc_sock_close(tc_sock *s) { if (s) { close(s->fd); free(s); } }

uint32_t tc_millis(void)          { return (uint32_t)(armGetSystemTick() / 19200ULL); }
void     tc_sleep_ms(uint32_t ms) { svcSleepThread((uint64_t)ms * 1000000ULL); }
void    *tc_alloc(size_t n)       { return malloc(n); }
void    *tc_realloc(void *p, size_t n) { return realloc(p, n); }
void     tc_free(void *p)         { free(p); }

void tc_log(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vprintf(fmt, ap); printf("\n"); va_end(ap);
    consoleUpdate(NULL);
}

/* ---- video: libnx linear framebuffer ---------------------------------
 * PIXEL_FORMAT_RGBA_8888 packs a u32 as R|G<<8|B<<16|A<<24, which on
 * little-endian AArch64 is the byte order R,G,B,A - identical to the core's
 * buffer, so each row is a straight memcpy. */
static Framebuffer g_fb;
static int         g_fb_ready = 0;
static PadState    g_pad;
static int         g_quit = 0;
static tc_pointer  g_ptr;

void tc_switch_video_init(void) {
    framebufferCreate(&g_fb, nwindowGetDefault(), 1280, 720, PIXEL_FORMAT_RGBA_8888, 2);
    framebufferMakeLinear(&g_fb);
    g_fb_ready = 1;
}
void tc_switch_video_exit(void) {
    if (g_fb_ready) { framebufferClose(&g_fb); g_fb_ready = 0; }
}

void tc_video_size(tc_screen w, int *ow, int *oh) { (void)w; *ow = 1280; *oh = 720; }

void tc_video_blit(tc_screen which, const uint8_t *rgba, int iw, int ih) {
    u32 stride; u8 *fbp; int y, rowbytes;
    (void)which;
    if (!g_fb_ready) return;
    fbp = (u8 *)framebufferBegin(&g_fb, &stride);
    rowbytes = (iw < 1280 ? iw : 1280) * 4;
    for (y = 0; y < ih && y < 720; y++)
        memcpy(fbp + (size_t)y * stride, rgba + (size_t)y * iw * 4, (size_t)rowbytes);
    framebufferEnd(&g_fb);
}
void tc_video_present(void) { /* framebufferEnd already flipped */ }

/* ---- input: touchscreen (handheld) + buttons ------------------------- */
void tc_switch_input_init(void) {
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&g_pad);
    hidInitializeTouchScreen();
    memset(&g_ptr, 0, sizeof(g_ptr));
}

void tc_input_poll(void) {
    HidTouchScreenState st;
    memset(&st, 0, sizeof(st));
    padUpdate(&g_pad);
    if (padGetButtonsDown(&g_pad) & HidNpadButton_Plus) g_quit = 1;
    if (hidGetTouchScreenStates(&st, 1) && st.count > 0) {
        g_ptr.x = (int)st.touches[0].x;
        g_ptr.y = (int)st.touches[0].y;
        g_ptr.down = 1;
    } else {
        g_ptr.down = 0;   /* x/y kept, so a release still knows where it was */
    }
}
void tc_input_pointer(tc_pointer *o) { *o = g_ptr; }
int  tc_input_quit(void) { return g_quit; }
