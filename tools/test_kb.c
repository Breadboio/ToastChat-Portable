#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"
#include "../core/tc_keyboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void shot(const char *out, int W, int H, const char *nick) {
    uint8_t *b = (uint8_t *)calloc((size_t)W * H * 4, 1);
    uint8_t *f = (uint8_t *)malloc((size_t)W * H * 3);
    long k;
    tc_keyboard_render(b, W, H, nick, 10);
    for (k = 0; k < (long)W * H; k++) {
        f[k*3+0] = b[k*4+0]; f[k*3+1] = b[k*4+1]; f[k*3+2] = b[k*4+2];
    }
    stbi_write_png(out, W, H, 3, f, W * 3);
    printf("  %-34s %dx%d\n", out, W, H);
    free(b); free(f);
}

int main(void) {
    int fails = 0, i;
    shot("/src/build/kb_switch.png", 1280, 720, "BREADBOI");
    shot("/src/build/kb_wii.png", 640, 480, "GARRETT");
    shot("/src/build/kb_3ds.png", 320, 240, "TOAST");

    /* hit test must agree with what was drawn */
    puts("== hit test ==");
    for (i = 0; i < 3; i++) {
        int W = (int[]){1280, 640, 320}[i], H = (int[]){720, 480, 240}[i];
        char nick[12] = "";
        char k;
        /* top-left cell is 'A' - probe just inside it */
        int ox, oy;
        /* walk the screen to find the first cell centre that reports 'A' */
        int found_a = 0, found_ok = 0, x, y;
        for (y = 0; y < H && !found_a; y += 2)
            for (x = 0; x < W; x += 2)
                if (tc_keyboard_key_at(W, H, x, y) == 'A') { found_a = 1; ox = x; oy = y; break; }
        for (y = H - 1; y >= 0 && !found_ok; y -= 2)
            for (x = W - 1; x >= 0; x -= 2)
                if (tc_keyboard_key_at(W, H, x, y) == '\n') { found_ok = 1; break; }
        printf("  [%s] %4dx%-4d 'A' reachable=%d  OK reachable=%d\n",
               (found_a && found_ok) ? "PASS" : "FAIL", W, H, found_a, found_ok);
        if (!found_a || !found_ok) fails++;
        (void)ox; (void)oy;
        /* typing */
        tc_keyboard_apply('T', nick, sizeof(nick));
        tc_keyboard_apply('O', nick, sizeof(nick));
        tc_keyboard_apply('\b', nick, sizeof(nick));
        tc_keyboard_apply('A', nick, sizeof(nick));
        k = tc_keyboard_apply('\n', nick, sizeof(nick));
        printf("  [%s] typing T,O,backspace,A -> \"%s\" (ok=%d)\n",
               strcmp(nick, "TA") == 0 && k == 1 ? "PASS" : "FAIL", nick, k);
        if (strcmp(nick, "TA") || k != 1) fails++;
    }
    /* cap at nick_max */
    {
        char n[11] = "";
        int j;
        for (j = 0; j < 30; j++) tc_keyboard_apply('X', n, sizeof(n));
        printf("  [%s] length capped -> %d chars\n", strlen(n) == 10 ? "PASS" : "FAIL", (int)strlen(n));
        if (strlen(n) != 10) fails++;
    }
    printf("\n%s\n", fails ? "FAILED" : "ALL PASS");
    return fails ? 1 : 0;
}
