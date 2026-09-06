/* ToastChat for the Wii (Homebrew Channel). Platform bring-up only; the app
 * itself is core/tc_app.c, shared with the Switch. */
#include <gccore.h>
#include "../../core/tc_app.h"
#include "../platform.h"

#ifndef TC_HOST
#define TC_HOST "breadtoasting.com"
#endif
#ifndef TC_PORT
#define TC_PORT 80
#endif
#ifndef TC_ROOM
#define TC_ROOM "C"
#endif
#ifndef TC_TLS
#define TC_TLS 0
#endif

void tc_wii_video_init(void);
void tc_wii_input_init(void);
int  tc_wii_net_init(void);

int main(int argc, char **argv) {
    int w = 640, h = 480;
    (void)argc; (void)argv;
    tc_wii_video_init();
    tc_wii_input_init();
    tc_video_size(TC_SCREEN_TOP, &w, &h);
    tc_wii_net_init();
    return tc_app_run(w, h, TC_HOST, TC_PORT, TC_TLS, TC_ROOM, "Wii", "POINT + HOLD A");
}
