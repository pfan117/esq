#include <stdio.h>
#include <stdlib.h>
#include "public/libesq.h"
#include "test-functions.h"

int test_driver(
		int max, int scale,
		char ** patterns, void ** return_values, int cnt,
		char ** strings, void ** espected_values, int result_cnt
		)
{
	int matched_length;
	void * pt;
	void * rv;
	int i;
	int r;

	pt = hmt_new2(max, scale);
	if (!pt)	{
		printf("%s, %d: failed to create new pattern_tree\n", __func__, __LINE__);
		return -1;
	}

	for (i = 0; i < cnt; i ++)	{
		if ((i & 1))	{
			r = hmt_add(pt, patterns[i], return_values[i]);
		}
		else	{
			r = hmt_add2(pt, patterns[i], return_values[i]);
		}
		if (r)	{
			printf("%s, %d: hmt_add return non zero value\n", __func__, __LINE__);
		}
	}

	r = hmt_compile(pt);
	if (r)	{
		printf("%s, %d: hmt_compile return non zero value\n", __func__, __LINE__);
	}

	for (i = 0; i < result_cnt; i ++)	{
		rv = hmt_match(pt, strings[i], &matched_length);
		if (rv != espected_values[i])	{
			printf("looking for string [%s], especting value [%p]. But got value [%p]\n"
					, strings[i], espected_values[i], rv
					);
			printf("dump tree:\n");
			hmt_dump(pt);
			hmt_free(pt);
			return -1;
		}
	}

	hmt_free(pt);

	return 0;
}

/* eof */
