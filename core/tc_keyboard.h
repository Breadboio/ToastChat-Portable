#ifndef TC_KEYBOARD_H
#define TC_KEYBOARD_H
#include <stdint.h>
#include <stddef.h>

/* On-screen keyboard for picking a nickname.
 *
 * Deliberately implemented in core rather than using each console's native
 * keyboard (3DS swkbd, Switch swkbd): the Wii has none, and one implementation
 * driven by the same pointer contract behaves identically everywhere and can be
 * reviewed as a PNG on the desktop like the rest of the UI.
 *
 * Draws into `rgba` (W x H). `key_at` is the hit test; the two are generated
 * from one layout so they cannot disagree. */

#define TC_KB_COLS 10
#define TC_KB_ROWS 4

void tc_keyboard_render(uint8_t *rgba, int W, int H, const char *nick, int nick_max);

/* Companion screen for dual-screen consoles while the keyboard is up. */
void tc_keyboard_render_top(uint8_t *rgba, int W, int H, const char *nick);

/* Returns the character for the cell under (x,y): a letter/digit, '\b' for
 * backspace, '\n' for OK, or 0 for no hit. */
char tc_keyboard_key_at(int W, int H, int x, int y);

/* Applies a key to the buffer. Returns 1 when the user pressed OK. */
int  tc_keyboard_apply(char key, char *nick, size_t nick_max);
#endif
