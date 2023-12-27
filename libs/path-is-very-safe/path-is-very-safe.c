#include <stdio.h>
#include "public/libesq.h"

/* #define DEBUG	1 */

#ifdef DEBUG
#define dbg_print(...)	printf(__VA_ARGS__)
#else
#define dbg_print(...)
#endif

static const char h2esq_path_safe_table[] = {
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0 - 15 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 16 - 31 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, /* 32 - 47: '+', '-' */
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,	/* 48 - 63: '0' - '9' */
0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 64 - 79: 'A' - */
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, /* 80 - 95: - 'Z', '_' */
0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 96 - 111: 'a' - */
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, /* 112 - 127: - 'z' */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 128 - 143 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 144 - 159 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 160 - 175 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 176 - 191 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 192 - 207 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 208 - 223 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 224 - 239 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 240 - 255 */
};

/* these are ok: 'a-z', 'A-Z', '0-9', '_', '-' */
/* these are ok: not leading '/' */
/* these are ok: in the range of a file name, not leading not ending '.' */
int
path_is_very_safe(const char * path)	{
	int filename_len;
	unsigned int c;
	int afterdot;
	int i;
	int r;

	if ('/' == path[0])	{
		dbg_print("error: start with slash\n");
		return 0;
	}

	filename_len = 0;
	afterdot = 0;

	for (i = 0; i < SAFE_PATH_MAX;)	{
		if (filename_len >= SAFE_FILENAME_MAX)	{
			dbg_print("error: filename too long\n");
			return 0;
		}

		r = get_code_from_utf8(path + i, &c);
		if (1 == r)	{
			i += 1;	/* ascii or '\0' */
		}
		else if (r <= 0)	{
			dbg_print("error: invalid utf8 code\n");
			return 0;
		}
		else	{
			i += r;	/* utf8 */
			filename_len += r;
			continue;
		}

		if (h2esq_path_safe_table[c])	{
			filename_len ++;
			afterdot = 0;
		}
		else	{
			if ('/' == c)	{
				if (afterdot)	{
					return 0;
				}
				else	{
					filename_len = 0;
					afterdot = 0;
				}
			}
			else if ('\0' == c)	{
				if (afterdot)	{
					dbg_print("error: end with dot\n");
					return 0;
				}
				else	{
					return 1;
				}
			}
			else if ('.' == c)	{
				afterdot = 1;
				if (filename_len)	{
					filename_len ++;
				}
				else	{
					dbg_print("error: start with dot\n");
					return 0;
				}
			}
			else {
				dbg_print("error: invalid char [%c]\n", c);
				return 0;
			}
		}
	}

	dbg_print("error: too long\n");

	return 0;
}

/* eof */
