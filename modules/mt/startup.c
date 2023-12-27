#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "public/esq.h"
#include "public/esqll.h"
#include "internal/types.h"
#include "internal/threads.h"
#include "modules/mt/internal.h"

static pthread_t esq_mt_thread;
pthread_spinlock_t esq_mod_test_spinlock;

#if 0
void
esq_tst_print_counters(esq_counters_t * counters)	{
	printf("counters->events = %d\n"
"counters->events_0ms_to_100ms = %d\n"
"counters->events_100ms_to_1s = %d\n"
"counters->events_1s_to_10s = %d\n"
"counters->events_longer_than_10s = %d\n"
"counters->master_thread_polling_period = %d\n"
"counters->session_active = %d\n"
"counters->session_wait = %d\n",
			counters->events,
			counters->events_0ms_to_100ms,
			counters->events_100ms_to_1s,
			counters->events_1s_to_10s,
			counters->events_longer_than_10s,
			counters->master_thread_polling_period,
			counters->session_active,
			counters->session_wait
			);
}
#endif

#define CHILD_SESSION_DONE_TEST
#define SINGLE_SHOT_SOCKET_SESSION_TEST
#define MULTI_SHOT_SOCKET_SESSION_TEST
#define HASH_LOCK_SESSION_TEST

STATIC void *
esq_mt(void * dummy)	{
	int r;

	(void)r;

	range_map_test_0();

#if defined SINGLE_SHOT_SOCKET_SESSION_TEST && 1
	void * session0;
	void * session1;
	void * session2;

	/* create and free empty sessions */
	session0 = esq_session_new("empty0");
	session1 = esq_session_new("empty1");
	session2 = esq_session_new("empty2");

	if (session0 && session1 && session2)	{
		/* no need to esq_session_wind_up_rr, just free */
		esq_session_wind_up(session0);
		esq_session_wind_up(session1);
		esq_session_wind_up(session2);
	}
	else	{
		printf("ERROR: failed to create empty sessions\n");
		goto __exx;
	}
#endif

#if defined CHILD_SESSION_DONE_TEST && 1
	r = esq_mt_single_shot_socket_pt_handle_task_done_event(200, 1, 1);
	if (r)	{
		goto __exx;
	}
#endif

#if defined CHILD_SESSION_DONE_TEST && 1
	r = esq_mt_single_shot_socket_pt_handle_task_done_event(200, 200, 1);
	if (r)	{
		goto __exx;
	}
#endif

#if defined CHILD_SESSION_DONE_TEST && 1
	r = esq_mt_single_shot_socket_pt_handle_task_done_event(200, 1, 0);
	if (r)	{
		goto __exx;
	}
#endif

#if defined CHILD_SESSION_DONE_TEST && 1
	r = esq_mt_single_shot_socket_pt_handle_task_done_event(200, 200, 0);
	if (r)	{
		goto __exx;
	}
#endif

#if defined SINGLE_SHOT_SOCKET_SESSION_TEST && 1
	r = esq_mt_single_shot_socket_pt_handle_timeout_event(1, 100, 200);
	if (r)	{
		goto __exx;
	}
#endif

#if defined SINGLE_SHOT_SOCKET_SESSION_TEST && 1
	r = esq_mt_single_shot_socket_pt_handle_timeout_event(100000, 100, 200);
	if (r)	{
		goto __exx;
	}
#endif

#if defined SINGLE_SHOT_SOCKET_SESSION_TEST && 1
	r = esq_mt_single_shot_socket_pt_handle_exit_event(1, 200);
	if (r)	{
		goto __exx;
	}
#endif

#if defined SINGLE_SHOT_SOCKET_SESSION_TEST && 1
	r = esq_mt_single_shot_socket_pt_handle_exit_event(100000, 200);
	if (r)	{
		goto __exx;
	}
#endif

#if defined MULTI_SHOT_SOCKET_SESSION_TEST && 1
	r = esq_mt_multi_shot_socket_pt_handle_exit_event(1, 200);
	if (r)	{
		goto __exx;
	}
#endif

#if defined MULTI_SHOT_SOCKET_SESSION_TEST && 1
	r = esq_mt_multi_shot_socket_pt_handle_exit_event(100000, 200);
	if (r)	{
		goto __exx;
	}
#endif

#if defined HASH_LOCK_SESSION_TEST && 1
	r = esq_mt_hash_lock_sessions();
	if (r)	{
		goto __exx;
	}
#endif

	printf("SUCCESS\n");
	printf("\n");

	esq_stop_server_on_purpose();
	
	return NULL;

__exx:

	printf("FAILED\n");
	printf("\n");

	esq_stop_server_on_purpose();

	return NULL;
}

void
esq_mt_start(void)	{
	int r;

	r = pthread_create(&esq_mt_thread, NULL, esq_mt, NULL);
	if (r)	{
		fprintf(stderr, "error: %s %d, %s: failed to create test thread\n", __FILE__, __LINE__, __func__);
	}

	return;
}

int
__esq_mod_init(void)	{
	pthread_spin_init(&esq_mod_test_spinlock, 0);
	return ESQ_OK;
}

void
__esq_mod_detach(void)	{
	pthread_join(esq_mt_thread, NULL);
	pthread_spin_destroy(&esq_mod_test_spinlock);
	return;
}

/* eof */
