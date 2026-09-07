/* Android shim (NDK / NativeActivity). Sockets, time, alloc and randomness are
 * plain bionic - Android is Linux - so the interesting parts are video, input,
 * and the fact that this is the first target with no fixed screen size.
 *
 * Screen strategy: the core renders at a FIXED logical width of 640 (see
 * TC_LOGICAL_W in main.c) and ANativeWindow_setBuffersGeometry hands that
 * buffer to the compositor, which scales it to the panel in hardware. So there
 * is no software scaler here at all - present() is a row memcpy - and the UI
 * lands in the same 640-wide regime the Wii layout was tuned for. The logical
 * HEIGHT is derived from the real surface aspect so the scale stays square and
 * nothing is letterboxed.
 *
 * Consequence worth knowing: touch events arrive in SURFACE pixels, not buffer
 * pixels, so they have to be scaled back down by hand. Getting this wrong is
 * invisible on a 1:1 emulator and obvious on a real phone.
 */
#include "../platform.h"
#include "tc_gesture.h"
#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/keycodes.h>
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

#define TAG "toastchat"

/* ---- 1. sockets: bionic is POSIX, so this matches plat_posix.c ---------- */
struct tc_sock { int fd; };

tc_sock *tc_sock_open(const char *host, int port) {
    struct addrinfo hints, *res = NULL, *it;
    char portstr[16];
    tc_sock *s;
    int fd = -1, one = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;      /* unlike the consoles, phones get v6 */
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
    if (r == 0) return -1;
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
    return -1;
}

void tc_sock_close(tc_sock *s) { if (s) { close(s->fd); free(s); } }

/* ---- 4/5/6/7. time, memory, randomness, logging ------------------------ */
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
void *tc_alloc(size_t n)            { return malloc(n); }
void *tc_realloc(void *p, size_t n) { return realloc(p, n); }
void  tc_free(void *p)              { free(p); }
void  tc_log(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    __android_log_vprint(ANDROID_LOG_INFO, TAG, fmt, ap);
    va_end(ap);
}

/* ---- app-global state -------------------------------------------------- */
static struct android_app *g_app;
static int  g_logical_w, g_logical_h;   /* what the core renders at          */
static int  g_surf_w, g_surf_h;         /* real surface px; touch lives here */
static int  g_window_ok;                /* a surface exists right now        */
static int  g_quit;

/* The pending frame is the core's own buffer, not a copy: blit records where
 * it is and present copies it into the locked window. The core always pairs
 * the two calls, and the buffer outlives both. */
static const uint8_t *g_pending;
static int g_pend_w, g_pend_h;

/* Log-band scrolling. The 3DS puts the log on a screen with no touch panel, so
 * tc_input_scroll() is buttons there; a phone has one screen and no buttons,
 * so a drag that STARTS above the canvas scrolls instead of poking the UI.
 * The state machine lives in tc_gesture.c, which has no Android headers and is
 * driven on x86 by tools/test_gesture.c. */
static tc_gesture g_gest;
static int g_gest_ready;

void tc_video_size(tc_screen which, int *w, int *h) {
    (void)which; *w = g_logical_w; *h = g_logical_h;
}

void tc_video_blit(tc_screen which, const uint8_t *rgba, int w, int h) {
    (void)which;
    g_pending = rgba; g_pend_w = w; g_pend_h = h;
}

void tc_video_present(void) {
    ANativeWindow_Buffer buf;
    int y, rowbytes;
    if (!g_window_ok || !g_app || !g_app->window || !g_pending) return;
    if (ANativeWindow_lock(g_app->window, &buf, NULL) != 0) return;
    /* WINDOW_FORMAT_RGBX_8888 is R,G,B,X in memory - the same byte order the
     * core writes - so a row is a memcpy. buf.stride counts PIXELS. */
    rowbytes = (g_pend_w < buf.width ? g_pend_w : buf.width) * 4;
    for (y = 0; y < g_pend_h && y < buf.height; y++)
        memcpy((uint8_t *)buf.bits + (size_t)y * buf.stride * 4,
               g_pending + (size_t)y * g_pend_w * 4, (size_t)rowbytes);
    ANativeWindow_unlockAndPost(g_app->window);
    g_pending = NULL;
}

