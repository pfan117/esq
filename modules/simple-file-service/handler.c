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

int
sfs_handler(nghttp2_session * session, void * h2esq_session, void * h2esq_stream, void * ctx_ptr)
{
	sfs_service_ctx_t * ctx = ctx_ptr;
	char * fullpath = NULL;
	struct stat statbuf;
	int matched_length;
	int full_path_len;
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

	full_path_len = path_len - matched_length + ctx->local_path_head_len;
	fullpath = malloc(full_path_len + 1 + sizeof(SFS_DEFAULT_PAGE));
	if (fullpath)	{
		memcpy(fullpath, ctx->local_path_head, ctx->local_path_head_len);
		memcpy(fullpath + ctx->local_path_head_len, path + matched_length, path_len - matched_length);
		fullpath[full_path_len] = '\0';
		remove_redundancy_slash(fullpath + ctx->local_path_head_len);
	}
	else	{
		goto __exx;
	}

	r = lstat(fullpath, &statbuf);
	if (r)	{
		goto __exx;
	}

	if (S_ISDIR(statbuf.st_mode))	{
		if ((SFS_FLAGS_SEND_INDEX_HTML & ctx->flags))	{
			memcpy(fullpath + full_path_len, SFS_DEFAULT_PAGE, sizeof(SFS_DEFAULT_PAGE));
			r = lstat(fullpath, &statbuf);
			if (r)	{
				fullpath[full_path_len] = '\0';
				r = sfs_folder(session, h2esq_session, h2esq_stream, fullpath, ctx);
			}
			else if (S_ISREG(statbuf.st_mode))	{
				r = sfs_file(session, h2esq_session, h2esq_stream, fullpath, ctx);
			}
			else	{
				r = sfs_folder(session, h2esq_session, h2esq_stream, fullpath, ctx);
			}
		}
		else	{
			r = sfs_folder(session, h2esq_session, h2esq_stream, fullpath, ctx);
		}
	}
	else if (S_ISREG(statbuf.st_mode))	{
		r = sfs_file(session, h2esq_session, h2esq_stream, fullpath, ctx);
	}
	else	{
		goto __exx;
	}

	if (r)	{
		free(fullpath);
	}

	return r;

__exx:

	if (fullpath)	{
		free(fullpath);
	}

	return NGHTTP2_ERR_CALLBACK_FAILURE;
}

void
sfs_on_stream_close(void * h2esq_stream)	{
	sfs_stream_ctx_t * ctx;

	ctx = h2esq_stream_get_app_ctx(h2esq_stream);
	if (!ctx)	{
		return;
	}

	h2esq_stream_clear_app_ctx(h2esq_stream);
	if (-1 != ctx->fd)	{
		close(ctx->fd);
	}

	if (ctx->dir)	{
		closedir(ctx->dir);
	}

	if (ctx->path)	{
		free(ctx->path);
	}

	if (-1 != ctx->post_fd)	{
		close(ctx->post_fd);
	}

	free(ctx);

	return;
}

/* eof */
