#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "public/esq.h"
#include "public/libesq.h"
#include "public/esqll.h"
#include "internal/types.h"
#include "internal/threads.h"
#include "internal/session.h"
#include "modules/mt/internal.h"

#define ESQ_TST_PARAM		((void *)0x1234abcd)

volatile static int mt_done_cnt = 0;
static int mt_wait_for_child = 1;
static int mt_child_cnt = 0;
static int new_fd_value = -100;

/******************************************* the child session *******************************************/

void
esq_mt_child_session_timeout(void * esq_session)	{
	int events;
	void * esq_param;

	events = esq_session_get_event_bits(esq_session);
	esq_param = esq_session_get_param(esq_session);

	if ((ESQ_EVENT_BIT_TIMEOUT == events))	{
		if (esq_param != ESQ_TST_PARAM)	{
			printf("ERROR: %s() %d: event returned wrong parameter %p, should be %p\n"
					, __func__, __LINE__
					, esq_param, ESQ_TST_PARAM
					);
		}
		//printf("DBG: %s() %d: child_session received timedout event, quit\n", __func__, __LINE__);
	}
	else	{
		#if 0
		char buf[128];

		printf("ERROR: %s() %d: event bit 0x%08x is not TIMEOUT, it is %s\n", __func__, __LINE__,
				events,
				esq_get_event_bits_string(events, buf, sizeof(buf))
				);
		#endif
	}

	return;
}

/******************************************* the parent session *******************************************/

static void
esq_mt_inc_counter()	{
	pthread_spin_lock(&esq_mod_test_spinlock);
	mt_done_cnt ++;
	pthread_spin_unlock(&esq_mod_test_spinlock);
}

static void
esq_mt_session_finish_cb(void * esq_session_ptr)	{

	if (!esq_session_ptr)	{
		NOT_SUPPOSE_TO_BE_HERE
		return;
	}

	if (mt_wait_for_child)	{
		if (esq_session_get_parent(esq_session_ptr))	{
			esq_mt_inc_counter();
		}
		else	{
			NOT_SUPPOSE_TO_BE_HERE
		}
	}
	else	{
		if (esq_session_get_parent(esq_session_ptr))	{
			NOT_SUPPOSE_TO_BE_HERE
		}
		else	{
			esq_mt_inc_counter();
		}
	}
}

static void
esq_mt_wait_for_child_session(void * esq_session)	{
	int r;
	int fd;
	int events;
	void * esq_param;

	events = esq_session_get_event_bits(esq_session);
	esq_param = esq_session_get_param(esq_session);
	if (esq_param != ESQ_TST_PARAM)	{
		printf("ERROR: %s() %d: event returned wrong parameter %p, should be %p\n"
				, __func__, __LINE__
				, esq_param, ESQ_TST_PARAM
				);
	}

	if ((ESQ_EVENT_BIT_TASK_DONE == events))	{
		esq_execute_session_done_cb(esq_session);
	}
	else if (ESQ_EVENT_BIT_EXIT == events)	{
		printf("DBG: %s() %d: parent session ignore exit event and continue\n", __func__, __LINE__);
		fd = esq_session_get_fd(esq_session);
		r = esq_session_set_socket_fd(esq_session, fd, esq_mt_wait_for_child_session,
				(ESQ_EVENT_BIT_READ | ESQ_EVENT_BIT_WRITE),
				0, 0, ESQ_TST_PARAM
				);
		if (r)	{
			printf("ERROR: %s() %d: failed to add test fd %d into session\n",
					__func__, __LINE__, fd
					);
		}
	}
	else if ((ESQ_EVENT_BIT_TASK_DONE | ESQ_EVENT_BIT_EXIT) == events)	{
		esq_execute_session_done_cb(esq_session);
	}
	else	{
		printf("ERROR: %s() %d: event bit 0x%08x is not TIMEOUT\n", __func__, __LINE__, events);
	}

	return;
}

