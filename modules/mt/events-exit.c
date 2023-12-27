#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "public/esq.h"
#include "public/esqll.h"
#include "internal/types.h"
#include "internal/threads.h"
#include "internal/session.h"
#include "modules/mt/internal.h"

#define ESQ_TST_PARAM		((void *)0x1234abcd)

volatile int esq_mt_handled_exit_events = 0;

void
esq_mt_exit_event_cb(void * current_session)	{
	int events;
	void * event_param;

	events = esq_session_get_event_bits(current_session);
	event_param = esq_session_get_param(current_session);

	if ((ESQ_EVENT_BIT_EXIT == events))	{
		pthread_spin_lock(&esq_mod_test_spinlock);
		esq_mt_handled_exit_events ++;
		pthread_spin_unlock(&esq_mod_test_spinlock);
		if (event_param != ESQ_TST_PARAM)	{
			printf("ERROR: %s() %d: event returned wrong parameter %p, should be %p\n"
					, __func__, __LINE__
					, event_param, ESQ_TST_PARAM
					);
		}
	}
	else	{
		printf("ERROR: %s() %d: event bit 0x%08x is not EXIT\n", __func__, __LINE__, events);
	}

	return;
}

void
esq_mt_exit_multi_event_cb(void * current_session)	{
	int events;
	void * event_param;

	events = esq_session_get_event_bits(current_session);
	event_param = esq_session_get_param(current_session);

	if ((ESQ_EVENT_BIT_EXIT == events))	{
		pthread_spin_lock(&esq_mod_test_spinlock);
		esq_mt_handled_exit_events ++;
		pthread_spin_unlock(&esq_mod_test_spinlock);
		if (event_param != ESQ_TST_PARAM)	{
			printf("ERROR: %s() %d: event returned wrong parameter %p, should be %p\n"
					, __func__, __LINE__
					, event_param, ESQ_TST_PARAM
					);
		}
		esq_session_remove_req(current_session);
	}
	else	{
		printf("ERROR: %s() %d: event bit 0x%08x is not EXIT\n", __func__, __LINE__, events);
	}

	return;
}

int
esq_mt_single_shot_socket_pt_handle_exit_event(int session_cnt, int wait_usecond)	{
	void * session;
	int r;
	int i;
	int fd = -100;

	printf("INFO: %s(): create %d sessions\n", __func__, session_cnt);

	esq_mt_handled_exit_events = 0;

	for (i = 0; i < session_cnt; i ++)	{

		session = esq_session_new("mt-exit-session");
		if (!session)	{
			printf("ERROR: %s() %d: failed to create new session, i = %d\n",
					__func__, __LINE__, i
					);
			return -1;
		}

		r = esq_session_set_socket_fd(session, fd, esq_mt_exit_event_cb
				, (ESQ_EVENT_BIT_READ | ESQ_EVENT_BIT_WRITE)
				, 0, 0, ESQ_TST_PARAM
				);
		if (r)	{
			printf("ERROR: %s() %d: failed to add test fd %d into session\n",
					__func__, __LINE__, fd
					);
			return -1;
		}

		fd --;

		esq_session_wind_up_rr(session);
	}

	esq_session_notify_exit_to_all_sessions();

	printf("INFO: all dispatched, sleep for %d usecond(s)\n", wait_usecond);
	usleep(wait_usecond * 1000);

	#if 0
	esq_counters_t counters;
	esq_get_counters(&counters);
	esq_tst_print_counters(&counters);
	#endif

	if (esq_mt_handled_exit_events == session_cnt)	{
		printf("INFO: case passed\n");
		return 0;
	}
	else	{
		printf("ERROR: %s() %d: %d event(s) reported, not %d\n",
				__func__, __LINE__,
				esq_mt_handled_exit_events, session_cnt
				);
		return -1;
	}
}

int
esq_mt_multi_shot_socket_pt_handle_exit_event(int session_cnt, int wait_usecond)	{
	void * session;
	int r;
	int i;
	int fd = -100;

	printf("INFO: %s(): create %d sessions\n", __func__, session_cnt);

	esq_mt_handled_exit_events = 0;

	for (i = 0; i < session_cnt; i ++)	{

		session = esq_session_new("mt-exit-session");
		if (!session)	{
			printf("ERROR: %s() %d: failed to create new session, i = %d\n",
					__func__, __LINE__, i
					);
			return -1;
		}

		r = esq_session_set_socket_fd_multi(session, fd, esq_mt_exit_multi_event_cb, ESQ_TST_PARAM);
		if (r)	{
			printf("ERROR: %s() %d: failed to add test fd %d into session\n",
					__func__, __LINE__, fd
					);
			return -1;
		}

		fd --;

		esq_session_wind_up_rr(session);
	}

	esq_session_notify_exit_to_all_sessions();

	printf("INFO: all dispatched, sleep for %d usecond(s)\n", wait_usecond);
	usleep(wait_usecond * 1000);

	#if 0
	esq_counters_t counters;
	esq_get_counters(&counters);
	esq_tst_print_counters(&counters);
	#endif

	if (esq_mt_handled_exit_events == session_cnt)	{
		printf("INFO: case passed\n");
		return 0;
	}
	else	{
		printf("ERROR: %s() %d: %d event(s) reported, not %d\n",
				__func__, __LINE__,
				esq_mt_handled_exit_events, session_cnt
				);
		return -1;
	}
}

/* eof */
