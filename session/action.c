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
#include "session/core_ctx.h"

int
esq_session_set_socket_fd(void * session_ptr
			, int fd
			, esq_event_cb_t cb
			, int event_bits
			, int timeouts, int timeoutms
			, void * user_param)
{
	esq_session_pt_action_t action_buf;	/* can be modified by callee function */
	esq_session_t * session;
	int r;

	if (!session_ptr)	{
		esq_design_error();
		return ESQ_ERROR_PARAM;
	}

	session = session_ptr;

	if (cb)	{
		;
	}
	else	{
		if (session->mco)	{
			;
		}
		else	{
			esq_design_error();
			return ESQ_ERROR_PARAM;
		}
	}

	action_buf.type = ACTION_ADD_SOCK_FD_SINGLE;
	action_buf.fd = fd;
	action_buf.timeouts = timeouts;
	action_buf.timeoutms = timeoutms;
	action_buf.user_param = user_param;
	action_buf.exp_event_bits = event_bits;
	action_buf.cb = cb;

	r = __esq_session_take_action(session, &action_buf);

	PRINTF("DBG: %s() %d: session %p: r = %d\n", __func__, __LINE__, session, r);

	return r;
}

#if 0
int
esq_session_set_done_cb(void * session_ptr
			, int fd
			, esq_event_cb_t cb
			, int event_bits
			, int timeouts, int timeoutms
			, void * user_param)
{
	esq_session_pt_action_t action_buf;	/* can be modified by callee function */
	esq_session_t * session;
	int r;

	if (!session_ptr)	{
		esq_design_error();
		return ESQ_ERROR_PARAM;
	}

	session = session_ptr;

	if (cb)	{
		;
	}
	else	{
		if (session->mco)	{
			;
		}
		else	{
			esq_design_error();
			return ESQ_ERROR_PARAM;
		}
	}

	action_buf.type = ACTION_ADD_SOCK_FD_SINGLE;
	action_buf.fd = fd;
	action_buf.timeouts = timeouts;
	action_buf.timeoutms = timeoutms;
	action_buf.user_param = user_param;
	action_buf.exp_event_bits = event_bits;
	action_buf.cb = cb;

	r = __esq_session_take_action(session, &action_buf);

	PRINTF("DBG: %s() %d: session %p: r = %d\n", __func__, __LINE__, session, r);

	return r;
}
#endif

int
esq_session_bundle_child(void * parent_session_ptr, void * child_session_ptr, void * child_finish_cb)	{
	esq_session_pt_action_t action_buf;	/* can be modified by callee function */
	esq_session_t * parent_session = parent_session_ptr;
	esq_session_t * child_session = child_session_ptr;
	int r;

	if (!parent_session_ptr || !child_session_ptr)	{
		esq_design_error();
		return ESQ_ERROR_PARAM;
	}

	ESQ_CHECK_SESSION(parent_session);
	ESQ_CHECK_SESSION(child_session);

	action_buf.type = ACTION_SET_PARENT;
	action_buf.user_param = parent_session_ptr;
	action_buf.cb = child_finish_cb;

	r = __esq_session_take_action(child_session_ptr, &action_buf);
	if (r)	{
		return r;
	}

	action_buf.type = ACTION_ADD_CHILD;
	action_buf.user_param = child_session_ptr;
	r = __esq_session_take_action(parent_session_ptr, &action_buf);
	if (r)	{
		return r;
	}

	PRINTF("DBG: %s() %d: session %p: r = %d\n", __func__, __LINE__, session, r);

	return ESQ_OK;
}

int
esq_session_set_socket_fd_multi(void * session, int fd, esq_event_cb_t cb, void * user_param)
{
	esq_session_pt_action_t action_buf;	/* can be modified by callee function */
	int r;

	if (!session || !cb)	{
		esq_design_error();
		return ESQ_ERROR_PARAM;
	}

	action_buf.type = ACTION_ADD_SOCK_FD_MULTI;
	action_buf.fd = fd;
	action_buf.timeouts = 0;
	action_buf.timeoutms = 0;
	action_buf.user_param = user_param;
	action_buf.exp_event_bits = (ESQ_EVENT_BIT_READ | ESQ_EVENT_BIT_WRITE);
	action_buf.cb = cb;

	r = __esq_session_take_action(session, &action_buf);

	PRINTF("DBG: %s() %d: session %p: r = %d\n", __func__, __LINE__, session, r);

	return r;
}

void
esq_session_remove_req(void * session)	{
	esq_session_pt_action_t action_buf;	/* can be modified by callee function */
	int r;

	(void)r;

	if (!session)	{
		esq_design_error();
		return;
	}

	action_buf.type = ACTION_REMOVE_FD;

	r = __esq_session_take_action(session, &action_buf);

	PRINTF("DBG: %s() %d: session %p: r = %d\n", __func__, __LINE__, session, r);

	return;
}

int
esq_session_check_for_executing(void * session)	{
	esq_session_pt_action_t action_buf;	/* can be modified by callee function */
	int r;

	action_buf.type = ACTION_EXECUTE_CB;

	r = __esq_session_take_action(session, &action_buf);

	return r;
}

