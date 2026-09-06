/* platform.h - the entire porting surface for ToastChat.
 *
 * A new console is done when these compile and behave. Nothing above this line
 * knows what machine it is on: the core is C99, uses no threads, no C11, no
 * float-heavy math, and allocates through tc_alloc so a 16MB Dreamcast and a
 * Wii U can use different strategies.
 *
 * Deliberately NOT in here: TLS, HTTP, JSON, PNG, drawing, timers-with-callbacks.
 * Those live in core/ and are shared, so a port cannot get them subtly wrong.
 */
#ifndef TC_PLATFORM_H
#define TC_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

/* ---- 1. sockets ---------------------------------------------------------
 * Blocking BSD-style TCP. Every target has some form of this: 3DS soc:U,
 * Wii libogc net_*, Wii U nsysnet, Dreamcast KOS lwIP, Android/desktop libc.
 * No TLS: the LAN build speaks plain ws://. See docs/PROTOCOL.md §7.
 */
typedef struct tc_sock tc_sock;

tc_sock *tc_sock_open(const char *host, int port);   /* NULL on failure       */
int      tc_sock_send(tc_sock *s, const void *b, size_t n); /* -1 err, else n */
int      tc_sock_recv(tc_sock *s, void *b, size_t n);       /* 0 = would-block
                                                             * -1 = closed/err */
void     tc_sock_close(tc_sock *s);

/* ---- 2. video -----------------------------------------------------------
 * The core rasterises into a plain RGBA8888 buffer and hands it over. A port
 * converts to whatever the hardware wants (RGB565 on DC/Wii, tiled on 3DS).
 * TC_SCREEN_TOP is the log, TC_SCREEN_BOTTOM is the canvas. On single-screen
 * targets (Dreamcast, Android, desktop) the port composites both itself.
 */
typedef enum { TC_SCREEN_TOP = 0, TC_SCREEN_BOTTOM = 1 } tc_screen;

void tc_video_size(tc_screen which, int *w, int *h);
void tc_video_blit(tc_screen which, const uint8_t *rgba, int w, int h);
void tc_video_present(void);

/* ---- 3. pointer ---------------------------------------------------------
 * One pointer, because that is what PictoChat needs. 3DS/Wii U touchscreen,
 * Wii IR, Dreamcast mouse (or analog stick as a cursor), Android touch, desktop
 * mouse. Coordinates are in TC_SCREEN_BOTTOM space; the port does the mapping.
 * `down` must be edge-accurate - the stroke model depends on press/release.
 */
typedef struct { int x, y, down; } tc_pointer;

void tc_input_poll(void);
void tc_input_pointer(tc_pointer *out);
int  tc_input_quit(void);     /* HOME/START/window-close: 1 = user wants out */

/* ---- 4. time ------------------------------------------------------------ */
uint32_t tc_millis(void);
void     tc_sleep_ms(uint32_t ms);

/* ---- 5. memory ----------------------------------------------------------
 * Routed so tight targets can use a fixed arena instead of a real heap.
 * The core's worst single allocation is a full 40-entry `joined` log; see
 * docs/PROTOCOL.md §4 before raising limits on a 16MB machine.
 */
void *tc_alloc(size_t n);
void *tc_realloc(void *p, size_t n);
void  tc_free(void *p);

/* ---- 6. logging --------------------------------------------------------- */
void tc_log(const char *fmt, ...);

#endif /* TC_PLATFORM_H */
