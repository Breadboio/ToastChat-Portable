#include "tc_recv.h"
#include "tc_json.h"
#include <string.h>

/* "#rrggbb" -> 0xRRGGBB, falling back to the DS grey. */
static uint32_t parse_color(const char *obj) {
    char c[10];
    uint32_t v = 0;
    int i;
    if (tc_json_top_str(obj, "color", c, sizeof(c)) < 7 || c[0] != '#') return 0x8c8f94;
    for (i = 1; i < 7; i++) {
        char d = c[i];
        v <<= 4;
        if (d >= '0' && d <= '9') v |= (uint32_t)(d - '0');
        else if (d >= 'a' && d <= 'f') v |= (uint32_t)(d - 'a' + 10);
        else if (d >= 'A' && d <= 'F') v |= (uint32_t)(d - 'A' + 10);
        else return 0x8c8f94;
    }
    return v;
}

#define SLOT ((size_t)TC_THUMB_W * TC_THUMB_H * 4)

void tc_recv_push(tc_ui *ui, uint8_t *pool, const char *obj) {
    const char *png;
    size_t plen = 0;
    int i, w = 0, h = 0;

    if (!obj) return;
    png = tc_json_top_raw(obj, "png", &plen);

    if (ui->nlog < TC_UI_LOG_MAX) ui->nlog++;
    for (i = ui->nlog - 1; i > 0; i--) {
        int had_image = (ui->log[i - 1].rgba != NULL);
        ui->log[i] = ui->log[i - 1];
        /* Only move pixels that exist - system lines are text only, and a room
         * with a lot of join/leave traffic would otherwise memcpy the whole
         * pool for every notice. */
        if (had_image) {
            memcpy(pool + (size_t)i * SLOT, pool + (size_t)(i - 1) * SLOT, SLOT);
            ui->log[i].rgba = pool + (size_t)i * SLOT;
        } else {
            ui->log[i].rgba = NULL;
        }
    }
    memset(&ui->log[0], 0, sizeof(ui->log[0]));
    tc_json_top_str(obj, "nick", ui->log[0].nick, sizeof(ui->log[0].nick));
    ui->log[0].color = parse_color(obj);

    if (!png) {                            /* system line: keep it, as PictoChat does */
        ui->log[0].is_sys = 1;
        tc_json_top_str(obj, "text", ui->log[0].text, sizeof(ui->log[0].text));
        ui->log[0].rgba = NULL;
        return;
    }
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