static void
esq_mt_create_child_session(void * esq_session)	{
	int i;
	int r;
	int fd;
	int events;
	void * esq_param;
	void * child_session;

	events = esq_session_get_event_bits(esq_session);
	esq_param = esq_session_get_param(esq_session);

	if ((ESQ_EVENT_BIT_EXIT == events))	{
		if (esq_param != ESQ_TST_PARAM)	{
			printf("ERROR: %s() %d: event returned wrong parameter %p, should be %p\n"
					, __func__, __LINE__
					, esq_param, ESQ_TST_PARAM
					);
		}

		for (i = 0; i < mt_child_cnt; i ++)	{
			/* create new */
			child_session = esq_session_new("mt-child-session");
			if (!child_session)	{
				printf("ERROR: %s() %d: failed to create new child session\n", __func__, __LINE__);
				return;
			}

			r = esq_session_set_socket_fd(child_session, new_fd_value, esq_mt_child_session_timeout,
					(ESQ_EVENT_BIT_READ | ESQ_EVENT_BIT_WRITE),
					0, 10, ESQ_TST_PARAM
					);
			if (r)	{
				printf("ERROR: %s() %d: failed to add test fd %d into child_session\n",
						__func__, __LINE__, new_fd_value
						);
				return;
			}

			new_fd_value --;

			esq_session_bundle_child(esq_session, child_session, esq_mt_session_finish_cb);
			esq_session_wind_up(child_session);
		}

		if (mt_wait_for_child)	{
			/* wait for new event */
			fd = esq_session_get_fd(esq_session);
			r = esq_session_set_socket_fd(esq_session, fd, esq_mt_wait_for_child_session,
					(ESQ_EVENT_BIT_READ | ESQ_EVENT_BIT_WRITE)
					, 0, 0, ESQ_TST_PARAM
					);
			if (r)	{
				printf("ERROR: %s() %d: failed to add test fd %d into session\n",
						__func__, __LINE__, fd
						);
			}
		}
		else	{
			//printf("DBG: %s() %d: parent quit after creating child\n", __func__, __LINE__);
		}
	}
	else	{
		printf("ERROR: %s() %d: event bit 0x%08x is not TIMEOUT\n", __func__, __LINE__, events);
	}

	return;
}

int
esq_mt_single_shot_socket_pt_handle_task_done_event(int parent_cnt, int child_cnt, int wait_for_child)	{
	int wait_usecond = 100;
	int total_child;
	void * session;
	int r;
	int i;

	printf("INFO: %s(): create %d parent-sessions\n", __func__, parent_cnt);

	mt_wait_for_child = wait_for_child;
	mt_done_cnt = 0;
	mt_child_cnt = child_cnt;
	total_child = parent_cnt * child_cnt;

	if ((total_child < parent_cnt) || (total_child < child_cnt))	{
		printf("ERROR: %s() %d: parent_cnt * child_cnt over sized\n", __func__, __LINE__);
		return -1;
	}

	for (i = 0; i < parent_cnt; i ++)	{

		session = esq_session_new("mt-parent-session");
		if (!session)	{
			printf("ERROR: %s() %d: failed to create new session, i = %d\n",
					__func__, __LINE__, i
					);
			return -1;
		}

		r = esq_session_set_socket_fd(session, new_fd_value, esq_mt_create_child_session,
				(ESQ_EVENT_BIT_READ | ESQ_EVENT_BIT_WRITE)
				, 0, 0, ESQ_TST_PARAM
				);
		if (r)	{
			printf("ERROR: %s() %d: failed to add test fd %d into session\n",
					__func__, __LINE__, new_fd_value
					);
			return -1;
		}

		new_fd_value --;

		esq_session_wind_up_rr(session);
	}

	printf("DBG: %s() %d: notify parent-sessions to create child-sessions\n", __func__, __LINE__);
	esq_session_notify_exit_to_all_sessions();

	printf("INFO: all dispatched, sleep for %d usecond(s)\n", wait_usecond);
	usleep(wait_usecond * 1000);
	printf("INFO: finished waiting\n");

	if (mt_done_cnt == total_child)	{
		printf("INFO: case passed (%d)\n", mt_done_cnt);
		return 0;
	}
	else	{
		printf("ERROR: %s() %d: %d event(s) reported, not %d\n",
				__func__, __LINE__,
				mt_done_cnt, total_child
				);
		return -1;
	}
}

/* eof */
