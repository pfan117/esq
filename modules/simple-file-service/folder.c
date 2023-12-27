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

enum	{
	SFS_OUTPUT_HEAD,
	SFS_OUTPUT_GO_UPPER,
	SFS_OUTPUT_DIR,
	SFS_OUTPUT_DIR_CONT,
	SFS_OUTPUT_FILE,
	SFS_OUTPUT_FILE_CONT,
	SFS_OUTPUT_TAIL,
	SFS_OUTPUT_FINISHED,
};

#define SFS_FOLDER_PAGE_HEAD	"<html><head><title>%s%s</title>"\
"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">"\
"<style type=\"text/css\">"\
"body{font-family:\"Courier New\", Courier, monospace;padding:2px;}"\
"td{padding-left:5px;padding-right:5px}"\
".tdo{background-color:#f0f0ff}"\
".tde{background-color:#f9f9ff}"\
"</style>"\
"</head><body><table><tr><td>Index of: %s%s</td></tr></table><hr><table>"

#define SFS_FOLDER_PAGE_HEAD2	"<html><head><title>%s%s</title>"\
"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">"\
"<style type=\"text/css\">"\
"body{font-family:\"Courier New\", Courier, monospace;padding:2px;}"\
"td{padding-left:5px;padding-right:5px}"\
".tdo{background-color:#f0f0ff}"\
".tde{background-color:#f9f9ff}"\
"</style>"\
"</head><body>"\
"<table width=100%%><tr><td>Index of: %s%s</td>"\
"<form method=\"post\" action=\"%s%s\" enctype=\"multipart/form-data\">"\
"<td align=\"right\"><input type=\"file\" id=\"ufile\" name=\"ufile\" />"\
"<input type=\"submit\" value=\"upload\" /></td></tr></table></form>"\
"<hr><table>"

#define SFS_FOLDER_PAGE_TAIL	"</table><hr></body></html>"
#define MAKE_NV(NAME, VALUE)	{ \
	(uint8_t *)NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, sizeof(VALUE) - 1 \
	, NGHTTP2_NV_FLAG_NONE \
}

STATIC ssize_t
sfs_local_folder_output_go_upper(char * buf, int len, sfs_stream_ctx_t * ctx)	{
	char * class_str;
	int last_slash;
	int r;

	if ((ctx->path + ctx->service_ctx->local_path_head_len)[0])	{
		;
	}
	else	{
		return 0;
	}

	class_str = "class=\"tdo\"";

	last_slash = find_last_slash(ctx->path + ctx->service_ctx->local_path_head_len);
	if (last_slash <= 0)	{
		r = snprintf((char *)buf, len, "<tr><td %s><a href=\"%s\">../</a></td></tr>\n"
				, class_str
				, ctx->service_ctx->path_head
				);
	}
	else	{
		ctx->path[ctx->service_ctx->local_path_head_len + last_slash] = '\0';
		r = snprintf((char *)buf, len, "<tr><td %s><a href=\"%s%s/\">../</a></td></tr>\n"
				, class_str
				, ctx->service_ctx->path_head
				, ctx->path + ctx->service_ctx->local_path_head_len
				);
		ctx->path[ctx->service_ctx->local_path_head_len + last_slash] = '/';
	}

	if (r >= len || r <= 0)	{
		return -3;
	}
	else	{
		ctx->item_idx ++;
		return r;
	}
}

STATIC ssize_t
sfs_local_folder_output_dir(char * buf, int len, struct dirent * ent, sfs_stream_ctx_t * ctx)	{
	char * class_str;
	int r;

	if ('.' == ent->d_name[0])	{
		return -1;
	}

	if ((ctx->item_idx & 1))	{
		class_str = "class=\"tde\"";
	}
	else	{
		class_str = "class=\"tdo\"";
	}

	if ((ctx->path + ctx->service_ctx->local_path_head_len)[0])	{
		r = snprintf((char *)buf, len, "<tr><td %s><a href=\"%s%s/%s\">%s/</a></td></tr>\n"
					, class_str
					, ctx->service_ctx->path_head
					, ctx->path + ctx->service_ctx->local_path_head_len
					, ent->d_name
					, ent->d_name
					);
	}
	else	{
		r = snprintf((char *)buf, len, "<tr><td %s><a href=\"%s%s\">%s/</a></td></tr>\n"
					, class_str
					, ctx->service_ctx->path_head
					, ent->d_name
					, ent->d_name
					);
	}
	if (r <= 0 || r >= len)	{
		return -2;
	}
	else	{
		ctx->item_idx ++;
		return r;
	}
}

