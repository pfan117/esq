#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <monkeycall.h>

#include "public/esq.h"
#include "public/libesq.h"
#include "public/esqll.h"
#include "internal/tree.h"
#include "internal/dashboard.h"

#define DASHBOARD_BUFFER_SIZE		(1024 * 4)
typedef struct _esq_dashboard_item_t	{
	int id;
	int opcode;
	RB_ENTRY(_esq_dashboard_item_t) rb;
	int buf_string_length;
	char buf[DASHBOARD_BUFFER_SIZE];
} esq_dashboard_item_t;

/* dashboard item rb tree */
STATIC int
esq_dashboard_item_compare(esq_dashboard_item_t * a, esq_dashboard_item_t * b)	{
	return a->id - b->id;
}

RB_HEAD(ESQ_DASHBOARD, _esq_dashboard_item_t);
RB_PROTOTYPE_STATIC(ESQ_DASHBOARD, _esq_dashboard_item_t, rb, esq_dashboard_item_compare);
RB_GENERATE_STATIC(ESQ_DASHBOARD, _esq_dashboard_item_t, rb, esq_dashboard_item_compare);

static struct ESQ_DASHBOARD esq_dashboard_rb;
static volatile int esq_dashboard_id_max = 0;

static pthread_spinlock_t esq_dashboard_spinlock;

/* return -1 for error */
/* return >= 0 for success */
int
esq_dashboard_new(void)	{
	esq_dashboard_item_t * p;
	esq_dashboard_item_t * e;
	int i;

	p = (void *)malloc(sizeof(esq_dashboard_item_t));
	if (!p)	{
		return -1;
	}

	bzero(p, sizeof(esq_dashboard_item_t));

	pthread_spin_lock(&esq_dashboard_spinlock);

	for (i = esq_dashboard_id_max;; i ++)	{

		if (i < 0)	{
			i = 0;
		}

		p->id = i;

		e = ESQ_DASHBOARD_RB_FIND(&esq_dashboard_rb, p);
		if (e)	{
			continue;
		}
		else	{
			break;
		}
	}

	ESQ_DASHBOARD_RB_INSERT(&esq_dashboard_rb, p);

	esq_dashboard_id_max = i + 1;

	pthread_spin_unlock(&esq_dashboard_spinlock);

	return i;
}

int
esq_dashboard_set_opcode(int id, int opcode)	{
	esq_dashboard_item_t * e;
	esq_dashboard_item_t n;

	n.id = id;

	pthread_spin_lock(&esq_dashboard_spinlock);

	e = ESQ_DASHBOARD_RB_FIND(&esq_dashboard_rb, &n);
	if (!e)	{
		pthread_spin_unlock(&esq_dashboard_spinlock);
		return ESQ_ERROR_NOTFOUND;
	}

	e->opcode = opcode;

	pthread_spin_unlock(&esq_dashboard_spinlock);

	return ESQ_OK;
}

int
esq_dashboard_get_opcode(int id, int *opcode)	{
	esq_dashboard_item_t * e;
	esq_dashboard_item_t n;

	n.id = id;

	pthread_spin_lock(&esq_dashboard_spinlock);

	e = ESQ_DASHBOARD_RB_FIND(&esq_dashboard_rb, &n);
	if (!e)	{
		pthread_spin_unlock(&esq_dashboard_spinlock);
		return ESQ_ERROR_NOTFOUND;
	}

	*opcode = e->opcode;

	pthread_spin_unlock(&esq_dashboard_spinlock);

	return ESQ_OK;
}

/* return the size of data that write into the buffer */
int
esq_dashboard_snprintf(int id, int offset, const char * format, ...)	{
	esq_dashboard_item_t * e;
	esq_dashboard_item_t n;
	int r;

	if (offset < 0 || offset >= DASHBOARD_BUFFER_SIZE)	{
		return -1;
	}

	n.id = id;

	pthread_spin_lock(&esq_dashboard_spinlock);

	e = ESQ_DASHBOARD_RB_FIND(&esq_dashboard_rb, &n);
	if (!e)	{
		pthread_spin_unlock(&esq_dashboard_spinlock);
		return -1;
	}

	{
		va_list args;
		va_start(args, format);
		r = vsnprintf(e->buf + offset, DASHBOARD_BUFFER_SIZE - offset, format, args);
		va_end(args);
		if (r <= 0 || r >= DASHBOARD_BUFFER_SIZE)	{
			pthread_spin_unlock(&esq_dashboard_spinlock);
			return r;
		}
		else	{
			e->buf_string_length = r;
		}
	}

	pthread_spin_unlock(&esq_dashboard_spinlock);

	return r;
}

