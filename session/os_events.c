#define _GNU_SOURCE /* for POLLRDHUP */

// #define PRINT_DBG_LOG

#include <stdio.h>

#include <poll.h>
#include <liburing.h>

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "public/esq.h"
#include "public/libesq.h"
#include "public/esqll.h"
#include "internal/log_print.h"

#include "internal/session.h"
#include "internal/os_events.h"

#define POLLFLAGS (POLLERR | POLLHUP)

#define OUTPUT(__BIT__,__S__)	do	{ \
	if ((bits & (__BIT__)))	{ \
		r = snprintf(esq_global_io_uring_bit_buf + offset, \
				sizeof(esq_global_io_uring_bit_buf) - offset, "%s+", __S__); \
		if (r <= 0 || r >= sizeof(esq_global_io_uring_bit_buf) - offset)	{ \
			return "size-issue"; \
		} \
		else	{ \
			offset += r; \
		} \
	} \
}while(0);

static char esq_global_io_uring_bit_buf[128];

const char *
esq_get_io_uring_bits_str(int bits)	{
	int offset;
	int r;

	offset = 0;

	OUTPUT(POLLIN, "read");
	OUTPUT(POLLOUT, "write");
	OUTPUT(POLLERR, "error");
	OUTPUT(POLLHUP, "close");
	OUTPUT(POLLRDHUP, "sockclose");

	if (offset > 0)	{
		esq_global_io_uring_bit_buf[offset - 1] = '\0';
	}
	else	{
		return "-";
	}

	return esq_global_io_uring_bit_buf;
}

int
esq_os_events_add_socket_fd(int fd, void * user_data, int exp_event_bits)	{
	struct io_uring_sqe * sqe;
	int event_flags;
	void * io_uring;
	int r;

	PRINTF("DBG: %s() %d: fd %d, user_data %p\n", __func__, __LINE__, fd, user_data);

	io_uring = esq_worker_get_io_uring();
	if (!io_uring)	{
		return ESQ_ERROR_PARAM;
	}

	event_flags = POLLFLAGS | POLLRDHUP;

	if ((ESQ_EVENT_BIT_READ & exp_event_bits))	{
		event_flags |= POLLIN;
	}

	if ((ESQ_EVENT_BIT_WRITE & exp_event_bits))	{
		event_flags |= POLLOUT;
	}

	sqe = io_uring_get_sqe(io_uring);
	io_uring_prep_poll_add(sqe, fd, event_flags);
	io_uring_sqe_set_data(sqe, user_data);
	r = io_uring_submit(io_uring);

	if (r <= 0)	{
		PRINTF("DBG: %s() %d: io_uring failed to add fd %d, user_data %p\n"
				, __func__, __LINE__, fd, user_data
				);
	}
	else	{
		PRINTF("DBG: %s() %d: io_uring added fd %d, user_data %p, event flags %s\n"
				, __func__, __LINE__, fd, user_data
				, esq_get_io_uring_bits_str(event_flags)
				);
	}

	return ESQ_OK;
}

int
esq_os_events_add_socket_fd_multishot(int fd, void * user_data, int exp_event_bits)	{
	struct io_uring_sqe * sqe;
	int event_flags;
	void * io_uring;

	PRINTF("DBG: %s() %d: fd %d, user_data %p\n", __func__, __LINE__, fd, user_data);

	io_uring = esq_worker_get_io_uring();
	if (!io_uring)	{
		return ESQ_ERROR_PARAM;
	}

	event_flags = POLLFLAGS | POLLRDHUP;

	if ((ESQ_EVENT_BIT_READ & exp_event_bits))	{
		event_flags |= POLLIN;
	}

	if ((ESQ_EVENT_BIT_WRITE & exp_event_bits))	{
		event_flags |= POLLOUT;
	}

	sqe = io_uring_get_sqe(io_uring);
	io_uring_prep_poll_multishot(sqe, fd, event_flags);
	io_uring_sqe_set_data(sqe, user_data);
	io_uring_submit(io_uring);

	return ESQ_OK;
}

STATIC int
esq_os_events_remove_fd(void * user_data)	{
	struct io_uring_sqe * sqe;
	void * io_uring;
	int r;

	io_uring = esq_worker_get_io_uring();
	if (!io_uring)	{
		return ESQ_ERROR_PARAM;
	}

	sqe = io_uring_get_sqe(io_uring);

	io_uring_prep_poll_remove(sqe, (long long unsigned int)user_data);
	r = io_uring_submit(io_uring);
	if (r <= 0)	{
		printf("DBG: %s() %d: io_uring: failed to remove %p, r = %d\n"
				, __func__, __LINE__, user_data, r
				);
	}
	else	{
		PRINTF("DBG: %s() %d: io_uring: %p removed ok, r = %d\n"
				, __func__, __LINE__, user_data, r
				);
	}

	return ESQ_OK;
}

int
esq_os_events_remove_socket_fd(void * user_data)	{
	PRINTF("DBG: %s() %d: io_uring: %p\n", __func__, __LINE__, user_data);
	return esq_os_events_remove_fd(user_data);
}

/* eof */
