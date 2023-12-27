#include <stdio.h>
#include <string.h>
#include <nghttp2/nghttp2.h>
#include <monkeycall.h>
#include "public/esq.h"
#include "public/libesq.h"

/* action tables */
/* '0' - '9' : 1 */
/* 'A' - 'Z' : 2 */
/* 'a' - 'z' : 3 */
/* '%' : 4 */
/* '=' : 14 */
/* '?' : 15 */
const static unsigned char esq_param_spliter_char_type[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/*   0 - 15 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/*  16 - 31 */
	0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/*  32 - 47 */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 14, 0, 15,	/*  48 - 63 */
	0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,	/*  64 - 79 */
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0,	/*  80 - 95 */
	0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,	/*  96 - 111 */
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0,	/* 112 - 127 */

	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 128 - 143 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 144 - 159 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 160 - 175 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 176 - 191 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 192 - 207 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 208 - 223 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 224 - 239 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 240 - 255 */
};


/* return -1 for error */
/* return count of parameters that recognized */
/* ?a=xxx?b=yyy?c=zzz */
int
esq_split_parameters(char * url_ptr, int url_len, esq_url_parameter_t * param, int param_max)	{
	/* start, after question mark, name, after equal mark, value */
	enum { S, Aq, Name, Ae, Value } s;
	unsigned char * url;
	unsigned int c;
	int idx = 0;
	int name_start;
	int value_start = 0;
	int r;
	int i;

	url = (unsigned char *)url_ptr;
	s = S;

	/* printf("input string: %s\n", url_ptr); */

	for (i = 0; i < url_len; i ++)	{
		c = url[i];
		switch(s)	{
		case S:
			if ('?' == c)	{
				s = Aq;
			}
			else	{
				return -1;
			}
			break;
		case Aq:
			r = esq_param_spliter_char_type[c];
			switch (r)	{
			case 1:
			case 2:
			case 3:	/* name */
				name_start = i;
				s = Name;
				break;
			case 4:  /* '%' */
				return -1;	/* not allowed to use '%' in name part */
			case 15:	/* '=' */
				break;
			case 14:	/* '?' */
				break;
			default:
				return -1;
			}
			break;
		case Name:
			r = esq_param_spliter_char_type[c];
			switch (r)	{
			case 1:
			case 2:
			case 3:	/* name */
				break;
			case 4:  /* '%' */
				return -1;	/* not allowed to use '%' in name part */
			case 14:	/* '=' */
				s = Ae;
				if (idx < param_max)	{
					url[i] = '\0';
					param[idx].name = url_ptr + name_start;
					param[idx].name_len = i - name_start;
				}
				else	{
					return -1;
				}
				break;
			case 15:	/* '?' */
				return -1;
			default:
				return -1;
			}
			break;
		case Ae:
			r = esq_param_spliter_char_type[c];
			switch (r)	{
			case 1:
			case 2:
			case 3:	/* name */
				s = Value;
				value_start = i;
				break;
			case 4:  /* '%' */
				break;
			case 14:	/* '=' */
				return -1;
			case 15:	/* '?' */
				s = Aq;
				param[idx].value = NULL;
				param[idx].value_len = 0;
				idx ++;
				break;
			default:
				return -1;
			}
			break;
		case Value:
			r = esq_param_spliter_char_type[c];
			switch (r)	{
			case 1:
			case 2:
			case 3:	/* name */
				break;
			case 4:  /* '%' */
				break;
			case 14:	/* '=' */
				break;
			case 15:	/* '?' */
				s = Aq;
				url[i] = '\0';
				param[idx].value = url_ptr + value_start;
				param[idx].value_len = i - value_start;
				idx ++;
				break;
			default:
				return -1;
			}
			break;
		default:
			return -1;
		}
	}

	if (Value == s)	{
		url[i] = '\0';
		param[idx].value = url_ptr + value_start;
		param[idx].value_len = i - value_start;
		idx ++;
	}
	else if (Ae == s)	{
		param[idx].value = NULL;
		param[idx].value_len = 0;
		idx ++;
	}

	for (i = idx; i < param_max; i ++)	{
		param[i].name = NULL;
		param[i].name_len = 0;
		param[i].value = NULL;
		param[i].value_len = 0;
	}

	return idx;
}

void
esq_url_params_print(esq_url_parameter_t * param, int param_max)	{
	int i;

	for (i = 0; i < param_max; i ++)	{
		if (param[i].name)	{
			if (param[i].value)	{
				printf("\"%s\" : \"%s\"\n", param[i].name, param[i].value);
			}
			else	{
				printf("\"%s\" : -\n", param[i].name);
			}
		}
		else	{
			break;
		}
	}

	return;
}

int
esq_url_parames_escape_in_place(esq_url_parameter_t * param, int param_max)	{
	int i;
	int r;

	for (i = 0; i < param_max; i ++)	{
		r = escape_percent_in_place(param[i].name, param[i].name_len);
		if (r < 0)	{
			return -1;
		}
		param[i].name[r] = '\0';
		param[i].name_len = r;

		r = escape_percent_in_place(param[i].value, param[i].value_len);
		if (r < 0)	{
			return -1;
		}
		param[i].value[r] = '\0';
		param[i].value_len = r;
	}

	return 0;
}

/* eof */
