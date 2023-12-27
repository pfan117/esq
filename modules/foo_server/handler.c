#include <nghttp2/nghttp2.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>

#include <sys/types.h>	/* stat(), lstat */
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>	/* open() */

#include "public/esq.h"
#include "public/h2esq.h"
#include "public/libesq.h"
#include "public/esqll.h"
#include "modules/foo_server/internal.h"

int
foo_handler(nghttp2_session * session, void * h2esq_session, void * h2esq_stream, void * ctx_ptr)
{
	foo_service_ctx_t * ctx = ctx_ptr;
	int matched_length;
	int method_len;
	char * method;
	int path_len;
	char * path;
	int r;

	matched_length = h2esq_stream_get_path_matched_length(h2esq_stream);

	path = h2esq_stream_get_header(h2esq_stream, H2ESQ_HEADER_PATH, &path_len);

	r = escape_percent_in_place(path + matched_length, path_len - matched_length);
	if (r < 0)	{
		goto __exx;
	}

	path[matched_length + r] = '\0';
	path_len = matched_length + r;

	if (path_is_very_safe(path + matched_length))	{
		;
	}
	else	{
		goto __exx;
	}

	method = h2esq_stream_get_header(h2esq_stream, H2ESQ_HEADER_METHOD, &method_len);
	if (method)	{
		; /* printf("method = [%s]\n", method); */
	}
	else	{
		goto __exx;
	}

	r = foo_page(session, h2esq_session, h2esq_stream, path + matched_length, ctx);

	return r;

__exx:

	return NGHTTP2_ERR_CALLBACK_FAILURE;
}

void
foo_on_stream_close(void * h2esq_stream)	{
	foo_stream_ctx_t * ctx;

	ctx = h2esq_stream_get_app_ctx(h2esq_stream);
	if (!ctx)	{
		return;
	}

	h2esq_stream_clear_app_ctx(h2esq_stream);

	free(ctx);

	return;
}

int
foo_chunk_recv_cb(void * h2esq_stream_ptr, const char * buf, int len)	{
	return 0;
}

/* eof */
