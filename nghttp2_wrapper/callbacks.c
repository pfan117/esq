#include <nghttp2/nghttp2.h>

#include "public/esq.h"
#include "public/h2esq.h"
#include "public/esqll.h"
#include "nghttp2_wrapper/includes/instance.h"
#include "nghttp2_wrapper/includes/session.h"
#include "nghttp2_wrapper/includes/stream.h"
#include "nghttp2_wrapper/includes/callbacks.h"

STATIC nghttp2_session_callbacks * h2esq_ngh2_callbacks = NULL;

void *
h2esq_get_ngh2_callbacks(void)	{
	return h2esq_ngh2_callbacks;
}

int
h2esq_ngh2_callbacks_init(void)	{
	int r;

	r = nghttp2_session_callbacks_new(&h2esq_ngh2_callbacks);
	if (r)	{
		return ESQ_ERROR_MEMORY;
	}

	nghttp2_session_callbacks_set_on_frame_recv_callback(h2esq_ngh2_callbacks, h2esq_frame_recv_cb);

#if 0
	nghttp2_session_callbacks_set_data_source_read_length_callback(h2esq_ngh2_callbacks, h2esq_data_source_read_length_cb);
#endif

	nghttp2_session_callbacks_set_recv_callback(h2esq_ngh2_callbacks, h2esq_session_recv_cb);
	nghttp2_session_callbacks_set_send_callback(h2esq_ngh2_callbacks, h2esq_session_send_cb);

	nghttp2_session_callbacks_set_on_header_callback(h2esq_ngh2_callbacks, h2esq_stream_on_header);
	nghttp2_session_callbacks_set_on_begin_headers_callback(h2esq_ngh2_callbacks, h2esq_stream_on_begin_headers);
	nghttp2_session_callbacks_set_on_stream_close_callback(h2esq_ngh2_callbacks, h2esq_stream_on_close);

	nghttp2_session_callbacks_set_on_data_chunk_recv_callback(h2esq_ngh2_callbacks, h2esq_stream_on_data_chunk_recv);

	return ESQ_OK;
}

void
h2esq_ngh2_callbacks_detach(void)	{
	if (h2esq_ngh2_callbacks)	{
		nghttp2_session_callbacks_del(h2esq_ngh2_callbacks);
	}
}

/* eof */
