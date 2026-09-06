#include "tc_recv.h"
#include "tc_json.h"
#include <string.h>

#define SLOT ((size_t)TC_THUMB_W * TC_THUMB_H * 4)

void tc_recv_push(tc_ui *ui, uint8_t *pool, const char *obj) {
    const char *png;
    size_t plen = 0;
    int i, w = 0, h = 0;

    if (!obj) return;
    png = tc_json_top_raw(obj, "png", &plen);
    if (!png) return;                      /* system line: nothing to show */

    if (ui->nlog < TC_UI_LOG_MAX) ui->nlog++;
    for (i = ui->nlog - 1; i > 0; i--) {
        ui->log[i] = ui->log[i - 1];
        memcpy(pool + (size_t)i * SLOT, pool + (size_t)(i - 1) * SLOT, SLOT);
        ui->log[i].rgba = pool + (size_t)i * SLOT;
    }
    memset(&ui->log[0], 0, sizeof(ui->log[0]));
    tc_json_top_str(obj, "nick", ui->log[0].nick, sizeof(ui->log[0].nick));

    if (tc_img_thumb(png, plen, pool, &w, &h) == 0) {
        ui->log[0].rgba = pool;
        ui->log[0].w = w;
        ui->log[0].h = h;
        ui->log[0].stride = TC_THUMB_W;
    } else {
        ui->log[0].rgba = NULL;            /* undecodable: placeholder box */
    }
}

void tc_recv_replay(tc_ui *ui, uint8_t *pool, const char *joined) {
    const char *e = tc_json_array_first(tc_json_top(joined, "log"));
    ui->nlog = 0;
    while (e) {
        tc_recv_push(ui, pool, e);
        e = tc_json_array_next(e);
    }
}
