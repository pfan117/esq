/* vi: set sw=4 ts=4: */

#define _GNU_SOURCE /* for POLLRDHUP */

//#define PRINT_DBG_LOG

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

#include "public/esq.h"
#include "public/libesq.h"
#include "internal/types.h"
#include "public/esqll.h"
#include "internal/log_print.h"

#include "internal/lock.h"
#include "internal/signals.h"
#include "internal/session.h"
#include "internal/threads.h"
#include "session/internal.h"
#include "session/core_ctx.h"
#include "internal/os_events.h"

static esq_worker_ctx_t * esq_worker_ctx_list = NULL;
static esq_worker_ctx_t * esq_worker_ctx_rr = NULL;
__thread esq_worker_ctx_t * esq_worker_ctx = NULL;

esq_session_t * volatile esq_global_new_head = NULL;
esq_session_t * volatile esq_global_new_tail = NULL;

esq_worker_ctx_t *
esq_worker_get_ctx(void)	{
	if (esq_worker_ctx)	{
		return esq_worker_ctx;
	}

	SAY_NOT_SUPPOSE_TO_BE_HERE;

	printf("ERROR: %s() %d: this function supposd to be called inside a worker thread (design error)\n"
			, __func__, __LINE__
			);

	esq_backtrace();

	exit(-1);

	return NULL;
}

void *
esq_worker_get_io_uring(void)	{
	if (esq_worker_ctx)	{
		return &esq_worker_ctx->io_uring;
	}

	SAY_NOT_SUPPOSE_TO_BE_HERE;

	printf("ERROR: %s() %d: this function supposd to be called inside a worker thread (design error)\n"
			, __func__, __LINE__
			);

	esq_backtrace();

	exit(-1);

	return NULL;
}

#define ENUM_SESSION_LIST(__L__,__F__)	do	{ \
	esq_session_t * session; \
	for(session = ctx->__L__; session; session = session->next)	{ \
		__F__(ctx, session); \
	} \
}while(0);

#define ENUM_SESSION_IN_CTX(__F__)	do	{\
	ENUM_SESSION_LIST(list_of_active_head, __F__);\
	ENUM_SESSION_LIST(list_of_0ms_to_100ms_head, __F__);\
	ENUM_SESSION_LIST(list_of_100ms_to_1s_head, __F__);\
	ENUM_SESSION_LIST(list_of_1s_to_10s_head, __F__);\
	ENUM_SESSION_LIST(list_of_longer_than_10s_head, __F__);\
	ENUM_SESSION_LIST(list_of_no_time_head, __F__);\
}while(0);

void
esq_print_session_name(esq_worker_ctx_t * ctx, esq_session_t * session)	{
	printf("pid %d, core id %d, session %s, state %s\n", getpid(), ctx->core_id, session->name, session->state);
}

void
esq_print_session_summary(void)	{
	esq_worker_ctx_t * ctx = esq_worker_ctx;

	ENUM_SESSION_IN_CTX(esq_print_session_name);

	return;
}

int
esq_get_worker_list_length(void)	{
	esq_worker_ctx_t * ctx;
	int i = 0;

	esq_global_lock();

	for(ctx = esq_worker_ctx_list; ctx; ctx = ctx->next)	{
		i ++;
	}

	esq_global_unlock();

	return i;
}

#define NOTIFY_EXIT_TO_SESSION_LIST(__head__)	\
	for (p = __head__; p; p = n)	{	\
		n = p->next; \
		__esq_notify_exit(p);	\
	}

STATIC void
esq_worker_notify_exit_to_all_sessions(esq_worker_ctx_t * worker_ctx)	{
	esq_session_t * p;
	esq_session_t * n;

	NOTIFY_EXIT_TO_SESSION_LIST(worker_ctx->list_of_active_head);
	NOTIFY_EXIT_TO_SESSION_LIST(worker_ctx->list_of_0ms_to_100ms_head);
	NOTIFY_EXIT_TO_SESSION_LIST(worker_ctx->list_of_100ms_to_1s_head);
	NOTIFY_EXIT_TO_SESSION_LIST(worker_ctx->list_of_1s_to_10s_head);
	NOTIFY_EXIT_TO_SESSION_LIST(worker_ctx->list_of_longer_than_10s_head);
	NOTIFY_EXIT_TO_SESSION_LIST(worker_ctx->list_of_no_time_head);

	return;
}

void
esq_session_notify_exit_to_all_sessions()	{
	esq_worker_ctx_t * ctx;

	esq_global_lock();
	for (ctx = esq_worker_ctx_list; ctx; ctx = ctx->next)	{
		ctx->esq_worker_stop_session_flag = 1;
	}
	esq_global_unlock();

	esq_wake_up_all_worker_thread();

	return;
}

