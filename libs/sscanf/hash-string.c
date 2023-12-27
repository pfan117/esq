#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "public/libesq.h"

/* 0 - invalid */
/* 1 - number */
/* 2 - start/end */
/* 3 - '*' */
static const int hash_string_handle_table[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 2, 0, 0, 	/* 0 TAB, LF, CR */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 16 */
	2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 32 space, '*' */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 	/* 48 : 0 - 9 */
	0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 64 : 'A' - 'F' */
	0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 	/* 80 : 'X' */
	0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 96 : 'a' - 'f' */
	0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 	/* 112: 'x' */
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
	S_HASH,
};

#define FETCH_NUMBER	do	{\
	switch(statu)	{\
	case S_START:\
		statu = S_HASH;\
		start = i;\
		break;\
	case S_HASH:\
		break;\
	}\
} while(0);

#define FETCH_BLANK	do	{\
	switch(statu)	{\
	case S_START:\
		break;\
	case S_HASH:\
		return -1;\
	}\
} while(0);

/* return parsed buffer size for success */
/* return -1 for error */
/* input strings like "x", "aax", "12acx" */
/* output *hash_len is the length of hash string not includes the end 'x' */
int
get_hash_string(const char * buf, int len, int * hash_start, int * hash_len)	{
	unsigned char c;
	int start;
	int statu;
	int h;
	int i;
	int l;

	statu = S_START;
	start = 0;

	for (i = 0; i < len; i ++)	{
		c = buf[i];
		h = hash_string_handle_table[c];
		switch(h)	{
		case 0:	/* invalid */
			return -1;
		case 1:	/* number */
			FETCH_NUMBER
			break;
		case 2:	/* start/end */
			FETCH_BLANK
			break;
		case 3:	/* end mark */
			goto __finished;
			break;
		default:
			return -1;
			break;
		}
	}

	return -1;

__finished:

	l = i - start;
	if ((l & 1))	{
		return -1;
	}

	*hash_start = start;
	*hash_len = l;

	return i + 1;
}

/* 0 - invalid */
/* 1 - number */
/* 2 - start/end */
/* 3 - '*' */
static const int hash_string_2_bin_table[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 0 TAB, LF, CR */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 16 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 32 space, '*' */
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 	/* 48 : 0 - 9 */
	0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 64 : 'A' - 'F' */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 80 */
	0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	/* 96 : 'a' - 'f' */
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


/* return 0 for success */
int
hash_str_2_bin(unsigned char * buf, const char * hash_string, int hash_str_len)	{
	int ch;
	int cl;
	int i;
	int w;

	if ((hash_str_len & 1))	{
		return -1;
	}

	w = 0;

	for (i = 0; i < hash_str_len; i += 2)	{
		ch = hash_string_2_bin_table[(unsigned char)(hash_string[i])];
		cl = hash_string_2_bin_table[(unsigned char)(hash_string[i + 1])];
		buf[w] = (ch << 4) + cl;
		w ++;
	}

	return 0;
}

/* eof */
