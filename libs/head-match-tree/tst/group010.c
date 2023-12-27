#include <stdio.h>
#include <stdlib.h>
#include "public/libesq.h"
#include "test-functions.h"

int
test_function_010(void)	{
	char * patterns[] = {
		"/",
		"/res/",
		"/userContent/",
		"/Views/",
		"/profile/",
		"/signup/",
		"/signout/",
		"/signin/",
		"/friends/",
		"/resource/",
		"/center/",
		"/bike/",
		"/index",
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
	};
	char * strings[] = {
		"abcdefghigklmnopqrstuvwxyz",
		"abcdefghigklmno",
		"abcdefghigklmn",
		"abcd",
		"abc",
		"ab",
		"a",
		"",

		"/",
		"/res/",
		"/userContent/",
		"/Views/",
		"/profile/",
		"/signup/",
		"/signout/",
		"/signin/",
		"/friends/",
		"/resource/",
		"/center/",
		"/bike/",
		"/index",

		"/index.html",
		"/res/index.html",
		"/userContent/index.html",
		"/Views/index.html",
		"/profile/index.html",
		"/signup/index.html",
		"/signout/index.html",
		"/signin/index.html",
		"/friends/index.html",
		"/resource/index.html",
		"/center/index.html",
		"/bike/index.html",
		"/index/index.html",
	};
	void * espected_values[] = {
		(void *)0,
		(void *)0,
		(void *)0,
		(void *)0,
		(void *)0,
		(void *)0,
		(void *)0,
		(void *)0,

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

		(void *)13,
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
