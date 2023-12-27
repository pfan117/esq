#include <stdio.h>
#include <nghttp2/nghttp2.h>

#include "public/esq.h"
#include "public/h2esq.h"
#include "public/esqll.h"
#include "nghttp2_wrapper/includes/alpn.h"
#include "nghttp2_wrapper/includes/ssl-ctx.h"
#include "nghttp2_wrapper/includes/callbacks.h"
#include "nghttp2_wrapper/includes/instance.h"
#include "nghttp2_wrapper/includes/session.h"
#include "nghttp2_wrapper/includes/stream.h"
#include "nghttp2_wrapper/nghttp2_wrapper.h"

#define IF_ERROR_RETURN(__OP__)	do	{	\
	r = __OP__();	\
	if (r)	{	\
		return r;	\
	}	\
} while(0);

int
h2esq_init(void)	{
	int r;
	IF_ERROR_RETURN(h2esq_openssl_alpn_init);
	IF_ERROR_RETURN(h2esq_ngh2_callbacks_init);
	IF_ERROR_RETURN(h2esq_ssl_ctx_init);
	IF_ERROR_RETURN(h2esq_instance_init);
	IF_ERROR_RETURN(h2esq_session_init);
	return ESQ_OK;
}

void
h2esq_detach(void)	{
	h2esq_session_detach();
	h2esq_instance_detach();
	h2esq_ssl_ctx_detach();
	h2esq_ngh2_callbacks_detach();
	h2esq_openssl_alpn_detach();
	return;
}

/* eof */
