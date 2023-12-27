#include <stdio.h>
#include <string.h>
#include "public/libesq.h"

#define S_INIT			0
#define S_AFTER_SLASH	1
int
remove_redundancy_slash(char * b)	{
	int s;
	int i;
	int o;
	char c;

	s = S_INIT;
	o = 0;
	for (i = 0;; i ++)	{
		c = b[i];
		if ('/' == c || '\\' == c)	{
			if (S_INIT == s)	{
				s = S_AFTER_SLASH;
			}
		}
		else if ('\0' == c)	{
			break;
		}
		else	{
			if (S_INIT == s)	{
				b[o] = c;
				o ++;
				continue;
			}
			else	{
				s = S_INIT;
				b[o] = '/';
				o ++;
				b[o] = c;
				o ++;
				continue;
			}
		}
	}

	b[o] = '\0';

	return o;
}

#ifdef LIBDEBUG
typedef struct _test_io_t	{
	char * i;
	char * o;
	int r;
} test_io_t;

int
main(void)	{
	int i;
	int r;
	char testbuf[1024];
	test_io_t test_list[] = {
		{"", "", 0},
		{"a", "a", 1},
		{"/", "", 0},
		{"//////", "", 0},
		{"abc", "abc", 3},
		{"abc/", "abc", 3},
		{"abc//", "abc", 3},
		{"abc///", "abc", 3},
		{"abc////", "abc", 3},
		{"/a/b/c/", "/a/b/c", 6},
		{"//a/b/c/", "/a/b/c", 6},
		{"/a//b/c/", "/a/b/c", 6},
		{"/a/b//c/", "/a/b/c", 6},
		{"/a/b/c//", "/a/b/c", 6},
		{"///a/b/c/", "/a/b/c", 6},
		{"/a///b/c/", "/a/b/c", 6},
		{"/a/b///c/", "/a/b/c", 6},
		{"/a/b/c///", "/a/b/c", 6},
		{"////a/b/c/", "/a/b/c", 6},
		{"/a////b/c/", "/a/b/c", 6},
		{"/a/b////c/", "/a/b/c", 6},
		{"/a/b/c////", "/a/b/c", 6},
	};

	for (i = 0; i < (sizeof(test_list) / sizeof(test_list[0])); i ++)	{
		printf("%02d: [%s]", i, test_list[i].i);
		strcpy(testbuf, test_list[i].i);
		r = remove_redundancy_slash(testbuf);
		if (strcmp(testbuf, test_list[i].o))	{
			printf(" failed, wrong string\n");
		}
		else if (r != test_list[i].r)	{
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
