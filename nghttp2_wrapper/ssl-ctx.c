#include <string.h>
#include <nghttp2/nghttp2.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <monkeycall.h>

#if OPENSSL_VERSION_NUMBER < 0x10002000L
	#error "to be nice, please update your openssl library"
#endif

#include "public/esq.h"
#include "public/libesq.h"
#include "public/h2esq.h"
#include "public/esqll.h"
#define __H2ESQ_NEED_TO_SEE_ALPN_INTERNAL__
#include "nghttp2_wrapper/includes/alpn.h"
#include "nghttp2_wrapper/includes/ssl-ctx.h"

#define MAX_SSL_CTX   8
#define MAX_SSL_CTX_NAME  32

typedef struct _h2esq_ecc_ssl_ctx_t	{
	char name[MAX_SSL_CTX_NAME];
	void * ctx;
} h2esq_ecc_ssl_ctx_t;

static h2esq_ecc_ssl_ctx_t h2esq_ssl_ecc_ctx_list[MAX_SSL_CTX];

void *
h2esq_ssl_get_ctx_by_name(const char * name)	{
	int i;

	for (i = 0; i < ESQ_ARRAY_SIZE(h2esq_ssl_ecc_ctx_list); i ++)	{
		if (!h2esq_ssl_ecc_ctx_list[i].ctx)	{
			continue;
		}

		if (strncmp(h2esq_ssl_ecc_ctx_list[i].name, name, sizeof(h2esq_ssl_ecc_ctx_list[i].name)))	{
			continue;
		}

		return h2esq_ssl_ecc_ctx_list[i].ctx;
	}

	return NULL;
}

STATIC int
h2esq_ssl_record_ctx(const char * name, void * ctx)	{
	int i;

	for (i = 0; i < ESQ_ARRAY_SIZE(h2esq_ssl_ecc_ctx_list); i ++)	{
		if (h2esq_ssl_ecc_ctx_list[i].ctx)	{
			continue;
		}

		h2esq_ssl_ecc_ctx_list[i].ctx = ctx;
		strcpy(h2esq_ssl_ecc_ctx_list[i].name, name);
		return ESQ_OK;
	}

	return ESQ_ERROR_SIZE;
}

STATIC void
h2esq_ssl_erase_ctx(void * ctx)	{
	int i;

	for (i = 0; i < MAX_SSL_CTX; i ++)	{
		if (h2esq_ssl_ecc_ctx_list[i].ctx == ctx)	{
			h2esq_ssl_ecc_ctx_list[i].ctx = NULL;
			return;
		}
	}

	esq_design_error();
}

STATIC const char *
esq_create_ssl_ecc_ctx(const char * name, const char * key_file, const char * cert_file)	{
	SSL_CTX * ssl_ctx;
	EC_KEY * ecdh;
	int namelen;
	int r;

	namelen = strnlen(name, MAX_SSL_CTX_NAME * 2);
	if (namelen <= 0 || namelen >= MAX_SSL_CTX_NAME)	{
		printf("ERROR: %s, %d: invalid name, length should be 1 - %d\n", __func__, __LINE__
				, MAX_SSL_CTX_NAME
				);
		return "ctx name invalid";
	}

	ssl_ctx = SSL_CTX_new(SSLv23_server_method());
	if (!ssl_ctx)	{
		printf("ERROR: %s, %d: failed to create SSL ctx\n", __func__, __LINE__);
		return "SSL_CTX_new() return error";
	}

	r = h2esq_ssl_record_ctx(name, ssl_ctx);
	if (r)	{
		SSL_CTX_free(ssl_ctx);
		printf("ERROR: %s, %d: too many ssl ctx\n", __func__, __LINE__);
		return "too many ssl ctx";
	}

	SSL_CTX_set_options(ssl_ctx
			, SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION
			| SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);

	ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
	if (!ecdh) {
		printf("ERROR: %s, %d: EC_KEY_new_by_curv_name failed: %s", __func__, __LINE__
				,ERR_error_string(ERR_get_error(), NULL));
		h2esq_ssl_erase_ctx(ssl_ctx);
		SSL_CTX_free(ssl_ctx);
		return "SSL_CTX_set_options() return error";
 	}
	SSL_CTX_set_tmp_ecdh(ssl_ctx, ecdh);
	EC_KEY_free(ecdh);

	if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file, SSL_FILETYPE_PEM) != 1)	{
		printf("ERROR: %s, %d: Could not read private key file %s", __func__, __LINE__, key_file);
		h2esq_ssl_erase_ctx(ssl_ctx);
		SSL_CTX_free(ssl_ctx);
		return "SSL_CTX_use_PrivateKey_file() return error";
	}

	if (SSL_CTX_use_certificate_chain_file(ssl_ctx, cert_file) != 1)	{
		printf("ERROR: %s, %d: Could not read certificate file %s", __func__, __LINE__, cert_file);
		h2esq_ssl_erase_ctx(ssl_ctx);
		SSL_CTX_free(ssl_ctx);
		return "SSL_CTX_use_certificate_chain_file() return error";
 	}

	if (!SSL_CTX_check_private_key(ssl_ctx))	{
		printf("ERROR: %s, %d: Files don't match!", __func__, __LINE__);
		SSL_CTX_free(ssl_ctx);
		return "SSL_CTX_check_private_key() return error";
	}

	SSL_CTX_set_next_protos_advertised_cb(ssl_ctx, h2esq_next_proto_cb, NULL);

	SSL_CTX_set_alpn_select_cb(ssl_ctx, h2esq_alpn_select_proto_cb, NULL);

	return NULL;
}

/* parameters: string, string, string */
/* return value: buffer */
int
esq_mkccb_create_ssl_ecc_ctx(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	mkc_data * ctx_name;
	mkc_data * key_file;
	mkc_data * cert_file;
	const char * errs;

	MKC_CB_ARGC_CHECK(3);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(1, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(2, MKC_D_BUF);
	result = MKC_CB_RESULT;
	ctx_name = MKC_CB_ARGV(0);
	key_file = MKC_CB_ARGV(1);
	cert_file = MKC_CB_ARGV(2);
	result->type = MKC_D_VOID;

	errs = esq_create_ssl_ecc_ctx(ctx_name->buffer, key_file->buffer, cert_file->buffer);
	if (errs)	{
		MKC_CB_ERROR("%s, %d: failed to create new ctx, %s", __func__, __LINE__, errs);
		return -1;
	}

	return 0;
}

int
h2esq_ssl_ctx_init(void)	{
	void * instance;

	SSL_library_init();
	bzero(h2esq_ssl_ecc_ctx_list, sizeof(h2esq_ssl_ecc_ctx_list));

	instance = esq_get_mkc();
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(instance, "CreateSslEccCtx", esq_mkccb_create_ssl_ecc_ctx));

	return 0;
}

void
h2esq_ssl_ctx_detach(void)	{
	int i;

	for (i = 0; i < MAX_SSL_CTX; i ++)	{
		if (h2esq_ssl_ecc_ctx_list[i].ctx)	{
			SSL_CTX_free(h2esq_ssl_ecc_ctx_list[i].ctx);
		}
	}

	return;
}

/* eof */