STATIC int
esq_worker_ctx_init(esq_worker_ctx_t * ctx)	{
	int r;

	if (esq_worker_ctx)	{
		SAY_NOT_SUPPOSE_TO_BE_HERE;
		return -1;
	}

	bzero(ctx, sizeof(esq_worker_ctx_t));
	ctx->core_id = os_get_current_core_id_slow();
	ctx->esq_os_event_wait_max = -1;
	ctx->pthread = pthread_self();
	esq_worker_ctx = ctx;

	esq_global_lock();
	ctx->next = esq_worker_ctx_list;
	esq_worker_ctx_list = ctx;
	esq_global_unlock();

	r = io_uring_queue_init(1024, &ctx->io_uring, 0);
	if (r < 0)	{
		printf("ERROR: %s() %d: failed to init polling object\n", __func__, __LINE__);
		return -1;
	}

	return 0;
}

STATIC void
esq_worker_ctx_detach(esq_worker_ctx_t * ctx)	{
	esq_worker_ctx_t * pp;
	esq_worker_ctx_t * p;

	io_uring_queue_exit(&ctx->io_uring);

	pp = NULL;

	/* ctx left link list */

	esq_global_lock();

	esq_worker_ctx_rr = NULL;

	for (p = esq_worker_ctx_list; p; p = p->next)	{
		if (p == ctx)	{
			if (pp)	{
				pp->next = p->next;
			}
			else	{
				esq_worker_ctx_list = p->next;
			}
			esq_global_unlock();
			return;
		}
		else	{
			pp = p;
			continue;
		}
	}

	esq_global_unlock();

	printf("ERROR: %s() %d: design error\n", __func__, __LINE__);

	return;
}

void
esq_wake_up_all_worker_thread(void)	{
	esq_worker_ctx_t * ctx;

	esq_global_lock();

	for (ctx = esq_worker_ctx_list; ctx; ctx = ctx->next)	{
		pthread_kill(ctx->pthread, SIGUSR1);
	}

	esq_global_unlock();

	return;
}

void
esq_worker_show_rr_status(void)	{
	int r;
	int offset;
	char buf[128];
	esq_worker_ctx_t * ctx;

	offset = 0;

	r = snprintf(buf + offset, sizeof(buf) - offset
			, "DBG: %s() %d: [ ", __func__, __LINE__
			);
	if ((r <= 0) || (r >= (sizeof(buf) - offset)))	{
		goto __exx;
	}

	offset += r;

	for (ctx = esq_worker_ctx_list; ctx; ctx = ctx->next)	{
		r = snprintf(buf + offset, sizeof(buf) - offset, " %d", ctx->core_id);
		if ((r <= 0) || (r >= (sizeof(buf) - offset)))	{
			goto __exx;
		}
		offset += r;
	}

	if (esq_worker_ctx_rr)	{
		r = snprintf(buf + offset, sizeof(buf) - offset
				, " ], after ctx->core_id = %d\n", esq_worker_ctx_rr->core_id);
	}
	else	{
		r = snprintf(buf + offset, sizeof(buf) - offset, " ], after ctx->core_id = NULL\n");
	}

	if ((r <= 0) || (r >= (sizeof(buf) - offset)))	{
		goto __exx;
	}
	offset += r;

	printf("%s", buf);

	return;

__exx:

	printf("ERROR: %s() %d: buffer issue\n", __func__, __LINE__);

	return;
}

void
esq_wake_up_worker_thread_rr(void)	{

	esq_worker_ctx_t * ctx;

	esq_global_lock();

	if (!esq_worker_ctx_rr)	{
		esq_worker_ctx_rr = esq_worker_ctx_list;
	}

	if (esq_worker_ctx_rr)	{
		ctx = esq_worker_ctx_rr;
		esq_worker_ctx_rr = ctx->next;
		pthread_kill(ctx->pthread, SIGUSR1);

		#if 0
		esq_worker_show_rr_status();
		#endif

		esq_global_unlock();
	}
	else	{
		esq_global_unlock();
		printf("ERROR %s() %d: not suppose to be here\n", __func__, __LINE__);
		esq_backtrace();
		return;
	}

	return;
}

STATIC esq_session_t *
esq_session_pop_the_first_active(esq_worker_ctx_t * ctx)	{
	esq_session_pt_action_t action_buf;	/* can be modified by callee function */
	esq_session_t * p;
	int r;

	for (p = ctx->list_of_active_head; p; p = p->next)
	{
		action_buf.type = ACTION_EXECUTE_CB;

		r = __esq_session_take_action(p, &action_buf);
		if (r)	{
			printf("ERROR: %s() %d: session refused to be executed\n", __func__, __LINE__);
		}
		else	{
			break;
		}
	}

	return p;
}

