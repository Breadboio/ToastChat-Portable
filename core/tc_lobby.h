#ifndef TC_LOBBY_H
#define TC_LOBBY_H
#include <stdint.h>
#include "tc_ui.h"

/* Room picker shown before joining, and reachable again from the RM button.
 * Mirrors the web client's lobby: a card per room with a big letter, the
 * occupancy count, and a fill bar. A full room is drawn dimmed and not
 * selectable. */

void    tc_lobby_render(uint8_t *rgba, int W, int H, const int counts[4],
                        int max, const char *nick, int connected);
void    tc_lobby_render_top(uint8_t *rgba, int W, int H, const int counts[4], int max);
tc_rect tc_lobby_card_rect(int W, int H, int i);

/* Room index 0-3 under (x,y), or -1. Returns -1 for a full room. */
int     tc_lobby_room_at(int W, int H, int x, int y, const int counts[4], int max);
#endif
