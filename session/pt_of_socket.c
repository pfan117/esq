/* vi: set sw=4 ts=4: */

//#define PRINT_DBG_LOG

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
esq_pt_of_single_shot_socket(esq_session_t * session, esq_session_pt_action_t * action)
{
	int r;

	ESQ_SESSION_PT_BEGIN

	PRINTF("DBG: %s() %d: session %p <%s> started\n", __func__, __LINE__, session, session->name);

	session->fd = action->fd;
	session->timeouts = action->timeouts;
	session->timeoutms = action->timeoutms;
	session->user_param = action->user_param;
	session->exp_event_bits = action->exp_event_bits;
	session->cb = action->cb;

	session->event_bits = 0;
	session->post_op = POP_NO_OP;

__initial:

	ESQ_SESSION_PT_SET

	SHOW_ACTION_TYPE_NAME;

	if (ACTION_WIND_UP == action->type)	{
		session->post_op = POP_JOIN_LOCAL_NEW_LIST;
		GOTO(__wait_for_cpu);
	}
	else if (ACTION_WIND_UP_RR == action->type)	{
		session->post_op = POP_JOIN_GLOBAL_NEW_LIST;
		GOTO(__wait_for_cpu);
	}
	else if (ACTION_SET_PARENT == action->type)	{
		if (session->have_parent_session)	{
			INVALID_ACTION_TYPE;
			session->post_op = POP_RETURN_VALUE;
			session->r = ESQ_ERROR_PARAM;
			GOTO(__initial);
			return;
		}
		else	{
			esq_session_t * parent;
			parent = action->user_param;
			session->have_parent_session = 1;
			session->session_done_cb = action->cb;
			session->parent_session = parent;
			session->post_op = POP_NO_OP;
			GOTO(__initial);
		}
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
		r = esq_os_events_add_socket_fd(session->fd, session, session->exp_event_bits);
		if (r)	{
			printf("ERROR: %s() %d: failed to install socket fd %d into os-event layer\n"
					, __func__, __LINE__, session->fd);
			session->event_bits |= ESQ_EVENT_BIT_ERROR;
			esq_session_join_active_list(session);
			session->post_op = POP_NO_OP;
			GOTO(__active);
		}
	}

	esq_session_join_timing_list(session);
	esq_update_polling_interval();
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
	else if (ACTION_NOTIFY_TIMEOUT == action->type)	{
		session->event_bits |= ESQ_EVENT_BIT_TIMEOUT;
		SHOW_SESSION_EVENT_BITS(ESQ_EVENT_BIT_TIMEOUT);
		session->post_op = POP_NO_OP;
		GOTO(__remove_fd_then_active);
	}
	else if (ACTION_NOTIFY_EXIT == action->type)	{
		session->event_bits |= ESQ_EVENT_BIT_EXIT;
		SHOW_SESSION_EVENT_BITS(ESQ_EVENT_BIT_EXIT);
		session->post_op = POP_NO_OP;
		GOTO(__remove_fd_then_active);
	}
	else	{
		INVALID_ACTION_TYPE;
		session->post_op = POP_RETURN_VALUE;
		session->r = ESQ_ERROR_PARAM;
		GOTO(__winded_up);
		return;
	}

__remove_fd_then_active:

	if (FD_NEED_INSTALL(session->fd))	{
		r = esq_os_events_remove_socket_fd(session);
		session->fd = 0;
		if (r)	{
			printf("ERROR: %s() %d: failed to remove socket fd %d from os-event layer\n"
					, __func__, __LINE__, session->fd);
			session->event_bits |= ESQ_EVENT_BIT_ERROR;
			SHOW_SESSION_EVENT_BITS(ESQ_EVENT_BIT_ERROR);
			esq_session_left_timing_list(session);
			esq_session_join_active_list(session);
			GOTO(__active);
		}
		else	{
			esq_session_change_to_no_timing_list(session);
			PRINTF("DBG: %s() %d: fd removed\n", __func__, __LINE__);
			GOTO(__wait_for_removed_then_active);
		}
	}
	else	{
		esq_session_left_timing_list(session);
		esq_session_join_active_list(session);
		GOTO(__active);
	}

__wait_for_removed_then_active:

	ESQ_SESSION_PT_SET

	SHOW_ACTION_TYPE_NAME;

	if (ACTION_NOTIFY_AIO_REMOVED == action->type)	{
		esq_session_left_timing_list(session);
		esq_session_join_active_list(session);
		session->post_op = POP_NO_OP;
		GOTO(__active);
	}
	else if (ACTION_NOTIFY_EXIT == action->type)	{
		session->event_bits |= ESQ_EVENT_BIT_EXIT;
		SHOW_SESSION_EVENT_BITS(ESQ_EVENT_BIT_EXIT);
		session->post_op = POP_NO_OP;
		GOTO(__wait_for_removed_then_active);
	}
	else	{
		INVALID_ACTION_TYPE;
		session->post_op = POP_RETURN_VALUE;
		session->r = ESQ_ERROR_PARAM;
		GOTO(__wait_for_removed_then_active);
	}

__active:

	ESQ_SESSION_PT_SET

	SHOW_ACTION_TYPE_NAME;

	if (ACTION_NOTIFY_EVENT == action->type)	{
		session->event_bits |= action->notify_event_bits;
		SHOW_SESSION_EVENT_BITS(action->notify_event_bits);
		session->post_op = POP_NO_OP;
		return;
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
		GOTO(__execute_cb);
	}
	else	{
		INVALID_ACTION_TYPE;
		session->post_op = POP_RETURN_VALUE;
		session->r = ESQ_ERROR_PARAM;
		return;
	}

__execute_cb:

	ESQ_SESSION_PT_SET

	SHOW_ACTION_TYPE_NAME;
#if 0
	printf("DBG: %s() %d: %p <%s> %s (%d)\n" \
		, __func__, __LINE__, session, session->name \
		, esq_get_action_type_name(action->type) \
		, action->type \
		)
#endif

	if (ACTION_WIND_UP == action->type)	{
		session->post_op = POP_FREE_SESSION;
		GOTO(__die);
		return;
	}
	else if (ACTION_ADD_SOCK_FD_MULTI == action->type)	{
		session->post_op = POP_SWITCH_SESSION_TYPE;
		session->session_type = ESQ_SESSION_TYPE_MULTI_SOCKET_CB;
		return;
	}
	else if (ACTION_ADD_CHILD == action->type)	{
		esq_session_t * child = action->user_param;
		child->next_child = session->child_session_list;
		session->child_session_list = child;
		session->post_op = POP_NO_OP;
		GOTO(__execute_cb);
	}
	else if (ACTION_ADD_SOCK_FD_SINGLE == action->type)	{
		session->fd = action->fd;
		session->timeouts = action->timeouts;
		session->timeoutms = action->timeoutms;
		session->user_param = action->user_param;
		session->exp_event_bits = action->exp_event_bits;
		session->cb = action->cb;
		session->post_op = POP_NO_OP;
		GOTO(__wait_for_reinstall);
	}
	else if (ACTION_NOTIFY_EXIT == action->type)	{
		session->event_bits |= ESQ_EVENT_BIT_EXIT;
		SHOW_SESSION_EVENT_BITS(ESQ_EVENT_BIT_EXIT);
		session->post_op = POP_NO_OP;
		GOTO(__execute_cb);
	}
	else if (ACTION_NOTIFY_EVENT == action->type)	{
		session->event_bits |= action->notify_event_bits;
		SHOW_SESSION_EVENT_BITS(action->notify_event_bits);
		session->post_op = POP_NO_OP;
		GOTO(__execute_cb);
	}
#if 0
	else if (ACTION_REMOVE_FD == action->type)	{
		if (FD_NEED_INSTALL(session->fd))	{
			r = esq_os_events_remove_socket_fd(session);
			if (r)	{
				printf("ERROR: %s() %d: failed to remove socket fd %d from os-event layer\n"
						, __func__, __LINE__, session->fd);
			}
			session->fd = 0;
		}

		session->post_op = POP_NO_OP;
		GOTO(__execute_cb);
	}
#endif
	else if (ACTION_HASH_LOCK_WRITE == action->type)	{
		session->session_type = ESQ_SESSION_TYPE_WAIT_FOR_HASH_LOCK;
		session->post_op = POP_SWITCH_SESSION_TYPE;
		return;
	}
	else if (ACTION_HASH_LOCK_READ == action->type)	{
		session->session_type = ESQ_SESSION_TYPE_WAIT_FOR_HASH_LOCK;
		session->post_op = ESQ_SESSION_TYPE_WAIT_FOR_HASH_LOCK;
		return;
	}
	else	{
		INVALID_ACTION_TYPE;
		session->post_op = POP_RETURN_VALUE;
		session->r = ESQ_ERROR_PARAM;
		return;
	}

__wait_for_reinstall:

	ESQ_SESSION_PT_SET

	SHOW_ACTION_TYPE_NAME;

	if (ACTION_WIND_UP == action->type)	{
		goto __install_fd;
	}
	else if (ACTION_NOTIFY_EXIT == action->type)	{
		session->event_bits |= ESQ_EVENT_BIT_EXIT;
		SHOW_SESSION_EVENT_BITS(ESQ_EVENT_BIT_EXIT);
		session->post_op = POP_NO_OP;
		GOTO(__wait_for_reinstall);
	}
	else if (ACTION_NOTIFY_EVENT == action->type)	{
		session->event_bits |= action->notify_event_bits;
		SHOW_SESSION_EVENT_BITS(action->notify_event_bits);
		session->post_op = POP_NO_OP;
		GOTO(__wait_for_reinstall);
	}
	else	{
		INVALID_ACTION_TYPE;
		session->post_op = POP_RETURN_VALUE;
		session->r = ESQ_ERROR_PARAM;
		return;
	}

__die:

	ESQ_SESSION_PT_SET
	SHOW_ACTION_TYPE_NAME;
	INVALID_ACTION_TYPE;
	session->post_op = POP_RETURN_VALUE;
	session->r = ESQ_ERROR_PARAM;
	esq_backtrace();

	return;

	ESQ_SESSION_PT_END

	return;
}

/* eof */
