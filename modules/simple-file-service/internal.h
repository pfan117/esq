#ifndef __H2ESQ_MOD_SIMPLE_FILE_SERVICE_INTERNAL_INCLUDED__
#define __H2ESQ_MOD_SIMPLE_FILE_SERVICE_INTERNAL_INCLUDED__

#include <dirent.h>
#include "public/libesq.h"

#define SFS_PREFEREC_BUFFER_SIZE	8192
#define SFS_DEFAULT_PAGE	"index.html"

#define SFS_FLAGS_ALLOW_BROWSE		0x00000001
#define SFS_FLAGS_ALLOW_UPLOAD		0x00000002
#define SFS_FLAGS_ALLOW_REMOVE		0x00000003
#define SFS_FLAGS_SEND_INDEX_HTML	0x00000004

#define SFS_STR_ALLOW_BROWSE	"AllowBrowse"
#define SFS_STR_ALLOW_UPLOAD	"AllowUpload"
#define SFS_STR_ALLOW_REMOVE	"AllowRemove"
#define SFS_STR_SEND_INDEX_HTML	"SendIndexHTML"

typedef struct _sfs_service_ctx_t	{
	int flags;
	int path_head_len;
	int local_path_head_len;
	char * path_head;
	char * local_path_head;
	char string_buf[];
} sfs_service_ctx_t;

typedef struct _sfs_stream_ctx_t	{
	int fd;
	DIR * dir;
	int dir_output_stat;
	int continue_offset;
	char * path;
	struct dirent * ent;
	int download_len;
	sfs_service_ctx_t * service_ctx;
	int item_idx;
	/* for file uploading */
	int post_fd;
	int post_state;
	post_data_ctx_t post_parser_ctx;
	int post_len;
} sfs_stream_ctx_t;

extern int sfs_folder(nghttp2_session * session, void * h2esq_session, void * h2esq_stream, char * path, sfs_service_ctx_t * service_ctx		);
extern int sfs_file(nghttp2_session * session, void * h2esq_session, void * h2esq_stream, char * path, sfs_service_ctx_t * service_ctx		);
extern int sfs_handler(nghttp2_session * session, void * h2esq_session, void * h2esq_stream, void * ctx);
extern void sfs_on_stream_close(void * h2esq_stream);
extern int sfs_chunk_recv_cb(void * h2esq_stream_ptr, const char * buf, int len);
extern sfs_stream_ctx_t * sfs_new_ctx(void);

#endif
