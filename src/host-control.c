#define PRINT_DBG_LOG

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <monkeycall.h>

#include "public/esq.h"
#include "public/libesq.h"
#include "internal/types.h"
#include "internal/host_control.h"
#include "internal/log_print.h"

#include "internal/lock.h"

#define MAXBUFSIZE	(1024 * 1024)
#define MAXERRBUF	1024

static int esq_hc_port;
static int esq_hc_fd = -1;
static void * esq_hc_session = NULL;

/* return -1 is a suggestion to close the session */
STATIC void
esq_hc_mkc_execute(int fd, const char * code, int codelen)
{
	char errbuf[MAXERRBUF];
	mkc_session * session;
	void * instance;
	int r;
	int l;

	instance = esq_get_mkc();
	errbuf[0] = '\0';

	session = mkc_new_session(instance, code, codelen);
	if (!session)	{
		return;
	}

	mkc_session_set_error_info_buffer(session, errbuf, sizeof(errbuf));

	r = mkc_parse(session);
	if (r)	{
		l = strlen(errbuf);
		mkc_free_session(session);
		if (r > 0)	{
			r = write(fd, errbuf, l);
			if (r == l)	{
				return;
			}
			else	{
				return;
			}
		}
		else	{
			return;
		}
	}

	r = mkc_go(session);
	if (r)	{
		mkc_free_session(session);
		l = strnlen(errbuf, sizeof(errbuf));
		if (l > 0 && l < sizeof(errbuf))	{
			write(fd, errbuf, l);
		}
		return;
	}

	if (MKC_D_BUF == session->result.type && session->result.length > 0)	{
		write(fd, session->result.buffer, session->result.length);
	}
	else if (MKC_D_INT == session->result.type)	{
		char buf[128];
		r = snprintf(buf, sizeof(buf), "%d", session->result.integer);
		if (r <= 0 || r >= sizeof(buf))	{
			;
		}
		else	{
			write(fd, buf, r);
		}
	}

	mkc_free_session(session);

	return;
}

STATIC void
esq_hc_chat(int fd)	{
	char * buffer;
	int r;

	buffer = malloc(MAXBUFSIZE);
	if (!buffer)	{
		return;
	}

	r = read(fd, buffer, MAXBUFSIZE);
	if (r <= 0)	{
		free(buffer);
		return;
	}

	esq_hc_mkc_execute(fd, buffer, r);

	free(buffer);

	return;
}

STATIC void
esq_hc_instance_close_sd()	{
	void * esq_session;
	int sd;

	esq_global_lock();

	sd = esq_hc_fd;
	esq_session = esq_hc_session;
	esq_hc_fd = -1;
	esq_hc_session = NULL;

	esq_global_unlock();

	if (esq_session)	{
		esq_session_remove_req(esq_session);
	}

	if (-1 == sd)	{
		;
	}
	else	{
		close(sd);
	}

	return;
}

void
esq_hc_create_new_session(int sd)	{
	struct sockaddr client_addr;
	socklen_t addrlen;
	int fd = -1;

	addrlen = sizeof(struct sockaddr_in);
	fd = accept(sd, &client_addr, &addrlen);
	if (-1 == fd)	{
		return;
	}

	esq_hc_chat(fd);

	close(fd);

	return;
}

STATIC void 
esq_hc_server_sd_cb(void * current_session)	{

	int event_bits;

	event_bits = esq_session_get_event_bits(current_session);

	if ((event_bits & ESQ_EVENT_BIT_EXIT))	{
		esq_hc_instance_close_sd();
		return;
	}

	if ((event_bits & ESQ_EVENT_BIT_READ))	{
		int fd;

		fd = esq_session_get_fd(current_session);
		if (fd > 0)	{
			esq_hc_create_new_session(fd);
		}
	}

	if ((event_bits & ESQ_EVENT_BIT_WRITE))	{
		PRINTF("ERROR: %s() %d: server socket receive file write event\n", __func__, __LINE__);
	}

	if ((event_bits & ESQ_EVENT_BIT_FD_CLOSE))	{
		PRINTF("ERROR: %s() %d: server socket receive file close event\n", __func__, __LINE__);
	}

	if ((event_bits & ESQ_EVENT_BIT_ERROR))	{
		PRINTF("ERROR: %s() %d: server socket receive file error event\n", __func__, __LINE__);
	}

	if ((event_bits & ESQ_EVENT_BIT_TIMEOUT))	{
		PRINTF("ERROR: %s() %d: server socket receive time out event\n", __func__, __LINE__);
	}

	if ((event_bits & ESQ_EVENT_BIT_LOCK_OBTAIN))	{
		PRINTF("ERROR: %s() %d: server session receive lock obtain event\n", __func__, __LINE__);
	}

	return;
}

STATIC cc *
esq_hc_server_prepare_socket(int port, void * esq_session)	{
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

	r = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (r)	{
		close(sd);
		return "failed to set socket option";
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
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

	esq_hc_fd = sd;
	esq_hc_port = port;

	r = esq_session_set_socket_fd_multi(esq_session, sd, esq_hc_server_sd_cb, NULL);
	if (r)	{
		close(sd);
		esq_hc_fd = -1;
		return "failed to wind up socket event";
	}

	return NULL;
}

STATIC cc *
esq_hc_start_instance(int port)	{
	void * esq_session;
	cc * errs;

	if (-1 != esq_hc_fd)	{
		return "already started";
	}

	if (port <= 0)	{
		return "invalid port number";
	}

	esq_session = esq_session_new("local-control-server-socket");
	if (!esq_session)	{
		return "failed to create new esq session";
	}

	esq_global_lock();

	if (esq_hc_session)	{
		esq_global_unlock();
		esq_session_wind_up(esq_session);
		return "only one instance allowed";
	}
	else	{
		esq_hc_session = esq_session;
		esq_global_unlock();
	}

	errs = esq_hc_server_prepare_socket(port, esq_session);
	if (errs)	{
		goto __exx;
	}

	esq_session_wind_up_rr(esq_session);

	return NULL;

__exx:

	esq_global_lock();
	esq_hc_session = NULL;
	esq_global_unlock();

	esq_session_wind_up(esq_session);

	return errs;
}

/* return value: buffer */
STATIC int
esq_hc_mkccb_start_instance(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	mkc_data * port_number;
	cc * errs;

	MKC_CB_ARGC_CHECK(1);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_INT);
	result = MKC_CB_RESULT;
	port_number = MKC_CB_ARGV(0);
	result->type = MKC_D_VOID;

	errs = esq_hc_start_instance(port_number->integer);
	if (errs)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, errs);
		return -1;
	}

	return 0;
}


int
esq_host_control_init(void)	{
	void * server;
	server = esq_get_mkc();
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(server, "StartHostControlInstance", esq_hc_mkccb_start_instance));
	return 0;
}

void
esq_host_control_detach(void)	{
	return;
}

/* eof */
