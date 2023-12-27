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
esq_pt_of_mco(esq_session_t * session, esq_session_pt_action_t * action)
{
	int r;

	ESQ_SESSION_PT_BEGIN

	PRINTF("DBG: %s() %d: session %p <%s> started\n", __func__, __LINE__, session, session->name);

	esq_session_join_active_list(session);
	session->post_op = POP_RETURN_VALUE;
	session->r = ESQ_OK;

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
		GOTO(__execute_cb);
	}
	else	{
		INVALID_ACTION_TYPE;
		session->post_op = POP_NO_OP;
		GOTO(__active);
	}

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
		session->post_op = POP_NO_OP;
		GOTO(__active);
	}
	else if (ACTION_NOTIFY_TIMEOUT == action->type)	{
		session->event_bits |= ESQ_EVENT_BIT_TIMEOUT;
		SHOW_SESSION_EVENT_BITS(ESQ_EVENT_BIT_TIMEOUT);
		esq_session_left_timing_list(session);
		esq_session_join_active_list(session);
		session->post_op = POP_NO_OP;
		GOTO(__active);
	}
	else	{
		INVALID_ACTION_TYPE;
		session->post_op = POP_NO_OP;
		GOTO(__winded_up);
	}

__execute_cb:

	ESQ_SESSION_PT_SET

	SHOW_ACTION_TYPE_NAME;

	if (ACTION_WIND_UP == action->type)	{
		if (mco_is_end(session->mco))	{
			mco_destroy(session->mco);
			session->mco = NULL;
			session->post_op = POP_FREE_SESSION;
			PRINTF("DBG: %s() %d: mco has been closed, will free the session\n", __func__, __LINE__);
			return;
		}
		else if (session->event_bits2)	{
			session->event_bits = session->event_bits2;
			session->event_bits2 = 0;
			PRINTF("DBG: %s() %d: session will be actived by stored event bit(s)\n", __func__, __LINE__);
			SHOW_SESSION_EVENT_BITS(session->event_bits);
			esq_session_join_active_list(session);
			GOTO(__active);
		}
		else	{
			INVALID_ACTION_TYPE;
			esq_design_error();	/* this mco is dead */
			GOTO(__winded_up);
		}
	}
	else if (ACTION_ADD_SOCK_FD_SINGLE == action->type)	{
		if (FD_NEED_INSTALL(action->fd))	{
			/* install socket fd */
			r = esq_os_events_add_socket_fd(action->fd, session, session->exp_event_bits);
			if (r)	{
				printf("ERROR: %s() %d: failed to install socket fd %d into os-event layer\n"
						, __func__, __LINE__, session->fd);
				session->post_op = POP_RETURN_VALUE;
				session->r = r;
				GOTO(__execute_cb);
			}
		}
		session->fd = action->fd;
		esq_session_join_timing_list(session);
		esq_update_polling_interval();
		session->post_op = POP_RETURN_VALUE;
		session->r = ESQ_OK;
		GOTO(__execute_cb_installed_socket_fd);
	}
	else if (ACTION_NOTIFY_EXIT == action->type)	{
		session->event_bits2 |= ESQ_EVENT_BIT_EXIT;
		SHOW_SESSION_EVENT_BITS(ESQ_EVENT_BIT_EXIT);
		session->post_op = POP_NO_OP;
		GOTO(__execute_cb);
	}
	else if (ACTION_NOTIFY_EVENT == action->type)	{
		session->event_bits2 |= action->notify_event_bits;
		SHOW_SESSION_EVENT_BITS(action->notify_event_bits);
		session->post_op = POP_NO_OP;
		GOTO(__execute_cb);
	}
	else	{
		INVALID_ACTION_TYPE;
		session->post_op = POP_NO_OP;
		GOTO(__execute_cb);
	}

__execute_cb_installed_socket_fd:

	ESQ_SESSION_PT_SET

	SHOW_ACTION_TYPE_NAME;

	if (ACTION_WIND_UP == action->type)	{
		if (mco_is_end(session->mco))	{
			INVALID_ACTION_TYPE;
			session->post_op = POP_NO_OP; /* mco close with open socket */
			GOTO(__execute_cb_installed_socket_fd);	
			return;
		}
		else if (session->event_bits2)	{
			session->event_bits = session->event_bits2;
			session->event_bits2 = 0;
			PRINTF("DBG: %s() %d: session will be actived by stored event bit(s)\n", __func__, __LINE__);
			SHOW_SESSION_EVENT_BITS(session->event_bits);
			esq_session_join_active_list(session);
			session->post_op = POP_NO_OP;
			GOTO(__active);
		}
		else	{
			session->post_op = POP_NO_OP;
			GOTO(__winded_up);
		}
	}
	else if (ACTION_REMOVE_FD == action->type)	{
		if (FD_NEED_INSTALL(session->fd))	{
			esq_os_events_remove_socket_fd(session);
		}
		esq_session_left_timing_list(session);
		session->post_op = POP_NO_OP;
		session->fd = 0;
		GOTO(__execute_cb);
	}
	else if (ACTION_NOTIFY_EXIT == action->type)	{
		session->event_bits2 |= ESQ_EVENT_BIT_EXIT;
		SHOW_SESSION_EVENT_BITS(ESQ_EVENT_BIT_EXIT);
		esq_session_left_timing_list(session);
		session->post_op = POP_NO_OP;
		GOTO(__execute_cb_installed_socket_fd);
	}
	else if (ACTION_NOTIFY_EVENT == action->type)	{
		session->event_bits2 |= action->notify_event_bits;
		SHOW_SESSION_EVENT_BITS(action->notify_event_bits);
		esq_session_left_timing_list(session);
		session->post_op = POP_NO_OP;
		GOTO(__execute_cb_installed_socket_fd);
	}
	else if (ACTION_NOTIFY_TIMEOUT == action->type)	{
		session->event_bits2 |= ESQ_EVENT_BIT_TIMEOUT;
		SHOW_SESSION_EVENT_BITS(ESQ_EVENT_BIT_TIMEOUT);
		esq_session_left_timing_list(session);
		session->post_op = POP_NO_OP;
		GOTO(__execute_cb_installed_socket_fd);
	}
	else	{
		INVALID_ACTION_TYPE;
		session->post_op = POP_NO_OP;
		GOTO(__execute_cb_installed_socket_fd);
	}

	ESQ_SESSION_PT_END

	return;
}

/* eof */
