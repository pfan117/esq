//#define PRINT_DBG_LOG

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <strings.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define _ESQ_LIB_SOCKET_FUNCTIONS

#include "public/esq.h"
#include "public/mco-utils.h"
#include "public/libesq.h"
#include "internal/types.h"
#include "public/esqll.h"
#include "internal/tree.h"
#include "internal/log_print.h"

#include "internal/lock.h"
#include "internal/signals.h"
#include "internal/session.h"
#include "internal/threads.h"
#include "internal/os_events.h"

#define MCO_SOCKET_IO_MAX_RETRY	3

int
esq_mco_socket_connect_write(int fd, struct sockaddr * address, int address_size, char * buf, int buflen)	{

	void * esq_session;
	int offset;
	int cl;
	int r;

	if (!mco_get_co())	{
		printf("ERROR: %s %d: supposed to be called inside service mco\n", __func__, __LINE__);
		return ESQ_ERROR_PARAM;
	}

	esq_session = mco_get_arg();
	if (!esq_session)	{
		printf("ERROR: %s %d: failed to get esq session of the current mco\n", __func__, __LINE__);
		return ESQ_ERROR_PARAM;
	}

	cl = 0;
	offset = 0;

	for(;;)	{

		if (cl >= MCO_SOCKET_IO_MAX_RETRY)	{
			return ESQ_ERROR_TIMEOUT;
		}

		r = os_fast_open_sendto_nb(fd, address, address_size, buf, buflen);
		PRINTF("DBG: %s() %d: fast open sendto return %d\n", __func__, __LINE__, r);
		if (0 == r)	{
			cl ++;

			r = esq_session_set_socket_fd(esq_session, fd, NULL, ESQ_EVENT_BIT_WRITE, 10, 0, NULL);
			if (r)	{
				return r;
			}

			mco_yield();

			continue;
		}
		else if (-1 == r)	{
			PRINTF("DBG: %s() %d: fast open send to return -1\n", __func__, __LINE__);
			return ESQ_ERROR_LIBCALL;
		}
		else if (r > 0)	{
			offset += r;
			if (offset == buflen)	{
				PRINTF("DBG: %s() %d: send complete\n", __func__, __LINE__);
				return ESQ_OK;
			}
			else if (offset > buflen)	{
				PRINTF("DBG: %s() %d: unknown error\n", __func__, __LINE__);
				return ESQ_ERROR_LIBCALL;
			}
			else	{
				continue;
			}
		}
		else	{
			PRINTF("DBG: %s() %d: unknown error\n", __func__, __LINE__);
			return ESQ_ERROR_LIBCALL;
		}
	}

	return ESQ_OK;
}

int
esq_mco_socket_write(int fd, const char * buf, int buflen)	{
	void * esq_session;
	int offset;
	int cl;
	int r;

	if (!mco_get_co())	{
		printf("ERROR: %s %d: supposed to be called inside service mco\n", __func__, __LINE__);
		return ESQ_ERROR_PARAM;
	}

	esq_session = mco_get_arg();
	if (!esq_session)	{
		printf("ERROR: %s %d: failed to get esq session of the current mco\n", __func__, __LINE__);
		return ESQ_ERROR_PARAM;
	}

	cl = 0;
	offset = 0;

	for(;;)	{

		if (cl >= MCO_SOCKET_IO_MAX_RETRY)	{
			PRINTF("DBG: %s() %d: retry too many times\n", __func__, __LINE__);
			return ESQ_ERROR_LIBCALL;
		}

		r = os_write_nb(fd, buf, buflen);
		if (0 == r)	{
			cl ++;

			r = esq_session_set_socket_fd(esq_session, fd, NULL, ESQ_EVENT_BIT_WRITE, 10, 0, NULL);
			if (r)	{
				return r;
			}

			mco_yield();

			continue;
		}
		else if (-1 == r)	{
			PRINTF("DBG: %s() %d: write error\n", __func__, __LINE__);
			return ESQ_ERROR_LIBCALL;
		}
		else if (r > 0)	{
			offset += r;
			if (offset == buflen)	{
				PRINTF("DBG: %s() %d: write complete\n", __func__, __LINE__);
				return ESQ_OK;
			}
			else if (offset > buflen)	{
				PRINTF("DBG: %s() %d: write error\n", __func__, __LINE__);
				return ESQ_ERROR_LIBCALL;
			}
			else	{
				PRINTF("DBG: %s() %d: again\n", __func__, __LINE__);
				continue;
			}
		}
		else	{
			PRINTF("DBG: %s() %d: unknown error\n", __func__, __LINE__);
			return ESQ_ERROR_LIBCALL;
		}
	}
}

int
esq_mco_socket_read(int fd, void * buf, int buflen)	{
	void * esq_session;
	int offset;
	int cl;
	int r;

	if (!mco_get_co())	{
		printf("ERROR: %s %d: supposed to be called inside service mco\n", __func__, __LINE__);
		return ESQ_ERROR_PARAM;
	}

	esq_session = mco_get_arg();
	if (!esq_session)	{
		printf("ERROR: %s %d: failed to get esq session of the current mco\n", __func__, __LINE__);
		return ESQ_ERROR_PARAM;
	}

	cl = 0;
	offset = 0;

	for(;;)	{

		if (cl >= MCO_SOCKET_IO_MAX_RETRY)	{
			PRINTF("DBG: %s() %d: mco read retry too many times\n", __func__, __LINE__);
			return ESQ_ERROR_TIMEOUT;
		}

		r = os_read_nb(fd, buf, buflen);
		if (0 == r)	{
			cl ++;

			r = esq_session_set_socket_fd(esq_session, fd, NULL, ESQ_EVENT_BIT_READ, 10, 0, NULL);
			if (r)	{
				return r;
			}

			mco_yield();

			continue;
		}
		else if (-1 == r)	{
			PRINTF("DBG: %s() %d: mco read error\n", __func__, __LINE__);
			return ESQ_ERROR_LIBCALL;
		}
		else if (r > 0)	{
			offset += r;
			if (offset == buflen)	{
				PRINTF("DBG: %s() %d: mco read complete\n", __func__, __LINE__);
				return ESQ_OK;
			}
			else if (offset > buflen)	{
				PRINTF("DBG: %s() %d: mco read error\n", __func__, __LINE__);
				return ESQ_ERROR_LIBCALL;
			}
			else	{
				PRINTF("DBG: %s() %d: mco read again\n", __func__, __LINE__);
				continue;
			}
		}
		else	{
			PRINTF("DBG: %s() %d: mco read error\n", __func__, __LINE__);
			return ESQ_ERROR_LIBCALL;
		}
	}
}

/* eof */

