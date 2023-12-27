/* vi: set sw=4 ts=4: */

#define _GNU_SOURCE /* for POLLRDHUP */

//#define PRINT_DBG_LOG

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>

#include "public/esq.h"
#include "public/libesq.h"
#include "internal/types.h"
#include "public/esqll.h"
#include "internal/log_print.h"

#include "internal/lock.h"
#include "internal/session.h"
#include "internal/threads.h"
#include "session/internal.h"
#include "session/core_ctx.h"

volatile int esq_os_event_wait_max = -1;

#define RECORD_WIND_UP_TIME	do	{ \
	r = gettimeofday(&time_now, NULL); \
	if (r)	{ \
		PRINTF("ERROR: %s() %d: gettimeofday() return %d, errno is %d\n", \
				__func__, __LINE__, \
				r, errno \
				); \
		esq_stop_server_on_purpose(); \
	} \
	else	{ \
		session->wind_up_s = time_now.tv_sec; \
		session->wind_up_ms = time_now.tv_usec; \
	} \
}while(0);

void
esq_session_join_timing_list(esq_session_t * session)	{
	esq_worker_ctx_t * ctx;
	struct timeval time_now;
	int s, ms;
	int errno;
	int r;

	ctx = esq_worker_get_ctx();
	if (!ctx)	{
		return;
	}

	s = session->timeouts;
	ms = session->timeoutms;

	if (s >= 10)	{
		DOUBLE_LL_JOIN_HEAD(
				ctx->list_of_longer_than_10s_head, ctx->list_of_longer_than_10s_tail,
				prev, next, session
				);
		RECORD_WIND_UP_TIME
	}
	else if (s >= 1)	{
		DOUBLE_LL_JOIN_HEAD(
				ctx->list_of_1s_to_10s_head, ctx->list_of_1s_to_10s_tail,
				prev, next, session
				);
		RECORD_WIND_UP_TIME
	}
	else if (ms >= 100)	{
		DOUBLE_LL_JOIN_HEAD(
				ctx->list_of_100ms_to_1s_head, ctx->list_of_100ms_to_1s_tail,
				prev, next, session
				);
		RECORD_WIND_UP_TIME
	}
	else if (ms > 0)	{
		DOUBLE_LL_JOIN_HEAD(
				ctx->list_of_0ms_to_100ms_head, ctx->list_of_0ms_to_100ms_tail,
				prev, next, session
				);
		RECORD_WIND_UP_TIME
	}
	else	{
		DOUBLE_LL_JOIN_HEAD(
				ctx->list_of_no_time_head, ctx->list_of_no_time_tail,
				prev, next, session
				);
		RECORD_WIND_UP_TIME
	}

	return;
}

void
esq_worker_ctx_show_ptr(esq_worker_ctx_t * ctx)	{
	return;
}

void
esq_session_left_timing_list(esq_session_t * session)	{
	esq_worker_ctx_t * ctx;
	int s, ms;

	ctx = esq_worker_get_ctx();
	if (!ctx)	{
		return;
	}

	s = session->timeouts;
	ms = session->timeoutms;

	if (s >= 10)	{
		DOUBLE_LL_LEFT(
				ctx->list_of_longer_than_10s_head, ctx->list_of_longer_than_10s_tail,
				prev, next, session
				);
	}
	else if (s >= 1)	{
		DOUBLE_LL_LEFT(
				ctx->list_of_1s_to_10s_head, ctx->list_of_1s_to_10s_tail,
				prev, next, session
				);
	}
	else if (ms >= 100)	{
		DOUBLE_LL_LEFT(
				ctx->list_of_100ms_to_1s_head, ctx->list_of_100ms_to_1s_tail,
				prev, next, session
				);
	}
	else if (ms > 0)	{
		DOUBLE_LL_LEFT(
				ctx->list_of_0ms_to_100ms_head, ctx->list_of_0ms_to_100ms_tail,
				prev, next, session
				);
	}
	else	{
		DOUBLE_LL_LEFT(
				ctx->list_of_no_time_head, ctx->list_of_no_time_tail,
				prev, next, session
				);
	}

	return;
}

void
esq_session_change_to_no_timing_list(esq_session_t * session)	{
	if (session->timeouts || session->timeoutms)	{
		esq_session_left_timing_list(session);
		session->timeouts = 0;
		session->timeoutms = 0;
		esq_session_join_timing_list(session);
	}
}

#define CHECK_EVENTS_TIMING2(__head__,__tail__)	do	{	\
	for (p = ctx->__head__; p; p = t_next)	{	\
		t_next = p->next;	\
		e_deadline_sec = p->wind_up_s + p->timeouts;	\
		if (now_sec > e_deadline_sec)	{	\
			;	\
		}	\
		else if (now_sec == e_deadline_sec)	{	\
			if (now_ms > (p->wind_up_ms + p->timeoutms))	{	\
				;	\
			}	\
			else	{	\
				continue;	\
			}	\
		}	\
		else	{	\
			continue;	\
		}	\
		__esq_notify_timeout_event(p); \
		there_is_timeout_event = 1;	\
	}	\
} while(0);

void
esq_session_update_polling_interval(esq_worker_ctx_t * ctx)	{
	int suggested_time_interval;

	if (ctx->list_of_0ms_to_100ms_head)	{
		suggested_time_interval = 5;
	}
	else if (ctx->list_of_100ms_to_1s_head)	{
		suggested_time_interval = 50;
	}
	else if (ctx->list_of_1s_to_10s_head)	{
		suggested_time_interval = 500;
	}
	else if (ctx->list_of_longer_than_10s_head)	{
		suggested_time_interval = 5000;
	}
	else	{
		suggested_time_interval = -1;
	}

	ctx->esq_os_event_wait_max = suggested_time_interval;

	return;
}

void
esq_update_polling_interval(void)	{
	esq_worker_ctx_t * ctx;

	ctx = esq_worker_get_ctx();
	if (!ctx)	{
		SAY_NOT_SUPPOSE_TO_BE_HERE;
		esq_backtrace();
		return;
	}

	esq_session_update_polling_interval(ctx);
}

void
esq_session_timing(esq_worker_ctx_t * ctx)	{
	int there_is_timeout_event;
	struct timeval time_now;
	int now_sec, now_ms;
	int e_deadline_sec;
	int r;

	esq_session_t * t_next;
	esq_session_t * p;

	r = gettimeofday(&time_now, NULL);
	if (r)	{
		PRINTF("ERROR: %s() %d: gettimeofday() return %d, errno is %d\n",
				__func__, __LINE__,
				r, errno
				);
		esq_stop_server_on_purpose();
		return;
	}

	there_is_timeout_event = 0;
	now_sec = time_now.tv_sec;
	now_ms = time_now.tv_usec;

	CHECK_EVENTS_TIMING2(list_of_0ms_to_100ms_head, list_of_0ms_to_100ms_tail);
	CHECK_EVENTS_TIMING2(list_of_100ms_to_1s_head, list_of_100ms_to_1s_tail);
	CHECK_EVENTS_TIMING2(list_of_1s_to_10s_head, list_of_1s_to_10s_tail);
	CHECK_EVENTS_TIMING2(list_of_longer_than_10s_head, list_of_longer_than_10s_tail);

	if (there_is_timeout_event)	{
		esq_session_update_polling_interval(ctx);
	}

	return;
}

/* eof */
