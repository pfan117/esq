/* vi: set sw=4 ts=4: */

//#define PRINT_DBG_LOG

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <strings.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>

#include "public/esq.h"
#include "public/libesq.h"
#include "internal/types.h"
#include "public/esqll.h"
#include "internal/tree.h"
#include "internal/log_print.h"
#include "internal/lock.h"
#include "internal/signals.h"
#include "internal/session.h"
#include "internal/threads.h"
#include "internal/os_events.h"
#include "session/internal.h"

#define OUTPUT(__BIT__,__S__)	do	{ \
	if ((bits & (__BIT__)))	{ \
		r = snprintf(buf + offset, length - offset, "%s-", __S__); \
		if (r <= 0 || r >= length - offset)	{ \
			return "BUFFER-TOO-SMALL"; \
		} \
		else	{ \
			offset += r; \
		} \
	} \
}while(0);

const char *
esq_get_event_bits_string(int bits, char * buf, int length)	{
	int offset;
	int r;

	offset = 0;

	OUTPUT(ESQ_EVENT_BIT_FD_CLOSE, "close");
	OUTPUT(ESQ_EVENT_BIT_ERROR, "error");
	OUTPUT(ESQ_EVENT_BIT_TIMEOUT, "timeout");
	OUTPUT(ESQ_EVENT_BIT_READ, "read");
	OUTPUT(ESQ_EVENT_BIT_WRITE, "write");
	OUTPUT(ESQ_EVENT_BIT_LOCK_OBTAIN, "lock");
	OUTPUT(ESQ_EVENT_BIT_TASK_DONE, "taskdone");
	OUTPUT(ESQ_EVENT_BIT_EXIT, "exit");

	if (offset > 0)	{
		buf[offset - 1] = '\0';
	}
	else	{
		return "BUFFER-TOO-SMALL";
	}

	return buf;
}

void
esq_session_init_mco_params(void * session_ptr, void * mco)	{
	esq_session_t * session = session_ptr;

	session->session_type = ESQ_SESSION_TYPE_MCO;
	session->mco = mco;

	return;
}

int
esq_session_check_magic(void * session_ptr)	{
	esq_session_t * session = session_ptr;

	ESQ_CHECK_SESSION(session);

	return ESQ_OK;
}

void *
esq_session_new(const char * name)	{
	esq_session_t * session;

	session = esq_malloc(esq_session_t);
	if (!session)	{
		return NULL;
	}
	bzero(session, sizeof(esq_session_t));
	session->magic0 = ESQ_SESSION_MAGIC;
	session->magic1 = ESQ_SESSION_MAGIC;
	session->name = name;
	session->session_type = ESQ_SESSION_TYPE_NEW;

	return session;
}

void
esq_session_set_param(void * session_ptr, void * user_param)	{
	esq_session_t * session = session_ptr;

	ESQ_CHECK_SESSION_VOID(session);

	session->user_param = user_param;

	return;
}

int
esq_session_set_cb(void * session_ptr, esq_event_cb_t cb)	{
	esq_session_t * session;

	session = session_ptr;

	ESQ_CHECK_SESSION(session);
	session->cb = cb;

	return ESQ_OK;
}

void *
esq_session_get_param(void * session_ptr)	{
	esq_session_t * session = session_ptr;

	ESQ_CHECK_SESSION_NULL(session);

	return session->user_param;
}

void *
esq_session_get_parent(void * session_ptr)	{
	esq_session_t * session = session_ptr;

	ESQ_CHECK_SESSION_NULL(session);

	return session->parent_session;
}

int
esq_session_get_fd(void * session_ptr)	{
	esq_session_t * session = session_ptr;

	ESQ_CHECK_SESSION_0(session);

	return session->fd;
}

int
esq_session_get_event_bits(void * session_ptr)	{
	esq_session_t * session = session_ptr;

	ESQ_CHECK_SESSION(session);

	return session->event_bits;
}

esq_hash_lock_ctx_t *
esq_alloc_hash_lock_ctx(esq_session_t * session)	{
	esq_hash_lock_ctx_t * p;
	int i;

	for (i = 0; i < ESQ_SESSION_HASH_LOCK_OWNER_MAX; i ++)	{
		p = session->hash_lock_ctx + i;
		if (p->session)	{
			continue;
		}
		else	{
			p->session = session;
			return p;
		}
	}

	p = malloc(sizeof(esq_hash_lock_ctx_t));
	if (p)	{
		bzero(p, sizeof(esq_hash_lock_ctx_t));
		p->session = session;
	}

	return p;
}

void
esq_free_hash_lock_ctx(esq_hash_lock_ctx_t * ctx)	{
	esq_session_t * session = ctx->session;

	if (!session)	{
		esq_design_error();
		return;
	}

	char * ps = (char *)session;
	char * pc = (char *)ctx;

	ctx->session = NULL;

	if (pc < ps)	{
		free(ctx);
	}
	else if (pc > (ps + sizeof(esq_session_t)))	{
		free(ctx);
	}
}

/* eof */