STATIC int
esq_dashboard_notify_stop_to_all_tasks(void)	{
	esq_dashboard_item_t * p;
	int i = 0;

	pthread_spin_lock(&esq_dashboard_spinlock);

	RB_FOREACH(p, ESQ_DASHBOARD, &esq_dashboard_rb)	{
		p->opcode = ESQ_DASHBOARD_OP_STOP;
		i ++;
	}

	pthread_spin_unlock(&esq_dashboard_spinlock);

	return i;
}

STATIC void
esq_dashboard_remove_all_items(void)	{
	esq_dashboard_item_t * p;

	pthread_spin_lock(&esq_dashboard_spinlock);

	for (p = RB_MIN(ESQ_DASHBOARD, &esq_dashboard_rb)
			; p
			; p = RB_MIN(ESQ_DASHBOARD, &esq_dashboard_rb)
			)
	{
		ESQ_DASHBOARD_RB_REMOVE(&esq_dashboard_rb, p);
		free(p);
	}

	pthread_spin_unlock(&esq_dashboard_spinlock);

	return;
}

STATIC int
esq_mkc_remove_all_dashboard_items(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;

	result = MKC_CB_RESULT;
	result->type = MKC_D_VOID;

	esq_dashboard_remove_all_items();

	return 0;
}

STATIC int
esq_dashboard_list_all_items(char * buf, int buflen)	{
	esq_dashboard_item_t * p;
	int offset;
	int r;

	pthread_spin_lock(&esq_dashboard_spinlock);

	offset = 0;

	for (p = RB_MIN(ESQ_DASHBOARD, &esq_dashboard_rb); p ; p = ESQ_DASHBOARD_RB_NEXT(p))	{
		if (offset)	{
			r = snprintf(buf + offset, buflen - offset, " %d", p->id);
		}
		else	{ 
			r = snprintf(buf + offset, buflen - offset, "%d", p->id);
		}

		if (r > 0 && r < buflen - offset)	{
			offset += r;
			continue;
		}
		else	{
			break;
		}
	}

	pthread_spin_unlock(&esq_dashboard_spinlock);

	return offset;
}

#define DASHBOARD_LIST_BUFFER_SIZE	2048

STATIC int
esq_mkc_list_all_dashboard_items(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	char * buf;
	int buflen;

	result = MKC_CB_RESULT;
	result->type = MKC_D_VOID;

	buflen = DASHBOARD_LIST_BUFFER_SIZE;
	buf = mkc_session_malloc(session, buflen);
	if (!buf)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, "out of memory");
		return -1;
	}

	bzero(buf, buflen);

	buflen = esq_dashboard_list_all_items(buf, buflen);

	result->type = MKC_D_BUF;
	result->buffer = buf;
	result->length = buflen;

	return 0;
}

#define DASHBOARD_DETACH_ALERT_CYCLE	100
STATIC void
esq_dashboard_detach_loop(void)	{
	int i;
	int r;

	i = 0;

	for (i = 0; ; i ++)	{

		if (DASHBOARD_DETACH_ALERT_CYCLE == i)	{
			#if 0
			fprintf(stderr, "warning: %s, %d: some dashboard item has not been closed in time, force removed\n", __FILE__, __LINE__);
			#endif
			break;
		}

		r = esq_dashboard_notify_stop_to_all_tasks();
		if (r)	{
			usleep(100);
		}
		else	{
			break;
		}
	}

	esq_dashboard_remove_all_items();

	return;
}

STATIC int
esq_dashboard_get_item_count(void)	{
	esq_dashboard_item_t * p;
	int i = 0;

	pthread_spin_lock(&esq_dashboard_spinlock);

	RB_FOREACH(p, ESQ_DASHBOARD, &esq_dashboard_rb)	{
		i ++;
	}

	pthread_spin_unlock(&esq_dashboard_spinlock);

	return i;
}

STATIC int
esq_mkc_get_dashboard_item_count(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;

	result = MKC_CB_RESULT;
	result->type = MKC_D_INT;
	result->integer = esq_dashboard_get_item_count();

	return 0;

}

