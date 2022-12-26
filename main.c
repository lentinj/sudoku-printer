#include "pico/stdlib.h"
#include <hardware/structs/rosc.h>
#include "hardware/adc.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "puzzles.h"

/* Pull out internal game_state definition from solo.c */
typedef unsigned char digit;
struct block_structure {
    int refcount;

    /*
     * For text formatting, we do need c and r here.
     */
    int c, r, area;

    /*
     * For any square index, whichblock[i] gives its block index.
     *
     * For 0 <= b,i < cr, blocks[b][i] gives the index of the ith
     * square in block b.  nr_squares[b] gives the number of squares
     * in block b (also the number of valid elements in blocks[b]).
     *
     * blocks_data holds the data pointed to by blocks.
     *
     * nr_squares may be NULL for block structures where all blocks are
     * the same size.
     */
    int *whichblock, **blocks, *nr_squares, *blocks_data;
    int nr_blocks, max_nr_squares;
};
struct game_state {
    /*
     * For historical reasons, I use `cr' to denote the overall
     * width/height of the puzzle. It was a natural notation when
     * all puzzles were divided into blocks in a grid, but doesn't
     * really make much sense given jigsaw puzzles. However, the
     * obvious `n' is heavily used in the solver to describe the
     * index of a number being placed, so `cr' will have to stay.
     */
    int cr;
    struct block_structure *blocks;
    struct block_structure *kblocks;   /* Blocks for killer puzzles.  */
    bool xtype, killer;
    digit *grid, *kgrid;
    bool *pencil;                      /* c*r*c*r elements */
    bool *immutable;                   /* marks which digits are clues */
    bool completed, cheated;
};

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

/* Print a horizontal line in ESC/POS speak: https://reference.epson-biz.com/modules/ref_escpos/index.php?content_id=88 */
void horiz_line(unsigned char intersect_ch, uint8_t cells, uint16_t bold_right, uint16_t bold_bottom) {
    uint8_t i, j, next_cnt = 29;
    uint16_t len = (29 + 1) * cells + 2;

    putchar(0x1b); putchar(0x2a);
    putchar(0x00); /* 8 dot mode */
    putchar(len & 255);  /* Length low bit */
    putchar(len / 256);  /* Length high bit */

    /* Initial bold line */
    putchar(intersect_ch);
    putchar(intersect_ch);
    next_cnt = 28;

    for (i = 0; i < cells; i++) {
        for (j = 0; j < next_cnt; j++) {
            putchar(bold_bottom & (1 << i) ? 0b00011000 : 0b00010000);
        }

        putchar(intersect_ch);
        if (bold_right & (1 << i)) {
            putchar(intersect_ch);
            next_cnt = 28;
        } else {
            next_cnt = 29;
        }
    }
}

void vert_line(bool bold) {
    /* Double-height - https://escpos.readthedocs.io/en/latest/font_cmds.html#select-character-size-1d-21-rel-phx */
    putchar(0x1d); putchar(0x21); putchar(0x01);

    putchar(' ');
    if (bold) {
        /* Double-strike mode - https://escpos.readthedocs.io/en/latest/font_cmds.html#select-double-strike-mode-1b-47-phx */
        putchar(0x1b); putchar(0x47); putchar(0x01);
    }
    putchar('|');
    if (bold) {
        putchar(0x1b); putchar(0x47); putchar(0x00);
    }
    putchar(' ');
}

int main() {
    stdio_uart_init();

    /* Seed random state */
    uint32_t seed = hwrand32();
    random_state *rs = random_new((void*)&seed, sizeof(seed));

    /* Configure ADC to read knob */
    adc_init();
    adc_gpio_init(26);
    adc_select_input(0);
    gpio_init(27);
    gpio_set_dir(27, GPIO_OUT);
    gpio_put(27, 1);

    /* Give printer a chance to boot */
    sleep_ms(2000);

    while (true) {
        game_params *p;
        game_state *s;
        char *desc, *aux = NULL;

        /* Init printer: https://escpos.readthedocs.io/en/latest/font_cmds.html#b40 */
        printf("\x1b\x40");

        /* Center-align: https://escpos.readthedocs.io/en/latest/layout.html#select-justification-1b-61-rel-phx */
        printf("\x1b\x61\x01");

        /* Set difficulty according to knob position */
        uint8_t knob_pos = adc_read() / (4070 / 6); /* Range: 17..4068~4074 */
        char *name;
        if (knob_pos == 0) {
            thegame.fetch_preset(2, &name, &p);
        } else if (knob_pos == 1) {
            thegame.fetch_preset(3, &name, &p);
        } else if (knob_pos == 2) {
            thegame.fetch_preset(5, &name, &p);
        } else if (knob_pos == 3) {
            thegame.fetch_preset(6, &name, &p);
        } else if (knob_pos == 4) {
            thegame.fetch_preset(8, &name, &p);
        } else if (knob_pos == 5) {
            thegame.fetch_preset(9, &name, &p);
        } else {
            thegame.fetch_preset(2, &name, &p);
        }
        printf("%d: %s\r\n", knob_pos + 1, name);

        /* Generate a puzzle (this may take a while) */
        desc = thegame.new_desc(p, rs, &aux, false);
        s = thegame.new_game(NULL, p, desc);

        /* Minimal Line spacing - https://escpos.readthedocs.io/en/latest/layout.html#line-spacing-1b-33-rel */
        printf("\x1b\x33"); putchar(0x00);

        /* Output grid */
        for (uint8_t y = 0; y < s->cr; y++) {
            uint16_t bold_right = 0;
            uint16_t bold_bottom = 0;

            for (uint8_t x = 0; x < s->cr; x++) {
                /* Should separator to right / bottom of this cell be bold? */
                if (x == s->cr - 1 || s->blocks->whichblock[y*s->cr+x] != s->blocks->whichblock[y*s->cr+x+1]) {
                    bold_right |= 1 << x;
                }
                if (y == s->cr - 1 || s->blocks->whichblock[y*s->cr+x] != s->blocks->whichblock[(y+1)*s->cr+x]) {
                    bold_bottom |= 1 << x;
                }
            }

            /* Initial horizontal / vertical lines */
            if (y == 0) {
                horiz_line(0b00011111, s->cr, bold_right, 0x1FF);
            }

            for (uint8_t x = 0; x < s->cr; x++) {
                digit d = s->grid[y*s->cr+x];

                if (x == 0) {
                    vert_line(true);
                }

                /* Double-size characters - https://escpos.readthedocs.io/en/latest/font_cmds.html#select-character-size-1d-21-rel-phx */
                putchar(0x1d); putchar(0x21); putchar(0x11);
                if (d > 0) {
                    printf("%d", d);
                } else {
                    putchar(' ');
                }

                vert_line(bold_right & (1 << x));
            }
            printf("\r\n");

            horiz_line(y == (s->cr - 1) ? 0b11111000 : 0b11111111, s->cr, bold_right, bold_bottom);
        }

        /* Re-init to reset spacing, feed ready for tear */
        printf("\x1b\x40\r\n\r\n");
        stdio_flush();

        /* Wait 60s, print another one */
        sleep_ms(60000);
    }

    return 0;
}
