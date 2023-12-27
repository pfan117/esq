#include <nghttp2/nghttp2.h>
#include <stdio.h>
#include <string.h>
#include <monkeycall.h>

#include "public/esq.h"
#include "public/libesq.h"
#include "public/esqll.h"
#include "internal/types.h"
#include "public/h2esq.h"

STATIC void
server_stop_on_stream_close(void * h2esq_stream)	{
	return;
}

static int
server_stop_chunk_recv_cb(void * h2esq_stream_ptr, const char * buf, int len)	{
	return 0;
}

STATIC void
server_stop_detach_instance(void * service_ctx_ptr)	{
	return;
}

STATIC int
server_stop_handler(nghttp2_session * session, void * h2esq_session, void * h2esq_stream, void * ctx_ptr)	{
	esq_stop_server_on_purpose();
	return 0;
}

STATIC h2esq_service_class_t server_stop_service_class = {
	"ServerStopServiceClass",
	server_stop_detach_instance,
	server_stop_handler,
	server_stop_on_stream_close,
	server_stop_chunk_recv_cb,
};

#define SERVER_STOP_PATH_LEN_MAX		512

STATIC cc *
server_stop_bind_instance(cc * instance_name, cc * path_head)	{
	int path_head_len;
	int r;

	path_head_len = strnlen(path_head, SERVER_STOP_PATH_LEN_MAX);
	if (path_head_len <= 0 || path_head_len >= SERVER_STOP_PATH_LEN_MAX)	{
		return "invalid path head length";
	}

	r = h2esq_instance_bind_service(instance_name, path_head, &server_stop_service_class, NULL);
	if (ESQ_OK != r)	{
		return "rejected by instance binder";
	}

	return NULL;
}

/* parameters: string */
/* return value: buffer */
STATIC int
server_stop_mkccb_bind_instance(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	mkc_data * instance_name;
	mkc_data * path_head;
	int argc;
	cc * errs;

	argc = MKC_CB_ARGC;
	if (argc != 2)	{
		MKC_CB_ERROR("%s", "need 2 parameters");
		return -1;
	}

	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(1, MKC_D_BUF);

	result = MKC_CB_RESULT;
	instance_name = MKC_CB_ARGV(0);
	path_head = MKC_CB_ARGV(1);
	result->type = MKC_D_VOID;

	errs = server_stop_bind_instance(
			instance_name->buffer
			, path_head->buffer
			);
	if (errs)	{
		MKC_CB_ERROR("%s, %d: server stop service binding failed - %s", __func__, __LINE__, errs);
		return -1;
	}

	return 0;
}

/* parameters: string */
/* return value: buffer */
STATIC int
server_stop_mkccb(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	int argc;

	argc = MKC_CB_ARGC;
	if (argc != 0)	{
		MKC_CB_ERROR("%s", "need zero parameters");
		return -1;
	}

	result = MKC_CB_RESULT;
	result->type = MKC_D_VOID;

	esq_stop_server_on_purpose();

	return 0;
}

int
server_stop_service_class_init(void)	{
	return ESQ_OK;
}

void
server_stop_service_class_detach(void)	{
	return;
}

int
__esq_mod_init(void)	{
	void * server;

	ESQ_IF_ERROR_RETURN(server_stop_service_class_init());
	server = esq_get_mkc();
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(server, "ServerStopBindInstance", server_stop_mkccb_bind_instance));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(server, "StopServer", server_stop_mkccb));

	return ESQ_OK;
}

void
__esq_mod_detach(void)	{
	server_stop_service_class_detach();
	return;
}

/* eof */