void
esq_session_join_active_list(esq_session_t * session)	{
	esq_worker_ctx_t * ctx;

	ctx = esq_worker_get_ctx();
	if (!ctx)	{
		return;
	}

	PRINTF("DBG: %s() %d: session %p join active list\n", __func__, __LINE__, session);

	DOUBLE_LL_JOIN_TAIL(
			ctx->list_of_active_head, ctx->list_of_active_tail,
			prev, next, session
			);

	return;
}

void
esq_session_left_active_list(esq_session_t * session)	{
	esq_worker_ctx_t * ctx;

	ctx = esq_worker_get_ctx();
	if (!ctx)	{
		return;
	}

	PRINTF("DBG: %s() %d: session %p left active list\n", __func__, __LINE__, session);

	DOUBLE_LL_LEFT(
			ctx->list_of_active_head, ctx->list_of_active_tail,
			prev, next, session
			);

	return;
}

#define PRINT_CURRENT_CORE_ID	do	{ \
	{ \
		int core_id; \
		sleep(1); \
		core_id = os_get_current_core_id_slow(); \
		if (core_id > 0)	{ \
			printf("INFO: %s() %d: running on processor %d\n", __func__, __LINE__, core_id); \
		} \
	} \
}while(0);

void
esq_worker_receive_fd_event(esq_worker_ctx_t * ctx, struct io_uring_cqe * cqe)	{
	esq_session_t * session;
	int io_uring_flags;
	int flags;

	session = io_uring_cqe_get_data(cqe);

	if (!session)	{
		io_uring_cqe_seen(&ctx->io_uring, cqe);
		PRINTF("DBG: %s() %d: drop notify for NULL session\n", __func__, __LINE__);
		return;
	}

	if (-ECANCELED == cqe->res)	{
		esq_notify_aio_removed(session);
		io_uring_cqe_seen(&ctx->io_uring, cqe);
		PRINTF("DBG: %s() %d: ECANDELED notify for %p\n", __func__, __LINE__, session);
		return;
	}
	else if (cqe->res < 0)	{
		io_uring_cqe_seen(&ctx->io_uring, cqe);
		printf("DBG: %s() %d: yes, but why?\n", __func__, __LINE__);
		return;
	}

	io_uring_flags = cqe->res;
	io_uring_cqe_seen(&ctx->io_uring, cqe);

	PRINTF("DBG: %s() %d: io_uring report \"%s\" to session %p\n"
			, __func__, __LINE__
			, esq_get_io_uring_bits_str(io_uring_flags), session
			);

	flags = 0;

	if ((io_uring_flags & POLLERR))	{
		flags |= ESQ_EVENT_BIT_ERROR;
	}

	if ((io_uring_flags & POLLHUP))	{
		flags |= ESQ_EVENT_BIT_FD_CLOSE;
	}

	if ((io_uring_flags & POLLRDHUP))	{
		flags |= ESQ_EVENT_BIT_FD_CLOSE;
	}

	if ((io_uring_flags & POLLIN))	{
		flags |= ESQ_EVENT_BIT_READ;
	}

	if ((io_uring_flags & POLLOUT))	{
		flags |= ESQ_EVENT_BIT_WRITE;
	}

	if (!flags)	{
		PRINTF("DBG: %s() %d: ignore 0 event flags\n", __func__, __LINE__);
		return;
	}

	ESQ_CHECK_SESSION_VOID(session);

	esq_notify_event_to_session(session, flags);

	return;
}

void
esq_join_local_new_list(esq_session_t * session)	{

	session->prev = NULL;
	session->next = NULL;

	if (esq_worker_ctx->list_of_new_tail)	{
		esq_worker_ctx->list_of_new_tail->next = session;
		esq_worker_ctx->list_of_new_tail = session;
	}
	else	{
		esq_worker_ctx->list_of_new_head = session;
		esq_worker_ctx->list_of_new_tail = session;
	}
}

void
esq_join_global_new_list(esq_session_t * session)	{

	session->prev = NULL;
	session->next = NULL;

	esq_global_lock();

	if (esq_global_new_tail)	{
		esq_global_new_tail->next = session;
		esq_global_new_tail = session;
	}
	else	{
		esq_global_new_head = session;
		esq_global_new_tail = session;
	}

	esq_global_unlock();
}

