#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "public/libesq.h"
#include "hmt-internal.h"

void
hmt_free_content(hmt_t * pt)	{

	if (pt->higher_jump_table)	{
		free(pt->higher_jump_table);
		pt->higher_jump_table = NULL;
	}

	if (pt->lower_jump_table)	{
		free(pt->lower_jump_table);
		pt->lower_jump_table = NULL;
	}

	if (pt->someone_else_matched_table)	{
		free(pt->someone_else_matched_table);
		pt->someone_else_matched_table = NULL;
	}

	if (pt->next_level_enter_point_table)	{
		free(pt->next_level_enter_point_table);
		pt->next_level_enter_point_table = NULL;
	}

	if (pt->patterns_length)	{
		free(pt->patterns_length);
		pt->patterns_length = NULL;
	}

	pt->enter_point = -1;

	return;
}

void *
hmt_new2(int max, int scale)	{
	hmt_t * pt;

	if (max >= HMT_ITEM_MAX)	{
		return NULL;
	}

	pt = malloc(sizeof(hmt_t) + sizeof(hmt_item_t) * max);
	if (!pt)	{
		return NULL;
	}

	pt->max = max;
	pt->cnt = 0;
	pt->enter_point = -1;
	pt->higher_jump_table = NULL;
	pt->lower_jump_table = NULL;
	pt->someone_else_matched_table = NULL;
	pt->next_level_enter_point_table = NULL;
	pt->patterns_length = NULL;

	switch (scale)	{
	case HMT_SCALE_16:
		HMT_PATH_LEN_MAX = 16;
		HMT_PATH_LEN_MAX_SHIFT = 4;
		break;
	case HMT_SCALE_32:
		HMT_PATH_LEN_MAX = 32;
		HMT_PATH_LEN_MAX_SHIFT = 5;
		break;
	case HMT_SCALE_64:
		HMT_PATH_LEN_MAX = 64;
		HMT_PATH_LEN_MAX_SHIFT = 6;
		break;
	case HMT_SCALE_128:
		HMT_PATH_LEN_MAX = 128;
		HMT_PATH_LEN_MAX_SHIFT = 7;
		break;
	case HMT_SCALE_512:
		HMT_PATH_LEN_MAX = 512;
		HMT_PATH_LEN_MAX_SHIFT = 9;
		break;
	default:
		/* default values */
		HMT_PATH_LEN_MAX = 128;
		HMT_PATH_LEN_MAX_SHIFT = 7;
		break;
	};

	return pt;
}

void *
hmt_new(int max)	{
	return hmt_new2(max, HMT_SCALE_32);
}

void
hmt_free(void * ppt)	{
	hmt_t * pt = ppt;
	int i;

	hmt_free_content(pt);

	for (i = 0; i < pt->cnt; i ++)	{
		if (pt->items[i].malloc_item)	{
			free(pt->items[i].malloc_item);
		}
	}

	free(pt);
}

/* eof */