STATIC int
esq_mkc_new_dashboard_item(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	int r;

	result = MKC_CB_RESULT;

	r = esq_dashboard_new();
	result->type = MKC_D_INT;
	result->integer = r;

	return 0;
}

STATIC const char *
esq_dashboard_get_item_string(int id, char * buf, int * buflen)	{
	esq_dashboard_item_t * e;
	esq_dashboard_item_t n;

	n.id = id;

	pthread_spin_lock(&esq_dashboard_spinlock);

	e = ESQ_DASHBOARD_RB_FIND(&esq_dashboard_rb, &n);
	if (!e)	{
		pthread_spin_unlock(&esq_dashboard_spinlock);
		return "dashboard item not found";
	}

	if (e->buf_string_length)	{
		if (e->buf_string_length > *buflen)	{
			pthread_spin_unlock(&esq_dashboard_spinlock);
			return "design issue, buffer size too small";
		}
		memcpy(buf, e->buf, e->buf_string_length + 1);
		*buflen = e->buf_string_length;
	}
	else	{
		*buflen = 0;
	}

	pthread_spin_unlock(&esq_dashboard_spinlock);

	return NULL;
}

STATIC int
esq_mkc_get_dashboard_item_string(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	const char * errs;
	mkc_data * result;
	mkc_data * id;
	int buflen;
	char * buf;

	MKC_CB_ARGC_CHECK(1);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_INT);

	result = MKC_CB_RESULT;
	id = MKC_CB_ARGV(0);
	result->type = MKC_D_VOID;


	buflen = DASHBOARD_BUFFER_SIZE;
	buf = mkc_session_malloc(session, buflen);
	if (!buf)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, "out of memory");
		return -1;
	}

	bzero(buf, buflen);

	errs = esq_dashboard_get_item_string(id->integer, buf, &buflen);
	if (errs)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, errs);
		return -1;
	}
	else	{
		result->type = MKC_D_BUF;
		result->buffer = buf;
		result->length = buflen;

		return 0;
	}
}

STATIC const char *
esq_dashboard_get_item_opcode(int id, int * opcode)	{
	esq_dashboard_item_t * e;
	esq_dashboard_item_t n;

	n.id = id;

	pthread_spin_lock(&esq_dashboard_spinlock);

	e = ESQ_DASHBOARD_RB_FIND(&esq_dashboard_rb, &n);
	if (!e)	{
		pthread_spin_unlock(&esq_dashboard_spinlock);
		return "dashboard item not found";
	}

	*opcode = e->opcode;

	pthread_spin_unlock(&esq_dashboard_spinlock);

	return NULL;
}

STATIC int
esq_mkc_get_dashboard_item_opcode(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	const char * errs;
	mkc_data * result;
	mkc_data * id;
	int opcode;

	MKC_CB_ARGC_CHECK(1);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_INT);

	result = MKC_CB_RESULT;
	id = MKC_CB_ARGV(0);
	result->type = MKC_D_VOID;

	errs = esq_dashboard_get_item_opcode(id->integer, &opcode);
	if (errs)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, errs);
		return -1;
	}
	else	{
		result->type = MKC_D_INT;
		result->integer = opcode;

		return 0;
	}
}

STATIC const char *
esq_dashboard_set_item_opcode(int id, int opcode)	{
	esq_dashboard_item_t * e;
	esq_dashboard_item_t n;

	n.id = id;

	pthread_spin_lock(&esq_dashboard_spinlock);

	e = ESQ_DASHBOARD_RB_FIND(&esq_dashboard_rb, &n);
	if (!e)	{
		pthread_spin_unlock(&esq_dashboard_spinlock);
		return "dashboard item not found";
	}

	e->opcode = opcode;

	pthread_spin_unlock(&esq_dashboard_spinlock);

	return NULL;
}

STATIC const char *
esq_dashboard_set_item_string(int id, const char * string, int string_length)	{
	esq_dashboard_item_t * e;
	esq_dashboard_item_t n;

	n.id = id;

	if (string_length >= DASHBOARD_BUFFER_SIZE)	{
		return "string too long";
	}

	pthread_spin_lock(&esq_dashboard_spinlock);

	e = ESQ_DASHBOARD_RB_FIND(&esq_dashboard_rb, &n);
	if (!e)	{
		pthread_spin_unlock(&esq_dashboard_spinlock);
		return "dashboard item not found";
	}

	memcpy(e->buf, string, string_length + 1);
	e->buf_string_length = string_length;

	pthread_spin_unlock(&esq_dashboard_spinlock);

	return NULL;
}

