#include <stdio.h>
#include <stdlib.h>
#include "public/libesq.h"
#include "test-functions.h"

int
test_function_006(void)	{
	char * patterns[] = {
		"a",
		"b",
		"c",
		"d",
		"e",
		"f",
		"g",
		"h",
		"i",
		"j",
		"k",
		"l",
		"m",
		"n",
	};
	void * return_values[] = {
		(void *)1,
		(void *)2,
		(void *)3,
		(void *)4,
		(void *)5,
		(void *)6,
		(void *)7,
		(void *)8,
		(void *)9,
		(void *)10,
		(void *)11,
		(void *)12,
		(void *)13,
		(void *)14,
	};
	char * strings[] = {
		"a",
		"b",
		"c",
		"d",
		"e",
		"f",
		"g",
		"h",
		"i",
		"j",
		"k",
		"l",
		"m",
		"n",
		"aa",
		"ba",
		"ca",
		"dd",
		"ed",
		"fd",
		"gd",
		"hc",
		"ic",
		"jc",
		"kc",
		"ld",
		"md",
		"nd",
	};
	void * espected_values[] = {
		(void *)1,
		(void *)2,
		(void *)3,
		(void *)4,
		(void *)5,
		(void *)6,
		(void *)7,
		(void *)8,
		(void *)9,
		(void *)10,
		(void *)11,
		(void *)12,
		(void *)13,
		(void *)14,
		(void *)1,
		(void *)2,
		(void *)3,
		(void *)4,
		(void *)5,
		(void *)6,
		(void *)7,
		(void *)8,
		(void *)9,
		(void *)10,
		(void *)11,
		(void *)12,
		(void *)13,
		(void *)14,
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
