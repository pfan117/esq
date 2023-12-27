#include <string.h>
#include <strings.h>
#include <nghttp2/nghttp2.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#if OPENSSL_VERSION_NUMBER < 0x10002000L
	#error "please kindly update your openssl library"
#endif

#include "public/esq.h"
#include "public/h2esq.h"
#include "public/esqll.h"
#define __H2ESQ_NEED_TO_SEE_ALPN_INTERNAL__
#include "nghttp2_wrapper/includes/alpn.h"

static unsigned char h2esq_next_proto_list[256];
static size_t h2esq_next_proto_list_len;

int
h2esq_next_proto_cb(SSL *ssl, const unsigned char **data, unsigned int *len, void *arg)	{
	(void)ssl;
	(void)arg;
 	*data = h2esq_next_proto_list;
 	*len = (unsigned int)h2esq_next_proto_list_len;
 	return SSL_TLSEXT_ERR_OK;
}

int
h2esq_alpn_select_proto_cb(SSL *ssl, const unsigned char **out
		, unsigned char *outlen, const unsigned char *in
		, unsigned int inlen, void *arg
		)
{
	int rv;
	(void)ssl;
	(void)arg;

	rv = nghttp2_select_next_protocol((unsigned char **)out, outlen, in, inlen);

	if (rv != 1)	{
		return SSL_TLSEXT_ERR_NOACK;
	}

	return SSL_TLSEXT_ERR_OK;
}

int
h2esq_openssl_alpn_init(void)	{
	h2esq_next_proto_list[0] = NGHTTP2_PROTO_VERSION_ID_LEN;
	memcpy(&h2esq_next_proto_list[1], NGHTTP2_PROTO_VERSION_ID, NGHTTP2_PROTO_VERSION_ID_LEN);
	h2esq_next_proto_list_len = 1 + NGHTTP2_PROTO_VERSION_ID_LEN;
	return 0;
}

void
h2esq_openssl_alpn_detach(void)	{
	return;
}

/* eof */
