#include <nghttp2/nghttp2.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>

#include <sys/types.h>	/* stat(), lstat */
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>	/* open() */

#include "public/esq.h"
#include "public/esqll.h"
#include "public/h2esq.h"
#include "public/libesq.h"
#include "modules/foo_server/internal.h"
#include "modules/foo_server/callbacks.h"

typedef void (*foo_test_function)(void *);

typedef struct _foo_cb_t	{
	char * cmd_str;
	char * title_str;
	foo_test_function test_function;
} foo_cb_t;

foo_cb_t foo_cbs[] = {
	{ NULL, "Network connection test group", NULL },
	{ "mco-basic-test", "Mco basic test", foo_mco_basic_test },
	{ NULL, "Disk DB test group", NULL },
	{ "disk-db-bsc-test", "Disk DB basic test", foo_disk_db_bsc_test },
	{ "disk-db-burst-set", "Multiple set disk DB records", foo_disk_db_burst_set_test },
	{ "disk-db-burst-get", "Multiple get disk DB records", foo_disk_db_burst_get_test },
	{ "disk-db-burst-del", "Multiple del disk DB records", foo_disk_db_burst_del_test },
	{ NULL, "Mem DB test group", NULL },
	{ "mem-db-bsc-test", "Memory DB basic test", foo_mem_db_bsc_test },
	{ "mem-db-burst-set", "Multiple set mem DB records", foo_mem_db_burst_set_test },
	{ "mem-db-burst-get", "Multiple get mem DB records", foo_mem_db_burst_get_test },
	{ "mem-db-burst-del", "Multiple del mem DB records", foo_mem_db_burst_del_test },
	{ NULL, "Monkey call test group", NULL },
	{ "pony-bee-bsc-monkey", "Monkey call basic test", foo_pony_query_monkey_bsc },
};

STATIC int
foo_write_page(char * path_head, char * buf, int buflen)	{
	int offset = 0;
	int i;
	int r;

	r = snprintf(buf + offset, buflen - offset, "<html><head><title>foo service</title></head><body>\n");
	if (r > 0 && r < buflen - offset)	{
		offset += r;
	}
	else	{
		return -1;
	}

	for (i = 0; i < (sizeof(foo_cbs) / sizeof(foo_cbs[0])); i ++)	{
		if (foo_cbs[i].cmd_str)	{
			r = snprintf(buf + offset, buflen - offset, "<p><a href=\"%s%s\">%s</a></p>\n"
					, path_head
					, foo_cbs[i].cmd_str
					, foo_cbs[i].title_str
					);
		}
		else	{
			r = snprintf(buf + offset, buflen - offset, "<p><b>%s</b></p>\n"
					, foo_cbs[i].title_str
					);
		}

		if (r > 0 && r < buflen - offset)	{
			offset += r;
		}
		else	{
			return -1;
		}
	}

	r = snprintf(buf + offset, buflen - offset, "</body></html>\n");
	if (r > 0 && r < buflen - offset)	{
		offset += r;
	}
	else	{
		return -1;
	}

	return offset;
}

STATIC void
foo_call_test_function(char * cmdstr, void * esq_session)	{
	int i;

	for (i = 0; i < (sizeof(foo_cbs) / sizeof(foo_cbs[0])); i ++)	{
		if (!foo_cbs[i].cmd_str)	{
			continue;
		}

		if (strcmp(cmdstr, foo_cbs[i].cmd_str))	{
			continue;
		}

		esq_start_mco_session(
				foo_cbs[i].cmd_str
				, NULL
				, foo_cbs[i].test_function
				, 64 * 1024
				, NULL
				);

		break;
	}

	return;
}

foo_stream_ctx_t *
foo_new_ctx(foo_service_ctx_t * ctx)	{
	foo_stream_ctx_t * p;
	int r;

	p = malloc(sizeof(foo_stream_ctx_t));
	if (!p)	{
		return NULL;
	}

	r = foo_write_page(ctx->path_head, p->page_buf, sizeof(p->page_buf));
	if (r <= 0)	{
		free(p);
		return NULL;
	}

	p->service_ctx = ctx;
	p->page_seek = 0;
	p->page_size = r;

	return p;
}

STATIC ssize_t
foo_read_page_buffer_cb(nghttp2_session *session, int32_t stream_id
		, uint8_t *buf, size_t length
		, uint32_t *data_flags
		, nghttp2_data_source *source, void *user_data)
{
	int len;

	foo_stream_ctx_t * ctx = source->ptr;

	len = ctx->page_size - ctx->page_seek;

	if (length >= len)	{
		memcpy(buf, ctx->page_buf + ctx->page_seek, len);
		*data_flags |= NGHTTP2_DATA_FLAG_EOF;
		return len;
	}
	else	{
		memcpy(buf, ctx->page_buf + ctx->page_seek, length);
		ctx->page_seek += length;
		return length;
	}

	return 0;

}

#define MAKE_NV(NAME, VALUE)	{ \
	(uint8_t *)NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, sizeof(VALUE) - 1 \
	, NGHTTP2_NV_FLAG_NONE \
}

int
foo_page(nghttp2_session * session, void * h2esq_session, void * h2esq_stream, char * path
		, foo_service_ctx_t * service_ctx
		)
{
	nghttp2_nv nva[] = { MAKE_NV(":status", "200") };
	size_t nvlen = ESQ_ARRAY_SIZE(nva);
	nghttp2_data_provider data_prd;
	foo_stream_ctx_t * ctx;
	int r;

	ctx = h2esq_stream_get_app_ctx(h2esq_stream);
	if (ctx)	{
		;
	}
	else	{
		ctx = foo_new_ctx(service_ctx);
		if (!ctx)	{
			return NGHTTP2_ERR_CALLBACK_FAILURE;
		}
		h2esq_stream_set_app_ctx(h2esq_stream, ctx);
	}

	data_prd.read_callback = foo_read_page_buffer_cb;
	data_prd.source.ptr = ctx;

	r = nghttp2_submit_response(session, h2esq_stream_get_id(h2esq_stream), nva, nvlen, &data_prd);
	if (r)	{
		h2esq_stream_clear_app_ctx(h2esq_stream);
		free(ctx);
		fprintf(stderr, "error: %s, %d, %s(): %s\n", __FILE__, __LINE__, __func__, nghttp2_strerror(r));
		return NGHTTP2_ERR_CALLBACK_FAILURE;
	}

	foo_call_test_function(path, session);

	return 0;
}

/* eof */
