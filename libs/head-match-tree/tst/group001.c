#include <stdio.h>
#include <stdlib.h>
#include "public/libesq.h"
#include "test-functions.h"

int
test_function_001(void)	{
	char * patterns[] = {
		"1",
		"2",
		"3",
	};
	void * return_values[] = {
		(void *)1,
		(void *)2,
		(void *)3,
	};
	char * strings[] = {
		"1",
		"2",
		"3",
	};
	void * espected_values[] = {
		(void *)1,
		(void *)2,
		(void *)3,
	};
	int r;

	r = test_driver(4, HMT_SCALE_16
			, patterns, return_values, (sizeof(patterns) / sizeof(patterns[0]))
			, strings, espected_values, (sizeof(strings) / sizeof(strings[0]))
			);
	if (r)	{
		printf("Please check from file %s\n", __FILE__);
	}

	return r;
}

/* eof */
