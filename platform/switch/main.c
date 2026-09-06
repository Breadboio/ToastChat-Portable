/* ToastChat for Nintendo Switch. Everything below platform bring-up is shared
 * core - see core/tc_app.c. */
#include <switch.h>
#include "../../core/tc_app.h"

#ifndef TC_HOST
#define TC_HOST "192.168.1.167"
#endif
#ifndef TC_PORT
#define TC_PORT 3401
#endif
#ifndef TC_ROOM
#define TC_ROOM "C"
#endif
#ifndef TC_TLS
#define TC_TLS 0
#endif

void tc_switch_video_init(void);
void tc_switch_video_exit(void);
void tc_switch_input_init(void);

int main(int argc, char **argv) {
    int rc;
    (void)argc; (void)argv;
    socketInitializeDefault();
    tc_switch_video_init();
    tc_switch_input_init();
    rc = tc_app_run(1280, 720, TC_HOST, TC_PORT, TC_TLS, TC_ROOM, "Switch", "TOUCH TO DRAW");
    socketExit();
    tc_switch_video_exit();
    return rc;
}
