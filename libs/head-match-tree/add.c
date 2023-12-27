#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "public/libesq.h"
#include "hmt-internal.h"

static int
hmt_item_compare(hmt_item_t * a, hmt_item_t * b, int len_max)	{
	return strncmp(a->pattern, b->pattern, len_max);
}

/* return -1: too many patterns */
/* return -2: pattern too long */
/* return -3: pattern exist */
int
__hmt_add(void * ppt, const char * pattern, void * userdata, void * malloc_item)	{
	hmt_t * pt = ppt;
	hmt_item_t k;
	int len_max;
	int i;
	int j;
	int r;

	if (pt->cnt >= pt->max)	{
		return -1;
	}

	k.pattern = pattern;
	k.userdata = userdata;
	k.malloc_item = malloc_item;

	len_max = HMT_PATH_LEN_MAX;

	for (i = 0; i < pt->cnt; i ++)	{
		r = hmt_item_compare(&k, pt->items + i, len_max);
		if (r > 0)	{
			break;
		}
		else if (r < 0)	{
			continue;
		}
		else	{
			return -3;
		}
	}

	for (j = pt->cnt; j > i; j --)	{
		pt->items[j] = pt->items[j - 1];
	}

	pt->items[i] = k;
	pt->cnt ++;

	return 0;
}

/* return -1: too many patterns */
/* return -2: pattern too long */
/* return -3: pattern exist */
int
hmt_add(void * ppt, const char * pattern, void * userdata)	{
	hmt_t * pt = ppt;
	int l;
	int r;

	l = strnlen(pattern, HMT_PATH_LEN_MAX);
	if (l >= HMT_PATH_LEN_MAX || l <= 0)	{
		/* only accept length value lower than "HMT_MAX_LEN_MAX - 1" */
		return -2;
	}

	r = __hmt_add(ppt, pattern, userdata, NULL);

	return r;
}

/* return -1: too many patterns */
/* return -2: pattern too long */
/* return -3: pattern exist */
/* return -4: pattern exist */
int
hmt_add2(void * ppt, const char * pattern, void * userdata)	{
	hmt_t * pt = ppt;
	char * local;
	int l;
	int r;

	l = strnlen(pattern, HMT_PATH_LEN_MAX);
	if (l >= HMT_PATH_LEN_MAX || l <= 0)	{
		/* only accept length value lower than "HMT_MAX_LEN_MAX - 1" */
		return -2;
	}

	local = malloc(l + 1);
	if (!local)	{
		return -4;
	}

	memcpy(local, pattern, l + 1);

	r = __hmt_add(ppt, local, userdata, local);

	if (r)	{
		free(local);
	}

	return r;
}

/* eof */
