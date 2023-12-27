#include <stdio.h>
#include <stdlib.h>
#include "public/libesq.h"
#include "test-functions.h"

int
test_function_000(void)	{
	char * patterns[] = {
		"foo",
	};
	void * return_values[] = {
		(void *)1,
	};
	char * strings[] = {
		"foo",
		"fo",
		"f",
		"o",
		"oo",
	};
	void * espected_values[] = {
		(void *)1,
		(void *)0,
		(void *)0,
		(void *)0,
		(void *)0,
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
