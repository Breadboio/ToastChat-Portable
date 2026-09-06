#ifndef TC_IMGDEC_H
#define TC_IMGDEC_H
#include <stdint.h>
#include <stddef.h>

/* Largest thumbnail we keep per log entry. Drawings arrive at up to ~640x465,
 * which is ~1.2MB decoded; we shrink immediately and free the full image, so
 * steady-state cost is bounded no matter what anyone sends. */
#define TC_THUMB_W 128
#define TC_THUMB_H 96

/* Decode a "data:image/png;base64,..." payload into `out` (TC_THUMB_W *
 * TC_THUMB_H * 4 bytes), shrunk to fit while preserving aspect ratio.
 * Writes the actual size used into *ow and *oh. Returns 0 on success. */
int tc_img_thumb(const char *b64url, size_t len, uint8_t *out, int *ow, int *oh);
#endif
