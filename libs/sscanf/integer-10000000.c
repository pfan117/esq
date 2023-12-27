#include <stdio.h>

#include "public/libesq.h"

/* 0 - invalid */
/* 1 - number */
/* 2 - start/end */
static const int str_2_integer_handle_table[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 2, 0, 0, 	/* 0 TAB, LF, CR */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 16 */
	2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 32 space */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 	/* 48 : 0 - 9 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 64 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 80 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 96 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 112 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 128 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 144 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 160 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 176 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 192 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 208 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 224 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 240 */
};

#define NUMBERLIMIT	10000000

enum	{
	S_START,
	S_NUMBER,
};

#define FETCH_NUMBER	do	{\
	switch(statu)	{\
	case S_START:\
		statu = S_NUMBER;\
		v = c - '0';\
		break;\
	case S_NUMBER:\
		v = (v * 10) + (c - '0');\
		if (v > NUMBERLIMIT)	{\
			return 0;\
		}\
		break;\
	}\
} while(0);

#define FETCH_BLANK	do	{\
	switch(statu)	{\
	case S_START:\
		break;\
	case S_NUMBER:\
		goto __finished;\
	}\
} while(0);

/* return parsed buffer size for success */
/* return 0 for error */
int
get_integer_1000000(const char * buf, int len, int * vp)	{
	unsigned char c;
	int statu;
	int h;
	int i;
	int v;

	statu = S_START;

	for (i = 0; i < len; i ++)	{
		c = buf[i];
		h = str_2_integer_handle_table[c];
		switch(h)	{
		case 0:
			return 0;
		case 1:
			FETCH_NUMBER
			break;
		case 2:
			FETCH_BLANK
			break;
		}
	}

	FETCH_BLANK

	return 0;

__finished:

	if (vp)	{
		*vp = v;
	}

	return i;
}

/* eof */
