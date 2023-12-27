#include <stdio.h>
#include <stdlib.h>
#include "public/libesq.h"
#include "test-functions.h"

int
test_function_008(void)	{
	char * patterns[] = {
		"",
		"a",
		"ab",
	};
	void * return_values[] = {
		(void *)1,
		(void *)2,
		(void *)3,
	};
	char * strings[] = {
		"",
		"a",
		"ab",
		"abc",
		"abcd",
		"abcdefghigklmnopqrstuvwxyz",
		"abcdefghigklmnopqrstuvwxyz",
		"abcdefghigklmnopqrstuvwxyz",
		"abcdefghigklmnopqrstuvwxyz",
		"abcd",
		"abc",
		"ab",
		"a",
		"",
	};
	void * espected_values[] = {
		(void *)0,
		(void *)2,
		(void *)3,
		(void *)3,
		(void *)3,

		(void *)3,
		(void *)3,
		(void *)3,
		(void *)3,

		(void *)3,
		(void *)3,
		(void *)3,
		(void *)2,
		(void *)0,
	};
	int r;

	r = test_driver(20, HMT_SCALE_16
			, patterns, return_values, (sizeof(patterns) / sizeof(patterns[0]))
			, strings, espected_values, (sizeof(strings) / sizeof(strings[0]))
			);
	if (r)	{
		printf("Please check from file %s\n", __FILE__);
	}

	return r;
}

/* eof */
