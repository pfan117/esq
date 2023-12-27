//#define PRINT_DBG_LOG

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <monkeycall.h>

#include "public/esq.h"
#include "public/mco-utils.h"
#include "public/pony-bee.h"
#include "public/esqll.h"
#include "internal/types.h"
#include "internal/monkeycall.h"
#include "internal/log_print.h"
#define _ESQ_LIB_SOCKET_FUNCTIONS
#include "public/libesq.h"

#include "internal/range_map.h"
#include "bee/public.h"

#define PONY_IO_MAX_RETRY 6

// #define ADD_SLEEP

STATIC void *
pony_get_esq_session(void)	{
	void * esq_session;

	if (!mco_get_co())	{
		printf("ERROR: %s, %d, %s(): %s\n", __FILE__, __LINE__, __func__
				, "supposed to be called inside mco"
				);
		return NULL;
	}

	esq_session = mco_get_arg();
	if (!esq_session)	{
		printf("ERROR: %s, %d, %s(): %s\n", __FILE__, __LINE__, __func__
				, "failed to get parent esq_session"
				);
		return NULL;
	}

	if (esq_session_check_magic(esq_session))	{
		printf("ERROR: %s, %d, %s(): %s\n", __FILE__, __LINE__, __func__
				, "mco parameter is not a esq_session object"
				);
		return NULL;
	}

	return esq_session;
}

/* start to transmit the request and collect return data */
int
pony(const char * range_map_name
		, char * key, int key_len
		, char * data, int data_len
		, int opcode
		, char * result_buf
		, int * result_buf_size
		)
{
	header_from_pony_to_bee_t header;
	reply_from_bee_to_pony_t reply;
	struct sockaddr_in address;
	void * esq_session;
	struct timeval tv;
	hash_t hash;
	int fd;
	int r;

	if (!key || (key_len <= 0))	{
		return ESQ_ERROR_PARAM;
	}

	esq_session = pony_get_esq_session();
	if (!esq_session)	{
		return ESQ_ERROR_PARAM;
	}

	r = gettimeofday(&tv, NULL);
	if (r)	{
		printf("ERROR: %s() %d: gettimeofday() return error\n", __func__, __LINE__);
		return ESQ_ERROR_LIBCALL;
	}

	#if 0
	printf("get from sys: sec = %ld, usec = %ld\n",tv.tv_sec, tv.tv_usec);
	#endif

	get_hash_value(key, key_len, &hash);

	r = range_map_get_main_address(range_map_name, &hash, &address);
	if (r)	{
		return ESQ_ERROR_NOTFOUND;
	}

	#if 0
	fd = os_open_tcp_socket(1, 10);	/* blocking */
	#else
	fd = os_open_tcp_socket(0, 0);	/* non blocking, no timeout value */
	#endif
	if (-1 == fd)	{
		PRINTF("DBG: %s() %d\n", __func__, __LINE__);
		return ESQ_ERROR_LIBCALL;
	}

#if 0
	r = esq_session_set_socket_fd(esq_session, fd, NULL
			, (ESQ_EVENT_BIT_READ | ESQ_EVENT_BIT_WRITE)
			, 10, 0, NULL);
	if (r)	{
		close(fd);
		PRINTF("DBG: %s() %d: failed to add socket into esq session\n", __func__, __LINE__);
		return r;
	}
#endif

	header.opcode = opcode;
	header.range_map_name_len = strlen(range_map_name);
	header.key_len = key_len;
	header.data_len = data_len;
	if (result_buf && result_buf_size)	{
		header.result_buf_size = *result_buf_size;
	}
	else	{
		header.result_buf_size = 0;
	}

	r = esq_mco_socket_connect_write(fd
			, (struct sockaddr *)&address, sizeof(address)
			, (char *)&header, sizeof(header)
			);
	if (r)	{
		PRINTF("DBG: %s() %d: mco connect send error\n", __func__, __LINE__);
		goto __communication_error;
	}

	r = esq_mco_socket_write(fd, range_map_name, header.range_map_name_len + 1);
	if (r)	{
		PRINTF("DBG: %s() %d: mco send error\n", __func__, __LINE__);
		goto __communication_error;
	}

	#ifdef ADD_SLEEP
	sleep(1);
	#endif

	r = esq_mco_socket_write(fd, key, key_len);
	if (r)	{
		PRINTF("DBG: %s() %d: mco send error\n", __func__, __LINE__);
		goto __communication_error;
	}

	#ifdef ADD_SLEEP
	sleep(1);
	#endif

	r = esq_mco_socket_write(fd, (void *)&tv, sizeof(tv));
	if (r)	{
		PRINTF("DBG: %s() %d: mco send error\n", __func__, __LINE__);
		goto __communication_error;
	}

	#ifdef ADD_SLEEP
	sleep(1);
	#endif

	if (data && data_len > 0)	{
		r = esq_mco_socket_write(fd, data, data_len);
		if (r)	{
			PRINTF("DBG: %s() %d: mco send error\n", __func__, __LINE__);
			goto __communication_error;
		}
	}

	r = esq_mco_socket_read(fd, &reply, sizeof(reply));
	if (r)	{
		PRINTF("DBG: %s() %d: mco read error\n", __func__, __LINE__);
		goto __communication_error;
	}

	if (!reply.result_len)	{
		close(fd);
		if (result_buf_size)	{
			*result_buf_size = 0;
		}
		return reply.return_code;
	}

	if (!result_buf || !result_buf_size || *result_buf_size < reply.result_len)	{
		close(fd);
		if (result_buf_size)	{
			*result_buf_size = 0;
		}
		return ESQ_ERROR_SIZE;
	}

	r = esq_mco_socket_read(fd, &tv, sizeof(tv));
	if (r)	{
		close(fd);
		if (result_buf_size)	{
			*result_buf_size = 0;
		}
		return ESQ_ERROR_TBD;
	}

	r = esq_mco_socket_read(fd, result_buf, reply.result_len);
	if (r)	{
		close(fd);
		if (result_buf_size)	{
			*result_buf_size = 0;
		}
		return r;
	}

	*result_buf_size = reply.result_len;

	close(fd);

	return reply.return_code;

__communication_error:

	close(fd);
	return r;
}

