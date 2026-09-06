/* tc.h - portable ToastChat client core. Platform-independent, C99.
 * Drive it with tc_client_poll() from the port's main loop; it never blocks
 * for longer than one recv, and never calls back into platform video/input.
 */
#ifndef TC_H
#define TC_H

#include <stdint.h>
#include <stddef.h>

#define TC_NICK_MAX     10        /* server truncates past this            */
#define TC_ROOMS        4         /* A-D                                   */
#define TC_PALETTE      16        /* DS favourite colours                  */
#define TC_MAX_SEND     (400*1024)/* stay under ws maxPayload (~464KB)     */
#define TC_LOG_KEEP     12        /* entries a port keeps decoded in RAM;
                                   * the server sends up to 40 - see §4    */

typedef enum {
    TC_ST_IDLE, TC_ST_CONNECTING, TC_ST_HANDSHAKE,
    TC_ST_LOBBY, TC_ST_JOINING, TC_ST_IN_ROOM, TC_ST_ERROR
} tc_state;

typedef struct {
    char     id[40];
    char     nick[TC_NICK_MAX + 1];
    uint32_t color;               /* 0xRRGGBB                              */
    int      w, h;
    uint8_t *rgba;                /* decoded; NULL until tc_entry_decode() */
    int      is_system;
    char    *text;                /* system lines only                     */
} tc_entry;

typedef struct tc_client tc_client;

tc_client *tc_client_new(const char *host, int port, const char *path);
void       tc_client_free(tc_client *c);

void       tc_client_identify(tc_client *c, const char *nick, int palette_idx);
void       tc_client_join(tc_client *c, int room /*0..3*/);
void       tc_client_leave(tc_client *c);

/* Pump once per frame. Returns 1 if anything changed (redraw warranted). */
int        tc_client_poll(tc_client *c);

tc_state   tc_client_state(const tc_client *c);
const char*tc_client_error(const tc_client *c);
int        tc_client_counts(const tc_client *c, int out[TC_ROOMS]);
uint32_t   tc_client_palette(const tc_client *c, int idx);

int              tc_client_entry_count(const tc_client *c);
const tc_entry  *tc_client_entry(const tc_client *c, int i);
int              tc_entry_decode(tc_client *c, int i);  /* base64+PNG -> rgba */

/* ---- drawing model ------------------------------------------------------
 * Strokes, not pixels: undo is free and the send path rasterises once.
 */
void tc_draw_begin(tc_client *c, int x, int y, int palette_idx, int width);
void tc_draw_to(tc_client *c, int x, int y);
void tc_draw_end(tc_client *c);
void tc_draw_undo(tc_client *c);
void tc_draw_clear(tc_client *c);
void tc_draw_render(tc_client *c, uint8_t *rgba, int w, int h);

/* Rasterise the current strokes, PNG-encode, base64, and send. 0 on success. */
int  tc_send_drawing(tc_client *c, int w, int h);

#endif /* TC_H */
