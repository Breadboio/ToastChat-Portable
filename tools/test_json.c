#include <stdio.h>
#include <string.h>
#include "../core/tc_json.h"

static int fails = 0;
static void eq(const char *what, int got, int want) {
    printf("  [%s] %-46s got=%d want=%d\n", got == want ? "PASS" : "FAIL", what, got, want);
    if (got != want) fails++;
}

int main(void) {
    /* real shapes from docs/PROTOCOL.md */
    const char *hello = "{\"t\":\"hello\",\"id\":\"abc\",\"rooms\":{\"A\":0,\"B\":2},\"max\":16,"
                        "\"colors\":[\"#8c8f94\",\"#7a4c2a\"],\"ttlMinutes\":20}";
    const char *joined0 = "{\"t\":\"joined\",\"room\":\"C\",\"log\":[],\"users\":[],\"rooms\":{\"A\":0}}";
    const char *joined1 = "{\"t\":\"joined\",\"room\":\"C\",\"log\":[],"
                          "\"users\":[{\"id\":\"a\",\"nick\":\"3DS\",\"color\":\"#2fa89a\"}],\"rooms\":{}}";
    const char *joined3 = "{\"t\":\"joined\",\"room\":\"C\","
                          "\"log\":[{\"kind\":\"sys\",\"text\":\"x entered\"}],"
                          "\"users\":[{\"nick\":\"a\"},{\"nick\":\"b\"},{\"nick\":\"c\"}],\"max\":16}";
    /* a nested key must not shadow the top level */
    const char *shadow = "{\"t\":\"entry\",\"entry\":{\"max\":99,\"nick\":\"bob\"},\"max\":16}";
    /* escaped quotes and braces inside strings must not confuse depth tracking */
    const char *tricky = "{\"t\":\"entry\",\"text\":\"he said \\\"hi\\\" {not a brace}\","
                         "\"users\":[{\"n\":\"a\"},{\"n\":\"b\"}],\"max\":7}";

    puts("== tc_json ==");
    eq("hello.max", tc_json_top_int(hello, "max", -1), 16);
    eq("hello.ttlMinutes", tc_json_top_int(hello, "ttlMinutes", -1), 20);
    eq("hello.colors length", tc_json_array_len(tc_json_top(hello, "colors")), 2);
    eq("joined users=[] -> 0", tc_json_array_len(tc_json_top(joined0, "users")), 0);
    eq("joined users=1", tc_json_array_len(tc_json_top(joined1, "users")), 1);
    eq("joined users=3", tc_json_array_len(tc_json_top(joined3, "users")), 3);
    eq("joined log=1 (not users)", tc_json_array_len(tc_json_top(joined3, "log")), 1);
    eq("nested max does not shadow", tc_json_top_int(shadow, "max", -1), 16);
    eq("missing key -> default", tc_json_top_int(hello, "nope", -5), -5);
    eq("escapes/braces in strings", tc_json_array_len(tc_json_top(tricky, "users")), 2);
    eq("max after tricky string", tc_json_top_int(tricky, "max", -1), 7);
    eq("array len on non-array", tc_json_array_len(tc_json_top(hello, "max")), -1);

    printf("\n%s (%d failure%s)\n", fails ? "FAILED" : "ALL PASS", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
