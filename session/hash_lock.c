/* vi: set sw=4 ts=4: */

//#define PRINT_DBG_LOG

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "public/esq.h"
#include "public/libesq.h"
#include "internal/types.h"
#include "public/esqll.h"
#include "internal/log_print.h"

#include "internal/lock.h"
#include "internal/signals.h"
#include "internal/session.h"
#include "internal/threads.h"
#include "session/internal.h"
#include "session/core_ctx.h"

#define HASH_LOCK_MAX	128

typedef struct _esq_hash_lock_t	{
	pthread_spinlock_t lock;
	esq_hash_lock_ctx_t * ctx_head;
	esq_hash_lock_ctx_t * ctx_tail;
} esq_hash_lock_t;

static esq_hash_lock_t esq_hash_locks[HASH_LOCK_MAX];

#define CTX_JOIN_TAIL do	{ \
		hash_lock->ctx_tail->next = ctx; \
		ctx->next = NULL; \
		hash_lock->ctx_tail = ctx; \
	} while(0);

#define CTX_JOIN_AS_THE_FIRST	do	{ \
		hash_lock->ctx_head = ctx; \
		hash_lock->ctx_tail = ctx; \
		ctx->next = NULL; \
	} while(0);

#define JOIN_SESSION	do	{	\
	ctx->next_in_esq_session = ctx->session->hash_lock_ctx_list; \
	ctx->session->hash_lock_ctx_list = ctx; \
} while(0);

static int
esq_try_read_hash_lock(esq_hash_lock_t * hash_lock, esq_hash_lock_ctx_t * ctx)	{
	esq_hash_lock_ctx_t * p;

	if (hash_lock->ctx_head)	{
		for (p = hash_lock->ctx_head; p; p = p->next)	{
			if (ESQ_SESSION_HASH_LOCK_READ == p->state)	{
				continue;
			}
			else	{
				CTX_JOIN_TAIL;
				JOIN_SESSION;
				return ESQ_ERROR_BUSY;
			}
		}
		CTX_JOIN_TAIL;
		JOIN_SESSION;
		return ESQ_OK;
	}
	else	{
		CTX_JOIN_AS_THE_FIRST;
		JOIN_SESSION;
		return ESQ_OK;
	}
}

static int
esq_try_write_hash_lock(esq_hash_lock_t * hash_lock, esq_hash_lock_ctx_t * ctx)	{

	if (hash_lock->ctx_head)	{
		CTX_JOIN_TAIL;
		JOIN_SESSION;
		return ESQ_ERROR_BUSY;
	}
	else	{
		CTX_JOIN_AS_THE_FIRST;
		JOIN_SESSION;
		return ESQ_OK;
	}
}

static int
esq_hash_lock_check_dup(esq_hash_lock_t * hash_lock, esq_hash_lock_ctx_t * ctx)	{
	esq_hash_lock_ctx_t * p;
	esq_session_t * session;

	session = ctx->session;

	for (p = hash_lock->ctx_head; p; p = p->next)	{
		if (ctx == p)	{
			esq_design_error();
			return ESQ_ERROR_DEAD_LOCK;
		}
		if (p->session == session)	{
			esq_design_error();
			return ESQ_ERROR_DEAD_LOCK;
		}
	}

	return ESQ_OK;
}

int
esq_try_fetch_hash_lock(esq_session_t * session)	{

	esq_hash_lock_ctx_t * ctx;
	esq_hash_lock_t * lock;
	int idx;
	int r;

	idx = session->hash_lock_idx;

	if (idx < 0 || idx >= HASH_LOCK_MAX)	{
		esq_design_error();
		return ESQ_ERROR_PARAM;
	}

	if (esq_get_server_stop_flag())	{
		return ESQ_ERROR_CLOSE;
	}

	ctx = esq_alloc_hash_lock_ctx(session);
	if (ctx)	{
		ctx->state = session->hash_lock_state;
		ctx->idx = session->hash_lock_idx;
		ctx->worker_ctx = esq_worker_get_ctx();
	}
	else	{
		return ESQ_ERROR_MEMORY;
	}

	lock = esq_hash_locks + idx;

	if (ESQ_SESSION_HASH_LOCK_READ == session->hash_lock_state)	{
		pthread_spin_lock(&lock->lock);
		r = esq_hash_lock_check_dup(lock, ctx);
		if (r)	{
			esq_free_hash_lock_ctx(ctx);
		}
		else	{
			r = esq_try_read_hash_lock(lock, ctx);
		}
		pthread_spin_unlock(&lock->lock);
	}
	else if (ESQ_SESSION_HASH_LOCK_WRITE == session->hash_lock_state)	{
		pthread_spin_lock(&lock->lock);
		r = esq_hash_lock_check_dup(lock, ctx);
		if (r)	{
			esq_free_hash_lock_ctx(ctx);
		}
		else	{
			r = esq_try_write_hash_lock(lock, ctx);
		}
		pthread_spin_unlock(&lock->lock);
	}
	else	{
		esq_design_error();
		r = ESQ_ERROR_PARAM;
	}

	return r;
}

