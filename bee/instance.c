//#define PRINT_DBG_LOG

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "public/esq.h"
#include "public/libesq.h"
#include "public/esqll.h"
#include "internal/types.h"
#include "internal/range_map.h"
#include "internal/log_print.h"

#include "internal/lock.h"

#include "bee/public.h"
#include "bee/internal.h"
#include "bee/pt.h"

void
bee_create_new_session(bee_server_instance_t * instance)	{
	bee_service_ctx_t * session = NULL;
	struct sockaddr client_addr;
	socklen_t addrlen;
	void * esq_session = NULL;
	int fd = -1;
	int r;

	PRINTF("DBG: %s() %d: enter\n", __func__, __LINE__);

	addrlen = sizeof(struct sockaddr_in);
	fd = accept(instance->sd, &client_addr, &addrlen);
	if (-1 == fd)	{
		PRINTF("DBG: %s() %d: accept() failed\n", __func__, __LINE__);
		goto __exx;
	}

	PRINTF("DBG: %s() %d\n", __func__, __LINE__);

	r = os_socket_set_non_blocking(fd);
	if(r)	{
		PRINTF("DBG: %s() %d: set non blocking() failed\n", __func__, __LINE__);
		goto __exx;
	}

	PRINTF("DBG: %s() %d\n", __func__, __LINE__);

	session = malloc(sizeof(bee_service_ctx_t));
	if (!session)	{
		PRINTF("DBG: %s() %d: malloc() failed\n", __func__, __LINE__);
		goto __exx;
	}

	PRINTF("DBG: %s() %d\n", __func__, __LINE__);

	bzero(session, sizeof(bee_service_ctx_t));
	session->instance = instance;
	session->fd = fd;

	esq_session = esq_session_new("bee");
	if (!esq_session)	{
		PRINTF("DBG: %s() %d: create new session failed\n", __func__, __LINE__);
		goto __exx;
	}

	PRINTF("DBG: %s() %d\n", __func__, __LINE__);

	PT_RESET(session);

	PRINTF("DBG: %s() %d\n", __func__, __LINE__);

	esq_session_set_socket_fd(esq_session, fd, bee_session_receive_request
			, (ESQ_EVENT_BIT_READ | ESQ_EVENT_BIT_WRITE)
			, 2, 0, session);

	esq_session_wind_up_rr(esq_session);

	return;

__exx:

	if (-1 != fd)	{
		close(fd);
	}

	if (session)	{
		free(session);
	}

	if (esq_session)	{
		esq_session_wind_up(esq_session);	/* free */
	}

	return;
}

static bee_server_instance_t bee_server_instance;

STATIC void
bee_server_instance_close(void)	{
	void * esq_session;
	int sd;

	esq_global_lock();
	sd = bee_server_instance.sd;
	esq_session = bee_server_instance.esq_session;
	bee_server_instance.sd = -1;
	bee_server_instance.esq_session = NULL;
	esq_global_unlock();

	if (-1 == sd)	{
		;
	}
	else	{
		close(sd);
	}

	if (esq_session)	{
		esq_session_remove_req(esq_session);
	}

	return;
}

STATIC void 
bee_server_listener(void * current_session)	{

	int event_bits;

	event_bits = esq_session_get_event_bits(current_session);

	if ((event_bits & ESQ_EVENT_BIT_EXIT))	{
		goto __exx;
	}

	if ((event_bits & ESQ_EVENT_BIT_READ))	{
		bee_create_new_session(esq_session_get_param(current_session));
	}

	if ((event_bits & ESQ_EVENT_BIT_WRITE))	{
		;
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

	if ((event_bits & ESQ_EVENT_BIT_LOCK_OBTAIN))	{
		goto __exx;
	}

	return;

__exx:

	bee_server_instance_close();

	return;
}

STATIC cc *
bee_server_prepare_socket(void)	{
	struct sockaddr_in addr;
	int optval = 1;
	int sd;
	int r;

	sd = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == sd)	{
		return "failed to open new socket";
	}

	r = os_socket_set_non_blocking(sd);
	if(r)	{
		close(sd);
		return "failed to set socket to non-blocking";
	}

	optval = 1;
	r = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (r)	{
		close(sd);
		return "failed to set socket option SO_REUSEADDR";
	}

	optval = 5;	/* a demo shows this value */
	r = setsockopt(sd, SOL_TCP, TCP_FASTOPEN, &optval, sizeof(optval));
	if (r)	{
		close(sd);
		return "failed to set socket option TCP_FASTOPEN";
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(bee_server_instance.port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(&(addr.sin_zero), 0, sizeof(addr.sin_zero));

	r = bind(sd, (struct sockaddr *)(&addr), sizeof(addr));
	if (r)	{
		close(sd);
		return "failed to bind socket";
	}

	r = listen(sd, 100);
	if (r)	{
		close(sd);
		return "failed to listen";
	}

	bee_server_instance.sd = sd;

	r = esq_session_set_socket_fd_multi(bee_server_instance.esq_session
			, sd, bee_server_listener
			, &bee_server_instance
			);
	if (r)	{
		close(sd);
		return "failed to wind up socket event";
	}

	return NULL;
}

int
bee_server_is_my_port(int port)	{
	if (-1  == bee_server_instance.port)	{
		return 0;
	}

	if (port == bee_server_instance.port)	{
		return 1;
	}
	else	{
		return 0;
	}
}

const char *
bee_server_start_instance(int port)	{
	void * esq_session;
	cc * errs;

	if (port <= 0)	{
		return "invalid port number";
	}

	if (-1 != bee_server_instance.sd)	{
		return "instances already started";
	}

	esq_session = esq_session_new("bee-server");
	if (!esq_session)	{
		return "failed to create new esq session";
	}

	esq_session_set_param(esq_session, &bee_server_instance);
	bee_server_instance.esq_session = esq_session;
	bee_server_instance.port = port;

	errs = bee_server_prepare_socket();
	if (errs)	{
		goto __exx;
	}

	esq_range_map_notify_self_port_setup();

	esq_session_wind_up_rr(esq_session);

	return NULL;

__exx:

	bee_server_instance_close();
	esq_session_wind_up(esq_session);

	return errs;
}

void
bee_create_target_cross_links(bucket_sync_session_t * sync_session)	{
	bucket_sync_target_t * target;
	bucket_sync_target_t * first;
	bucket_sync_target_t * p;
	int i;

	first = sync_session->targets;

	for (i = 1; i < sync_session->target_cnt; i ++)	{

		target = sync_session->targets + i;

		/* look for same hash target */
		for (p = first; p; p = p->next_hash_list)	{
			if (p->hash_length != target->hash_length)	{
				continue;
			}

			if (!p->hash_length)	{
				goto __found_same_hash_target_list;
			}

			if (memcmp(p->hash.buf, target->hash.buf, p->hash_length))	{
				continue;
			}

			goto __found_same_hash_target_list;
		}

		/* there is no same hash target list, create one */
		for (p = first; p->next_hash_list; p = p->next_hash_list)	{
			;
		}

		p->next_hash_list = target;

		continue;

__found_same_hash_target_list:

		for (; p->same_hash_next; p = p->same_hash_next)	{
			;
		}

		p->same_hash_next = target;

		continue;
	}

	return;
}

int
esq_bee_init(void)	{

	/* instances */
	bee_server_instance.sd = -1;

	ESQ_IF_ERROR_RETURN(esq_bee_install_mkc_commands());

	return 0;
}

void
esq_bee_detach(void)	{

	/* instances */
	bee_server_instance_close();
	bee_bucket_detach();

	return;
}

/* eof */
