#ifndef __H2ESQ_MOD_FOO_SERVICE_INTERNAL_INCLUDED__
#define __H2ESQ_MOD_FOO_SERVICE_INTERNAL_INCLUDED__

#include <dirent.h>
//#include "public/libesq.h"

#define FOO_PREFEREC_BUFFER_SIZE	8192
#define FOO_DEFAULT_PAGE	"index.html"

#define FOO_FLAGS_ALLOW_BROWSE		0x00000001
#define FOO_FLAGS_ALLOW_UPLOAD		0x00000002
#define FOO_FLAGS_ALLOW_REMOVE		0x00000003
#define FOO_FLAGS_SEND_INDEX_HTML	0x00000004

#define FOO_STR_ALLOW_BROWSE	"AllowBrowse"
#define FOO_STR_ALLOW_UPLOAD	"AllowUpload"
#define FOO_STR_ALLOW_REMOVE	"AllowRemove"
#define FOO_STR_SEND_INDEX_HTML	"SendIndexHTML"

typedef struct _foo_service_ctx_t	{
	int flags;
	int path_head_len;
	int local_path_head_len;
	char * path_head;
	char * local_path_head;
	char string_buf[];
} foo_service_ctx_t;

#define FOOPAGEMAX	(1024 * 8)

typedef struct _foo_stream_ctx_t	{
	foo_service_ctx_t * service_ctx;
	int page_seek;
	int page_size;
	char page_buf[FOOPAGEMAX];
} foo_stream_ctx_t;

extern int foo_page(nghttp2_session * session, void * h2esq_session, void * h2esq_stream, char * path, foo_service_ctx_t * service_ctx);
extern int foo_handler(nghttp2_session * session, void * h2esq_session, void * h2esq_stream, void * ctx);
extern void foo_on_stream_close(void * h2esq_stream);
extern int foo_chunk_recv_cb(void * h2esq_stream_ptr, const char * buf, int len);

#endif
