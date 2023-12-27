#ifndef __H2ESQ_SESSION_HEADER_INCLDUED__
#define __H2ESQ_SESSION_HEADER_INCLDUED__

#include <openssl/ssl.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct _h2esq_session_t	{
	h2esq_instance_t * h2esq_parent_instance;
	nghttp2_session * ngh2_session;
	void * esq_session;
	int fd;
	SSL * ssl;
	struct sockaddr client_addr;
	socklen_t addrlen;
	void * stream_list_head;
	void * stream_list_tail;
} h2esq_session_t;

#if 0
extern void * h2esq_session_add_stream(void * h2esq_session, void * h2esq_stream);
#endif

/* for nghttp2 callbacks */
extern int h2esq_frame_recv_cb(nghttp2_session * session, const nghttp2_frame * frame, void * user_data);
extern long h2esq_session_recv_cb(struct nghttp2_session * session, uint8_t * buf, unsigned long length
		, int flags, void * user_data);
extern long h2esq_session_send_cb(struct nghttp2_session * session, const uint8_t * data, unsigned long length
		, int flags, void * user_data);

/* for startup */
extern int h2esq_session_init(void);
extern void h2esq_session_detach(void);
extern void h2esq_create_new_session(int sd, void * instance);

#endif
