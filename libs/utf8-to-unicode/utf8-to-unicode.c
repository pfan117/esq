#include <stdio.h>
#include "public/libesq.h"

/* #define DEBUG	1 */

#ifdef DEBUG
#define dbg_print(...)	printf(__VA_ARGS__)
#else
#define dbg_print(...)
#endif

/* return value <= 0 means error */
int
get_code_from_utf8(const char * buf, unsigned int * codep)	{
	unsigned int c;
	unsigned int code;
	int l;
	int i;

	code = 0;

	c = buf[0];

	if ((0x80 & c))	{
		;
	}
	else	{
		*codep = c;	/* not utf8 */
		return 1;
	}

	if (!(0x40 & c))	{
		dbg_print("error: utf8 leading code error\n");
		return -1;
	}
	else if (!(0x20 & c))	{
		l = 2;
		code = (c & 0x1f);
	}
	else if (!(0x10 & c))	{
		l = 3;
		code = (c & 0x0f);
	}
	else if (!(0x08 & c))	{
		l = 4;
		code = (c & 0x07);
	}
	else	{
		dbg_print("error: utf8 leading code error\n");
		return -1;
	}

	if (!code)	{
		dbg_print("error: not long enough to use utf8\n");
		return -1;
	}

	for (i = 1; i < l; i ++)	{
		c = buf[i];
		if ((c & 0xc0) == 0x80)	{
			code = (code << 6) + (c & 0x3f);
		}
		else	{
			dbg_print("error: utf8 following code error - 0x%x\n", c);
			return -1;
		}
	}

	*codep = code;

	return l;
}

/* eof */
