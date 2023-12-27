#include <nghttp2/nghttp2.h>
#include <stdio.h>
#include <string.h>
#include <monkeycall.h>

#include "public/esq.h"
#include "public/libesq.h"
#include "public/esqll.h"
#include "internal/types.h"
#include "public/h2esq.h"
#include "modules/foo_server/internal.h"

STATIC void
foo_detach_instance(void * service_ctx_ptr)	{
	foo_service_ctx_t * ctx = service_ctx_ptr;
	free(ctx);
	return;
}

STATIC h2esq_service_class_t foo_service_class = {
	"FooServerServiceClass",
	foo_detach_instance,
	foo_handler,
	foo_on_stream_close,
	foo_chunk_recv_cb,
};

#define FOO_PATH_LEN_MAX		512
#define FOO_LOCAL_PATH_LEN_MAX	512
STATIC cc *
foo_bind_instance(cc * instance_name, cc * path_head)	{
	foo_service_ctx_t * ctx;
	int path_head_len;
	int r;

	path_head_len = strnlen(path_head, FOO_PATH_LEN_MAX);
	if (path_head_len <= 0 || path_head_len >= FOO_PATH_LEN_MAX)	{
		return "invalid path head length";
	}

	ctx = malloc(sizeof(foo_service_ctx_t) + path_head_len + 1);
	if (!ctx)	{
		return "out of memory";
	}

	bzero(ctx, sizeof(foo_service_ctx_t));
	ctx->path_head_len = path_head_len;
	ctx->path_head = ctx->string_buf;
	memcpy(ctx->string_buf, path_head, path_head_len + 1);

	r = h2esq_instance_bind_service(instance_name, path_head, &foo_service_class, ctx);
	if (ESQ_OK != r)	{
		free(ctx);
		return "rejected by instance binder";
	}

	return NULL;
}

/* parameters: string, string, string, string */
/* return value: buffer */
STATIC int
foo_mkccb_bind_instance(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	mkc_data * instance_name;
	mkc_data * path_head;
	int argc;
	cc * errs;

	argc = MKC_CB_ARGC;
	if (argc != 2)	{
		MKC_CB_ERROR("%s", "only accept 2 parameter");
		return -1;
	}

	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(1, MKC_D_BUF);

	result = MKC_CB_RESULT;
	instance_name = MKC_CB_ARGV(0);
	path_head = MKC_CB_ARGV(1);
	result->type = MKC_D_VOID;

	errs = foo_bind_instance(
			instance_name->buffer
			, path_head->buffer
			);
	if (errs)	{
		MKC_CB_ERROR("%s, %d: foo service binding failed - %s", __func__, __LINE__, errs);
		return -1;
	}

	return 0;
}

int
__esq_mod_init(void)	{
	void * server;

	server = esq_get_mkc();
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(server, "FooServiceBindInstance", foo_mkccb_bind_instance));

	return ESQ_OK;
}

void
__esq_mod_detach(void)	{
	return;
}

/* eof */
