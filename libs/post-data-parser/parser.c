#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "public/libesq.h"
#ifdef LIBDEBUG
	#include "tst/test-frame.h"
#endif

static int
pdp_locate_boundary(const char * buf, size_t len)	{
	char * finish_mark = "\x0d\x0a";
	char * r;
	int max;
	int l;

	if (PDP_BOUNDARY_MAX < len)	{
		max = PDP_BOUNDARY_MAX;
	}
	else	{
		max = len;
	}

	r = memmem(buf, max, finish_mark, 2);
	if (r)	{
		l = r - buf - 1;
		if (l >= len || l <= 0)	{
			return -1;
		}
		else	{
			return l;
		}
	}
	else	{
		return -1;
	}
}

#define CONSUME_ITEM(__ETYPE__,__BUFFER__,__LEN__)	do	{\
	if (cb(app_ctx, (__ETYPE__), (__BUFFER__), (__LEN__)))	{\
		return -1;\
	}\
	else	{\
		last_item_type = (__ETYPE__);\
		i += (__LEN__);\
	}\
}while(0);

#define CONSUME_ITEM2(__ETYPE__,__BUFFER__,__LEN__,__VALUE__,__VLEN__)	do	{\
	if (cb(app_ctx, (__ETYPE__), (__VALUE__), (__VLEN__)))	{\
		return -1;\
	}\
	else	{\
		last_item_type = (__ETYPE__);\
		i += (__LEN__);\
	}\
}while(0);

#define GET_NAME_END(__START__)	do	{\
	int t;\
	name_end = -1;\
	for (t = (__START__); t < len; t ++)	{\
		if ('\"' == buf[t])	{\
			name_end = t;\
			break;\
		}\
	}\
} while(0);