typedef struct _pony_monkey_param_t	{
	char * name;
	void * data;
	int data_length;
	int name_length;	/* string length includes the terminating zero */
	int type;
	int value;
} pony_monkey_param_t;

#define PONY_MONKEY_CTX_MAGIC	0xAEEA1253

typedef struct _pony_monkey_ctx_t	{
	char * script;
	int script_length;	/* string length includes the terminating zero */
	int values_max;
	int values_cnt;
	int magic;
	pony_monkey_param_t params[];
} pony_monkey_ctx_t;

#define MAX_VALUES	20

void *
pony_monkey_ctx_new(int values_max)	{
	int sz;
	pony_monkey_ctx_t * p;

	if (values_max > MAX_VALUES)	{
		return NULL;
	}

	sz = sizeof(pony_monkey_ctx_t) + (sizeof(pony_monkey_param_t) * values_max);
	p = malloc(sz);

	if (!p)	{
		return NULL;
	}

	bzero(p, sz);
	p->values_max = values_max;
	p->magic = PONY_MONKEY_CTX_MAGIC;

	return p;
}

void
pony_monkey_ctx_free(void * ctx)	{
	pony_monkey_ctx_t * p = (pony_monkey_ctx_t *) ctx;
	if (PONY_MONKEY_CTX_MAGIC != p->magic)	{
		esq_design_error();
	}
	free(ctx);
	return;
}

int
pony_monkey_ctx_set_script2(void * ctx, char * script, int length)	{
	pony_monkey_ctx_t * p = (pony_monkey_ctx_t *) ctx;
	if (PONY_MONKEY_CTX_MAGIC != p->magic)	{
		esq_design_error();
		return ESQ_ERROR_PARAM;
	}
	p->script = script;
	p->script_length = length;
	return ESQ_OK;
}

int
pony_monkey_ctx_set_script(void * ctx, char * script)	{
	int length;
	int r;
	length = strlen(script) + 1;
	r = pony_monkey_ctx_set_script2(ctx, script, length);
	return r;
}

