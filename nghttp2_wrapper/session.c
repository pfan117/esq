// #define PRINT_DBG_LOG

#include <unistd.h>
#include <strings.h>
#include <nghttp2/nghttp2.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "public/esq.h"
#include "public/h2esq.h"
#include "public/libesq.h"
#include "public/esqll.h"
#include "internal/log_print.h"

#include "nghttp2_wrapper/includes/ssl-ctx.h"
#include "nghttp2_wrapper/includes/callbacks.h"
#include "nghttp2_wrapper/includes/instance.h"
#include "nghttp2_wrapper/includes/session.h"
#include "nghttp2_wrapper/includes/stream.h"

long
h2esq_session_recv_cb(struct nghttp2_session * session, uint8_t * buf, unsigned long length
		, int flags, void * user_data)
{
	h2esq_session_t * h2esq_session = user_data;
	int r;

	r = SSL_read(h2esq_session->ssl, buf, length);
	if (r >= 0)	{
		return r;
	}
	else	{
		r = SSL_get_error(h2esq_session->ssl, r);
		if (SSL_ERROR_WANT_READ == r)	{
			return NGHTTP2_ERR_WOULDBLOCK;
		}
		else	{
			return NGHTTP2_ERR_CALLBACK_FAILURE;
		}
	}
}

long
h2esq_session_send_cb(
		struct nghttp2_session * session,
		const uint8_t * data, unsigned long length,
		int flags, void * user_data)
{
	h2esq_session_t * h2esq_session = user_data;
	int r;

	r = SSL_write(h2esq_session->ssl, data, length);
	if (r >= 0)	{
		return r;
	}
	else	{
		r = SSL_get_error(h2esq_session->ssl, r);
		if (SSL_ERROR_WANT_WRITE == r)	{
			return NGHTTP2_ERR_WOULDBLOCK;
		}
		else	{
			return NGHTTP2_ERR_CALLBACK_FAILURE;
		}
	}
}

/* return values:
 * 0: finished
 * 1: continue
 * -1: error
 */
STATIC int
h2esq_SSL_accept(SSL * ssl)	{
	int r;

	r = SSL_accept(ssl);
	if (0 == r)	{
		/* The TLS/SSL handshake was not successful but was shut down controlled
		 *  and by the specifications of the TLS/SSL protocol.
		 *  Call SSL_get_error() with the return value ret to find out the reason. */
		;
	}
	else if (1 == r)	{
		/* The TLS/SSL handshake was successfully completed, a TLS/SSL connection
		 *  has been established. */
		return 0;
	}
	else if (r < 0)	{
		/* The TLS/SSL handshake was not successful because a fatal error occurred
		 *  either at the protocol level or a connection failure occurred. The shutdown
		 *  was not clean. It can also occur of action is need to continue
		 *  the operation for non-blocking BIOs. Call SSL_get_error() with the return
		 *  value ret to find out the reason.
		 */
		;
	}

	r = SSL_get_error(ssl, r);
	switch(r)	{
	case SSL_ERROR_NONE:
		return -1;
	case SSL_ERROR_ZERO_RETURN:
		return -1;
	case SSL_ERROR_WANT_READ:
		return 1;
	case SSL_ERROR_WANT_WRITE:
		return 1;
	case SSL_ERROR_WANT_CONNECT:
		return 1;
	case SSL_ERROR_WANT_ACCEPT:
		return 1;
	case SSL_ERROR_WANT_X509_LOOKUP:
		return -1;
#ifdef SSL_ERROR_WANT_ASYNC
	case SSL_ERROR_WANT_ASYNC:
		return -1;
#endif
#ifdef SSL_ERROR_WANT_ASYNC_JOB
	case SSL_ERROR_WANT_ASYNC_JOB:
		return -1;
#endif
	case SSL_ERROR_SYSCALL:
		return -1;
	case SSL_ERROR_SSL:
		return -1;
	default:
		return -1;
	}
}