STATIC int
esq_worker_pick_up_from_local_list(esq_worker_ctx_t * worker_ctx)	{
	esq_session_t * session;

	if (worker_ctx->list_of_new_head)	{

		session = worker_ctx->list_of_new_head;

		if (session->next)	{
			worker_ctx->list_of_new_head = session->next;
		}
		else	{
			worker_ctx->list_of_new_head = worker_ctx->list_of_new_tail = NULL;
		}

		esq_session_pick_up(session);

		return 1;

	}
	else	{
		return 0;
	}
}

STATIC int
esq_worker_pick_up_from_local_wake_up_list(esq_worker_ctx_t * worker_ctx)	{
	esq_hash_lock_ctx_t * ctx;
	esq_session_t * session;

	if (!worker_ctx->hash_lock_wake_up_list_head)	{
		return 0;
	}

	esq_global_lock();

	ctx = worker_ctx->hash_lock_wake_up_list_head;

	if (ctx->next_in_local_wake_up_list)	{
		worker_ctx->hash_lock_wake_up_list_head = ctx->next_in_local_wake_up_list;
	}
	else	{
		worker_ctx->hash_lock_wake_up_list_head = NULL;
		worker_ctx->hash_lock_wake_up_list_tail = NULL;
	}

	if (ctx->worker_ctx != worker_ctx)	{
		esq_global_unlock();
		esq_design_error();
		return 0;
	}

	session = ctx->session;

	esq_global_unlock();

	if (esq_get_server_stop_flag())	{
		session->event_bits |= ESQ_EVENT_BIT_EXIT;
	}
	else	{
		session->event_bits |= ESQ_EVENT_BIT_LOCK_OBTAIN;
	}

	esq_session_pick_up(session);

	return 1;

}

STATIC int
esq_worker_pick_up_from_global_list(esq_worker_ctx_t * worker_ctx)	{
	esq_session_t * session;
	int r = 0;

	if (!esq_global_new_head)	{
		return 0;
	}

	for (;;)	{
		esq_global_lock();

		if (esq_global_new_head)	{
			session = esq_global_new_head;
			if (esq_global_new_head->next)	{
				esq_global_new_head = esq_global_new_head->next;
			}
			else	{
				esq_global_new_head = esq_global_new_tail = NULL;
			}
			esq_global_unlock();

			esq_session_pick_up(session);

			r = 1;
		}
		else	{
			esq_global_unlock();
			break;
		}
	}

	return r;
}

STATIC void
esq_worker_execute_session(esq_worker_ctx_t * ctx, esq_session_t * session)
{
	void * mco;

	PRINTF("DBG: %s() %d: will execute session 0x%p <%s>\n", __func__, __LINE__
			, session, session->name
			);

	mco = session->mco;
	if (mco)	{
		PRINTF("DBG: %s() %d: execute mco\n", __func__, __LINE__);
		mco_resume(mco);
		if (mco_check_stack(mco))	{
			esq_design_error();
		}
		esq_session_wind_up(session);
	}
	else if (session->cb)	{
		PRINTF("DBG: %s() %d: execute cb\n", __func__, __LINE__);
		session->cb(session);
		esq_session_wind_up(session);
	}
	else	{
		printf("ERROR: session has no mco or cb %p\n", session);
		esq_design_error();
	}

	return;
}

int
esq_worker_still_work_to_do(esq_worker_ctx_t * worker_ctx)	{
	if (worker_ctx->list_of_0ms_to_100ms_head)	{
		PRINTF("DBG: %s() %d\n", __func__, __LINE__);
		return 1;
	}
	else if (worker_ctx->list_of_100ms_to_1s_head)	{
		PRINTF("DBG: %s() %d\n", __func__, __LINE__);
		return 1;
	}
	else if (worker_ctx->list_of_1s_to_10s_head)	{
		PRINTF("DBG: %s() %d\n", __func__, __LINE__);
		return 1;
	}
	else if (worker_ctx->list_of_longer_than_10s_head)	{
		PRINTF("DBG: %s() %d\n", __func__, __LINE__);
		return 1;
	}
	else if (worker_ctx->list_of_no_time_head)	{
		PRINTF("DBG: %s() %d\n", __func__, __LINE__);
		return 1;
	}
	else if (worker_ctx->list_of_active_head)	{
		PRINTF("DBG: %s() %d\n", __func__, __LINE__);
		return 1;
	}
	else if (esq_global_new_head)	{
		PRINTF("DBG: %s() %d\n", __func__, __LINE__);
		return 1;
	}

	return 0;
}

