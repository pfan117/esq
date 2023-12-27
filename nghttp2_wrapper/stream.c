#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
//#include <sys/time.h>
#include <time.h>
#include <nghttp2/nghttp2.h>

#include "public/libesq.h"
#include "public/esq.h"
#include "public/h2esq.h"

#include "public/esqll.h"
#include "nghttp2_wrapper/includes/instance.h"
#include "nghttp2_wrapper/includes/session.h"
#include "nghttp2_wrapper/includes/stream.h"
#include "nghttp2_wrapper/includes/cookie.h"

/* #define DUMP_POST_DATA */

/*
 * todo: using these:
 * nghttp2_option_set_max_reserved_remote_streams
 * nghttp2_option_set_no_auto_ping_ack
 */

int
h2esq_stream_get_id(void * h2esq_stream_ptr)	{
	h2esq_stream_t * h2esq_stream = h2esq_stream_ptr;
	return h2esq_stream->id;
}

void *
h2esq_stream_get_cookie_root(void * h2esq_stream_ptr)	{
	h2esq_stream_t * h2esq_stream = h2esq_stream_ptr;
	return h2esq_stream->cookies_root;
}

/* return 0 for success, -1 for already exist */
int
h2esq_stream_set_app_ctx(void * h2esq_stream_ptr, void * app_stream_ctx_ptr)	{
	h2esq_stream_t * h2esq_stream = h2esq_stream_ptr;
	if (h2esq_stream->app_stream_ctx)	{
		return -1;
	}
	h2esq_stream->app_stream_ctx = app_stream_ctx_ptr;
	return 0;
}

void *
h2esq_stream_get_app_ctx(void * h2esq_stream_ptr)	{
	h2esq_stream_t * h2esq_stream = h2esq_stream_ptr;
	return h2esq_stream->app_stream_ctx;
}

void
h2esq_stream_clear_app_ctx(void * h2esq_stream_ptr)	{
	h2esq_stream_t * h2esq_stream = h2esq_stream_ptr;
	h2esq_stream->app_stream_ctx = NULL;
	return;
}

int
h2esq_stream_get_path_matched_length(void * h2esq_stream_ptr)	{
	h2esq_stream_t * h2esq_stream = h2esq_stream_ptr;
	return h2esq_stream->path_matched_length;
}

void *
h2esq_stream_get_service_app_ctx(void * h2esq_stream_ptr)	{
	h2esq_stream_t * h2esq_stream = h2esq_stream_ptr;
	h2esq_service_t * h2esq_service;
	h2esq_service = h2esq_stream->h2esq_service;

	if (h2esq_service)	{
		return h2esq_service->ctx;
	}
	else	{
		return NULL;
	}

}

char *
h2esq_stream_get_header(void * h2esq_stream_ptr, int header_idx, int * header_len)	{
	h2esq_stream_t * h2esq_stream = h2esq_stream_ptr;

	if (h2esq_stream->header_strs[header_idx])	{
		*header_len = h2esq_stream->header_lens[header_idx];
		return h2esq_stream->header_strs[header_idx];
	}
	else	{
		*header_len = 0;
		return NULL;
	}
}

int
h2esq_stream_on_request_recv(
		nghttp2_session * nghttp2_session
		, h2esq_session_t * h2esq_session
		, h2esq_stream_t * h2esq_stream)
{
	h2esq_service_t * service;
	int matched_length;
	char * path;
	int r;

	path = h2esq_stream->header_strs[H2ESQ_HEADER_PATH];
	if (path)	{
		;
	}
	else	{
		return NGHTTP2_ERR_CALLBACK_FAILURE;
	}

	if (h2esq_stream->h2esq_service)	{
		service = h2esq_stream->h2esq_service;
		matched_length = h2esq_stream->path_matched_length;
	}
	else	{
		service = h2esq_instance_find_service(
				h2esq_session->h2esq_parent_instance
				, path
				, &matched_length
				);
		h2esq_stream->h2esq_service = service;
		h2esq_stream->path_matched_length = matched_length;
	}

	if (service)	{
		r = service->service_class->handler(nghttp2_session, h2esq_session, h2esq_stream, service->ctx);
		if (r)	{
			return NGHTTP2_ERR_CALLBACK_FAILURE;
		}
		else	{
			return 0;
		}
	}
	else	{
		nghttp2_submit_rst_stream(nghttp2_session, NGHTTP2_FLAG_NONE, h2esq_stream->id, NGHTTP2_NO_ERROR);
		return 0;
	}
}

