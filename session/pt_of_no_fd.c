/* vi: set sw=4 ts=4: */

//#define PRINT_DBG_LOG

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "public/esq.h"
#include "public/libesq.h"
#include "internal/types.h"
#include "public/esqll.h"
#include "internal/log_print.h"

#include "internal/lock.h"
#include "internal/session.h"
#include "session/internal.h"

void esq_pt_of_no_fd(
		esq_session_t * session,
		esq_session_pt_action_t * action)
{

	if (ACTION_WIND_UP == action->type)	{
		session->post_op = POP_FREE_SESSION;
	}
	else if (ACTION_ADD_SOCK_FD_MULTI == action->type)	{
		session->session_type = ESQ_SESSION_TYPE_MULTI_SOCKET_CB;
		session->post_op = POP_SWITCH_SESSION_TYPE;
	}
	else if (ACTION_ADD_SOCK_FD_SINGLE == action->type)	{
		session->session_type = ESQ_SESSION_TYPE_SOCKET_CB;
		session->post_op = POP_SWITCH_SESSION_TYPE;
	}
	else if (ACTION_HASH_LOCK_READ == action->type)	{
		session->session_type = ESQ_SESSION_TYPE_WAIT_FOR_HASH_LOCK;
		session->post_op = POP_SWITCH_SESSION_TYPE;
	}
	else if (ACTION_HASH_LOCK_WRITE == action->type)	{
		session->session_type = ESQ_SESSION_TYPE_WAIT_FOR_HASH_LOCK;
		session->post_op = POP_SWITCH_SESSION_TYPE;
	}
	else	{
		INVALID_ACTION_TYPE;
		session->post_op = POP_FREE_SESSION;
	}

	return;
}

/* eof */
