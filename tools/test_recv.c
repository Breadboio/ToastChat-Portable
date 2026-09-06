/* Feeds the receive path a known `joined` message and checks the list matches. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../core/tc_recv.h"
#include "../core/tc_png.h"
#include "../core/tc_base64.h"

static int fails = 0;
static void eq(const char *what, int got, int want) {
    printf("  [%s] %-44s got=%d want=%d\n", got == want ? "PASS" : "FAIL", what, got, want);
    if (got != want) fails++;
}

/* a real, tiny PNG as a data URL */
static char *make_png_url(int w, int h) {
    uint8_t *rgba = (uint8_t *)calloc((size_t)w * h * 4, 1);
    size_t plen = 0; uint8_t *png; char *b64; char *url;
    int i;
    for (i = 0; i < w * h; i++) { rgba[i*4+0]=0x2f; rgba[i*4+1]=0x7d; rgba[i*4+2]=0x34; rgba[i*4+3]=255; }
    png = tc_png_encode(rgba, w, h, &plen);
    b64 = (char *)malloc(((plen + 2) / 3) * 4 + 8);
    tc_b64_encode(png, plen, b64);
    url = (char *)malloc(strlen(b64) + 40);
    sprintf(url, "data:image/png;base64,%s", b64);
    free(rgba); free(png); free(b64);
    return url;
}

int main(void) {
    tc_ui ui;
    uint8_t *pool = (uint8_t *)malloc(TC_POOL_BYTES);
    char *p1 = make_png_url(40, 20), *p2 = make_png_url(20, 40);
    char *doc = (char *)malloc(strlen(p1) + strlen(p2) + 1024);

    /* mirrors what the server really sends: mostly sys lines, a couple of msgs */
    sprintf(doc,
        "{\"t\":\"joined\",\"room\":\"C\",\"users\":[{\"n\":\"a\"}],\"log\":["
        "{\"kind\":\"sys\",\"id\":\"1\",\"text\":\"a entered Room C.\"},"
        "{\"kind\":\"sys\",\"id\":\"2\",\"text\":\"b entered Room C.\"},"
        "{\"kind\":\"msg\",\"id\":\"3\",\"nick\":\"Alice\",\"color\":\"#2f7d34\",\"w\":40,\"h\":20,\"png\":\"%s\"},"
        "{\"kind\":\"sys\",\"id\":\"4\",\"text\":\"c entered Room C.\"},"
        "{\"kind\":\"msg\",\"id\":\"5\",\"nick\":\"Bob\",\"color\":\"#c8352b\",\"w\":20,\"h\":40,\"png\":\"%s\"},"
        "{\"kind\":\"sys\",\"id\":\"6\",\"text\":\"d left Room C.\"}]}", p1, p2);

    memset(&ui, 0, sizeof(ui));
    puts("== receive path ==");
    tc_recv_replay(&ui, pool, doc);
    /* system lines are kept and shown, the way PictoChat shows them */
    eq("all 6 entries kept", ui.nlog, 6);
    eq("newest first: log[0] is the last sys line", ui.log[0].is_sys, 1);
    eq("log[0] text", strcmp(ui.log[0].text, "d left Room C.") == 0, 1);
    eq("log[1] is Bob", strcmp(ui.log[1].nick, "Bob") == 0, 1);
    eq("log[3] is Alice", strcmp(ui.log[3].nick, "Alice") == 0, 1);
    eq("sys rows carry no image", ui.log[0].rgba == NULL, 1);
    eq("Bob decoded", ui.log[1].rgba != NULL, 1);
    eq("Alice decoded", ui.log[3].rgba != NULL, 1);
    eq("portrait keeps aspect (20x40 -> h>w)", ui.log[1].h > ui.log[1].w, 1);
    eq("landscape keeps aspect (40x20 -> w>h)", ui.log[3].w > ui.log[3].h, 1);
    eq("distinct pool slots", ui.log[1].rgba != ui.log[3].rgba, 1);
    eq("colour parsed from the entry", ui.log[1].color != 0, 1);

    /* replay must not accumulate across calls */
    tc_recv_replay(&ui, pool, doc);
    eq("replay resets rather than appends", ui.nlog, 6);

    /* live entries push onto the front and cap at TC_UI_LOG_MAX */
    {
        char *e = (char *)malloc(strlen(p1) + 256);
        int i;
        sprintf(e, "{\"kind\":\"msg\",\"nick\":\"New\",\"png\":\"%s\"}", p1);
        for (i = 0; i < 10; i++) tc_recv_push(&ui, pool, e);
        eq("capped at TC_UI_LOG_MAX", ui.nlog, TC_UI_LOG_MAX);
        eq("front is the newest", strcmp(ui.log[0].nick, "New") == 0, 1);
        eq("newest decoded after shifting", ui.log[0].rgba != NULL, 1);
        free(e);
    }
    /* a system line becomes a text row, not a drawing */
    {
        tc_recv_push(&ui, pool, "{\"kind\":\"sys\",\"text\":\"x entered\"}");
        eq("sys line becomes a row", ui.log[0].is_sys, 1);
        eq("sys row has no image", ui.log[0].rgba == NULL, 1);
        eq("sys text captured", strcmp(ui.log[0].text, "x entered") == 0, 1);
    }
    printf("\n%s (%d failure%s)\n", fails ? "FAILED" : "ALL PASS", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