STATIC int
h2esq_stream_dup_header(h2esq_stream_t * h2esq_stream, int idx, const char * value, size_t length)	{
	char * b;

	if (h2esq_stream->header_strs[idx])	{
		return 0;
	}

	b = malloc(length + 1);
	if (!b)	{
		return -1;
	}

	memcpy(b, value, length + 1);
	b[length] = '\0';
	h2esq_stream->header_strs[idx] = b;
	h2esq_stream->header_lens[idx] = length;

	return 0;
}

STATIC int
h2esq_stream_record_header(h2esq_stream_t * h2esq_stream
		, const char * name, size_t namelen
		, const char * value, size_t valuelen
		)
{
	/*
	 * <:authority>
	 * <:method>
	 * <:path>
	 * <accept>
	 * <accept-encoding>
	 * <accept-language>
	 * <cache-control>
	 * <cookie>
	 * <user-agent>
	 */

	/* printf("DBG: %s() %d: name = [%s], value = [%s]\n", __func__, __LINE__, name, value); */

	if (':' == name[0])	{
		if (!strncmp(":authority", name, sizeof(":authority")))	{
			return h2esq_stream_dup_header(h2esq_stream, H2ESQ_HEADER_AUTHORITY, value, valuelen);
		}
		else if (!strncmp(":method", name, sizeof(":method")))	{
			return h2esq_stream_dup_header(h2esq_stream, H2ESQ_HEADER_METHOD, value, valuelen);
		}
		else if (!strncmp(":path", name, sizeof(":path")))	{
			return h2esq_stream_dup_header(h2esq_stream, H2ESQ_HEADER_PATH, value, valuelen);
		}
	}
	else if ('a' == name[0])	{
		if (!strncmp("accept", name, sizeof("accept")))	{
			return h2esq_stream_dup_header(h2esq_stream, H2ESQ_HEADER_ACCEPT, value, valuelen);
		}
		else if (!strncmp("accept-encoding", name, sizeof("accept-encoding")))	{
			return h2esq_stream_dup_header(h2esq_stream, H2ESQ_HEADER_ACCEPT_ENCODING, value, valuelen);
		}
		else if (!strncmp("accept-language", name, sizeof("accept-language")))	{
			return h2esq_stream_dup_header(h2esq_stream, H2ESQ_HEADER_ACCEPT_LANGUAGE, value, valuelen);
		}
	}
	else if ('c' == name[0])	{
		if (!strncmp("cache-control", name, sizeof("cache-control")))	{
			return h2esq_stream_dup_header(h2esq_stream, H2ESQ_HEADER_CACHE_CONTROL, value, valuelen);
		}
		else if (!strncmp("cookie", name, sizeof("cookie")))	{
			return h2esq_save_cookie(h2esq_stream->cookies_root, value, valuelen);
		}
	}
	else if ('u' == name[0])	{
		if (!strncmp("user-agent", name, sizeof("user-agent")))	{
			return h2esq_stream_dup_header(h2esq_stream, H2ESQ_HEADER_USER_AGENT, value, valuelen);
		}
	}
	else if (('h' == name[0]) || ('H' == name[0]))	{
		if (!strncasecmp("host", name, sizeof("host")))	{
			return h2esq_stream_dup_header(h2esq_stream, H2ESQ_HEADER_HOST, value, valuelen);
		}
	}

	return 0;
}

