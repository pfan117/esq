#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "public/libesq.h"
#include "hmt-internal.h"

void
hmt_dump(void * ppt)	{
	hmt_t * pt = ppt;
	int i;
	int k;

	if (pt->someone_else_matched_table)	{
		printf("pt->enter_point = %d\n", pt->enter_point);
		for (i = 0; i < pt->cnt; i ++)	{
			printf("%2d: [%s] - [%p]\n"
					, i
					, pt->items[i].pattern
					, pt->items[i].userdata
					);
			printf("               (Matched: %2d", pt->someone_else_matched_table[i << HMT_PATH_LEN_MAX_SHIFT]);
			for (k = 1; k < HMT_PATH_LEN_MAX; k ++)	{
				printf(", %2d", pt->someone_else_matched_table[(i << HMT_PATH_LEN_MAX_SHIFT) + k]);
			}
			printf(")\n");

			printf("               ( Higher: %2d", pt->higher_jump_table[i << HMT_PATH_LEN_MAX_SHIFT]);
			for (k = 1; k < HMT_PATH_LEN_MAX; k ++)	{
				printf(", %2d", pt->higher_jump_table[(i << HMT_PATH_LEN_MAX_SHIFT) + k]);
			}
			printf(")\n");

			printf("               (  Lower: %2d", pt->lower_jump_table[i << HMT_PATH_LEN_MAX_SHIFT]);
			for (k = 1; k < HMT_PATH_LEN_MAX; k ++)	{
				printf(", %2d", pt->lower_jump_table[(i << HMT_PATH_LEN_MAX_SHIFT) + k]);
			}
			printf(")\n");

			printf("               (NextEnt: %2d", pt->next_level_enter_point_table[i << HMT_PATH_LEN_MAX_SHIFT]);
			for (k = 1; k < HMT_PATH_LEN_MAX; k ++)	{
				printf(", %2d", pt->next_level_enter_point_table[(i << HMT_PATH_LEN_MAX_SHIFT) + k]);
			}
			printf(")\n");
		}
	}
	else	{
		for (i = 0; i < pt->cnt; i ++)	{
			printf("%2d: [%s] - [%p]\n"
					, i
					, pt->items[i].pattern
					, pt->items[i].userdata
					);
		}
	}

	return;
}

/* eof */
