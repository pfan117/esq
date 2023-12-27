/* vi: set sw=4 ts=4: */

// #define PRINT_DBG_LOG

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "public/esq.h"
#include "public/libesq.h"
#include "internal/types.h"
#include "public/esqll.h"
#include "internal/tree.h"
#include "internal/log_print.h"

#include "internal/lock.h"
#include "internal/os_events.h"

#include "internal/session.h"
#include "session/internal.h"

void
esq_pt_of_multi_shot_socket(esq_session_t * session, esq_session_pt_action_t * action)
{
	int r;

	ESQ_SESSION_PT_BEGIN

	PRINTF("DBG: %s() %d: session %p <%s> started\n", __func__, __LINE__, session, session->name);

	session->fd = action->fd;
	session->timeouts = 0;
	session->timeoutms = 0;
	session->user_param = action->user_param;
	session->exp_event_bits = action->exp_event_bits;
	session->cb = action->cb;

	session->event_bits = 0;
	session->post_op = POP_NO_OP;

	ESQ_SESSION_PT_SET

	SHOW_ACTION_TYPE_NAME;

	if (ACTION_WIND_UP == action->type)	{
		session->post_op = POP_JOIN_LOCAL_NEW_LIST;
		//session->post_op = POP_JOIN_GLOBAL_NEW_LIST;
		GOTO(__wait_for_cpu);
	}
	else if (ACTION_WIND_UP_RR == action->type)	{
		session->post_op = POP_JOIN_GLOBAL_NEW_LIST;
		GOTO(__wait_for_cpu);
	}
	else	{
		INVALID_ACTION_TYPE;
		session->post_op = POP_RETURN_VALUE;
		session->r = ESQ_ERROR_PARAM;
		return;
	}

__wait_for_cpu:

	ESQ_SESSION_PT_SET

	SHOW_ACTION_TYPE_NAME;

	if (ACTION_PICK_UP == action->type)	{
		goto __install_fd;
	}
	else	{
		INVALID_ACTION_TYPE;
		session->post_op = POP_RETURN_VALUE;
		session->r = ESQ_ERROR_PARAM;
		return;
	}

__install_fd:

	if (FD_NEED_INSTALL(session->fd))	{
		r = esq_os_events_add_socket_fd_multishot(session->fd, session, session->exp_event_bits);
		if (r)	{
			printf("ERROR: %s() %d: failed to install socket fd %d into os-event layer\n"
					, __func__, __LINE__, session->fd);
			session->event_bits |= ESQ_EVENT_BIT_ERROR;
			esq_session_join_active_list(session);
			session->post_op = POP_NO_OP;
			GOTO(__active);
		}
		else	{
			PRINTF("DBG: %s() %d: session %p <%s> fd installed\n", __func__, __LINE__, session, session->name);
		}
	}

	esq_session_join_timing_list(session);
	session->post_op = POP_NO_OP;
	GOTO(__winded_up);

__winded_up:

	ESQ_SESSION_PT_SET

	SHOW_ACTION_TYPE_NAME;

	if (ACTION_NOTIFY_EVENT == action->type)	{
		session->event_bits |= action->notify_event_bits;
		SHOW_SESSION_EVENT_BITS(action->notify_event_bits);
		esq_session_left_timing_list(session);
		esq_session_join_active_list(session);
		session->post_op = POP_NO_OP;
		GOTO(__active);
	}
	else if (ACTION_NOTIFY_EXIT == action->type)	{
		session->event_bits |= ESQ_EVENT_BIT_EXIT;
		SHOW_SESSION_EVENT_BITS(ESQ_EVENT_BIT_EXIT);
		esq_session_left_timing_list(session);
		esq_session_join_active_list(session);
		GOTO(__active);
	}
	else	{
		INVALID_ACTION_TYPE;
		session->post_op = POP_NO_OP;
		GOTO(__winded_up);
	}

__active:

	ESQ_SESSION_PT_SET

	SHOW_ACTION_TYPE_NAME;

	if (ACTION_NOTIFY_EVENT == action->type)	{
		session->event_bits |= action->notify_event_bits;
		SHOW_SESSION_EVENT_BITS(action->notify_event_bits);
		session->post_op = POP_NO_OP;
		GOTO(__active);
	}
	else if (ACTION_NOTIFY_EXIT == action->type)	{
		session->event_bits |= ESQ_EVENT_BIT_EXIT;
		SHOW_SESSION_EVENT_BITS(ESQ_EVENT_BIT_EXIT);
		session->post_op = POP_NO_OP;
		GOTO(__active);
	}
	else if (ACTION_EXECUTE_CB == action->type)	{
		esq_session_left_active_list(session);
		session->post_op = POP_RETURN_VALUE;
		session->r = ESQ_OK;
		session->event_bits2 = 0;
		GOTO(__execute_cb);
	}
	else	{
		INVALID_ACTION_TYPE;
		session->post_op = POP_NO_OP;
		GOTO(__active);
	}

__execute_cb:

	ESQ_SESSION_PT_SET

	SHOW_ACTION_TYPE_NAME;

	if (ACTION_WIND_UP == action->type)	{
		if (session->fd)	{
			if (session->event_bits2)	{
				session->event_bits = session->event_bits2;
				PRINTF("DBG: %s() %d: session will be actived by stored event bit(s)\n", __func__, __LINE__);
				SHOW_SESSION_EVENT_BITS(session->event_bits);
				esq_session_join_active_list(session);
				GOTO(__active);
			}
			else	{
				esq_session_join_timing_list(session);
				session->post_op = POP_NO_OP;
				GOTO(__winded_up);
			}
		}
		else	{
			session->post_op = POP_FREE_SESSION;
			return;
		}
	}
	else if (ACTION_REMOVE_FD == action->type)	{
		if (FD_NEED_INSTALL(session->fd))	{
			r = esq_os_events_remove_socket_fd(session);
			session->fd = 0;
			if (r)	{
				printf("ERROR: %s() %d: failed to remove socket fd %d from os-event layer\n"
						, __func__, __LINE__, session->fd);
				session->event_bits2 |= ESQ_EVENT_BIT_ERROR;
				SHOW_SESSION_EVENT_BITS(ESQ_EVENT_BIT_ERROR);
				GOTO(__execute_cb);
			}
			else	{
				PRINTF("DBG: %s() %d: wait for REMOVED and WINDUP\n", __func__, __LINE__);
				esq_session_join_timing_list(session);
				GOTO(__wait_for_removed_and_wind_up);
			}
		}
		else	{
			session->post_op = POP_NO_OP;
			session->fd = 0;
			GOTO(__execute_cb);
		}
	}
	else if (ACTION_NOTIFY_EVENT == action->type)	{
		session->event_bits2 |= action->notify_event_bits;
		SHOW_SESSION_EVENT_BITS(action->notify_event_bits);
		session->post_op = POP_NO_OP;
		GOTO(__execute_cb);
	}
	else if (ACTION_NOTIFY_EXIT == action->type)	{
		session->event_bits2 |= ESQ_EVENT_BIT_EXIT;
		SHOW_SESSION_EVENT_BITS(ESQ_EVENT_BIT_EXIT);
		session->post_op = POP_NO_OP;
		GOTO(__execute_cb);
	}
	else	{
		INVALID_ACTION_TYPE;
		session->post_op = POP_NO_OP;
		GOTO(__execute_cb);
	}

__wait_for_removed_and_wind_up:

	ESQ_SESSION_PT_SET

	SHOW_ACTION_TYPE_NAME;

	if (ACTION_NOTIFY_AIO_REMOVED == action->type)	{
		session->post_op = POP_NO_OP;
		GOTO(__wait_for_wind_up);
	}
	else if (ACTION_WIND_UP == action->type)	{
		session->post_op = POP_NO_OP;
		GOTO(__wait_for_removed);
	}
	else	{
		INVALID_ACTION_TYPE;
		session->post_op = POP_RETURN_VALUE;
		session->r = ESQ_ERROR_PARAM;
		GOTO(__wait_for_removed_and_wind_up);
	}

__wait_for_removed:

	ESQ_SESSION_PT_SET

	SHOW_ACTION_TYPE_NAME;

	if (ACTION_NOTIFY_AIO_REMOVED == action->type)	{
		esq_session_left_timing_list(session);
		session->post_op = POP_FREE_SESSION;
		GOTO(__die);
	}
	else if (ACTION_NOTIFY_EXIT == action->type)	{
		session->post_op = POP_NO_OP;	/* ignore */
		GOTO(__wait_for_removed);
	}
	else	{
		INVALID_ACTION_TYPE;
		session->post_op = POP_RETURN_VALUE;
		session->r = ESQ_ERROR_PARAM;
		GOTO(__wait_for_removed);
	}

__wait_for_wind_up:

	ESQ_SESSION_PT_SET

	SHOW_ACTION_TYPE_NAME;

	if (ACTION_WIND_UP == action->type)	{
		esq_session_left_timing_list(session);
		session->post_op = POP_FREE_SESSION;
		GOTO(__die);
	}
	else if (ACTION_NOTIFY_EXIT == action->type)	{
		session->post_op = POP_NO_OP;	/* ignore */
		GOTO(__wait_for_wind_up);
	}
	else	{
		INVALID_ACTION_TYPE;
		session->post_op = POP_RETURN_VALUE;
		session->r = ESQ_ERROR_PARAM;
		GOTO(__wait_for_wind_up);
	}

__die:

	ESQ_SESSION_PT_SET
	SHOW_ACTION_TYPE_NAME;
	INVALID_ACTION_TYPE;
	session->post_op = POP_RETURN_VALUE;
	session->r = ESQ_ERROR_PARAM;
	GOTO(__die);

	ESQ_SESSION_PT_END

	return;
}

/* eof */
