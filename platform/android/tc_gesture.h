#ifndef TC_GESTURE_H
#define TC_GESTURE_H
/* Touch interpretation for the Android port, kept free of every Android
 * header so it can be driven on x86 by tools/test_gesture.c. This is where the
 * port's only real logic lives - the rest of plat_android.c is bionic sockets
 * and a memcpy - so it is the part worth testing without a phone.
 */
#include "../platform.h"

enum { TC_G_DOWN = 0, TC_G_MOVE = 1, TC_G_UP = 2 };

/* Logical px of drag that equals one row of scroll. */
#define TC_SCROLL_STEP 48

typedef struct {
    int surf_w, surf_h;         /* real surface px - what touch reports in */
    int logical_w, logical_h;   /* what the core renders at                */
    int canvas_y;               /* first row of canvas; above it is the log */
    int scrolling;              /* this gesture is a log drag, not a stroke */
    int accum, last_y;
    int ticks;                  /* signed, drained one per tc_gesture_scroll */
    tc_pointer ptr;
} tc_gesture;

void tc_gesture_init(tc_gesture *g, int surf_w, int surf_h, int lw, int lh);
/* Raw surface coordinates in, pointer/scroll state out. */
void tc_gesture_event(tc_gesture *g, int action, float sx, float sy);
int  tc_gesture_scroll(tc_gesture *g);

#endif