void
esq_execute_session_done_cb(void * session_ptr)	{
	esq_session_t * session = session_ptr;
	esq_session_t * p;
	esq_session_t * n;

	if (esq_session_check_magic(session))	{
		esq_design_error();
		return;
	}

	for (p = session->finished_child_session_list; p; p = n)	{
		if (esq_session_check_magic(p))	{
			esq_design_error();
			return;
		}

		n = p->next_child;

		if (p->session_done_cb)	{
			p->session_done_cb(p);
		}

		free(p);
	}

	session->finished_child_session_list = NULL;
}

void *
esq_worker_loop(void * dummy)	{
	esq_worker_ctx_t worker_ctx;
	esq_session_t * session;
	int r;

	struct __kernel_timespec kt;
	struct io_uring_cqe * cqe;

	int polling_interval = -1;

	#if 0
	PRINT_CURRENT_CORE_ID;
	#endif

	mco_thread_init();

	r = esq_worker_thread_mask_signals();
	if (r)	{
		printf("ERROR: %s, %d: %s\n", __func__, __LINE__, "failed to mask signals in worker thread");
		return NULL;
	}

	r = esq_install_notify_signal_handler();
	if (r)	{
		return NULL;
	}

	r = esq_worker_ctx_init(&worker_ctx);
	if (r)	{
		return NULL;
	}

	r = esq_worker_thread_open_signals();
	if (r)	{
		return NULL;
	}

	esq_wake_up_master_thread();	/* wake up master thread to check worker thread number */

	for(;;)	{

		session = esq_session_pop_the_first_active(&worker_ctx);
		if (session)	{
			esq_worker_execute_session(&worker_ctx, session);
			continue;
		}

		if (esq_worker_pick_up_from_global_list(&worker_ctx))	{
			continue;
		}

		if (esq_worker_pick_up_from_local_list(&worker_ctx))	{
			continue;
		}

		if (esq_worker_pick_up_from_local_wake_up_list(&worker_ctx))	{
			continue;
		}

		if (worker_ctx.esq_worker_stop_session_flag)	{
			worker_ctx.esq_worker_stop_session_flag = 0;
			esq_worker_notify_exit_to_all_sessions(&worker_ctx);
			continue;
		}

		polling_interval = worker_ctx.esq_os_event_wait_max;

		if (polling_interval <= 0)	{
			PRINTF("DBG: %s() %d, polling without interval (core_id = %d)\n", __func__, __LINE__
					, esq_worker_ctx->core_id);
			r = io_uring_wait_cqe(&worker_ctx.io_uring, &cqe);
			PRINTF("DBG: %s() %d, after polling without interval (core_id = %d)\n", __func__, __LINE__
					, esq_worker_ctx->core_id);
		}
		else	{
			kt.tv_sec = 0;
			kt.tv_nsec = polling_interval * 1000 * 1000;
			PRINTF("DBG: %s() %d, polling with interval = %d (core_id = %d)\n", __func__, __LINE__
					, polling_interval, esq_worker_ctx->core_id);
			r = io_uring_wait_cqe_timeout(&worker_ctx.io_uring, &cqe, &kt);
			PRINTF("DBG: %s() %d, after polling with interval = %d (core_id = %d)\n", __func__, __LINE__
					, polling_interval, esq_worker_ctx->core_id);
		}

		if (r >= 0)	{
			esq_worker_receive_fd_event(&worker_ctx, cqe);
			esq_session_update_polling_interval(&worker_ctx);
		}
		else	{
			if (-EINTR == r)	{
				PRINTF("DBG: %s() %d: signal received\n", __func__, __LINE__);
			}
			else if (-ETIME == r)	{
				PRINTF("DBG: %s() %d: timing event received\n", __func__, __LINE__);
				esq_session_timing(&worker_ctx);
				esq_session_update_polling_interval(&worker_ctx);
			}
			else	{
				printf("ERROR: %s() %d: io_uring_wait_cqe() return unhandled value %d\n"
						, __func__, __LINE__, r);
			}
		}

		if (esq_get_server_stop_flag())	{
			if (esq_worker_still_work_to_do(&worker_ctx))	{
				#if defined PRINT_DBG_LOG
				esq_print_session_summary();
				#endif
				esq_worker_notify_exit_to_all_sessions(&worker_ctx);
			}
			else	{
				PRINTF("DBG: %s() %d: no session in thread\n", __func__, __LINE__);
				break;	/* bye */
			}
		}
	}

	PRINTF("DBG: %s() %d: quit (core_id = %d)\n", __func__, __LINE__, esq_worker_ctx->core_id);

	esq_worker_ctx_detach(&worker_ctx);

	esq_wake_up_master_thread();

	return NULL;
}

/* eof */
