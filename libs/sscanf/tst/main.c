#include <stdio.h>
#include <string.h>

#include "public/libesq.h"

/* ipv4 */
typedef struct _tt_ipv4	{
	const char * string;
	int r;
	unsigned long ip;
} tt_ipv4;

tt_ipv4 t0[] = {
	{ "1.1.1.1", 7, 0x1010101 },
	{ "1.0.0.1", 7, 0x1000001 },
	{ "1.2.3.4", 7, 0x1020304 },
	{ "8.8.8.8", 7, 0x8080808 },
	{ "  1.2.3.4", 9, 0x1020304 },
	{ "\t\t1.2.3.4", 9, 0x1020304 },
	{ "\n\n1.2.3.4", 9, 0x1020304 },
	{ "\r\r1.2.3.4", 9, 0x1020304 },
	{ "1.2.3.4  ", 7, 0x1020304 },
	{ "1.2.3.4\t\t", 7, 0x1020304 },
	{ "1.2.3.4\n\n", 7, 0x1020304 },
	{ "1.2.3.4\r\r", 7, 0x1020304 },
	{ "254.254.254.254", 15, 0xfefefefe },

	{ "255.1.1.1", 0, 0 },
	{ "1.255.1.1", 0, 0 },
	{ "1.1.255.1", 0, 0 },
	{ "1.1.1.255", 0, 0 },

	{ "", 0, 0},
	{ "\n", 0, 0 },
	{ "   ", 0, 0},
	{ "\r\n   ", 0, 0},
};

/* integer 10000000 */
typedef struct _tt_int_10000000	{
	const char * string;
	int r;
	int v;
} tt_int_10000000;

tt_int_10000000 t1[] = {
	{ "1", 1, 1 },
	{ "10000000", 8, 10000000 },
	{ "10000001", 0, 0 },
};

/* hash */
typedef struct _tt_hash	{
	const char * string;
	int r;
	int start;
	int length;
} tt_hash;

tt_hash t2[] = {
	{ "x", 1, 0, 0 },
	{ "1ax", 3, 0, 2 },
	{ "10000001x", 9, 0, 8 },
	{ "   10000001x", 12, 3, 8 },
	{ "   10000001x  ", 12, 3, 8 },
	{ "   100g0001x  ", -1, 0, 0, },
};

int
main(void)	{
	int i;
	int r;
	unsigned long a;
	int v;
	int l;
	int start;

	printf("get_ipv4_addr()\n");

	for (i = 0; i < (sizeof(t0) / sizeof(t0[0])); i ++)	{
		r = get_ipv4_addr(t0[i].string, strlen(t0[i].string), &a);

		if (r != t0[i].r)	{
			printf("TC %d failed - [%s] suppose to return [%d], but [%d] returned\n", i, t0[i].string, t0[i].r, r);
			return -1;
		}

		if (!r)	{
			continue;
		}

		if (a != t0[i].ip)	{
			printf("TC %d failed - [%s] suppose to get [%lu], but [%lu] returned\n", i, t0[i].string, t0[i].ip, a);
			return -1;
		}
	}

	printf("get_integer_1000000()\n");

	for (i = 0; i < (sizeof(t1) / sizeof(t1[0])); i ++)	{
		r = get_integer_1000000(t1[i].string, strlen(t1[i].string), &v);

		if (r != t1[i].r)	{
			printf("TC %d failed - [%s] suppose to return [%d], but [%d] returned\n", i, t1[i].string, t1[i].r, r);
			return -1;
		}

		if (!r)	{
			continue;
		}

		if (v != t1[i].v)	{
			printf("TC %d failed - [%s] suppose to get [%d], but [%d] returned\n", i, t1[i].string, t1[i].v, v);
			return -1;
		}
	}

	printf("get_hash_string()\n");

	for (i = 0; i < (sizeof(t2) / sizeof(t2[0])); i ++)	{
		r = get_hash_string(t2[i].string, strlen(t2[i].string), &start, &l);

		if (r != t2[i].r)	{
			printf("TC %d failed - [%s] suppose to return [%d], but [%d] returned\n", i, t2[i].string, t2[i].r, r);
			return -1;
		}

		if (-1 == r)	{
			continue;
		}

		if (start != t2[i].start)	{
			printf("TC %d failed - [%s] start pos, suppose to get [%d], but [%d] returned\n"
					, i, t2[i].string, t2[i].start, start
					);
			return -1;
		}

		if (l != t2[i].length)	{
			printf("TC %d failed - [%s] length, suppose to get [%d], but [%d] returned\n"
					, i, t2[i].string, t2[i].length, l
					);
			return -1;
		}
	}

	printf("ok\n");

	return 0;
}

/* eof */
