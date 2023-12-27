#include <stdio.h>
#include <string.h>
#include "public/libesq.h"

int
find_last_slash(const char * b)	{
	int i;
	int p;
	char c;

	p = -1;

	for (i = 0;; i ++)	{
		c = b[i];
		if (c)	{
			if ('/' == c || '\\' == c)	{
				p = i;
			}
		}
		else	{
			return p;
		}
	}
}

int
find_last_slash2(const char * b, int len)	{
	int i;
	int p;

	p = -1;

	for (i = 0; i < len; i ++)	{
		if ('/' == b[i] || '\\' == b[i])	{
			p = i;
		}
	}

	return p;
}

#ifdef LIBDEBUG
typedef struct _test_io_t	{
	char * i;
	int r;
} test_io_t;

int
main(void)	{
	int i;
	int r;
	char testbuf[1024];
	test_io_t test_list[] = {
		{"", -1},
		{"a", -1},
		{"aa", -1},
		{"akk", -1},
		{"/", 0},
		{"//////", 5},
		{"abc", -1},
		{"abc/", 3},
		{"abc//", 4},
		{"abc///", 5},
		{"abc////", 6},
		{"/a/b/c/", 6},
		{"//a/b/c/", 7},
		{"/a//b/c/", 7},
		{"/a/b//c/", 7},
		{"/a/b/c//", 7},
		{"///a/b/c/", 8},
		{"/a///b/c/", 8},
		{"/a/b///c/a", 8},
		{"/a/b/c///ab", 8},
		{"////a/b/c/",9},
		{"/a////b/c/kkk", 9},
		{"/a/b////c/", 9},
		{"/a/b/c////..", 9},
	};

	for (i = 0; i < (sizeof(test_list) / sizeof(test_list[0])); i ++)	{
		printf("%02d: [%s]", i, test_list[i].i);
		strcpy(testbuf, test_list[i].i);
		r = find_last_slash(testbuf);
		if (r != test_list[i].r)	{
			printf(" failed, wrong return value\n");
		}
		else	{
			printf(" ok\n");
		}
	}

	return 0;
}

#endif
/* eof */