STATIC void
h2esq_session_fd_cb(void * current_session)	{

	h2esq_session_t * h2esq_session;
	int new_event_bits;
	int event_bits;
	int r;

	event_bits = esq_session_get_event_bits(current_session);
	h2esq_session = esq_session_get_param(current_session);

	#if defined PRINT_DBG_LOG
	char event_bits_str[64];
	PRINTF("DBG: %s() %d: events are \"%s\" (0x%x)\n", __func__, __LINE__
			, esq_get_event_bits_string(event_bits, event_bits_str, sizeof(event_bits_str))
			, event_bits
			);
	#endif

	if ((event_bits & ESQ_EVENT_BIT_TASK_DONE))	{
		esq_execute_session_done_cb(current_session);
	}

	if ((event_bits & ESQ_EVENT_BIT_EXIT))	{
		goto __exx;
	}

	if ((event_bits & ESQ_EVENT_BIT_LOCK_OBTAIN))	{
		goto __exx;
	}

	if ((event_bits & ESQ_EVENT_BIT_FD_CLOSE))	{
		goto __exx;
	}

	if ((event_bits & ESQ_EVENT_BIT_ERROR))	{
		goto __exx;
	}

	if ((event_bits & ESQ_EVENT_BIT_TIMEOUT))	{
		goto __exx;
	}

	if ((event_bits & ESQ_EVENT_BIT_READ))	{
		if (nghttp2_session_want_read(h2esq_session->ngh2_session))	{
			r = nghttp2_session_recv(h2esq_session->ngh2_session);
			/* will call h2esq_session_recv_cb() */
			if (r)	{
				goto __exx;
			}
		}
	}

	if ((event_bits & ESQ_EVENT_BIT_WRITE))	{
		if (nghttp2_session_want_write(h2esq_session->ngh2_session))	{
			r = nghttp2_session_send(h2esq_session->ngh2_session);
			/* will call h2esq_session_send_cb() */
			if (r)	{
				goto __exx;
			}
		}
	}

	new_event_bits = 0;

	if (nghttp2_session_want_read(h2esq_session->ngh2_session))	{
		new_event_bits |= ESQ_EVENT_BIT_READ;
	}

	if (nghttp2_session_want_write(h2esq_session->ngh2_session))	{
		new_event_bits |= ESQ_EVENT_BIT_WRITE;
	}

	if (new_event_bits)	{
		int fd;

		fd = esq_session_get_fd(current_session);
		if (fd > 0)	{
			;
		}
		else	{
			esq_design_error();
		}

		PRINTF("DBG: %s() %d: new_event_bits 0x%x, setup client fd again\n"
				, __func__, __LINE__, new_event_bits);
		r = esq_session_set_socket_fd(
				current_session,
				fd,
				h2esq_session_fd_cb,
				new_event_bits,
				10, 0,
				h2esq_session
				);
		if (r)	{
			goto __exx;
		}
	}
	else	{
		PRINTF("DBG: %s() %d: new_event_bits 0\n", __func__, __LINE__);
		goto __exx;
	}

	return;

__exx:

	while(h2esq_session->stream_list_head)	{
		h2esq_stream_close(h2esq_session->stream_list_head);
	}

	nghttp2_session_del(h2esq_session->ngh2_session);
	SSL_shutdown(h2esq_session->ssl);
	SSL_free(h2esq_session->ssl);
	close(h2esq_session->fd);
	free(h2esq_session);

	return;
}

int
h2esq_send_server_connection_header(h2esq_session_t * h2esq_session)	{
	nghttp2_settings_entry iv[] = {{NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100}};
	int r;

	r = nghttp2_submit_settings(h2esq_session->ngh2_session,
			NGHTTP2_FLAG_NONE,
			iv, ESQ_ARRAY_SIZE(iv)
			);
	if (r)	{
		return -1;
	}

	return 0;
}

