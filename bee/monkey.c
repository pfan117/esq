#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <monkeycall.h>

#define _ESQ_LIB_SOCKET_FUNCTIONS
#include "public/esq.h"
#include "public/libesq.h"
#include "public/esqll.h"
#include "internal/types.h"
#include "internal/range_map.h"

#include "bee/public.h"
#include "bee/internal.h"

/* mkc session params */
typedef struct _esq_session_param_des_t	{
	char * payload;
	int length;
} esq_session_param_des_t;

STATIC int
esq_bee_monkey_execute(
		char * script, int script_length,
		char * data, int data_length,
		char * reply, int * reply_length,
		int * reply_type
		)
{
	esq_session_param_des_t session_param_des;
	mkc_session * session;
	void * instance;
	int r;
	int l;

	instance = esq_get_mkc();
	session = mkc_new_session(instance, script, script_length);
	if (!session)	{
		r = ESQ_ERROR_MEMORY;
		goto __exx;
	}

	if (data && data_length > 0)	{
		session_param_des.payload = data;
		session_param_des.length = data_length;
		mkc_session_set_user_param0(session, &session_param_des);
	}

	if (reply)	{
		mkc_session_set_error_info_buffer(session, reply, *reply_length);
	}

	r = mkc_parse(session);
	if (r)	{
		goto __exx;
	}

	r = mkc_go(session);
	if (r)	{
		goto __exx;
	}

	if (!reply)	{
		/* no data required */
		return ESQ_OK;
	}

	if (MKC_D_BUF == session->result.type)	{
		/* reply buffer */
		if (session->result.length > 0 || session->result.length <= *reply_length)	{
			memcpy(reply, session->result.buffer, session->result.length);
			*reply_length = session->result.length;
			*reply_type = MKC_D_BUF;
		}
		else	{
			*reply_length = 0;
			*reply_type = MKC_D_VOID;
		}
	}
	else if (MKC_D_INT == session->result.type)	{
		/* reply integer */
		if (*reply_length >= sizeof(session->result.integer))	{
			memcpy(reply, (char *)&(session->result.integer), sizeof(session->result.integer));
			*reply_length = sizeof(session->result.integer);
			*reply_type = MKC_D_INT;
		}
		else	{
			*reply_length = 0;
			*reply_type = MKC_D_VOID;
		}
	}
	else	{
		*reply_length = 0;
		*reply_type = MKC_D_VOID;
	}

	mkc_free_session(session);

	return ESQ_OK;

__exx:

	if (session)	{
		mkc_free_session(session);
	}

	if (!reply)	{
		/* no data required */
	}
	else	{
		l = strnlen(reply, *reply_length);
		if (l >= *reply_length || l <= 0)	{
			*reply_type = MKC_D_VOID;
			*reply_length = 0;
		}
		else	{
			*reply_type = MKC_D_VOID;
			*reply_length = strlen(reply);
		}
	}

	return r;
}

void
bee_monkey(int fd, header_from_pony_to_bee_t * header, char * recv_buf)	{
	monkey_reply_from_bee_to_pony_t reply_s;
	int r;

	char * script;
	int script_length;

	char * data;
	int data_len;

	char * reply = NULL;
	int reply_length;
	int reply_type;

	script = recv_buf + header->range_map_name_len + 1;
	script_length = header->key_len;

	if (script_length <= 0)	{
		r = ESQ_ERROR_PARAM;
		goto __exx;
	}

	if (header->data_len < 0)	{
		r = ESQ_ERROR_PARAM;
		goto __exx;
	}
	else if (header->data_len > 0)	{
		data = recv_buf + header->range_map_name_len + 1 + header->key_len + sizeof(struct timeval);
		data_len = header->data_len;
	}
	else	{
		data = NULL;
		data_len = 0;
	}

	if (header->result_buf_size < 0)	{
		r = ESQ_ERROR_PARAM;
		goto __exx;
	}
	else if (header->result_buf_size > 0)	{
		reply_length = header->result_buf_size;
		reply = malloc(reply_length);
		if (!reply)	{
			r = ESQ_ERROR_MEMORY;
			goto __exx;
		}
		bzero(reply, reply_length);
	}
	else	{
		reply = NULL;
		reply_length = 0;
	}

	/* script, script_length, */
	/* data, data_len, */
	/* reply, reply_length */
	r = esq_bee_monkey_execute(script, script_length - 1, data, data_len, reply, &reply_length, &reply_type);
	if (r)	{
		goto __exx;
	}

	/* reply head */
	if (reply && reply_length)	{
		reply_s.return_code = r;
		reply_s.result_len = reply_length;
		reply_s.type = reply_type;
	}
	else	{
		reply_s.return_code = r;
		reply_s.result_len = 0;
		reply_s.type = MKC_D_VOID;
	}

	r = write(fd, &reply_s, sizeof(reply_s));
	if (sizeof(reply_s) != r)	{
		if (reply)	{
			free(reply);
			reply = NULL;
		}
		return;
	}

	/* reply data */
	if (reply && reply_length)	{
		if (reply_length)	{
			os_write_b(fd, reply, reply_length);
		}
		free(reply);
		reply = NULL;
	}

	return;

__exx:

	reply_s.return_code = r;
	reply_s.result_len = 0;
	reply_s.type = MKC_D_VOID;
	write(fd, &reply_s, sizeof(reply_s));

	if (reply)	{
		free(reply);
		reply = NULL;
	}

	return;
}

/* head structure
 * 2 bit type: 0 - MKC_D_INT, 1 - MKC_D_BUF (byte 0, low bits)
 * 6 bits reserved:
 * 8 bits name length: (byte 1, 8 bits)
 * 16 bits data length: (byte 2, byte3)
 */

#define SIZEOFHEAD	4

/* return data type, MKC_D_INT, MKC_D_BUF, MKC_D_VOID */
int
esq_mkc_get_session_param_by_name(
		void * params_des, char * name, int name_len
		, int * int_value
		, char ** buf, int * buf_len)
{
	esq_session_param_des_t * sd = params_des;
	char * p = sd->payload;
	int l = sd->length;
	int offset = 0;
	int namelen;
	int datalen;
	int type;
	int i;
	int v;
	int s;

	for (;;)	{
		/* read head */
		if (l - offset <= SIZEOFHEAD)	{
			return MKC_D_VOID;	/* invalid data */
		}

		type = ((p[offset] & 0xc) >> 2);
		namelen = (unsigned char)p[offset + 1];
		datalen = (unsigned char)p[offset + 2];
		datalen = (datalen << 8) + (unsigned char)p[offset + 3];

		offset += SIZEOFHEAD;

		if (l - offset < (namelen + datalen))	{
			return MKC_D_VOID;	/* invalid data */
		}

		if ((name_len + 1 != namelen) || memcmp(name, p + offset, name_len))	{
			/* name_len includes the terminiate zero */
			offset += (namelen + datalen);
			continue;
		}

		offset += namelen;

		if (MKC_D_INT == type)	{
			v = 0;
			s = 0;
			for (i = 0; i < sizeof(v); i ++)	{
				v |= (p[offset + i] << s);
				s += 8;
			}
			*int_value = v;
			return MKC_D_INT;
		}
		else if (MKC_D_BUF == type)	{
			*buf = p + offset;
			*buf_len = datalen;
			return MKC_D_BUF;
		}
		else if (MKC_D_ID == type)	{	/* string */
			*buf = p + offset;
			*buf_len = datalen;
			return MKC_D_ID;
		}
		else	{
			return MKC_D_VOID;
		}
	}

	return MKC_D_VOID;
}

/* eof */
