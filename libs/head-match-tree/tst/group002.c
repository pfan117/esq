#include <stdio.h>
#include <stdlib.h>
#include "public/libesq.h"
#include "test-functions.h"

int
test_function_002(void)	{
	char * patterns[] = {
		"1",
	};
	void * return_values[] = {
		(void *)1,
	};
	char * strings[] = {
		"1",
		"12",
		"13",
	};
	void * espected_values[] = {
		(void *)1,
		(void *)1,
		(void *)1,
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