int
post_data_parser(const char * buf, int len, post_data_parser_app_cb cb, post_data_ctx_t * ctx, void * app_ctx)	{
	int last_item_type;
	char * item_end;
	int name_end;
	int i;
	int r;

	i = 0;

	if (POST_ET_VOID == ctx->last_item_type)	{
		r = pdp_locate_boundary(buf, len);
		if (r <= 2 || (r + 2) >= sizeof(ctx->boundary))	{
			return -1;
		}
		ctx->boundary[0] = '\x0d';	/* leading 0d 0a alse recorded */
		ctx->boundary[1] = '\x0a';
		memcpy(ctx->boundary + 2, buf, r);
		ctx->boundary_len = r + 2;
		ctx->boundary[r + 2] = '\0';
		CONSUME_ITEM(POST_ET_BOUNDARY, buf, r + 1);
	}
	else	{
		last_item_type = ctx->last_item_type;
	}

	while(i < len)	{
		switch(last_item_type)	{
		case POST_ET_VOID:
			goto __exx;
		case POST_ET_ERROR:
			return -1;
		case POST_ET_EMPTY_LINE:
			item_end = memmem(buf + i, len - i, ctx->boundary, ctx->boundary_len);
			if (item_end)	{
				if (item_end == buf + i)	{
					CONSUME_ITEM(POST_ET_VALUE, buf + i, 0);
					CONSUME_ITEM(POST_ET_BOUNDARY, buf + i, ctx->boundary_len + 1);
				}
				else	{
					CONSUME_ITEM(POST_ET_VALUE, buf + i, item_end - buf - i);
					CONSUME_ITEM(POST_ET_BOUNDARY, buf + i, ctx->boundary_len + 1);
				}
			}
			else	{
				CONSUME_ITEM(POST_ET_VALUE, buf + i, len - i);
				goto __ok;
			}
			break;
		case POST_ET_BOUNDARY:
			#define CONTENT_DFD	"\x0d\x0a""Content-Disposition: form-data;"
			if ((len - i >= (sizeof(CONTENT_DFD) - 1)) && (!memcmp(CONTENT_DFD, buf + i, sizeof(CONTENT_DFD) - 1)))	{
				last_item_type = POST_ET_CONTENT_FORM_DATA;
				CONSUME_ITEM(POST_ET_CONTENT_FORM_DATA, buf + i, sizeof(CONTENT_DFD) - 1);
			}
			else if ((len - i == 4) && ('-' == buf[i]) && ('-' == buf[i + 1])
					&& ('\x0d' == buf[i + 2]) && ('\x0a' == buf[i + 3]))
			{
				cb(app_ctx, POST_ET_FINISH, buf + i, 4);
				ctx->last_item_type = POST_ET_FINISH;
				return 0;
			}
			else	{
				goto __exx;
			}
			break;
		case POST_ET_CONTENT_FORM_DATA:
			#define NAME_S	" name=\""
			if ((len - i >= (sizeof(NAME_S) - 1)) && (!memcmp(NAME_S, buf + i, sizeof(NAME_S) - 1)))	{
				GET_NAME_END(i + sizeof(NAME_S) - 1);
				if (name_end <= 0)	{
					goto __exx;
				}
				else	{
					CONSUME_ITEM2(POST_ET_NAME
							, buf + i, name_end - i + 1
							, buf + i + sizeof(NAME_S) - 1, name_end - i - sizeof(NAME_S) + 1
							);
				}
				break;
			}
			else	{
				goto __exx;
			}
		case POST_ET_FINISH:
			return -1;
		case POST_ET_NAME:
			#define FNAME_S	"; filename=\""
			#define EMPTY_LINE	"\x0d\x0a\x0d\x0a"
			if ((len - i >= (sizeof(FNAME_S) - 1)) && (!memcmp(FNAME_S, buf + i, sizeof(FNAME_S) - 1)))	{
				GET_NAME_END(i + sizeof(FNAME_S) - 1);
				if (name_end <= 0)	{
					goto __exx;
				}
				else	{
					CONSUME_ITEM2(POST_ET_FILENAME
							, buf + i, name_end - i + 1
							, buf + i + sizeof(FNAME_S) - 1, name_end - i - sizeof(FNAME_S) + 1
							);
				}
			}
			else if ((len - i >= (sizeof(EMPTY_LINE) - 1)) && (!memcmp(EMPTY_LINE, buf + i, sizeof(EMPTY_LINE) - 1)))	{
				CONSUME_ITEM(POST_ET_EMPTY_LINE, buf + i, sizeof(EMPTY_LINE) - 1);
			}
			else	{
				goto __exx;
			}
			break;
		case POST_ET_FILENAME:
			#define CONTENT_TYPE_PREFIX "\x0d\x0a""Content-Type: "
			if ((len - i > sizeof(CONTENT_TYPE_PREFIX))
					&& (!memcmp(CONTENT_TYPE_PREFIX, buf + i, sizeof(CONTENT_TYPE_PREFIX) - 1)))
			{
				char * type_end;
				int type_len;
				int item_len;

				type_end = memmem(buf + i + sizeof(CONTENT_TYPE_PREFIX), len - i, "\x0d\x0a", 2);
				if (!type_end)	{
					goto __exx;
				}

				item_len = type_end - buf - i;
				type_len = item_len - (sizeof(CONTENT_TYPE_PREFIX) - 1);

				CONSUME_ITEM2(POST_ET_CONTENT_TYPE
						, buf + i, item_len
						, buf + i + sizeof(CONTENT_TYPE_PREFIX) - 1, type_len
						);
			}
			else	{
				goto __exx;
			}
			break;
		case POST_ET_VALUE:
			item_end = memmem(buf + i, len - i, ctx->boundary, ctx->boundary_len);
			if (item_end)	{
				if (item_end == buf + i)	{
					CONSUME_ITEM(POST_ET_BOUNDARY, buf + i, ctx->boundary_len + 1);
				}
				else	{
					CONSUME_ITEM(POST_ET_VALUE, buf + i, item_end - buf - i);
					CONSUME_ITEM(POST_ET_BOUNDARY, buf + i, ctx->boundary_len + 1);
				}
			}
			else	{
				CONSUME_ITEM(POST_ET_VALUE, buf + i, len - i);
				goto __ok;
			}
			break;
		case POST_ET_CONTENT_TYPE:
			if ((len - i >= (sizeof(EMPTY_LINE) - 1)) && (!memcmp(EMPTY_LINE, buf + i, sizeof(EMPTY_LINE) - 1)))	{
				CONSUME_ITEM(POST_ET_EMPTY_LINE, buf + i, sizeof(EMPTY_LINE) - 1);
			}
			else	{
				goto __exx;
			}
			break;
		default:
			goto __exx;
		}
	}

__ok:

	ctx->last_item_type = last_item_type;

	return 0;

__exx:

	cb(app_ctx, POST_ET_ERROR, NULL, 0);
	ctx->last_item_type = POST_ET_ERROR;

	return -1;
}

/* eof */
