/*
 * misc.c: Miscellaneous helpful functions.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "puzzles.h"

char UI_UPDATE[] = "";

static void memswap(void *av, void *bv, int size)
{
    char tmpbuf[512];
    char *a = av, *b = bv;

    while (size > 0) {
	int thislen = min(size, sizeof(tmpbuf));
	memcpy(tmpbuf, a, thislen);
	memcpy(a, b, thislen);
	memcpy(b, tmpbuf, thislen);
	a += thislen;
	b += thislen;
	size -= thislen;
    }
}

void shuffle(void *array, int nelts, int eltsize, random_state *rs)
{
    char *carray = (char *)array;
    int i;

    for (i = nelts; i-- > 1 ;) {
        int j = random_upto(rs, i+1);
        if (j != i)
            memswap(carray + eltsize * i, carray + eltsize * j, eltsize);
    }
}

/* vim: set shiftwidth=4 tabstop=8: */
