/* ToastChat for Android (NativeActivity). There is no Java in this app: the
 * framework's android.app.NativeActivity loads libtoastchat.so and calls
 * android_main on its own thread, so the blocking core loop is free to block.
 *
 * The only decision made here is the logical render size. The core is given a
 * fixed 640-wide screen - the regime the compact layout was tuned for - with
 * the height derived from the real surface aspect, so the hardware scale stays
 * square. See plat_android.c for why that also removes the software scaler.
 */
#include <android_native_app_glue.h>
#include <android/log.h>
#include "../../core/tc_app.h"

#ifndef TC_HOST
#define TC_HOST "breadtoasting.com"
#endif
#ifndef TC_PORT
#define TC_PORT 443
#endif
#ifndef TC_TLS
#define TC_TLS 1
#endif

/* 640 is not arbitrary: below ~572 the compact toolbar's room button runs off
 * the right edge, and at 900 the layout switches to the wide one, which wants
 * 16 swatches in a single row. */
#define TC_LOGICAL_W    640
#define TC_LOGICAL_H_MIN 480
#define TC_LOGICAL_H_MAX 1600

void tc_android_bind(struct android_app *app);
int  tc_android_wait_for_window(void);
void tc_android_surface_size(int *w, int *h);
void tc_android_set_logical(int w, int h);

void android_main(struct android_app *app) {
    int sw = 0, sh = 0, lw = TC_LOGICAL_W, lh;

    tc_android_bind(app);
    if (tc_android_wait_for_window() != 0) return;
    tc_android_surface_size(&sw, &sh);
    if (sw <= 0 || sh <= 0) return;

    lh = (int)((long long)lw * sh / sw);
    if (lh < TC_LOGICAL_H_MIN) lh = TC_LOGICAL_H_MIN;
    if (lh > TC_LOGICAL_H_MAX) lh = TC_LOGICAL_H_MAX;
    lh &= ~1;                                  /* keep rows even for the scaler */
    tc_android_set_logical(lw, lh);

    __android_log_print(ANDROID_LOG_INFO, "toastchat",
                        "surface %dx%d -> logical %dx%d, %s:%d tls=%d",
                        sw, sh, lw, lh, TC_HOST, TC_PORT, TC_TLS);

    tc_app_run(lw, lh, TC_HOST, TC_PORT, TC_TLS, "Android", "TOUCH TO DRAW");

    /* The core returned, so the user asked to leave. Tell the framework, then
     * keep pumping until it actually tears the activity down - returning from
     * android_main without this leaves a zombie window on screen. */
    ANativeActivity_finish(app->activity);
    while (!app->destroyRequested) {
        int events;
        struct android_poll_source *src;
        while (ALooper_pollOnce(-1, NULL, &events, (void **)&src) >= 0) {
            if (src) src->process(app, src);
            if (app->destroyRequested) break;
        }
    }
}
