#include <stdio.h>
#include <ctype.h>

#include "public/libesq.h"

#ifndef STATIC
	#define STATIC static
#endif

#define DUMP_LINE_WIDTH  16
typedef enum    {
    BEFORE_BLANK,
    DSTATT,
    DSTATL0,
    DSTATL1,
    DSTATV,
} dump_state_t;

STATIC void
__hex_dump_line(const char * b, int l, dump_state_t * st, int * tlvl)	{
    /* tlvl - TLV length. Type-Length-Value length */
    int i;

    for(i = 0; i < l; i ++) {
        switch(*st) {
        case BEFORE_BLANK:
            printf("%02hhx ", (unsigned char)b[i]);
            if (' ' == b[i])    {
                *st = DSTATT;
            }
            break;
        case DSTATT:
            printf("\33[1m%02hhx\33[0m ", (unsigned char)b[i]);
            *st = DSTATL0;
            break;
        case DSTATL0:
            printf("\33[32;1m%02hhx\33[0m ", (unsigned char)b[i]);
            *st = DSTATL1;
            *tlvl = ((unsigned char)b[i] << 8);
            break;
        case DSTATL1:
            printf("\33[32;1m%02hhx\33[0m ", (unsigned char)b[i]);
            *st = DSTATV;
            (*tlvl) |= (unsigned char)b[i];
            break;
        case DSTATV:
            printf("\33[33;1m%02hhx\33[0m ", (unsigned char)b[i]);
            (*tlvl) --;
            if (!*tlvl) {
                *st = DSTATT;
            }
            break;
        }
    }

    for(;i < DUMP_LINE_WIDTH; i ++)  {
        printf("   ");
    }

    printf("| ");

    for(i = 0; i < l; i ++) {
        if (isprint((int)(b[i])))  {
            printf("%c", b[i]);
        }
        else    {
            printf(" ");
        }
    }

    for(;i < DUMP_LINE_WIDTH; i ++)  {
        printf(" ");
    }

    printf(" |\n");

    return;
}

void hex_dump(const char * b, int l) {
    const char * p = b;
    int left = l;
    int tlvl = 0;   /* mask Coverity issue */
    dump_state_t dump_st = BEFORE_BLANK;

    while(left) {
        if (left >= DUMP_LINE_WIDTH) {
            __hex_dump_line(p, DUMP_LINE_WIDTH, &dump_st, &tlvl);
            left -= DUMP_LINE_WIDTH;
            p += DUMP_LINE_WIDTH;
        }
        else    {
            __hex_dump_line(p, left, &dump_st, &tlvl);
            break;
        }
    }

    printf("\n");
    return;
}

/* eof */
