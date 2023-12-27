#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "public/libesq.h"
#include "hmt-internal.h"

void *
hmt_match(void * ppt, const char * string, int * matched_length)	{
	int len_max_shift;
	hmt_t * pt = ppt;
	int len_max;
	int depth;
	char pc;
	char c;
	int m;
	int n;

	len_max_shift = pt->pattern_len_max_shift;
	len_max = pt->pattern_len_max;
	m = pt->enter_point;

	if (-1 == m)	{
		return NULL;
	}

	for(depth = 0; depth < len_max; )	{
		c = string[depth];
		pc = pt->items[m].pattern[depth];

		if (!pc)	{
			if (matched_length)	{
				*matched_length = pt->patterns_length[m];
			}
			return pt->items[m].userdata;
		}

		if (c == pc)	{
			n = pt->next_level_enter_point_table[(m << len_max_shift) + depth];
			if (-1 == n)	{
				goto __find_someone_else;
			}
			else	{
				m = n;
				depth ++;
				continue;
			}
		}
		else if (c > pc)	{
			n = pt->higher_jump_table[(m << len_max_shift) + depth];
			if (-1 == n)	{
				goto __find_someone_else;
			}
			else	{
				m = n;
				continue;
			}
		}
		else if (c < pc)	{
			n = pt->lower_jump_table[(m << len_max_shift) + depth];
			if (-1 == n)	{
				goto __find_someone_else;
			}
			else	{
				m = n;
				continue;
			}
		}
	}

	return NULL;

__find_someone_else:

	n = pt->someone_else_matched_table[(m << len_max_shift) + depth];
	if (-1 == n)	{
		return NULL;
	}
	else	{
		if (matched_length)	{
			*matched_length = pt->patterns_length[n];
		}
		return pt->items[n].userdata;
	}
}

/* eof */
