#ifndef TC_SEND_H
#define TC_SEND_H
#include "tc_ws.h"
#include "tc_draw.h"
/* Rasterise -> downsample -> PNG -> base64 -> {"t":"msg"}. 0 ok, negative else.
 * -10 means the encoded frame exceeded TC_MAX_SEND (the ws maxPayload cliff). */
int tc_send_canvas(tc_ws *ws, const tc_canvas *c, int w, int h, int shrink);

/* The rasterise -> downsample -> PNG step on its own, so tests can inspect the
 * exact bytes that go on the wire. Caller tc_free()s the result. */
uint8_t *tc_render_png(const tc_canvas *c, int w, int h, int shrink,
                       size_t *out_len, int *out_w, int *out_h);
#endif
