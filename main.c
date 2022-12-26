#include "pico/stdlib.h"
#include <hardware/structs/rosc.h>
#include <stdarg.h>
#include "puzzles.h"

void fatal(const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "fatal error: ");

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\n");
    exit(1);
}

uint32_t hwrand32()
{
    /*
     * https://forums.raspberrypi.com/viewtopic.php?p=1815162&sid=31b805cc028d4e7f81f59f5cd0fab4af#p1815162
     */
    uint32_t random = 0;
    uint32_t random_bit;

    for (int k = 0; k < 32; k++) {
        while (1) {
            random_bit = rosc_hw->randombit & 1;
            if (random_bit != (rosc_hw->randombit & 1)) break;
        }

        random = (random << 1) | random_bit;
    }

    return random;
}

int main() {
    stdio_uart_init();

    /* Seed random state */
    uint32_t seed = hwrand32();
    random_state *rs = random_new((void*)&seed, sizeof(seed));

    while (true) {
        game_params *p;
        game_state *s;
        char *desc, *aux = NULL;

        p = thegame.default_params();

        /* Generate a puzzle (this may take a while) */
        desc = thegame.new_desc(p, rs, &aux, false);
        s = thegame.new_game(NULL, p, desc);

        printf("%s\r\n", thegame.text_format(s));

        /* Wait 60s, print another one */
        sleep_ms(60000);
    }

    return 0;
}
