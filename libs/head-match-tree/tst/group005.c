#include <stdio.h>
#include <stdlib.h>
#include "public/libesq.h"
#include "test-functions.h"

int
test_function_005(void)	{
	char * patterns[] = {
		"1111111111",
		"1",
	};
	void * return_values[] = {
		(void *)1,
		(void *)2,
	};
	char * strings[] = {
		"1111111111",
		"111111111",
		"11111111",
		"1111111",
		"111111",
		"11111",
		"1111",
		"111",
		"11",
		"1",
		"0",
		"00",
		"000",
		"0000",
		"00000",
		"000000",
		"0000000",
	};
	void * espected_values[] = {
		(void *)1,
		(void *)2,
		(void *)2,
		(void *)2,
		(void *)2,
		(void *)2,
		(void *)2,
		(void *)2,
		(void *)2,
		(void *)2,
		(void *)0,
		(void *)0,
		(void *)0,
		(void *)0,
		(void *)0,
		(void *)0,
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