/* server connection headder - sch */
STATIC void 
h2esq_session_fd_sch_cb(void * current_esq_session)	{
	h2esq_session_t * h2esq_session;
	int event_bits;
	int fd;
	int r;

	fd = esq_session_get_fd(current_esq_session);
	if (fd <= 0)	{
		esq_design_error();
	}

	h2esq_session = esq_session_get_param(current_esq_session);

	event_bits = esq_session_get_event_bits(current_esq_session);

	if ((event_bits & ESQ_EVENT_BIT_EXIT))	{
		goto __exx;
	}

	if ((event_bits & ESQ_EVENT_BIT_LOCK_OBTAIN))	{
		goto __exx;
	}

	if ((event_bits & ESQ_EVENT_BIT_FD_CLOSE))	{
		goto __exx;
	}

	if ((event_bits & ESQ_EVENT_BIT_ERROR))	{
		goto __exx;
	}

	if ((event_bits & ESQ_EVENT_BIT_TIMEOUT))	{
		goto __exx;
	}

	if ((event_bits & ESQ_EVENT_BIT_READ) || (event_bits & ESQ_EVENT_BIT_WRITE))	{
		r = h2esq_send_server_connection_header(h2esq_session);
		if (r)	{
			goto __exx;
		}

		r = esq_session_set_socket_fd(
				current_esq_session,
				fd,
				h2esq_session_fd_cb,
				(ESQ_EVENT_BIT_READ | ESQ_EVENT_BIT_WRITE),
				10, 0,
				h2esq_session
				);


		if (r)	{
			goto __exx;
		}
	}

	return;

__exx:

	nghttp2_session_del(h2esq_session->ngh2_session);
	SSL_shutdown(h2esq_session->ssl);
	SSL_free(h2esq_session->ssl);
	close(h2esq_session->fd);
	free(h2esq_session);

	return;
}

STATIC void 
h2esq_session_fd_alpn_cb(void * current_esq_session)	{
	h2esq_session_t * h2esq_session;
	int event_bits;
	int fd;
	int r;

	fd = esq_session_get_fd(current_esq_session);
	if (fd <= 0)	{
		esq_design_error();
	}

	h2esq_session = esq_session_get_param(current_esq_session);

	event_bits = esq_session_get_event_bits(current_esq_session);

	if (!event_bits)	{
		esq_design_error();
	}

	if ((event_bits & ESQ_EVENT_BIT_EXIT))	{
		goto __exx;
	}

	if ((event_bits & ESQ_EVENT_BIT_LOCK_OBTAIN))	{
		goto __exx;
	}

	if ((event_bits & ESQ_EVENT_BIT_FD_CLOSE))	{
		goto __exx;
	}

	if ((event_bits & ESQ_EVENT_BIT_ERROR))	{
		goto __exx;
	}

	if ((event_bits & ESQ_EVENT_BIT_TIMEOUT))	{
		goto __exx;
	}

	if ((event_bits & ESQ_EVENT_BIT_READ) || (event_bits & ESQ_EVENT_BIT_WRITE))	{
		r = h2esq_SSL_accept(h2esq_session->ssl);
		if (1 == r)	{
			PRINTF("DBG: %s() %d\n", __func__, __LINE__);
			r = esq_session_set_socket_fd(
					current_esq_session,
					fd,
					h2esq_session_fd_alpn_cb,
					(ESQ_EVENT_BIT_READ | ESQ_EVENT_BIT_WRITE),
					10, 0,
					h2esq_session
					);
			if (r)	{
				goto __exx;
			}

			return;
		}
		else if (0 == r)	{
			PRINTF("DBG: %s() %d\n", __func__, __LINE__);
			r = esq_session_set_socket_fd(
					current_esq_session,
					fd,
					h2esq_session_fd_sch_cb,
					(ESQ_EVENT_BIT_READ | ESQ_EVENT_BIT_WRITE),
					10, 0,
					h2esq_session
					);
			if (r)	{
				goto __exx;
			}
		}
		else	{
			goto __exx;
		}
	}

	return;

__exx:

	nghttp2_session_del(h2esq_session->ngh2_session);
	SSL_shutdown(h2esq_session->ssl);
	SSL_free(h2esq_session->ssl);
	close(fd);
	free(h2esq_session);

	return;
}