void
esq_session_wind_up(void * session)	{
	esq_session_pt_action_t action_buf;	/* can be modified by callee function */
	int r;

	(void)r;

	if (!session)	{
		esq_design_error();
		return;
	}

	action_buf.type = ACTION_WIND_UP;

	r = __esq_session_take_action(session, &action_buf);

	PRINTF("DBG: %s() %d: session %p: r = %d\n", __func__, __LINE__, session, r);

	return;
}

void
esq_session_wind_up_rr(void * session)	{
	esq_session_pt_action_t action_buf;	/* can be modified by callee function */
	int r;

	(void)r;

	if (!session)	{
		esq_design_error();
		return;
	}

	action_buf.type = ACTION_WIND_UP_RR;

	r = __esq_session_take_action(session, &action_buf);

	PRINTF("DBG: %s() %d: session %p: r = %d\n", __func__, __LINE__, session, r);

	return;
}

void
esq_session_pick_up(void * session)	{
	esq_session_pt_action_t action_buf;	/* can be modified by callee function */
	int r;

	(void)r;

	if (!session)	{
		esq_design_error();
		return;
	}

	action_buf.type = ACTION_PICK_UP;

	r = __esq_session_take_action(session, &action_buf);

	PRINTF("DBG: %s() %d: session %p: r = %d\n", __func__, __LINE__, session, r);

	return;
}

void
esq_notify_event_to_session(void * session, int event_bits)	{
	esq_session_pt_action_t action_buf;	/* can be modified by callee function */
	int r;

	(void)r;

	if (!session)	{
		esq_design_error();
		return;
	}

	#if defined PRINT_DBG_LOG
	{
		char eb[64];
		printf("DBG: %s() %d: %p: event_bits = 0x%d <%s>\n"
				, __func__, __LINE__, session
				, event_bits
				, esq_get_event_bits_string(event_bits, eb, sizeof(eb))
				);
	}
	#endif

	action_buf.type = ACTION_NOTIFY_EVENT;
	action_buf.notify_event_bits = event_bits;

	r = __esq_session_take_action(session, &action_buf);

	PRINTF("DBG: %s() %d: session %p: r = %d\n", __func__, __LINE__, session, r);

	return;
}

void
esq_hash_lock_read(void * session, int hash_lock_idx, esq_event_cb_t cb, void * user_param, int timeout_s)
{
	esq_session_pt_action_t action_buf;	/* can be modified by callee function */
	int r;

	(void)r;

	if (!session)	{
		esq_design_error();
		return;
	}

	if (timeout_s < -1)	{
		esq_design_error();
		return;
	}

	action_buf.timeouts = timeout_s;
	action_buf.type = ACTION_HASH_LOCK_READ;
	action_buf.user_param = user_param;
	action_buf.cb = cb;
	action_buf.hash_lock_idx = hash_lock_idx;

	r = __esq_session_take_action(session, &action_buf);

	PRINTF("DBG: %s() %d: session %p: r = %d\n", __func__, __LINE__, session, r);

	return;
}

void
esq_hash_lock_write(void * session, int hash_lock_idx, esq_event_cb_t cb, void * user_param, int timeout_s)
{
	esq_session_pt_action_t action_buf;	/* can be modified by callee function */
	int r;

	(void)r;

	if (!session)	{
		esq_design_error();
		return;
	}

	if (timeout_s < -1)	{
		esq_design_error();
		return;
	}

	action_buf.timeouts = timeout_s;
	action_buf.timeoutms = 0;
	action_buf.type = ACTION_HASH_LOCK_WRITE;
	action_buf.user_param = user_param;
	action_buf.cb = cb;
	action_buf.hash_lock_idx = hash_lock_idx;

	r = __esq_session_take_action(session, &action_buf);

	PRINTF("DBG: %s() %d: session %p: r = %d\n", __func__, __LINE__, session, r);

	return;
}

void
esq_notify_aio_removed(void * session)	{
	esq_session_pt_action_t action_buf;	/* can be modified by callee function */
	int r;

	(void)r;

	if (!session)	{
		esq_design_error();
		return;
	}

	action_buf.type = ACTION_NOTIFY_AIO_REMOVED;

	r = __esq_session_take_action(session, &action_buf);

	PRINTF("DBG: %s() %d: session %p: r = %d\n", __func__, __LINE__, session, r);

	return;
}

void
__esq_notify_timeout_event(esq_session_t * session)	{
	esq_session_pt_action_t action_buf;	/* can be modified by callee function */
	int r;

	(void)r;

	if (!session)	{
		esq_design_error();
		return;
	}

	action_buf.type = ACTION_NOTIFY_TIMEOUT;

	r = __esq_session_take_action(session, &action_buf);

	PRINTF("DBG: %s() %d: session %p: r = %d\n", __func__, __LINE__, session, r);

	return;
}

void
__esq_notify_exit(esq_session_t * session)	{
	esq_session_pt_action_t action_buf;	/* can be modified by callee function */
	int r;

	(void)r;

	action_buf.type = ACTION_NOTIFY_EXIT;

	r = __esq_session_take_action(session, &action_buf);

	PRINTF("DBG: %s() %d: session %p: r = %d\n", __func__, __LINE__, session, r);

	return;
}

/* eof */