/* unlockAndPost throttles on the buffer queue, so a drawn frame already waits
 * for vsync. A skipped frame has nothing to block on and must sleep. */
void tc_video_wait(void) { tc_sleep_ms(16); }

/* ---- input ------------------------------------------------------------- */
static int32_t on_input(struct android_app *app, AInputEvent *ev) {
    (void)app;
    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_KEY) {
        if (AKeyEvent_getKeyCode(ev) == AKEYCODE_BACK) {
            if (AKeyEvent_getAction(ev) == AKEY_EVENT_ACTION_UP) g_quit = 1;
            return 1;                       /* consume, or it also finishes us */
        }
        return 0;
    }
    if (AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION) return 0;
    if (!g_gest_ready) return 1;          /* no surface measured yet */
    {
        int32_t act = AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_MASK;
        int g;
        if      (act == AMOTION_EVENT_ACTION_DOWN)   g = TC_G_DOWN;
        else if (act == AMOTION_EVENT_ACTION_MOVE)   g = TC_G_MOVE;
        else if (act == AMOTION_EVENT_ACTION_UP ||
                 act == AMOTION_EVENT_ACTION_CANCEL) g = TC_G_UP;
        else return 1;                    /* extra pointers: one finger only */
        tc_gesture_event(&g_gest, g, AMotionEvent_getX(ev, 0),
                                     AMotionEvent_getY(ev, 0));
        return 1;
    }
}

static void on_cmd(struct android_app *app, int32_t cmd) {
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
        if (app->window) {
            /* Measure BEFORE setting geometry: afterwards getWidth reports the
             * buffer size, and touch coordinates are still in surface px. */
            g_surf_w = ANativeWindow_getWidth(app->window);
            g_surf_h = ANativeWindow_getHeight(app->window);
            if (g_logical_w > 0) {
                ANativeWindow_setBuffersGeometry(app->window, g_logical_w,
                                                 g_logical_h, WINDOW_FORMAT_RGBX_8888);
                g_window_ok = 1;
            }
        }
        break;
    case APP_CMD_TERM_WINDOW:
        g_window_ok = 0;
        g_pending = NULL;
        break;
    case APP_CMD_DESTROY:
        g_quit = 1;
        break;
    default:
        break;
    }
}

/* Drains everything the looper has without blocking. Called once per frame by
 * the core loop, which is also what keeps the activity responsive. */
void tc_input_poll(void) {
    int events;
    struct android_poll_source *src;
    while (ALooper_pollOnce(0, NULL, &events, (void **)&src) >= 0) {
        if (src) src->process(g_app, src);
        if (g_app->destroyRequested) { g_quit = 1; break; }
    }
}
void tc_input_pointer(tc_pointer *o) { *o = g_gest.ptr; }
int  tc_input_quit(void) { return g_quit; }
int  tc_input_scroll(void) { return tc_gesture_scroll(&g_gest); }

/* ---- bring-up, called from main.c -------------------------------------- */
void tc_android_bind(struct android_app *app) {
    g_app = app;
    app->onAppCmd     = on_cmd;
    app->onInputEvent = on_input;
}

/* Blocks until there is a surface to measure, or the user leaves. Returns 0
 * once g_surf_w/h are known. */
int tc_android_wait_for_window(void) {
    while (!g_quit) {
        int events;
        struct android_poll_source *src;
        while (ALooper_pollOnce(-1, NULL, &events, (void **)&src) >= 0) {
            if (src) src->process(g_app, src);
            if (g_app->destroyRequested) return -1;
            if (g_surf_w > 0 && g_surf_h > 0) return 0;
        }
    }
    return -1;
}

void tc_android_surface_size(int *w, int *h) { *w = g_surf_w; *h = g_surf_h; }

void tc_android_set_logical(int w, int h) {
    g_logical_w = w; g_logical_h = h;
    tc_gesture_init(&g_gest, g_surf_w, g_surf_h, w, h);
    g_gest_ready = 1;
    if (g_app && g_app->window) {
        ANativeWindow_setBuffersGeometry(g_app->window, w, h, WINDOW_FORMAT_RGBX_8888);
        g_window_ok = 1;
    }
}
