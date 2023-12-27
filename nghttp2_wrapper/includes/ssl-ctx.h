#ifndef __H2ESQ_OPENSSL_CTX_HEADER_INCLDUED__
#define __H2ESQ_OPENSSL_CTX_HEADER_INCLDUED__

extern int h2esq_ssl_ctx_init(void);
extern void h2esq_ssl_ctx_detach(void);
extern void * h2esq_ssl_get_ctx_by_name(const char * name);

#endif
