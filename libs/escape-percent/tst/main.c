#include <stdio.h>
#include <string.h>
#include "public/libesq.h"

struct test_io	{
	char * i;
	char * o;
	int r;
};

struct test_io tio[] = {
	{
		"abcdefg",
		"abcdefg",
		7,
	},
	{
		"",
		"",
		0,
	},
	{
		"k",
		"k",
		1,
	},
	{
		"k%ss",
		"k",
		-1,
	},
	{
		"k%ss",
		"k",
		-1,
	},
	{
		"%30",
		"0",
		1,
	},
	{
		"%30%30",
		"00",
		2,
	},
	{
		"%30k%30",
		"0k0",
		3,
	},
	{
		"k%30k",
		"k0k",
		3,
	},
};

int
main(void)	{
	char buf[1024];
	int l;
	int i;
	int r;
	int sl;

	l = sizeof(tio) / sizeof(tio[0]);

	for (i = 0; i < l; i ++)	{
		sl = strlen(tio[i].i);
		memcpy(buf, tio[i].i, sl);
		r = escape_percent_in_place(buf, sl);
		if (r == tio[i].r)	{
			if (-1 == r)	{
				printf("pass: %d\n", i);
			}
			else if (memcmp(buf, tio[i].o, r))	{
				printf("error: %d: output string missmatched\n", i);
			}
			else	{
				printf("pass: %d\n", i);
			}
		}
		else	{
			printf("error: %d: return value error\n", i);
		}
	}

	return 0;
}

/* eof */
