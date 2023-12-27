#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <monkeycall.h>

#include "public/esq.h"
#include "public/libesq.h"
#include "public/esqll.h"
#include "internal/threads.h"

STATIC void * esq_mkc_instance = NULL;

/* user provided functions for monkey call library */
void mkc_user_provide_free(void * p)	{
	free(p);
}

void * mkc_user_provide_malloc(int size, const char * filename, int lineno)	{
	return malloc(size);
}

#define CORE_INFO_BUF_MAX	1024
STATIC int
esq_mkccb_get_core_info(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	int offset = 0;
	char * buf;
	int argc;
	int r;

	argc = MKC_CB_ARGC;
	if (argc != 0)	{
		MKC_CB_ERROR("%s", "need 0 parameter");
		return -1;
	}

	result = MKC_CB_RESULT;
	result->type = MKC_D_VOID;

	buf = mkc_session_malloc(session, CORE_INFO_BUF_MAX);
	if (!buf)	{
		return -1;
	}

	r = esq_threads_dump_info(buf, CORE_INFO_BUF_MAX - offset);
	if (r <= 0 || r >= (CORE_INFO_BUF_MAX - offset))	{
		mkc_session_free(session, buf);
		return -1;
	}
	offset += r;

	result->type = MKC_D_BUF;
	result->buffer = buf;
	result->length = offset;

	return 0;

}

/* initial/detach for esq core */
int
esq_mkc_init(void)	{
	int r;

	esq_mkc_instance = mkc_new_instance();
	if (!esq_mkc_instance)	{
		fprintf(stderr, "error: %s, %d: failed to create monkeycall instance\n", __func__, __LINE__);
		return -1;
	}

	r = mkc_buildin_cbs_init(esq_mkc_instance);
	if (r)	{
		fprintf(stderr, "error: %s, %d: failed to install monkeycall internal callbacks\n", __func__, __LINE__);
		return -1;
	}

	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(esq_mkc_instance, "GetESQCoreInfo", esq_mkccb_get_core_info));

	return 0;
}

void
esq_mkc_detach(void)	{

	if (esq_mkc_instance)	{
		mkc_free_instance(esq_mkc_instance);
		esq_mkc_instance = NULL;
	}
	return;
}

void *
esq_get_mkc(void)	{
	return esq_mkc_instance;
}

int
esq_mkc_execute(const char * code, int codelen, char * errbuf, int * errlen)
{
	void * instance;
	mkc_session * session;
	int r;

	instance = esq_get_mkc();

	session = mkc_new_session(instance, code, codelen);
	if (!session)	{
		*errlen = 0;
		return ESQ_ERROR_MEMORY;
	}

	mkc_session_set_error_info_buffer(session, errbuf, *errlen);

	r = mkc_parse(session);
	if (r)	{
		*errlen = strlen(errbuf);
		mkc_free_session(session);
		return ESQ_ERROR_PARAM;
	}

	r = mkc_go(session);
	if (r)	{
		*errlen = strlen(errbuf);
		mkc_free_session(session);
		return ESQ_ERROR_PARAM;
	}
	else	{
		*errlen = 0;
		mkc_free_session(session);
		return ESQ_OK;
	}
}

/* eof */
