#include <nghttp2/nghttp2.h>
#include <stdio.h>
#include <string.h>
#include <monkeycall.h>

#include "public/esq.h"
#include "public/esqll.h"
#include "internal/types.h"
#include "public/h2esq.h"
#include "modules/simple-file-service/internal.h"

STATIC void
sfs_detach_instance(void * service_ctx_ptr)	{
	sfs_service_ctx_t * ctx = service_ctx_ptr;
	free(ctx);
	return;
}

STATIC h2esq_service_class_t sfs_service_class = {
	"SimpleFileServiceClass",
	sfs_detach_instance,
	sfs_handler,
	sfs_on_stream_close,
	sfs_chunk_recv_cb,
};

#define SFS_PATH_LEN_MAX		512
#define SFS_LOCAL_PATH_LEN_MAX	512
STATIC cc *
sfs_bind_instance(cc * instance_name, cc * path_head, cc * local_path_head, int flags)	{
	sfs_service_ctx_t * ctx;
	int local_path_head_len;
	int path_head_len;
	int r;

	path_head_len = strnlen(path_head, SFS_PATH_LEN_MAX);
	if (path_head_len <= 0 || path_head_len >= SFS_PATH_LEN_MAX)	{
		return "invalid path head length";
	}

	local_path_head_len = strnlen(local_path_head, SFS_LOCAL_PATH_LEN_MAX);
	if (local_path_head_len <= 0 || local_path_head_len >= SFS_LOCAL_PATH_LEN_MAX)	{
		return "invalid local path head length";
	}

	ctx = malloc(sizeof(sfs_service_ctx_t) + path_head_len + 1 + local_path_head_len + 1);
	if (!ctx)	{
		return "out of memory";
	}

	bzero(ctx, sizeof(sfs_service_ctx_t));
	ctx->flags = flags;
	ctx->path_head_len = path_head_len;
	ctx->local_path_head_len = local_path_head_len;
	ctx->path_head = ctx->string_buf;
	ctx->local_path_head = ctx->string_buf + path_head_len + 1;
	memcpy(ctx->string_buf, path_head, path_head_len + 1);
	memcpy(ctx->string_buf + path_head_len + 1, local_path_head, local_path_head_len + 1);

	r = h2esq_instance_bind_service(instance_name, path_head, &sfs_service_class, ctx);
	if (ESQ_OK != r)	{
		free(ctx);
		return "rejected by instance binder";
	}

	return NULL;
}

/* parameters: string, string, string, string */
/* return value: buffer */
STATIC int
sfs_mkccb_bind_instance(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	mkc_data * instance_name;
	mkc_data * path_head;
	mkc_data * local_path_head;
	int argc;
	int i;
	cc * errs;
	int flags;

	argc = MKC_CB_ARGC;
	if (argc < 3)	{
		MKC_CB_ERROR("%s", "need more than 2 parameters");
		return -1;
	}

	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(1, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(2, MKC_D_BUF);

	flags = 0;

	for (i = 3; i < argc; i ++)	{

		MKC_CB_ARG_TYPE_CHECK(i, MKC_D_BUF);

		if (!strncmp(SFS_STR_ALLOW_BROWSE, MKC_CB_ARGV(i)->buffer, sizeof(SFS_STR_ALLOW_BROWSE)))	{
			flags |= SFS_FLAGS_ALLOW_BROWSE;
		}
		else if (!strncmp(SFS_STR_ALLOW_UPLOAD, MKC_CB_ARGV(i)->buffer, sizeof(SFS_STR_ALLOW_UPLOAD)))	{
			flags |= SFS_FLAGS_ALLOW_UPLOAD;
		}
		else if (!strncmp(SFS_STR_ALLOW_REMOVE, MKC_CB_ARGV(i)->buffer, sizeof(SFS_STR_ALLOW_REMOVE)))	{
			flags |= SFS_FLAGS_ALLOW_REMOVE;
		}
		else if (!strncmp(SFS_STR_SEND_INDEX_HTML, MKC_CB_ARGV(i)->buffer, sizeof(SFS_STR_SEND_INDEX_HTML)))	{
			flags |= SFS_FLAGS_SEND_INDEX_HTML;
		}
		else	{
			MKC_CB_ERROR("%s - %s", "unrecognizable parameter", (char *)(MKC_CB_ARGV(i)->buffer));
			return -1;
		}
	}

	result = MKC_CB_RESULT;
	instance_name = MKC_CB_ARGV(0);
	path_head = MKC_CB_ARGV(1);
	local_path_head = MKC_CB_ARGV(2);
	result->type = MKC_D_VOID;

	errs = sfs_bind_instance(
			instance_name->buffer
			, path_head->buffer
			, local_path_head->buffer
			, flags
			);
	if (errs)	{
		MKC_CB_ERROR("%s, %d: simple file service binding failed - %s", __func__, __LINE__, errs);
		return -1;
	}

	return 0;
}

int
sfs_service_class_init(void)	{
#if 0
	int r;

	r = h2esq_register_service_class(&sfs_service_class);
	if (r)	{
		fprintf(stderr, "error: %s, %d: failed to register servic class\n", __func__, __LINE__);
		return r;
	}
#endif
	return ESQ_OK;
}

void
sfs_service_class_detach(void)	{
	return;
}

int
__esq_mod_init(void)	{
	void * server;

	ESQ_IF_ERROR_RETURN(sfs_service_class_init());
	server = esq_get_mkc();
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(server, "SimpleFileServiceBindInstance", sfs_mkccb_bind_instance));

	return ESQ_OK;
}

void
__esq_mod_detach(void)	{
	sfs_service_class_detach();
	return;
}

/* eof */