int
pony_monkey_ctx_add_int_param(void * ctx, char * name, int value)	{
	pony_monkey_ctx_t * p = (pony_monkey_ctx_t *) ctx;
	int cnt;

	if (PONY_MONKEY_CTX_MAGIC != p->magic)	{
		esq_design_error();
		return ESQ_ERROR_PARAM;
	}

	cnt = p->values_cnt;

	if (cnt >= p->values_max)	{
		esq_design_error();
		return ESQ_ERROR_SIZE;
	}

	p->params[cnt].type = MKC_D_INT;
	p->params[cnt].name = name;
	p->params[cnt].name_length = strlen(name) + 1;
	p->params[cnt].value = value;
	p->params[cnt].data_length = sizeof(value);
	p->values_cnt ++;

	return ESQ_OK;
}

STATIC int
__pony_monkey_ctx_add_buf_param(void * ctx, char * name, void * buf, int buf_len, int type)	{
	pony_monkey_ctx_t * p = (pony_monkey_ctx_t *) ctx;
	int cnt;

	if (PONY_MONKEY_CTX_MAGIC != p->magic)	{
		esq_design_error();
		return ESQ_ERROR_PARAM;
	}

	cnt = p->values_cnt;

	if ((unsigned)buf_len > 0xffff)	{
		return ESQ_ERROR_SIZE;
	}

	if (cnt >= p->values_max)	{
		esq_design_error();
		return ESQ_ERROR_SIZE;
	}

	p->params[cnt].type = type;
	p->params[cnt].name = name;
	p->params[cnt].name_length = strlen(name) + 1;
	p->params[cnt].data = buf;
	p->params[cnt].data_length = buf_len;
	p->values_cnt ++;

	return ESQ_OK;
}

int
pony_monkey_ctx_add_buf_param(void * ctx, char * name, void * buf, int buf_len)	{
	return __pony_monkey_ctx_add_buf_param(ctx, name, buf, buf_len, MKC_D_BUF);
}

int
pony_monkey_ctx_add_str_param(void * ctx, char * name, char * str, int str_len)	{
	return __pony_monkey_ctx_add_buf_param(ctx, name, str, str_len, MKC_D_ID);
}

STATIC int
pony_check_ctx(pony_monkey_ctx_t * p)	{
	if (PONY_MONKEY_CTX_MAGIC != p->magic)	{
		esq_design_error();
		return ESQ_ERROR_PARAM;
	}

	if (!p->script)	{
		return ESQ_ERROR_PARAM;
	}

	if (p->script_length <= 0)	{
		return ESQ_ERROR_PARAM;
	}

	return ESQ_OK;
}

STATIC int
pony_monkey_get_data_length(pony_monkey_ctx_t * p)	{
	int i;
	int s;

	s = 0;

	for (i = 0; i < p->values_cnt; i ++)	{
		s = s + 4 + p->params[i].data_length + p->params[i].name_length;
	}

	return s;
}

STATIC int
pony_monkey_send_data(int fd, pony_monkey_ctx_t * p)	{
	unsigned char header[4];
	int i;
	int r;

	for (i = 0; i < p->values_cnt; i ++)	{

		header[0] = ((p->params[i].type << 2) & 0xc);
		header[1] = (unsigned char)p->params[i].name_length;
		header[2] = (unsigned char)((p->params[i].data_length >> 8) & 0xff);
		header[3] = (unsigned char)(p->params[i].data_length & 0xff);

		r = esq_mco_socket_write(fd, (void *)header, sizeof(header));
		if (r)	{
			return ESQ_ERROR_LIBCALL;
		}

		r = esq_mco_socket_write(fd, p->params[i].name, p->params[i].name_length);
		if (r)	{
			return r;
		}

		if (MKC_D_INT == p->params[i].type)	{
			r = esq_mco_socket_write(fd, (void *)&p->params[i].value, sizeof(p->params[i].value));
		}
		else if (MKC_D_BUF == p->params[i].type)	{
			r = esq_mco_socket_write(fd, p->params[i].data, p->params[i].data_length);
		}
		else if (MKC_D_ID == p->params[i].type)	{
			r = esq_mco_socket_write(fd, p->params[i].data, p->params[i].data_length);
		}
		else	{
			esq_design_error();
			return ESQ_ERROR_LIBCALL;
		}

		if (r)	{
			return r;
		}
	}

	return ESQ_OK;
}