STATIC int
esq_mkc_set_dashboard_item_opcode(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	const char * errs;
	mkc_data * result;
	mkc_data * opcode;
	mkc_data * id;

	MKC_CB_ARGC_CHECK(2);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_INT);
	MKC_CB_ARG_TYPE_CHECK(1, MKC_D_INT);

	result = MKC_CB_RESULT;
	id = MKC_CB_ARGV(0);
	opcode = MKC_CB_ARGV(1);
	result->type = MKC_D_VOID;

	errs = esq_dashboard_set_item_opcode(id->integer, opcode->integer);
	if (errs)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, errs);
		return -1;
	}

	return 0;
}

STATIC int
esq_mkc_set_dashboard_item_string(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	const char * errs;
	mkc_data * result;
	mkc_data * string;
	mkc_data * id;

	MKC_CB_ARGC_CHECK(2);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_INT);
	MKC_CB_ARG_TYPE_CHECK(1, MKC_D_BUF);

	result = MKC_CB_RESULT;
	id = MKC_CB_ARGV(0);
	string = MKC_CB_ARGV(1);
	result->type = MKC_D_VOID;

	errs = esq_dashboard_set_item_string(id->integer, string->buffer, string->length);
	if (errs)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, errs);
		return -1;
	}

	return 0;
}

int
esq_dashboard_remove_item(int id)	{
	esq_dashboard_item_t * e;
	esq_dashboard_item_t n;

	n.id = id;

	pthread_spin_lock(&esq_dashboard_spinlock);

	e = ESQ_DASHBOARD_RB_FIND(&esq_dashboard_rb, &n);
	if (!e)	{
		pthread_spin_unlock(&esq_dashboard_spinlock);
		return -1;
	}

	ESQ_DASHBOARD_RB_REMOVE(&esq_dashboard_rb, e);

	free(e);

	pthread_spin_unlock(&esq_dashboard_spinlock);

	return 0;
}

STATIC const char *
esq_dashboard_remove_item2(int id)	{
	int r;

	r = esq_dashboard_remove_item(id);

	if (r)	{
		return "dashboard item not found";
	}

	return NULL;
}

STATIC int
esq_mkc_remove_dashboard_item(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	const char * errs;
	mkc_data * result;
	mkc_data * id;

	MKC_CB_ARGC_CHECK(1);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_INT);

	result = MKC_CB_RESULT;
	id = MKC_CB_ARGV(0);
	result->type = MKC_D_VOID;

	errs = esq_dashboard_remove_item2(id->integer);
	if (errs)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, errs);
		return -1;
	}
	else	{
		return 0;
	}
}

int
esq_dashboard_init(void)	{
	void * mkc;
	int r;

	esq_dashboard_id_max = 0;

	RB_INIT(&esq_dashboard_rb);

	r = pthread_spin_init(&esq_dashboard_spinlock, 0);
	if (r)	{
		fprintf(stderr, "error: %s, %d, %s(): %s\n", __FILE__, __LINE__, __func__, "failed to initialize dashboard spinlock");
		return ESQ_ERROR_LIBCALL;
	}

	mkc = esq_get_mkc();
	if (!mkc)	{
		fprintf(stderr, "error: %s, %d, %s(): %s\n", __FILE__, __LINE__, __func__, "failed to get mkc instance");
		return ESQ_ERROR_PARAM;
	}

	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "NewDashboardItem", esq_mkc_new_dashboard_item));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "GetDashboardItemCount", esq_mkc_get_dashboard_item_count));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "GetDashboardItemString", esq_mkc_get_dashboard_item_string));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "GetDashboardItemOpcode", esq_mkc_get_dashboard_item_opcode));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "SetDashboardItemOpcode", esq_mkc_set_dashboard_item_opcode));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "SetDashboardItemString", esq_mkc_set_dashboard_item_string));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "RemoveDashboardItem", esq_mkc_remove_dashboard_item));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "RemoveAllDashboardItems", esq_mkc_remove_all_dashboard_items));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "ListAllDashboardItems", esq_mkc_list_all_dashboard_items));

	return ESQ_OK;
}

void
esq_dashboard_stop(void)	{
	esq_dashboard_detach_loop();
}

void
esq_dashboard_detach(void)	{
	pthread_spin_destroy(&esq_dashboard_spinlock);
	return;
}

/* eof */
