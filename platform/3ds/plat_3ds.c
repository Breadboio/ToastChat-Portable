/* 3DS shim (libctru). Two quirks absorbed here:
 *   - the framebuffer is stored ROTATED: it is column-major, so a pixel at
 *     screen (x,y) lives at (x * 240 + (239 - y)), not (y * width + x)
 *   - the default pixel format is BGR8, three bytes, blue first
 * We stay on the plain gfx framebuffer rather than the GPU. citro3d (see the
 * devkitPro `lenny` example) is the right tool when you want the GPU to scale
 * or rotate for you; here we hand over a finished RGBA buffer once a frame, so
 * the extra pipeline would buy nothing.
 */
#include "../platform.h"
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <malloc.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SOC_ALIGN      0x1000
#define SOC_BUFFERSIZE 0x100000

struct tc_sock { int fd; };
static u32 *g_soc_buf;
static tc_pointer g_ptr;
static int g_quit;

int tc_3ds_net_init(void) {
    g_soc_buf = (u32 *)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    if (!g_soc_buf) return -1;
    if (R_FAILED(socInit(g_soc_buf, SOC_BUFFERSIZE))) return -1;
    return 0;
}
void tc_3ds_net_exit(void) { socExit(); }

tc_sock *tc_sock_open(const char *host, int port) {
    struct addrinfo hints, *res = NULL, *it;
    char portstr[16];
    tc_sock *s;
    int fd = -1;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    sprintf(portstr, "%d", port);
    if (getaddrinfo(host, portstr, &hints, &res) != 0) return NULL;
    for (it = res; it; it = it->ai_next) {
        fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, it->ai_addr, it->ai_addrlen) == 0) break;
        close(fd); fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) return NULL;
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    s = (tc_sock *)malloc(sizeof(*s));
    if (!s) { close(fd); return NULL; }
    s->fd = fd;
    return s;
}
int tc_sock_send(tc_sock *s, const void *b, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        int r = send(s->fd, (const char *)b + sent, n - sent, 0);
        if (r > 0) { sent += (size_t)r; continue; }
        if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { tc_sleep_ms(1); continue; }
        return -1;
    }
    return (int)sent;
}
int tc_sock_recv(tc_sock *s, void *b, size_t n) {
    int r = recv(s->fd, b, n, 0);
    if (r > 0) return r;
    if (r == 0) return -1;
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
    return -1;
}
void tc_sock_close(tc_sock *s) { if (s) { close(s->fd); free(s); } }

/* ---- video ------------------------------------------------------------ */
void tc_3ds_video_init(void) {
    psInit();              /* hardware RNG, needed to seed TLS */
    gfxInitDefault();
    memset(&g_ptr, 0, sizeof(g_ptr));
    g_quit = 0;
}
void tc_3ds_video_exit(void) { gfxExit(); psExit(); }

void tc_video_size(tc_screen which, int *w, int *h) {
    *w = (which == TC_SCREEN_TOP) ? 400 : 320;
    *h = 240;
}

void tc_video_blit(tc_screen which, const uint8_t *rgba, int iw, int ih) {
    gfxScreen_t scr = (which == TC_SCREEN_TOP) ? GFX_TOP : GFX_BOTTOM;
    u16 fbw = 0, fbh = 0;
    u8 *fb = gfxGetFramebuffer(scr, GFX_LEFT, &fbw, &fbh);
    int x, y, sw, sh;
    if (!fb) return;
    /* libctru reports the ROTATED dimensions: fbw is the screen height (240)
     * and fbh is the screen width (400/320). */
    sw = (iw < (int)fbh) ? iw : (int)fbh;
    sh = (ih < (int)fbw) ? ih : (int)fbw;
    for (y = 0; y < sh; y++) {
        const uint8_t *src = rgba + (size_t)y * iw * 4;
        for (x = 0; x < sw; x++) {
            const uint8_t *p = src + (size_t)x * 4;
            u8 *d = fb + ((size_t)x * fbw + (fbw - 1 - y)) * 3;
            d[0] = p[2];   /* B */
            d[1] = p[1];   /* G */
            d[2] = p[0];   /* R */
        }
    }
}
void tc_video_present(void) {
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
}
void tc_video_wait(void) { gspWaitForVBlank(); }   /* no swap: see platform.h */

/* ---- input: touchscreen (bottom screen) ------------------------------- */
void tc_input_poll(void) {
    touchPosition tp;
    u32 held;
    if (!aptMainLoop()) { g_quit = 1; return; }
    hidScanInput();
    held = hidKeysHeld();
    if (hidKeysDown() & KEY_START) g_quit = 1;
    if (held & KEY_TOUCH) {
        hidTouchRead(&tp);
        g_ptr.x = tp.px; g_ptr.y = tp.py; g_ptr.down = 1;
    } else {
        g_ptr.down = 0;
    }
}
void tc_input_pointer(tc_pointer *o) { *o = g_ptr; }
int  tc_input_quit(void) { return g_quit; }
int  tc_input_scroll(void) {
    u32 h = hidKeysHeld();
    if (h & (KEY_DUP | KEY_L))   return -1;
    if (h & (KEY_DDOWN | KEY_R)) return  1;
    return 0;
}

/* ---- time / memory / log ---------------------------------------------- */
uint32_t tc_millis(void)          { return (uint32_t)(svcGetSystemTick() / 268123); }
void     tc_sleep_ms(uint32_t ms) { svcSleepThread((s64)ms * 1000000LL); }
int      tc_random(void *buf, size_t n) {
    /* psInit() is done once in tc_3ds_video_init; PS is the hardware RNG. */
    return R_SUCCEEDED(PS_GenerateRandomBytes(buf, n)) ? 0 : -1;
}
void    *tc_alloc(size_t n)       { return malloc(n); }
void    *tc_realloc(void *p, size_t n) { return realloc(p, n); }
void     tc_free(void *p)         { free(p); }
void     tc_log(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); printf("\n"); va_end(ap);
}
