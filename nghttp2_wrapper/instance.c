#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <nghttp2/nghttp2.h>
#include <netinet/in.h>
#include <monkeycall.h>
#include <errno.h>

#include "public/esq.h"
#include "public/h2esq.h"
#include "public/libesq.h"
#include "public/esqll.h"
#include "internal/types.h"
#include "nghttp2_wrapper/includes/ssl-ctx.h"
#include "nghttp2_wrapper/includes/service-class.h"
#include "nghttp2_wrapper/includes/instance.h"
#include "nghttp2_wrapper/includes/session.h"

#define H2ESQ_SERVER_INSTANCE_MAX	8
static h2esq_instance_t h2esq_instance_list[H2ESQ_SERVER_INSTANCE_MAX];

STATIC h2esq_instance_t *
h2esq_find_instance(cc * name)	{
	int i;

	for(i = 0; i < H2ESQ_SERVER_INSTANCE_MAX; i ++)	{
		if (strncmp(name, h2esq_instance_list[i].name, H2ESQ_SERVER_INSTANCE_NAME_MAX))	{
			continue;
		}
		else	{
			return h2esq_instance_list + i;
		}
	}

	return NULL;
}

int
h2esq_instance_bind_service(
		const char * instance_name
		, const char * path
		, h2esq_service_class_t * service_class
		, void * service_ctx)
{
	h2esq_instance_t * h2esq_instance;
	int i;
	int r;

	h2esq_instance = h2esq_find_instance(instance_name);
	if (!h2esq_instance)	{
		esq_log("ERROR: %s %d: can't find instance by name [%s]\n"
				, __func__, __LINE__, instance_name);
		return ESQ_ERROR_LIBCALL;
	}

	for (i = 0; i < ESQ_ARRAY_SIZE(h2esq_instance->service); i ++)	{
		if (h2esq_instance->service[i].service_class)	{
			continue;
		}
		else	{
			goto __continue_bind;
		}
	}

	esq_log("ERROR: %s %d: too many service for one instance\n", __func__, __LINE__);

	return ESQ_ERROR_LIBCALL;

__continue_bind:

	r = hmt_add2(h2esq_instance->path_hmt, path, h2esq_instance->service + i);
	if (!r)	{
		;
	}
	else if (-1 == r)	{
		esq_log("ERROR: %s %d: to many pathes for one server instance[%s]\n"
				, __func__, __LINE__, path);
		return ESQ_ERROR_LIBCALL;
	}
	else if (-2 == r)	{
		esq_log("ERROR: %s %d: path too long[%s]\n", __func__, __LINE__, path);
		return ESQ_ERROR_LIBCALL;
	}
	else if (-3 == r)	{
		esq_log("ERROR: %s %d: path already exist [%s]\n", __func__, __LINE__, path);
		return ESQ_ERROR_LIBCALL;
	}
	else if (-4 == r)	{
		esq_log("ERROR: %s %d: out of memory\n", __func__, __LINE__);
		return ESQ_ERROR_LIBCALL;
	}
	else	{
		esq_log("ERROR: %s %d: hmt_add2() failed\n", __func__, __LINE__);
		return ESQ_ERROR_LIBCALL;
	}

	r = hmt_compile(h2esq_instance->path_hmt);
	if (r)	{
		esq_log("ERROR: %s %d: hmt_compile() failed (out of memory)\n"
				, __func__, __LINE__);
		return ESQ_ERROR_LIBCALL;
	}

	h2esq_instance->service[i].service_class = service_class;
	h2esq_instance->service[i].ctx = service_ctx;

	return ESQ_OK;
}

h2esq_service_t *
h2esq_instance_find_service(
			h2esq_instance_t * instance, const char * path, int * matched_length)
{
	return hmt_match(instance->path_hmt, path, matched_length);
}

void *
h2esq_instance_get_ssl_ctx(void * instance)	{
	h2esq_instance_t * i = instance;
	return i->ssl_ctx;
}

STATIC void
h2esq_instance_close_sd(void * instance)	{
	h2esq_instance_t * i = instance;
	void * esq_session;
	int sd;

	sd = i->sd;
	esq_session = i->esq_session;

	if (-1 == sd)	{
		;
	}
	else	{
		i->sd = -1;
		close(sd);
	}

	if (esq_session)	{
		esq_session_remove_req(esq_session);
	}

	return;
}

STATIC void 
h2esq_server_sd_cb(void * current_session)
{
	int event_bits;

	event_bits = esq_session_get_event_bits(current_session);

	if (!event_bits)	{
		esq_design_error();
 		return;
	}

	if ((event_bits & ESQ_EVENT_BIT_EXIT))	{
		h2esq_instance_close_sd(esq_session_get_param(current_session));
		return;
	}

	if ((event_bits & ESQ_EVENT_BIT_READ))	{
		h2esq_create_new_session(
				esq_session_get_fd(current_session),
				esq_session_get_param(current_session)
				);
	}

	if ((event_bits & ESQ_EVENT_BIT_WRITE))	{
		;
	}

	if ((event_bits & ESQ_EVENT_BIT_LOCK_OBTAIN))	{
		esq_design_error();
	}

	if ((event_bits & ESQ_EVENT_BIT_FD_CLOSE))	{
		esq_design_error();
	}

	if ((event_bits & ESQ_EVENT_BIT_ERROR))	{
		esq_design_error();
	}

	if ((event_bits & ESQ_EVENT_BIT_TIMEOUT))	{
		esq_design_error();
	}

	if ((event_bits & ESQ_EVENT_BIT_LOCK_OBTAIN))	{
		esq_design_error();
	}

	return;
}

