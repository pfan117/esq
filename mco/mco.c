/* vi: set sw=4 ts=4: */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <strings.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "public/esq.h"
#include "public/mco-utils.h"
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

typedef struct _esq_service_mco_startup_param_t	{
	void * parent_session;
	void * user_param;
	esq_service_mco_entry_t entry;
} esq_service_mco_startup_param_t;

STATIC void
esq_mco_session_entry(void)	{
	esq_service_mco_startup_param_t * param;
	esq_service_mco_entry_t entry;
	void * user_param;
	void * session;

	session = mco_get_arg();
	if (!session)	{
		printf("ERROR: %s() %d: failed to get mco session\n", __func__, __LINE__);
		return;
	}

	param = esq_session_get_param(session);
	if (!param)	{
		printf("ERROR: %s() %d: failed to get mco session param\n", __func__, __LINE__);
		return;
	}

	user_param = param->user_param;
	entry = param->entry;

	free(param);

	if (entry)	{
		entry(user_param);
	}
	else	{
		printf("ERROR: %s() %d: no service mco entry\n", __func__, __LINE__);
	}

	return;
}

int
esq_start_mco_session(
		const char * name
		, void * parent_session
		, esq_service_mco_entry_t entry
		, size_t stack_size
		, void * service_mco_param
		)
{
	esq_service_mco_startup_param_t * param;
	void * session;
	void * mco;

	param = malloc(sizeof(esq_service_mco_startup_param_t));
	if (!param)	{
		return ESQ_ERROR_MEMORY;
	}

	param->parent_session = parent_session;
	param->user_param = service_mco_param;
	param->entry = entry;

	session = esq_session_new(name);
	if (!session)	{
		free(param);
		return ESQ_ERROR_MEMORY;
	}

	esq_session_set_param(session, param);

	mco = mco_create(esq_mco_session_entry, session, stack_size);
	if (!mco)	{
		free(param);
		esq_session_wind_up(session);
		return ESQ_ERROR_MEMORY;
	}

	esq_session_init_mco_params(session, mco);

	esq_session_wind_up(session);

	return ESQ_OK;
}

/* eof */