int
h2esq_stream_on_header(nghttp2_session * nghttp2_session, const nghttp2_frame * frame
		, const uint8_t * name, size_t namelen
		, const uint8_t * value, size_t valuelen
		, uint8_t flags, void * user_data
		)
{
	h2esq_stream_t * stream;
	int r;

	if  (NGHTTP2_HEADERS == frame->hd.type )	{
		;	/* ok */
	}
	else if  (NGHTTP2_PUSH_PROMISE == frame->hd.type )	{
		esq_design_error();	/* TODO: could server meet this */
		return NGHTTP2_ERR_CALLBACK_FAILURE;
	}
	else	{
		esq_design_error();	/* TODO: could server meet this */
		return NGHTTP2_ERR_CALLBACK_FAILURE;
	}

	if (NGHTTP2_HCAT_REQUEST == frame->headers.cat)	{
		;	/* ok */
	}
	else	{
		esq_design_error();	/* TODO: could server meet this */
		return NGHTTP2_ERR_CALLBACK_FAILURE;
	}

	stream = nghttp2_session_get_stream_user_data(nghttp2_session, frame->hd.stream_id);
	if (stream)	{
		r = h2esq_stream_record_header(stream, (const char *)name, namelen, (const char *)value, valuelen);
		if (r)	{
			return NGHTTP2_ERR_CALLBACK_FAILURE;
		}
	}
	else	{
		esq_design_error();
		return NGHTTP2_ERR_CALLBACK_FAILURE;
	}

	return 0;
}

STATIC h2esq_stream_t *
h2esq_create_stream(h2esq_session_t * h2esq_session, int id)	{
	h2esq_stream_t * stream;

	stream = esq_malloc(h2esq_stream_t);
	if (!stream)	{
		return NULL;
	}

	bzero(stream, sizeof(h2esq_stream_t));
	stream->id = id;

	return stream;
}

int
h2esq_stream_on_begin_headers(nghttp2_session * nghttp2_session, const nghttp2_frame * frame, void * user_data)
{
	h2esq_session_t * h2esq_session = user_data;
	h2esq_stream_t * h2esq_stream;
	h2esq_stream_t * head;
	h2esq_stream_t * tail;
	int id;

	if (frame->hd.type != NGHTTP2_HEADERS)	{
		return 0;
	}

	if (frame->headers.cat != NGHTTP2_HCAT_REQUEST)	{
		return 0;
	}

	id = frame->hd.stream_id;
	h2esq_stream = h2esq_create_stream(h2esq_session, id);
	if (!h2esq_stream)	{
		return NGHTTP2_ERR_CALLBACK_FAILURE;
	}

	nghttp2_session_set_stream_user_data(nghttp2_session, id, h2esq_stream);
	h2esq_stream->parent_session = h2esq_session;

	head = h2esq_session->stream_list_head;
	tail = h2esq_session->stream_list_tail;
	DOUBLE_LL_JOIN_HEAD(
			head, tail
			, prev, next
			, h2esq_stream
			);
	h2esq_session->stream_list_head = head;
	h2esq_session->stream_list_tail = tail;

	return 0;
}

STATIC void
h2esq_stream_free_headers(h2esq_stream_t * h2esq_stream)	{
	int i;

	for (i = 0; i < H2ESQ_HEADER_MAX; i ++)	{
		if (h2esq_stream->header_strs[i])	{
			free(h2esq_stream->header_strs[i]);
		}
	}

	return;
}

void
h2esq_stream_close(h2esq_stream_t * h2esq_stream)	{
	h2esq_session_t * h2esq_session = h2esq_stream->parent_session;

	if (h2esq_stream->h2esq_service)	{
		h2esq_stream->h2esq_service->service_class->on_stream_close(h2esq_stream);
	}

	h2esq_stream_free_headers(h2esq_stream);
	h2esq_free_cookies(h2esq_stream->cookies_root);
	DOUBLE_LL_LEFT(
			h2esq_session->stream_list_head
			, h2esq_session->stream_list_tail
			, prev, next
			, h2esq_stream
			);
	free(h2esq_stream);

	return;
}

int
h2esq_stream_on_close(nghttp2_session * nghttp2_session, int32_t stream_id, uint32_t error_code, void * user_data)	{
	h2esq_stream_t * h2esq_stream;

	h2esq_stream = nghttp2_session_get_stream_user_data(nghttp2_session, stream_id);
	if (!h2esq_stream)	{
		return 0;
	}

	h2esq_stream_close(h2esq_stream);

	return 0;
}

