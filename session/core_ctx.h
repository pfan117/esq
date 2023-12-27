/* vi: set sw=4 ts=4: */

#ifndef __ESQ_SESSION_CORE_CTX_HEADER_INCLUDED__
#define __ESQ_SESSION_CORE_CTX_HEADER_INCLUDED__

#include <poll.h>
#include <liburing.h>
#include <pthread.h>

typedef struct _esq_worker_ctx_t	{
	struct _esq_worker_ctx_t * next;
	int core_id;
	int esq_os_event_wait_max;
	int esq_worker_stop_session_flag;
	struct io_uring io_uring;
	pthread_t pthread;

	esq_session_t * list_of_new_head;
	esq_session_t * list_of_new_tail;

	esq_session_t * list_of_active_head;
	esq_session_t * list_of_active_tail;

	esq_session_t * list_of_0ms_to_100ms_head;
	esq_session_t * list_of_0ms_to_100ms_tail;
	esq_session_t * list_of_100ms_to_1s_head;
	esq_session_t * list_of_100ms_to_1s_tail;
	esq_session_t * list_of_1s_to_10s_head;
	esq_session_t * list_of_1s_to_10s_tail;
	esq_session_t * list_of_longer_than_10s_head;
	esq_session_t * list_of_longer_than_10s_tail;
	esq_session_t * list_of_no_time_head;
	esq_session_t * list_of_no_time_tail;

	esq_hash_lock_ctx_t * hash_lock_wake_up_list_head;
	esq_hash_lock_ctx_t * hash_lock_wake_up_list_tail;
} esq_worker_ctx_t;

extern void esq_session_timing(esq_worker_ctx_t * ctx);
extern void esq_session_update_polling_interval(esq_worker_ctx_t * ctx);
extern esq_worker_ctx_t * esq_worker_get_ctx(void);

#endif
