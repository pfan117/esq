#ifndef __H2ESQ_STREAM_HEADER_INCLDUED__
#define __H2ESQ_STREAM_HEADER_INCLDUED__

#include "public/libesq.h"

#define H2ESQ_ITEM_SPLITER_MAX	(256)

typedef struct _h2esq_stream_t	{
	int id;
	void * parent_session;	/* required during stream close */
	struct _h2esq_stream_t * next;
	struct _h2esq_stream_t * prev;
	void * app_stream_ctx;
	char * header_strs[H2ESQ_HEADER_MAX];
	size_t header_lens[H2ESQ_HEADER_MAX];
	void * cookies_root[2];
	h2esq_service_t * h2esq_service;
	int path_matched_length;

	int post_state;
	char post_boundary[H2ESQ_ITEM_SPLITER_MAX];
	char post_boundary_length;

} h2esq_stream_t;

extern int h2esq_stream_on_request_recv(nghttp2_session * session, h2esq_session_t * h2esq_session, h2esq_stream_t * h2esq_stream);
extern int h2esq_stream_on_header(nghttp2_session * nghttp2_session, const nghttp2_frame * frame
		, const uint8_t * name, size_t namelen
		, const uint8_t * value, size_t valuelen
		, uint8_t flags, void * user_data);
extern int h2esq_stream_on_begin_headers(nghttp2_session *session
		, const nghttp2_frame *frame
		, void *user_data);
extern int h2esq_stream_on_close(nghttp2_session *session, int32_t stream_id, uint32_t error_code, void *user_data);
extern int h2esq_stream_on_data_chunk_recv(nghttp2_session *session, uint8_t flags, int32_t stream_id
		, const uint8_t *data, size_t len, void *user_data);
extern void h2esq_stream_close(h2esq_stream_t * h2esq_stream);

#endif
