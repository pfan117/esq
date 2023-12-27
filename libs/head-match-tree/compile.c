#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "public/libesq.h"
#include "hmt-internal.h"

static void
hmt_find_center_group_border(hmt_t * pt, int depth, char start, char end, char * bs, char * be)	{
	char c;
	int m;
	int i;
	int s;
	int e;

	m = ((start + end) >> 1);
	c = pt->items[m].pattern[depth];

	s = m;
	for (i = m - 1; i >= start; i --)	{
		if (pt->items[i].pattern[depth] == c)	{
			s = i;
		}
		else	{
			break;
		}
	}
	*bs = s;

	e = m;
	for (i = m + 1; i <= end; i ++)	{
		if (pt->items[i].pattern[depth] == c)	{
			e = i;
		}
		else	{
			break;
		}
	}
	*be = e;

	return;
}

static char
hmt_fill_jump_tables(hmt_t * pt, int depth, char start, char end)	{
	char higher;
	char lower;
	char bs;
	char be;
	int idx;

	if (depth >= HMT_PATH_LEN_MAX)	{
		return start;
	}

	if (!pt->items[(int)start].pattern[depth])	{
		return start;
	}

	hmt_find_center_group_border(pt, depth, start, end, &bs, &be);
	if (bs == start)	{
		higher = -1;
	}
	else	{
		higher = hmt_fill_jump_tables(pt, depth, start, bs - 1);
	}

	if (be == end)	{
		lower = -1;
	}
	else	{
		lower = hmt_fill_jump_tables(pt, depth, be + 1, end);
	}

	idx = (bs << HMT_PATH_LEN_MAX_SHIFT) + depth;
	pt->higher_jump_table[idx] = higher;
	pt->lower_jump_table[idx] = lower;

	if (depth < HMT_PATH_LEN_MAX)	{
		pt->next_level_enter_point_table[idx] = hmt_fill_jump_tables(pt, depth + 1, bs, be);
	}

	return bs;
}

static void
hmt_fill_patterns_length_table(hmt_t * pt)	{
	int * lengthes;
	int l;
	int i;

	lengthes = pt->patterns_length;

	for (i = 0; i < pt->cnt; i ++)	{
		l = strnlen(pt->items[i].pattern, HMT_PATH_LEN_MAX);
		lengthes[i] = l;
	}

	return;
}

static void
hmt_find_someone_else_matched(hmt_t * pt, int idx)	{
	hmt_item_t * items;
	char * matches;
	int * lengthes;
	int m;
	int i;
	int k;

	lengthes = pt->patterns_length;
	matches = pt->someone_else_matched_table + (idx << HMT_PATH_LEN_MAX_SHIFT);
	items = pt->items;

	for (i = 0; i < lengthes[idx]; i ++)	{
		m = -1;
		for (k = idx + 1; k < pt->cnt; k ++)	{
			if (lengthes[k] >= i + 1)	{
				continue;
			}
			else if (memcmp(items[idx].pattern, items[k].pattern, lengthes[k]))	{
				continue;
			}
			else	{
				m = k;
				break;
			}
		}
		matches[i] = m;
	}

	return;
}

static void
hmt_fill_someone_else_matched_table(hmt_t * pt)	{
	int i;

	for (i = 0; i < pt->cnt; i ++)	{
		hmt_find_someone_else_matched(pt, i);
	}

	return;
}

/*
 * ** longest match **
 * "/jenkins/userContent/plain-text/foo.txt"
 * "/jenkins/userContent/plain-text/"
 * "/jenkins/userContent/"
 * "/"
 */
int
hmt_compile(void * ppt)	{
	int jump_table_size;
	hmt_t * pt = ppt;

	hmt_free_content(pt);

	jump_table_size = (pt->cnt << HMT_PATH_LEN_MAX_SHIFT);
	if (!jump_table_size)	{
		goto __exx;
	}

	pt->higher_jump_table = malloc(jump_table_size);
	if (!pt->higher_jump_table)	{
		goto __exx;
	}

	pt->lower_jump_table = malloc(jump_table_size);
	if (!pt->lower_jump_table)	{
		goto __exx;
	}

	pt->someone_else_matched_table = malloc(jump_table_size);
	if (!pt->someone_else_matched_table)	{
		goto __exx;
	}

	pt->next_level_enter_point_table = malloc(jump_table_size);
	if (!pt->next_level_enter_point_table)	{
		goto __exx;
	}

	pt->patterns_length = malloc(sizeof(int) * pt->cnt);
	if (!pt->patterns_length)	{
		goto __exx;
	}

	hmt_fill_patterns_length_table(pt);
	hmt_fill_someone_else_matched_table(pt);
	pt->enter_point = hmt_fill_jump_tables(pt, 0, 0, pt->cnt - 1);

	return 0;

__exx:

	hmt_free_content(pt);

	return -1;
}

/* eof */
