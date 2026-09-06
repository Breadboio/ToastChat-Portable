/* ToastChat for the Nintendo 3DS. Bottom screen draws, top screen shows the
 * log - the layout PictoChat had. App logic is shared core/tc_app.c. */
#include <3ds.h>
#include "../../core/tc_app.h"
#include "../platform.h"

#ifndef TC_HOST
#define TC_HOST "192.168.1.167"
#endif
#ifndef TC_PORT
#define TC_PORT 3401
#endif
#ifndef TC_ROOM
#define TC_ROOM "C"
#endif

void tc_3ds_video_init(void);
void tc_3ds_video_exit(void);
int  tc_3ds_net_init(void);
void tc_3ds_net_exit(void);

int main(int argc, char **argv) {
    int rc;
    (void)argc; (void)argv;
    tc_3ds_video_init();
    tc_3ds_net_init();
    rc = tc_app_run(320, 240, TC_HOST, TC_PORT, TC_ROOM, "3DS", "DRAW");
    tc_3ds_net_exit();
    tc_3ds_video_exit();
    return rc;
}