#define HASH_LOCK_CTX_JOIN_WORK_CTX	do	{	\
	worker_ctx = p->worker_ctx;	\
	if (worker_ctx->hash_lock_wake_up_list_head)	{	\
		worker_ctx->hash_lock_wake_up_list_tail->next_in_local_wake_up_list = p;	\
		worker_ctx->hash_lock_wake_up_list_tail = p;	\
		p->next_in_local_wake_up_list = NULL;	\
	}	\
	else	{	\
		worker_ctx->hash_lock_wake_up_list_head = p;	\
		worker_ctx->hash_lock_wake_up_list_tail = p;	\
		p->next_in_local_wake_up_list = NULL;	\
	}	\
}while(0);

static void
__esq_hash_lock_ctx_join_local_wake_up_list(esq_hash_lock_t * lock)	{
	esq_worker_ctx_t * worker_ctx;
	esq_hash_lock_ctx_t * p;

	p = lock->ctx_head;

	if (!p)	{
		return;
	}

	if (ESQ_SESSION_HASH_LOCK_READ == p->state)	{
		for (; p; p = p->next)	{
			if (ESQ_SESSION_HASH_LOCK_READ == p->state)	{
				HASH_LOCK_CTX_JOIN_WORK_CTX;
			}
			else	{
				break;
			}
		}
	}
	else if (ESQ_SESSION_HASH_LOCK_WRITE == p->state)	{
		HASH_LOCK_CTX_JOIN_WORK_CTX;
	}
	else	{
		esq_design_error();
	}

	return;
}

int
esq_hash_lock_release(esq_session_t * session, int idx)	{

	esq_hash_lock_ctx_t * pp;
	esq_hash_lock_ctx_t * p;
	esq_hash_lock_t * lock;

	if (idx < 0 || idx >= HASH_LOCK_MAX)	{
		esq_design_error();
		return ESQ_ERROR_PARAM;
	}

	lock = esq_hash_locks + idx;

	pthread_spin_lock(&lock->lock);

	p = lock->ctx_head;
	if (!p)	{
		pthread_spin_unlock(&lock->lock);
		esq_design_error();
		return ESQ_ERROR_PARAM;
	}

	if (ESQ_SESSION_HASH_LOCK_READ == p->state)	{
		for (pp = NULL;; pp = p, p = p->next)	{
			if (!p)	{
				pthread_spin_unlock(&lock->lock);
				esq_design_error();
				return ESQ_ERROR_PARAM;
			}

			if (ESQ_SESSION_HASH_LOCK_READ == p->state)	{
				if (p->session == session)	{
					if (pp)	{
						pp->next = p->next;
						goto __good_to_go;
					}
					else	{
						lock->ctx_head = p->next;
						if (ESQ_SESSION_HASH_LOCK_READ == p->next->state)	{
							goto __good_to_go;
						}
						else	{
							goto __wake_up_other_sessions;
						}
					}
				}
				else	{
					continue;
				}
			}
			else	{
				pthread_spin_unlock(&lock->lock);
				esq_design_error();
				return ESQ_ERROR_PARAM;
			}
		}
	}
	else if (ESQ_SESSION_HASH_LOCK_WRITE == p->state)	{
		if (p->session == session)	{
			lock->ctx_head = p->next;
			goto __wake_up_other_sessions;
		}
		else	{
			pthread_spin_unlock(&lock->lock);
			esq_design_error();
			return ESQ_ERROR_PARAM;
		}
	}
	else	{
		pthread_spin_unlock(&lock->lock);
		esq_design_error();
		return ESQ_ERROR_PARAM;
	}

__good_to_go:

	pthread_spin_unlock(&lock->lock);
	esq_free_hash_lock_ctx(p);
	return ESQ_OK;

__wake_up_other_sessions:

	esq_global_lock();

	__esq_hash_lock_ctx_join_local_wake_up_list(lock);

	esq_global_unlock();

	pthread_spin_unlock(&lock->lock);

	esq_free_hash_lock_ctx(p);

	return ESQ_OK;
}

void
esq_hash_lock_release_all(void)	{

	esq_worker_ctx_t * worker_ctx;
	esq_hash_lock_ctx_t * p;
	esq_hash_lock_t * lock;
	int i;

	if (!esq_get_server_stop_flag())	{
		esq_design_error();
		return;
	}

	for (i = 0; i < HASH_LOCK_MAX; i ++)	{
		lock = esq_hash_locks + i;
		pthread_spin_lock(&lock->lock);
		for (p = lock->ctx_head; p; p = p->next)	{
			esq_global_lock();
			HASH_LOCK_CTX_JOIN_WORK_CTX
			esq_global_unlock();
		}
		pthread_spin_unlock(&lock->lock);
	}

	return;
}

int
esq_hash_lock_init(void)	{
	int i;
	int r;

	bzero(esq_hash_locks, sizeof(esq_hash_locks));

	for (i = 0; i < HASH_LOCK_MAX; i ++)	{
		r = pthread_spin_init(&esq_hash_locks[i].lock, 0);
		if (r)	{
			printf("ERROR: %s() %d: failed to initial spinlock\n", __func__, __LINE__);
			return -1;
		}
	}

	return ESQ_OK;
}

void
esq_hash_lock_detach(void)	{
	int i;

	for (i = 0; i < HASH_LOCK_MAX; i ++)	{
		pthread_spin_destroy(&esq_hash_locks[i].lock);
	}
}

/* eof */
