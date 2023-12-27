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
#include "modules/simple-file-service/internal.h"

STATIC ssize_t
sfs_read_local_file_cb(nghttp2_session *session, int32_t stream_id
		, uint8_t *buf, size_t length
		, uint32_t *data_flags
		, nghttp2_data_source *source, void *user_data)
{
	int r;

	r = read(source->fd, buf, length);
	if (0 == r)	{
		*data_flags |= NGHTTP2_DATA_FLAG_EOF;
		return r;
	}
	else if (r < 0)	{
		return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
	}
	else	{
		return r;
	}
}

#define MAKE_NV(NAME, VALUE)	{ \
	(uint8_t *)NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, sizeof(VALUE) - 1 \
	, NGHTTP2_NV_FLAG_NONE \
}

int
sfs_file(nghttp2_session * session, void * h2esq_session, void * h2esq_stream, char * path
		, sfs_service_ctx_t * service_ctx
		)
{
	nghttp2_nv nva[] = { MAKE_NV(":status", "200") };
	size_t nvlen = ESQ_ARRAY_SIZE(nva);
	nghttp2_data_provider data_prd;
	sfs_stream_ctx_t * ctx = NULL;
	int fd = -1;
	int r;

	ctx = h2esq_stream_get_app_ctx(h2esq_stream);
	if (ctx)	{
		;
	}
	else	{
		ctx = sfs_new_ctx();
		if (!ctx)	{
			return NGHTTP2_ERR_CALLBACK_FAILURE;
		}
		h2esq_stream_set_app_ctx(h2esq_stream, ctx);
	}

	if (-1 == ctx->fd)	{
		fd = open(path, O_RDONLY);
	}

	if (-1 == fd)	{
		goto __exx;
	}

	ctx->fd = fd;
	ctx->path = path;
	ctx->service_ctx = service_ctx;

	data_prd.read_callback = sfs_read_local_file_cb;
	data_prd.source.fd = ctx->fd;

	r = nghttp2_submit_response(session, h2esq_stream_get_id(h2esq_stream), nva, nvlen, &data_prd);
	if (r)	{
		fprintf(stderr, "error: %s, %d, %s(): %s\n", __FILE__, __LINE__, __func__, nghttp2_strerror(r));
		goto __exx;
	}

	return 0;

__exx:

	h2esq_stream_clear_app_ctx(h2esq_stream);
	if (-1 != fd)	{
		close(fd);
	}

	if (ctx)	{
		free(ctx);
	}

	return NGHTTP2_ERR_CALLBACK_FAILURE;
}

/* eof */