STATIC ssize_t
sfs_local_folder_output_file(char * buf, int len, struct dirent * ent, sfs_stream_ctx_t * ctx)	{
	char * class_str;
	int r;

	if ('.' == ent->d_name[0])	{
		return -1;
	}

	if ((ctx->item_idx & 1))	{
		class_str = "class=\"tde\"";
	}
	else	{
		class_str = "class=\"tdo\"";
	}

	if ((ctx->path + ctx->service_ctx->local_path_head_len)[0])	{
		r = snprintf((char *)buf, len, "<tr><td %s><a href=\"%s%s/%s\">%s</a></td></tr>\n"
					, class_str
					, ctx->service_ctx->path_head
					, ctx->path + ctx->service_ctx->local_path_head_len
					, ent->d_name
					, ent->d_name
					);
	}
	else	{
		r = snprintf((char *)buf, len, "<tr><td %s><a href=\"%s%s%s\">%s</a></td></tr>\n"
					, class_str
					, ctx->service_ctx->path_head
					, ctx->path + ctx->service_ctx->local_path_head_len
					, ent->d_name
					, ent->d_name
					);
	}
	if (r <= 0 || r >= len)	{
		return -2;
	}
	else	{
		ctx->item_idx ++;
		return r;
	}
}

/* return value > 0 means successfully output, please continue */
/* return value == 0 means end */
/* return value -1 means need new cycle */
/* return value -2 means need new buffer */
/* return value -3 means failure */
STATIC ssize_t
sfs_read_local_folder_output_one_item(char * buf, int length, sfs_stream_ctx_t * ctx)	{
	int r;

	if (SFS_OUTPUT_HEAD == ctx->dir_output_stat)	{
		if ((SFS_FLAGS_ALLOW_UPLOAD & ctx->service_ctx->flags))	{
			r = snprintf((char *)buf, length, SFS_FOLDER_PAGE_HEAD2
					, ctx->service_ctx->path_head, ctx->path + ctx->service_ctx->local_path_head_len
					, ctx->service_ctx->path_head, ctx->path + ctx->service_ctx->local_path_head_len
					, ctx->service_ctx->path_head, ctx->path + ctx->service_ctx->local_path_head_len
					);
		}
		else	{
			r = snprintf((char *)buf, length, SFS_FOLDER_PAGE_HEAD
					, ctx->service_ctx->path_head, ctx->path + ctx->service_ctx->local_path_head_len
					, ctx->service_ctx->path_head, ctx->path + ctx->service_ctx->local_path_head_len
					);
		}
		if (r <= 0 || r >= length)	{
			return -3;
		}
		else	{
			ctx->dir_output_stat = SFS_OUTPUT_GO_UPPER;
			return r;
		}
	}
	else if (SFS_OUTPUT_GO_UPPER == ctx->dir_output_stat)	{
		r = sfs_local_folder_output_go_upper((char *) buf, length, ctx);
		if (0 == r)	{
			ctx->dir_output_stat = SFS_OUTPUT_DIR;
			return -1;
		}
		else if (r < 0 || r >= length)	{
			return -3;
		}
		else	{
			ctx->dir_output_stat = SFS_OUTPUT_DIR;
			return r;
		}
	}
	else if (SFS_OUTPUT_DIR == ctx->dir_output_stat)	{
		ctx->ent = readdir(ctx->dir);
		if (ctx->ent)	{
			if (DT_DIR == ctx->ent->d_type)	{
				r = sfs_local_folder_output_dir((char *)buf, length, ctx->ent, ctx);
				if (-2 == r)	{
					ctx->dir_output_stat = SFS_OUTPUT_DIR_CONT;
				}
				return r;
			}
			else	{
				return -1;
			}
		}
		else	{
			rewinddir(ctx->dir);
			ctx->dir_output_stat = SFS_OUTPUT_FILE;
			return -1;
		}
	}
	else if (SFS_OUTPUT_DIR_CONT == ctx->dir_output_stat)	{
		ctx->dir_output_stat = SFS_OUTPUT_DIR;
		if (DT_DIR == ctx->ent->d_type)	{
			r = sfs_local_folder_output_dir((char *)buf, length, ctx->ent, ctx);
			if (r <= 0)	{
				return -3;
			}
			else	{
				return r;
			}
		}
		else	{
			return -1;
		}
	}
	else if (SFS_OUTPUT_FILE == ctx->dir_output_stat)	{
		ctx->ent = readdir(ctx->dir);
		if (ctx->ent)	{
			if (DT_REG == ctx->ent->d_type)	{
				r = sfs_local_folder_output_file((char *)buf, length, ctx->ent, ctx);
				if (-2 == r)	{
					ctx->dir_output_stat = SFS_OUTPUT_FILE_CONT;
				}
				return r;
			}
			else	{
				return -1;
			}
		}
		else	{
			ctx->dir_output_stat = SFS_OUTPUT_TAIL;
			return -1;
		}

		return 0;
	}
	else if (SFS_OUTPUT_FILE_CONT == ctx->dir_output_stat)	{
		ctx->dir_output_stat = SFS_OUTPUT_FILE;
		if (DT_REG == ctx->ent->d_type)	{
			r = sfs_local_folder_output_file((char *)buf, length, ctx->ent, ctx);
			if (r <= 0)	{
				return -3;
			}
			else	{
				return r;
			}
		}
		else	{
			return -1;
		}
	}
	else if (SFS_OUTPUT_TAIL == ctx->dir_output_stat)	{
		r = snprintf((char *)buf, length, SFS_FOLDER_PAGE_TAIL);
		if (r <= 0 || r >= length)	{
			return -3;
		}
		else	{
			ctx->dir_output_stat = SFS_OUTPUT_FINISHED;
			return r;
		}
	}
	else if (SFS_OUTPUT_FINISHED == ctx->dir_output_stat)	{
		return 0;
	}
	else	{
		return -3;
	}
}

