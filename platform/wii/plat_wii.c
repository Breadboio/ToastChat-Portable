/* Wii shim (libogc). Three things differ from every other port and all three
 * are absorbed here so core/ never learns about them:
 *   - libogc has no BSD sockets: it is net_*, and DHCP needs if_config()
 *   - the framebuffer is YUV 4:2:2, so a blit is a colour-space conversion
 *   - the pointer is a Wiimote IR cursor, "down" is the A button
 */
#include "../platform.h"
#include "../../core/tc_yuv.h"
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <network.h>
#include <ogc/lwp_watchdog.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

struct tc_sock { s32 fd; };

/* ---- network ---------------------------------------------------------- */
static int g_net_up = 0;

int tc_wii_net_init(void) {
    char ip[16], nm[16], gw[16];
    if (g_net_up) return 0;
    if (if_config(ip, nm, gw, TRUE, 20) < 0) return -1;
    g_net_up = 1;
    return 0;
}

tc_sock *tc_sock_open(const char *host, int port) {
    struct sockaddr_in sa;
    struct hostent *he;
    tc_sock *s;
    s32 fd;

    if (!g_net_up && tc_wii_net_init() != 0) return NULL;

    he = net_gethostbyname((char *)host);
    if (!he || !he->h_addr_list || !he->h_addr_list[0]) return NULL;

    fd = net_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return NULL;

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((u16)port);
    memcpy(&sa.sin_addr, he->h_addr_list[0], 4);

    if (net_connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) { net_close(fd); return NULL; }
    /* Left BLOCKING on purpose. libogc only defines O_NONBLOCK if newlib has
     * not already, so the value net_fcntl() ends up with is not reliably the
     * one IOS wants. net_select() with a zero timeout gives the same
     * non-blocking read semantics without depending on that. */

    s = (tc_sock *)malloc(sizeof(*s));
    if (!s) { net_close(fd); return NULL; }
    s->fd = fd;
    return s;
}

int tc_sock_send(tc_sock *s, const void *b, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        s32 r = net_send(s->fd, (const char *)b + sent, (s32)(n - sent), 0);
        if (r > 0) { sent += (size_t)r; continue; }
        if (r == -EAGAIN) { tc_sleep_ms(1); continue; }
        return -1;
    }
    return (int)sent;
}

int tc_sock_recv(tc_sock *s, void *b, size_t n) {
    struct timeval tv;
    fd_set rd;
    s32 r;
    FD_ZERO(&rd);
    FD_SET(s->fd, &rd);
    tv.tv_sec = 0; tv.tv_usec = 0;
    r = net_select(s->fd + 1, &rd, NULL, NULL, &tv);
    if (r < 0) return -1;
    if (r == 0) return 0;                 /* nothing ready yet */
    r = net_recv(s->fd, b, (s32)n, 0);
    if (r > 0) return (int)r;
    return -1;                            /* 0 = peer closed, <0 = error */
}

void tc_sock_close(tc_sock *s) { if (s) { net_close(s->fd); free(s); } }

/* ---- video: RGBA -> YUY2 ---------------------------------------------- */
static void      *g_xfb;
static GXRModeObj *g_rmode;

void tc_wii_video_init(void) {
    VIDEO_Init();
    g_rmode = VIDEO_GetPreferredMode(NULL);
    g_xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(g_rmode));
    VIDEO_Configure(g_rmode);
    VIDEO_SetNextFramebuffer(g_xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (g_rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
}

void tc_video_size(tc_screen which, int *w, int *h) {
    (void)which;
    *w = g_rmode ? g_rmode->fbWidth : 640;
    *h = g_rmode ? g_rmode->xfbHeight : 480;
}

void tc_video_blit(tc_screen which, const uint8_t *rgba, int iw, int ih) {
    int fw, fh, x, y;
    u32 *fb = (u32 *)g_xfb;
    (void)which;
    if (!fb || !g_rmode) return;
    fw = g_rmode->fbWidth; fh = g_rmode->xfbHeight;
    if (ih < fh) fh = ih;
    for (y = 0; y < fh; y++) {
        const uint8_t *src = rgba + (size_t)y * iw * 4;
        u32 *dst = fb + (size_t)y * (fw / 2);
        for (x = 0; x + 1 < fw && x + 1 < iw; x += 2) {
            const uint8_t *a = src + (size_t)x * 4;
            const uint8_t *b = a + 4;
            dst[x / 2] = tc_rgb_to_yuy2(a[0], a[1], a[2], b[0], b[1], b[2]);
        }
    }
}
void tc_video_present(void) { VIDEO_Flush(); VIDEO_WaitVSync(); }
void tc_video_wait(void) { VIDEO_WaitVSync(); }

/* ---- input: Wiimote IR ------------------------------------------------ */
static tc_pointer g_ptr;
static int        g_quit;

void tc_wii_input_init(void) {
    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(WPAD_CHAN_0, 640, 480);
    memset(&g_ptr, 0, sizeof(g_ptr));
    g_quit = 0;
}

void tc_input_poll(void) {
    ir_t ir;
    u32 held, down;
    WPAD_ScanPads();
    down = WPAD_ButtonsDown(WPAD_CHAN_0);
    held = WPAD_ButtonsHeld(WPAD_CHAN_0);
    if (down & WPAD_BUTTON_HOME) g_quit = 1;
    WPAD_IR(WPAD_CHAN_0, &ir);
    if (ir.valid) { g_ptr.x = (int)ir.x; g_ptr.y = (int)ir.y; }
    g_ptr.down = (ir.valid && (held & WPAD_BUTTON_A)) ? 1 : 0;
}
void tc_input_pointer(tc_pointer *o) { *o = g_ptr; }
int  tc_input_quit(void) { return g_quit; }
int  tc_input_scroll(void) {
    u32 h = WPAD_ButtonsHeld(WPAD_CHAN_0);
    if (h & WPAD_BUTTON_UP)   return -1;
    if (h & WPAD_BUTTON_DOWN) return  1;
    return 0;
}

/* ---- time / memory / log ---------------------------------------------- */
uint32_t tc_millis(void)          { return (uint32_t)ticks_to_millisecs(gettime()); }
void     tc_sleep_ms(uint32_t ms) { usleep((useconds_t)ms * 1000); }
/* NOT cryptographic. devkitPro ships no mbedtls portlib for Wii, so TLS is
 * not built here and nothing security-relevant consumes this. If Wii ever
 * gains TLS, replace this with a real entropy source first. */
int      tc_random(void *buf, size_t n) {
    uint8_t *p = (uint8_t *)buf;
    size_t i;
    uint32_t s = (uint32_t)gettime() ^ (uint32_t)(uintptr_t)buf;
    for (i = 0; i < n; i++) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; p[i] = (uint8_t)s; }
    return 0;
}
void    *tc_alloc(size_t n)       { return malloc(n); }
void    *tc_realloc(void *p, size_t n) { return realloc(p, n); }
void     tc_free(void *p)         { free(p); }
void     tc_log(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); printf("\n"); va_end(ap);
}
