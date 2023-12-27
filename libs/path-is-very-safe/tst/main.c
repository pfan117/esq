#include <stdio.h>
#include "lib/libesq.h"

typedef struct _t	{
	const char * path;
	int r;
} t;

int
main(void)	{
	int i;
	int r;

	t t[] = {
		{"", 1},
		{"/", 0},
		{"/index.html", 0},
		{"index.html", 1},
		{"./index.html", 0},
		{"res/usr/index.html", 1},
		{"res/usr", 1},
		{"res/./kk", 0},
		{"res/../../..", 0},
		{"res/.", 0},
		{"test/./test", 0},
		{"test/../test", 0},
		{"test/.../test", 0},
		{"test", 1},
		{"tes.t", 1},
		{"tes..t", 1},
		{"tes...t", 1},
		{"tes....t", 1},
		{"test.", 0},
		{"test..", 0},
		{"test...", 0},
		{"test....", 0},
		{".test", 0},
		{"..test", 0},
		{"...test", 0},
		{"....test", 0},
		{"a", 1},
		{".", 0},
		{"..", 0},
		{"...", 0},
		{"....", 0},
		{"_", 1},
		{"-", 1},
		{"a", 1},
		{"A", 1},
		{"0", 1},
		{"z", 1},
		{"Z", 1},
		{"9", 1},

		{"aa_", 1},
		{"aa-", 1},
		{"aaa", 1},
		{"aaA", 1},
		{"aa0", 1},
		{"aaz", 1},
		{"aaZ", 1},
		{"aa9", 1},

		{"_aa", 1},
		{"-aa", 1},
		{"aaa", 1},
		{"Aaa", 1},
		{"0aa", 1},
		{"zaa", 1},
		{"Zaa", 1},
		{"9aa", 1},

		{"a_a", 1},
		{"a-a", 1},
		{"aaa", 1},
		{"aAa", 1},
		{"a0a", 1},
		{"aza", 1},
		{"aZa", 1},
		{"a9a", 1},
	};

	for (i = 0; i < (sizeof(t) / sizeof(t[0])); i ++)	{
		r = path_is_very_safe(t[i].path);
		if (r == t[i].r)	{
			;//	printf("TC %d ok\n", i);
		}
		else	{
			printf("TC %d failed - [%s] suppose to return [%d]\n", i, t[i].path, t[i].r);
			return -1;
		}
	}

	printf("All success\n");

	return 0;
}

/* eof */