int
pony_monkey(const char * range_map_name
		, void * ctx
		, char * result_buf
		, int * result_buf_size
		)
{
	pony_monkey_ctx_t * p = (pony_monkey_ctx_t *) ctx;
	header_from_pony_to_bee_t header;
	monkey_reply_from_bee_to_pony_t reply;
	struct sockaddr_in address;
	void * esq_session;
	struct timeval tv;
	hash_t hash;
	int fd;
	int r;
	int data_length;

	r = pony_check_ctx(p);
	if (r)	{
		return r;
	}

	esq_session = pony_get_esq_session();
	if (!esq_session)	{
		return ESQ_ERROR_PARAM;
	}

	r = gettimeofday(&tv, NULL);
	if (r)	{
		return ESQ_ERROR_LIBCALL;
	}

	#if 0
	printf("get from sys: sec = %ld, usec = %ld\n",tv.tv_sec, tv.tv_usec);
	#endif

	get_hash_value(p->script, p->script_length - 1, &hash);

	r = range_map_get_main_address(range_map_name, &hash, &address);
	if (r)	{
		return ESQ_ERROR_NOTFOUND;
	}

	#if 0
	fd = os_open_tcp_socket(1, 10);	/* blocking */
	#else
	fd = os_open_tcp_socket(0, 10);
	#endif
	if (-1 == fd)	{
		return ESQ_ERROR_LIBCALL;
	}

	r = esq_session_set_socket_fd(esq_session, fd, NULL
			, (ESQ_EVENT_BIT_READ | ESQ_EVENT_BIT_WRITE)
			, 0, 0, NULL);
	if (r)	{
		close(fd);
		return r;
	}

	data_length = pony_monkey_get_data_length(p);

	header.opcode = BEE_OP_MONKEY;
	header.range_map_name_len = strlen(range_map_name);
	header.key_len = p->script_length;
	header.data_len = data_length;
	if (result_buf && result_buf_size)	{
		header.result_buf_size = *result_buf_size;
	}
	else	{
		header.result_buf_size = 0;
	}

	r = esq_mco_socket_connect_write(fd, (struct sockaddr *)&address, sizeof(address), (char *)&header, sizeof(header));
	if (r)	{
		goto __communication_error;
	}

	r = esq_mco_socket_write(fd, range_map_name, header.range_map_name_len + 1);
	if (r)	{
		goto __communication_error;
	}

	r = esq_mco_socket_write(fd, p->script, p->script_length);
	if (r)	{
		goto __communication_error;
	}

	r = esq_mco_socket_write(fd, (void *)&tv, sizeof(tv));
	if (r)	{
		goto __communication_error;
	}

	r = pony_monkey_send_data(fd, p);
	if (r)	{
		goto __communication_error;
	}

	r = esq_mco_socket_read(fd, &reply, sizeof(reply));
	if (r)	{
		goto __communication_error;
	}

	if (!result_buf || !result_buf_size)	{
		/* the result is not cared about */
		close(fd);
		return reply.return_code;
	}

	if (!reply.result_len)	{
		close(fd);
		*result_buf_size = 0;
		return reply.return_code;
	}

	if (ESQ_OK != reply.return_code)	{
		close(fd);
		*result_buf_size = 0;
		return reply.return_code;
	}

	int required_length;

	if (MKC_D_BUF == reply.type)	{
		required_length = reply.result_len + 1;
	}
	else	{
		required_length = reply.result_len;
	}

	if (*result_buf_size < required_length)	{
		close(fd);
		*result_buf_size = 0;
		return ESQ_ERROR_SIZE;
	}

	r = esq_mco_socket_read(fd, result_buf, reply.result_len);
	if (r)	{
		close(fd);
		*result_buf_size = 0;
		return r;
	}

	if (MKC_D_BUF == reply.type)	{
		/* add terminating zero */
		result_buf[reply.result_len] = '\0';
	}

	*result_buf_size = reply.result_len;

	close(fd);

	return reply.return_code;

__communication_error:

	close(fd);

	return r;
}

/* eof */
