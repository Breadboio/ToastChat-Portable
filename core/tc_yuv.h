#ifndef TC_YUV_H
#define TC_YUV_H
#include <stdint.h>
/* RGB -> YUY2 (YUV 4:2:2), two horizontal pixels per 32-bit word: Y1 Cb Y2 Cr.
 * Lives in core/ rather than the Wii shim so the desktop target can round-trip
 * it and prove the colour maths, which is otherwise invisible until it is on a
 * TV. Chroma is taken from the left pixel of each pair, as the hardware does. */
static uint32_t tc_rgb_to_yuy2(uint8_t r1, uint8_t g1, uint8_t b1,
                               uint8_t r2, uint8_t g2, uint8_t b2) {
    int y1 = (299 * r1 + 587 * g1 + 114 * b1) / 1000;
    int y2 = (299 * r2 + 587 * g2 + 114 * b2) / 1000;
    int cb = (-16874 * r1 - 33126 * g1 + 50000 * b1 + 12800000) / 100000;
    int cr = (50000 * r1 - 41869 * g1 - 8131 * b1 + 12800000) / 100000;
    if (y1 < 0) y1 = 0;
    if (y1 > 255) y1 = 255;
    if (y2 < 0) y2 = 0;
    if (y2 > 255) y2 = 255;
    if (cb < 0) cb = 0;
    if (cb > 255) cb = 255;
    if (cr < 0) cr = 0;
    if (cr > 255) cr = 255;
    return (uint32_t)((y1 << 24) | (cb << 16) | (y2 << 8) | cr);
}
#endif
