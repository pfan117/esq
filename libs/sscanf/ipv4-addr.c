#include <stdio.h>
#include <string.h>

#include "public/libesq.h"

/*
 * ok:
 * "  1.1.1.1 "
 * "1.1.1.1 "
 *
 * not ok:
 * "1.1.1.1x"
 * "x1.1.1.1"
 *
 */

/* 0 - invalid */
/* 1 - number */
/* 2 - . */
/* 3 - start/end */
static const int str_2_ipv4_handle_table[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 3, 0, 0, 	/* 0 TAB, LF, CR */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 16 */
	3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 	/* 32 space */
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

enum	{
	S_START,
	S_NUMBER1,
	S_DOT1,
	S_NUMBER2,
	S_DOT2,
	S_NUMBER3,
	S_DOT3,
	S_NUMBER4,
};

#define FETCH_NUMBER	do	{\
	switch(statu)	{\
	case S_START:\
		statu = S_NUMBER1;\
		number1 = c - '0';\
		break;\
	case S_NUMBER1:\
		number1 = (number1 * 10) + (c - '0');\
		if (number1 >= 255)	{\
			return 0;\
		}\
		break;\
	case S_DOT1:\
		statu = S_NUMBER2;\
		number2 = c - '0';\
		break;\
	case S_NUMBER2:\
		number2 = (number2 * 10) + (c - '0');\
		if (number2 >= 255)	{\
			return 0;\
		}\
		break;\
	case S_DOT2:\
		statu = S_NUMBER3;\
		number3 = c - '0';\
		break;\
	case S_NUMBER3:\
		number3 = (number3 * 10) + (c - '0');\
		if (number3 >= 255)	{\
			return 0;\
		}\
		break;\
	case S_DOT3:\
		statu = S_NUMBER4;\
		number4 = c - '0';\
		break;\
	case S_NUMBER4:\
		number4 = (number4 * 10) + (c - '0');\
		if (number4 >= 255)	{\
			return 0;\
		}\
		break;\
	default:\
		return 0;\
	}\
} while(0);

#define FETCH_DOT	do	{\
	switch(statu)	{\
	case S_START:\
		return 0;\
	case S_NUMBER1:\
		statu = S_DOT1;\
		break;\
	case S_DOT1:\
		return 0;\
	case S_NUMBER2:\
		statu = S_DOT2;\
		break;\
	case S_DOT2:\
		return 0;\
	case S_NUMBER3:\
		statu = S_DOT3;\
		break;\
	case S_DOT3:\
		return 0;\
	case S_NUMBER4:\
		return 0;\
	default:\
		return 0;\
	}\
} while(0);

#define FETCH_BLANK	do	{\
	switch(statu)	{\
	case S_START:\
		break;\
	case S_NUMBER1:\
		return 0;\
	case S_DOT1:\
		return 0;\
	case S_NUMBER2:\
		return 0;\
	case S_DOT2:\
		return 0;\
	case S_NUMBER3:\
		return 0;\
	case S_DOT3:\
		return 0;\
	case S_NUMBER4:\
		goto __finished;\
	default:\
		return 0;\
	}\
} while(0);

/* return parsed buffer size for success */
/* return 0 for error */
int
get_ipv4_addr(const char * buf, int len, unsigned long * ap)	{
	unsigned char c;
	int statu;
	int h;
	int i;
	unsigned int number1;
	unsigned int number2;
	unsigned int number3;
	unsigned int number4;
	unsigned int a;

	statu = S_START;
	number1 = 0;
	number2 = 0;
	number3 = 0;
	number4 = 0;

	for (i = 0; i < len; i ++)	{
		c = buf[i];
		h = str_2_ipv4_handle_table[c];
		switch(h)	{
		case 0:
			return 0;
		case 1:
			FETCH_NUMBER
			break;
		case 2:
			FETCH_DOT
			break;
		case 3:
			FETCH_BLANK
			break;
		default:
			return 0;
			break;
		}
	}

	FETCH_BLANK

	return 0;

__finished:

	if (ap)	{
		a = (number1 << 24) + (number2 << 16) + (number3 << 8) + number4;
		*ap = a;
	}

	return i;
}

int
match_ipv4_addr(const char * buf, int len)	{
	unsigned long a;
	int r;

	r = get_ipv4_addr(buf, len, &a);

	return r;
}

/* return parsed buffer size for success */
/* return 0 for error */
int
get_ipv4_addr2(const char * buf, unsigned long * ap)	{
	int r;
	int len;

	len = strnlen(buf, 16);

	if (len > 15 || len <= 0)	{
		return 0;
	}

	r = get_ipv4_addr(buf, len, ap);

	return r;
}

/* eof */
