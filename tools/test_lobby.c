#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"
#include "../core/tc_lobby.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void shot(const char *out, int W, int H, const int c[4], int max, int top) {
    uint8_t *b = (uint8_t *)calloc((size_t)W * H * 4, 1);
    uint8_t *f = (uint8_t *)malloc((size_t)W * H * 3);
    long k;
    if (top) tc_lobby_render_top(b, W, H, c, max);
    else     tc_lobby_render(b, W, H, c, max, "BREADBOI", 1);
    for (k = 0; k < (long)W * H; k++) {
        f[k*3] = b[k*4]; f[k*3+1] = b[k*4+1]; f[k*3+2] = b[k*4+2];
    }
    stbi_write_png(out, W, H, 3, f, W * 3);
    printf("  %-36s %dx%d\n", out, W, H);
    free(b); free(f);
}

int main(void) {
    static const int counts[4] = { 3, 0, 16, 7 };   /* C is full */
    int fails = 0, i;
    shot("/src/build/lobby_switch.png", 1280, 720, counts, 16, 0);
    shot("/src/build/lobby_wii.png", 640, 480, counts, 16, 0);
    shot("/src/build/lobby_3ds.png", 320, 240, counts, 16, 0);
    shot("/src/build/lobby_3ds_top.png", 400, 240, counts, 16, 1);

    puts("== hit test ==");
    for (i = 0; i < 3; i++) {
        int W = (int[]){1280, 640, 320}[i], H = (int[]){720, 480, 240}[i];
        int ok = 1, r;
        int j;
        for (j = 0; j < 4; j++) {
            tc_rect c = tc_lobby_card_rect(W, H, j);
            r = tc_lobby_room_at(W, H, c.x + c.w / 2, c.y + c.h / 2, counts, 16);
            if (j == 2) { if (r != -1) ok = 0; }        /* full room refuses */
            else if (r != j) ok = 0;
            if (c.x < 0 || c.y < 0 || c.x + c.w > W || c.y + c.h > H) ok = 0;
        }
        r = tc_lobby_room_at(W, H, 1, H - 1, counts, 16);
        printf("  [%s] %4dx%-4d cards on-screen, centres hit, full room refused\n",
               ok ? "PASS" : "FAIL", W, H);
        if (!ok) fails++;
    }
    printf("\n%s\n", fails ? "FAILED" : "ALL PASS");
    return fails ? 1 : 0;
}