STATIC ssize_t
sfs_read_local_folder_cb(nghttp2_session *session, int32_t stream_id
		, uint8_t *buf, size_t length
		, uint32_t *data_flags
		, nghttp2_data_source *source, void * user_data)
{
	sfs_stream_ctx_t * ctx = source->ptr;
	int offset;
	int r;

	if (length < SFS_PREFEREC_BUFFER_SIZE)	{
		/* make it easier */
		*data_flags |= NGHTTP2_DATA_FLAG_EOF;
		return 0;
	}

	if (length > SFS_PREFEREC_BUFFER_SIZE)	{
		length = SFS_PREFEREC_BUFFER_SIZE;
	}

	offset = 0;

	for (;;)	{
		r = sfs_read_local_folder_output_one_item((char *)(buf + offset), length - offset, ctx);
		if (r > 0)	{
			offset += r;
			continue;
		}
		else if (r == 0)	{
			*data_flags |= NGHTTP2_DATA_FLAG_EOF;
			return offset;
		}
		else if (-1 == r)	{
			continue;
		}
		else if (-2 == r)	{
			return offset;
		}
		else if (-3 == r)	{
			*data_flags |= NGHTTP2_DATA_FLAG_EOF;
			return 0;
		}
		else	{
			*data_flags |= NGHTTP2_DATA_FLAG_EOF;
			return 0;
		}
	}
}

int
sfs_folder(nghttp2_session * session, void * h2esq_session, void * h2esq_stream, char * path
		, sfs_service_ctx_t * service_ctx
		)
{
	nghttp2_nv nva[] = { MAKE_NV(":status", "200") };
	size_t nvlen = ESQ_ARRAY_SIZE(nva);
	nghttp2_data_provider data_prd;
	sfs_stream_ctx_t * ctx;
	int r;

	if ((SFS_FLAGS_ALLOW_BROWSE & service_ctx->flags))	{
		;
	}
	else	{
		return NGHTTP2_ERR_CALLBACK_FAILURE;
	}

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

	ctx->dir = opendir(path);
	if (!ctx->dir)	{
		return NGHTTP2_ERR_CALLBACK_FAILURE;
	}

	ctx->service_ctx = service_ctx;
	data_prd.read_callback = sfs_read_local_folder_cb;
	data_prd.source.ptr = ctx;

	r = nghttp2_submit_response(session, h2esq_stream_get_id(h2esq_stream), nva, nvlen, &data_prd);
	if (r)	{
		fprintf(stderr, "error: %s, %d, %s(): %s\n", __FILE__, __LINE__, __func__, nghttp2_strerror(r));
		h2esq_stream_clear_app_ctx(h2esq_stream);
		closedir(ctx->dir);
		ctx->dir = NULL;
		return NGHTTP2_ERR_CALLBACK_FAILURE;
	}

	ctx->path = path;

	return 0;
}

/* eof */
