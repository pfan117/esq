#ifndef __H2ESQ_OPENSSL_ALPN_HEADER_INCLDUED__
#define __H2ESQ_OPENSSL_ALPN_HEADER_INCLDUED__

#ifdef __H2ESQ_NEED_TO_SEE_ALPN_INTERNAL__
extern int h2esq_next_proto_cb(SSL *ssl, const unsigned char **data, unsigned int *len, void *arg);
extern int h2esq_alpn_select_proto_cb(SSL *ssl, const unsigned char **out,
								unsigned char *outlen, const unsigned char *in,
								unsigned int inlen, void *arg);
#endif

extern int h2esq_openssl_alpn_init(void);
extern void h2esq_openssl_alpn_detach(void);

#endif