STATIC cc *
h2esq_prepare_server_socket(h2esq_instance_t * instance)	{
	struct sockaddr_in addr;
	int optval = 1;
	int sd;
	int r;

	sd = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == sd)	{
		return "failed to open new socket";
	}

	r = os_socket_set_non_blocking(sd);
	if(r)	{
		close(sd);
		return "failed to set socket to non-blocking";
	}

	r = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (r)	{
		close(sd);
		return "failed to set socket option";
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(instance->port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(&(addr.sin_zero), 0, sizeof(addr.sin_zero));

	r = bind(sd, (struct sockaddr *)(&addr), sizeof(addr));
	if (r)	{
		close(sd);
		esq_log("ERROR: %s %d: errno = %d (%s)\n"
				, __func__, __LINE__, errno, strerror(errno));
		return "failed to bind socket";
	}

	r = listen(sd, 100);
	if (r)	{
		close(sd);
		return "failed to listen";
	}

	instance->sd = sd;

	r = esq_session_set_socket_fd_multi(
			instance->esq_session, sd, h2esq_server_sd_cb, instance
			);
	if (r)	{
		close(sd);
		return "failed to wind up socket event";
	}

	return NULL;
}

STATIC cc *
h2esq_start_instance(cc * new_server_name, cc * ssl_ctx_name, int port)	{
	void * esq_session;
	void * ssl_ctx;
	void * path_hmt;
	void * var_hmt;
	cc * errs;
	int l;
	int b;
	int i;

	if (port <= 0)	{
		return "invalid port number";
	}

	b = sizeof(((h2esq_instance_t *)NULL)->name);
	l = strnlen(new_server_name, b);
	if (l >= b || l <= 0)	{
		return "new server name too long";
	}

	for (i = 0; i < ESQ_ARRAY_SIZE(h2esq_instance_list); i ++)	{
		if (h2esq_instance_list[i].name[0])	{
			continue;
		}
		break;
	}

	if (i >= ESQ_ARRAY_SIZE(h2esq_instance_list))	{
		return "too many instances";
	}

	ssl_ctx = h2esq_ssl_get_ctx_by_name(ssl_ctx_name);
	if (!ssl_ctx)	{
		return "ssl ctx not found";
	}

	esq_session = esq_session_new("h2-server");
	if (!esq_session)	{
		return "failed to create new esq session";
	}

	path_hmt = hmt_new2(H2ESQ_PATH_HANDLER_MAX, HMT_SCALE_32);
	if (!path_hmt)	{
		esq_session_wind_up(esq_session);
		return "out of memory";
	}

	var_hmt = hmt_new2(H2ESQ_INSTANCE_VAR_MAX, HMT_SCALE_64);
	if (!var_hmt)	{
		hmt_free(path_hmt);
		esq_session_wind_up(esq_session);
		return "out of memory";
	}

	strcpy(h2esq_instance_list[i].name, new_server_name);
	h2esq_instance_list[i].esq_session = esq_session;
	h2esq_instance_list[i].ssl_ctx = ssl_ctx;
	h2esq_instance_list[i].port = port;
	h2esq_instance_list[i].path_hmt = path_hmt;
	h2esq_instance_list[i].var_hmt = var_hmt;

	errs = h2esq_prepare_server_socket(h2esq_instance_list + i);
	if (errs)	{
		h2esq_instance_list[i].name[0] = '\0';
		hmt_free(path_hmt);
		hmt_free(var_hmt);
		esq_session_wind_up(esq_session);
		return errs;
	}
	else	{
		esq_session_wind_up_rr(esq_session);
	}

	return NULL;
}

/* parameters: string, string, integer */
/* return value: buffer */
STATIC int
esq_mkccb_h2_start_instance(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	mkc_data * server_name;
	mkc_data * ssl_ctx_name;
	mkc_data * port_number;
	cc * errs;

	MKC_CB_ARGC_CHECK(3);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(1, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(2, MKC_D_INT);
	result = MKC_CB_RESULT;
	server_name = MKC_CB_ARGV(0);
	ssl_ctx_name = MKC_CB_ARGV(1);
	port_number = MKC_CB_ARGV(2);
	result->type = MKC_D_VOID;

	errs = h2esq_start_instance(
			server_name->buffer
			, ssl_ctx_name->buffer
			, port_number->integer
			);
	if (errs)	{
		MKC_CB_ERROR("%s, %d: failed to start server: %s", __func__, __LINE__, errs);
		return -1;
	}

	return 0;
}

int
h2esq_instance_init(void)	{
	void * server;
	int i;

	ESQ_ARRAY_BZERO(h2esq_instance_list);
	for (i = 0; i < ESQ_ARRAY_SIZE(h2esq_instance_list); i ++)	{
		h2esq_instance_list[i].sd = -1;
	}

	server = esq_get_mkc();

	#define StartH2Instance
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(server, "StartH2Instance", esq_mkccb_h2_start_instance));

	return 0;
}

void
h2esq_instance_detach(void)	{
	h2esq_service_class_t * class_ptr;
	int i;
	int c;

	for (i = 0; i < ESQ_ARRAY_SIZE(h2esq_instance_list); i ++)	{
		for (c = 0; c < ESQ_ARRAY_SIZE(h2esq_instance_list[i].service); c ++)	{
			class_ptr = h2esq_instance_list[i].service[c].service_class;
			if (class_ptr)	{
				class_ptr->detach(h2esq_instance_list[i].service[c].ctx);
			}
		}
	}

	return;
}

/* eof */