#ifdef DUMP_POST_DATA

#define HEXLINELEN	20
static int frame_idx = 0;
#define TIMESTRLEN	128
static char time_str[TIMESTRLEN];

STATIC int
h2esq_dump_line(char * data, int pos)	{
	char c;
	int i;

	printf("/* %4d: ", pos);

	for (i = 0; i < HEXLINELEN; i ++)	{
		c = data[i];
		if (isprint(c))	{
			printf("%c", c);
		}
		else	{
			printf(" ");
		}
	}

	printf(" */ \"");

	for (i = 0; i < HEXLINELEN; i ++)	{
		printf("\\x%02hhx", data[i]);
	}

	printf("\"\\\n");

	return HEXLINELEN;
}

STATIC int
h2esq_dump_line2(char * data, int len, int pos)	{
	char c;
	int i;

	printf("/* %4d: ", pos);

	for (i = 0; i < len; i ++)	{
		c = data[i];
		if (isprint(c))	{
			printf("%c", c);
		}
		else	{
			printf(" ");
		}
	}

	for (; i < HEXLINELEN; i ++)	{
		printf(" ");
	}

	printf(" */ \"");

	for (i = 0; i < len; i ++)	{
		printf("\\x%02hhx", data[i]);
	}

	printf("\"\\\n");

	return len;
}

int
h2esq_stream_on_data_chunk_recv(nghttp2_session * nghttp2_session, uint8_t flags, int32_t stream_id
		, const uint8_t * data, size_t len, void * user_data)
{
	int i;

	if (!frame_idx)	{
		time_t timep;
		struct tm * p;
		time(&timep);
		p = gmtime(&timep);
		snprintf(time_str, TIMESTRLEN, "%04d%02d%02d_%02d%02d%02d"
				, (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday
				, p->tm_hour, p->tm_min, p->tm_sec
				);
	}

	printf("char * frame_%s_%02d = \n", time_str, frame_idx);

	for (i = 0; i < len; )	{
		if (len - i >= HEXLINELEN)	{
			i += h2esq_dump_line((char *)data + i, i);
		}
		else	{
			i += h2esq_dump_line2(((char *)data) + i, len - i, i);
		}
	}

	printf(";\n\n");

	frame_idx ++;

	return 0;
}

#else

int
h2esq_stream_on_data_chunk_recv(nghttp2_session * nghttp2_session, uint8_t flags, int32_t stream_id
		, const uint8_t * data, size_t len, void * user_data)
{
	h2esq_session_t * h2esq_session = user_data;
	h2esq_stream_t * h2esq_stream;
	h2esq_service_t * service;
	int matched_length;
	int method_len;
	char * method;
	char * path;
	int r;

	h2esq_stream = nghttp2_session_get_stream_user_data(nghttp2_session, stream_id);
	if (!h2esq_stream)	{
		goto __exx;
	}

	method = h2esq_stream_get_header(h2esq_stream, H2ESQ_HEADER_METHOD, &method_len);
	if (memcmp(method, "POST", sizeof("POST")))	{
		goto __exx;
	}

	path = h2esq_stream->header_strs[H2ESQ_HEADER_PATH];
	if (path)	{
		;
	}
	else	{
		return NGHTTP2_ERR_CALLBACK_FAILURE;
	}

	if (h2esq_stream->h2esq_service)	{
		service = h2esq_stream->h2esq_service;
	}
	else	{
		service = h2esq_instance_find_service(
				h2esq_session->h2esq_parent_instance
				, path
				, &matched_length
				);
		h2esq_stream->h2esq_service = service;
		h2esq_stream->path_matched_length = matched_length;
	}

	if (service)	{
		r = service->service_class->on_parse_post_data(h2esq_stream, (const char *)data, len);
		if (r)	{
			goto __exx;
		}
	}
	else	{
		nghttp2_submit_rst_stream(nghttp2_session, NGHTTP2_FLAG_NONE, h2esq_stream->id, NGHTTP2_NO_ERROR);
		return 0;
	}

	return 0;

__exx:

	return NGHTTP2_ERR_CALLBACK_FAILURE;
}

#endif

/* eof */