int
h2esq_frame_recv_cb(
		nghttp2_session * session,
		const nghttp2_frame * frame,
		void * user_data)
{
	h2esq_session_t * h2esq_session = user_data;
	void * stream_data;

	if (NGHTTP2_DATA == frame->hd.type)	{
		;
	}
	else if (NGHTTP2_HEADERS == frame->hd.type)	{
		;
	}
	else	{
		return 0;
	}

	if ((frame->hd.flags & NGHTTP2_FLAG_END_STREAM))	{
		stream_data = nghttp2_session_get_stream_user_data(session, frame->hd.stream_id);
		if (stream_data)	{
			return h2esq_stream_on_request_recv(session, h2esq_session, stream_data);
		}
		else	{
			return 0;
		}
	}

	return 0;
}

STATIC nghttp2_session *
h2esq_new_nghttp2_session(void * nghttp2_session_data)	{
	nghttp2_session_callbacks * callbacks = NULL;
	nghttp2_session * ngh2_session = NULL;
	int r;

	callbacks = h2esq_get_ngh2_callbacks();
	if (!callbacks)	{
		return NULL;
	}

	r = nghttp2_session_server_new(&ngh2_session, callbacks, nghttp2_session_data);
	if (r)	{
		return NULL;
	}

	return ngh2_session;
}

void
h2esq_create_new_session(int sd, void * parent_instance)	{
	nghttp2_session * ngh2_session = NULL;
	h2esq_session_t * h2esq_session = NULL;
	struct sockaddr client_addr;
	void * esq_session = NULL;
	socklen_t addrlen;
	SSL * ssl = NULL;
	void * ssl_ctx;
	int fd = -1;
	int r;

	h2esq_session = malloc(sizeof(h2esq_session_t));
	if (h2esq_session)	{
		bzero(h2esq_session, sizeof(h2esq_session_t));
		h2esq_session->addrlen = sizeof(struct sockaddr_in);
		fd = accept(sd, &(h2esq_session->client_addr), &(h2esq_session->addrlen));
		if (-1 == fd)	{
			goto __exx;
		}
	}
	else	{
		addrlen = sizeof(struct sockaddr_in);
		fd = accept(sd, &client_addr, &addrlen);
		goto __exx;
	}

	r = os_socket_set_non_blocking(fd);
	if(r)	{
		goto __exx;
	}

	ssl_ctx = h2esq_instance_get_ssl_ctx(parent_instance);

	ssl = SSL_new(ssl_ctx);
	if (!ssl)	{
		goto __exx;
	}

	esq_session = esq_session_new("h2-connection");
	if (!esq_session)	{
		goto __exx;
	}

	ngh2_session = h2esq_new_nghttp2_session(h2esq_session);
	if (!ngh2_session)	{
		goto __exx;
	}

	r = SSL_set_fd(ssl, fd);
	if (!r)	{
		goto __exx;
	}

	h2esq_session->h2esq_parent_instance = parent_instance;
	h2esq_session->esq_session = esq_session;
	h2esq_session->fd = fd;
	h2esq_session->ssl = ssl;
	h2esq_session->ngh2_session = ngh2_session;

	PRINTF("DBG: %s() %d\n", __func__, __LINE__);
	r = esq_session_set_socket_fd(
			esq_session,
			fd,
			h2esq_session_fd_alpn_cb,
			(ESQ_EVENT_BIT_READ | ESQ_EVENT_BIT_WRITE),
			10, 0,
			h2esq_session
			);
	if (r)	{
		goto __exx;
	}

	esq_session_wind_up_rr(esq_session);

	return;

__exx:

	if (ssl)	{
		SSL_shutdown(ssl);
		SSL_free(ssl);
	}

	if (-1 != fd)	{
		close(fd);
	}

	if (h2esq_session)	{
		free(h2esq_session);
	}

	if (esq_session)	{
		esq_session_wind_up(esq_session);
	}

	if (ngh2_session)	{
		nghttp2_session_del(ngh2_session);
	}

	return;
}

int
h2esq_session_init(void)	{
	return 0;
}

void
h2esq_session_detach(void)	{
	return;
}

/* eof */
