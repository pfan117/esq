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
#include "internal/os_events.h"
#include "session/internal.h"

STATIC void
esq_unhook_child_session(esq_session_t * session)	{

	esq_session_t * p;
	esq_session_t * n;

	for (p = session->child_session_list; p; p = n)	{
		n = p->next_child;
		p->next_child = NULL;
		p->parent_session = NULL;
	}

	for (p = session->finished_child_session_list; p; p = n)	{
		n = p->next_child;
		p->parent_session = NULL;
		if (p->session_done_cb)	{
			p->session_done_cb(p);
		}
		p->magic0 = 0;
		p->magic1 = 0;
		free(p);
	}

	p = session->child_session_list = NULL;
	p = session->finished_child_session_list = NULL;

	return;
}

STATIC void
esq_move_child_session_into_finish_list(esq_session_t * parent_session, esq_session_t * session)
{
	esq_session_t * pp;
	esq_session_t * p;

	pp = NULL;

	for (p = parent_session->child_session_list; p; p = p->next_child)	{
		if (p == session)	{
			break;
		}
		pp = p;
	}

	if (!p)	{
		esq_design_error();
		return;
	}

	if (pp)	{
		pp->next_child = p->next_child;
	}
	else	{
		parent_session->child_session_list = p->next_child;
	}

	session->next_child = parent_session->finished_child_session_list;
	parent_session->finished_child_session_list = session;

	return;
}

int
__esq_session_take_action(esq_session_t * session, esq_session_pt_action_t * request)
{

__switch_session_type:

	ESQ_CHECK_SESSION(session);

	session->post_op = POP_NO_OP;

	switch(session->session_type)	{
	case ESQ_SESSION_TYPE_NEW:
		esq_pt_of_no_fd(session, request);
		break;
	case ESQ_SESSION_TYPE_SOCKET_CB:
		esq_pt_of_single_shot_socket(session, request);
		break;
	case ESQ_SESSION_TYPE_MULTI_SOCKET_CB:
		esq_pt_of_multi_shot_socket(session, request);
		break;
	case ESQ_SESSION_TYPE_MCO:
		esq_pt_of_mco(session, request);
		break;
	case ESQ_SESSION_TYPE_NO_FD:
		esq_pt_of_no_fd(session, request);
		break;
	case ESQ_SESSION_TYPE_WAIT_FOR_HASH_LOCK:
		esq_pt_of_hash_lock(session, request);
		break;
	default:
		PRINTF("ERROR: %s() %d: unknown session session_type %d (error or TODO)\n"
				, __func__, __LINE__, session->session_type);
		esq_design_error();
		return ESQ_ERROR_PARAM;
	}

	/* post actions */

	if ((POP_JOIN_GLOBAL_NEW_LIST & session->post_op))	{
		esq_join_global_new_list(session);
        esq_wake_up_worker_thread_rr();
		return ESQ_OK;
	}
	else if ((POP_JOIN_LOCAL_NEW_LIST & session->post_op))	{
		esq_join_local_new_list(session);
		return ESQ_OK;
	}

	if ((POP_FREE_SESSION & session->post_op))	{

		esq_unhook_child_session(session);

		if (session->have_parent_session)	{
			if (session->parent_session)	{
				esq_move_child_session_into_finish_list(session->parent_session, session);
				esq_notify_event_to_session(session->parent_session, ESQ_EVENT_BIT_TASK_DONE);
			}
			else	{
				if (session->session_done_cb)	{
					session->session_done_cb(session);
				}
				session->magic0 = 0;
				session->magic1 = 0;
				free(session);
			}
		}
		else	{	
			session->magic0 = 0;
			session->magic1 = 0;
			free(session);
		}

		return ESQ_OK;
	}

	if ((POP_SWITCH_SESSION_TYPE & session->post_op))	{
		ESQ_SESSION_PT_RESET
		PRINTF("DBG: %s() %d: session %p change type\n", __func__, __LINE__, session);
		goto __switch_session_type;
	}

	if ((POP_RETURN_VALUE & session->post_op))	{
		int r;
		r = session->r;
		PRINTF("DBG: %s() %d: session %p return value %d\n", __func__, __LINE__, session, r);
		return r;
	}

	return ESQ_OK;
}

#define CASE_N_RETURN(__v__) \
	case __v__: \
		return #__v__; \

const char *
esq_get_action_type_name(int request_type)	{
	switch (request_type)	{
	CASE_N_RETURN(ACTION_NO)
	CASE_N_RETURN(ACTION_WIND_UP)
	CASE_N_RETURN(ACTION_PICK_UP)
	CASE_N_RETURN(ACTION_ADD_SOCK_FD_MULTI)
	CASE_N_RETURN(ACTION_ADD_SOCK_FD_SINGLE)
	CASE_N_RETURN(ACTION_REMOVE_FD)
	CASE_N_RETURN(ACTION_NOTIFY_EVENT)
	CASE_N_RETURN(ACTION_NOTIFY_AIO_REMOVED)
	CASE_N_RETURN(ACTION_NOTIFY_TIMEOUT)
	CASE_N_RETURN(ACTION_NOTIFY_EXIT)
	CASE_N_RETURN(ACTION_EXECUTE_CB)
	CASE_N_RETURN(ACTION_HASH_LOCK_READ)
	CASE_N_RETURN(ACTION_HASH_LOCK_WRITE)
	default:
		return "INVALID-ACTION";
	}
}

/* eof */
