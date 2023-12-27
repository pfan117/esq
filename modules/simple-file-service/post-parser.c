#include <nghttp2/nghttp2.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#include <sys/types.h>	/* stat(), lstat */
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>	/* open() */

#include "public/esq.h"
#include "public/esqll.h"
#include "public/h2esq.h"
#include "modules/simple-file-service/internal.h"
#include "public/libesq.h"

STATIC int
sfs_open_post_file(void * h2esq_stream_ptr, sfs_stream_ctx_t * sfs_stream_ctx, const char * buf, int len)	{
	int path_len;
	const char * path;
	int matched_length;
	char * full_path = NULL;
	int full_path_len;
	sfs_service_ctx_t * service_ctx;
	const char * basename;
	int basename_len;
	const char * local_path;
	int local_path_len;
	int fd;
	int r;

	if (!len)	{
		return 0;
	}

	r = find_last_slash2(buf, len);
	if (-1 == r)	{
		basename = buf;
		basename_len = len;
	}
	else	{
		basename = buf + r + 1;
		basename_len = len - r - 1;
	}

	service_ctx = h2esq_stream_get_service_app_ctx(h2esq_stream_ptr);
	matched_length = h2esq_stream_get_path_matched_length(h2esq_stream_ptr);
	path = h2esq_stream_get_header(h2esq_stream_ptr, H2ESQ_HEADER_PATH, &path_len);

	local_path = path + matched_length;
	local_path_len = path_len - matched_length;

	full_path_len = service_ctx->local_path_head_len + local_path_len + len + 1;	/* add a / */
	full_path = malloc(full_path_len + 1);
	if (full_path)	{
		memcpy(full_path, service_ctx->local_path_head, service_ctx->local_path_head_len);
		memcpy(full_path + service_ctx->local_path_head_len, local_path, local_path_len);

		full_path[service_ctx->local_path_head_len + local_path_len] = '/';
		memcpy(full_path + service_ctx->local_path_head_len + local_path_len + 1, basename, basename_len);
		full_path[full_path_len] = '\0';
	}
	else	{
		goto __exx;
	}

	/* hex_dump(full_path + service_ctx->local_path_head_len + local_path_len + 1, 100); */

	if (path_is_very_safe(full_path + service_ctx->local_path_head_len + local_path_len + 1))	{
		;
	}
	else	{
		goto __exx;
	}

	remove_redundancy_slash(full_path);

	fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH));
	if (-1 == fd)	{
		/* printf("error: failed to create [%s]: %d, %s\n", full_path, errno, strerror(errno)); */
		goto __exx;
	}

	sfs_stream_ctx->post_fd = fd;

	free(full_path);

	return 0;

__exx:

	if (full_path)	{
		free(full_path);
	}

	return -1;
}

STATIC int
sfs_write_post_file(sfs_stream_ctx_t * sfs_stream_ctx, const char * buf, int len)	{
	int wr;
	int fd;
	int r;

	fd = sfs_stream_ctx->post_fd;
	if (fd <= 0)	{
		return -1;
	}

	wr = 0;

	while(wr < len)	{
		r = write(fd, buf + wr, len - wr);
		if (r <= 0)	{
			return -1;
		}
		else	{
			wr += r;
		}
	}

	sfs_stream_ctx->post_len += len;

	return 0;
}

#define STATE_INIT		0
#define STATE_OPEN		1
#define STATE_RECV		2
#define STATE_FINISHED	3
#define STATE_ERROR		4

STATIC int
sfs_post_data_parser_cb(void * app_ctx, int element_type, const char * buf, int len)	{
	sfs_stream_ctx_t * sfs_stream_ctx;
	void * h2esq_stream_ptr = app_ctx;
	sfs_service_ctx_t * service_ctx;
	int r;

	service_ctx = h2esq_stream_get_service_app_ctx(h2esq_stream_ptr);
	if ((SFS_FLAGS_ALLOW_UPLOAD & service_ctx->flags))	{
		;
	}
	else	{
		return 0;
	}

	sfs_stream_ctx = h2esq_stream_get_app_ctx(h2esq_stream_ptr);
	if (!sfs_stream_ctx)	{
		return -1;
	}

	if (STATE_ERROR == sfs_stream_ctx->post_state)	{
		return -1;
	}
	else if (STATE_FINISHED == sfs_stream_ctx->post_state)	{
		return -1;
	}

	if (POST_ET_FILENAME == element_type)	{
		if (STATE_INIT == sfs_stream_ctx->post_state)	{
			r = sfs_open_post_file(h2esq_stream_ptr, sfs_stream_ctx, buf, len);
			if (r)	{
				return -1;
			}
			else	{
				sfs_stream_ctx->post_state = STATE_OPEN;
			}
		}
		else	{
			goto __exx;
		}
	}
	if (POST_ET_VALUE == element_type)	{
		if (STATE_OPEN == sfs_stream_ctx->post_state)	{
			r = sfs_write_post_file(sfs_stream_ctx, buf, len);
			if (r)	{
				goto __exx;
			}
			else	{
				sfs_stream_ctx->post_state = STATE_RECV;
				return 0;
			}
		}
		else if (STATE_RECV == sfs_stream_ctx->post_state)	{
			r = sfs_write_post_file(sfs_stream_ctx, buf, len);
			if (r)	{
				goto __exx;
			}
			else	{
				sfs_stream_ctx->post_state = STATE_RECV;
				return 0;
			}
		}
		else	{
			return 0;
		}
	}
	else if (POST_ET_BOUNDARY == element_type)	{
		if (STATE_RECV == sfs_stream_ctx->post_state)	{
			close(sfs_stream_ctx->post_fd);
			sfs_stream_ctx->post_fd = -1;
			sfs_stream_ctx->post_state = STATE_FINISHED;
			return 0;
		}
		else	{
			return 0;
		}
	}
	else	{
		if (STATE_RECV == sfs_stream_ctx->post_state)	{
			goto __exx;
		}
		else	{
			return 0;
		}
	}

	return 0;

__exx:

	sfs_stream_ctx->post_state = STATE_ERROR;
	close(sfs_stream_ctx->post_fd);
	sfs_stream_ctx->post_fd = -1;

	return -1;
}

int
sfs_chunk_recv_cb(void * h2esq_stream_ptr, const char * buf, int len)	{
	sfs_stream_ctx_t * sfs_stream_ctx;
	int r;

	sfs_stream_ctx = h2esq_stream_get_app_ctx(h2esq_stream_ptr);
	if (sfs_stream_ctx)	{
		;
	}
	else	{
		sfs_stream_ctx = sfs_new_ctx();
		if (!sfs_stream_ctx)	{
			return -1;
		}
		r = h2esq_stream_set_app_ctx(h2esq_stream_ptr, sfs_stream_ctx);
		if (r)	{
			free(sfs_stream_ctx);
			return -1;
		}
	}

	r = post_data_parser(buf, len, sfs_post_data_parser_cb
			, &(sfs_stream_ctx->post_parser_ctx)
			, h2esq_stream_ptr
			);

	return r;
}

/* eof */
