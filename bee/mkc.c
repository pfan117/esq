#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <monkeycall.h>

#include "public/esq.h"
#include "public/libesq.h"
#include "public/esqll.h"
#include "internal/types.h"
#include "internal/range_map.h"

#include "bee/public.h"
#include "bee/internal.h"

#include "internal/lock.h"

/* return value: buffer */
STATIC int
bee_server_mkccb_start_instance(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	mkc_data * port_number;
	cc * errs;

	MKC_CB_ARGC_CHECK(1);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_INT);
	result = MKC_CB_RESULT;
	port_number = MKC_CB_ARGV(0);
	result->type = MKC_D_VOID;

	errs = bee_server_start_instance(port_number->integer);
	if (errs)	{
		MKC_CB_ERROR("%s, %d: failed to start server: %s", __func__, __LINE__, errs);
		return -1;
	}

	return 0;
}

STATIC int
bee_mkccb_create_bucket_sync_session(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	mkc_data * range_map;
	mkc_data * hash;
	mkc_data * target_ip;
	mkc_data * target_port;
	mkc_data * target_hash;
	int target_cnt;
	int malloc_size;
	bucket_sync_session_t * sync_session;
	cc * errs;
	int i;
	int k;
	int dashboard;
	int hash_start;
	int hash_len;
	int r;

	/* 0, range map name */
	/* 1, hash string */
	/* 2, 3, 4, ip, port, hash triad */
	/* ..., triads */
	if (MKC_CB_ARGC < 5)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, "need to specify range map name, hash string, target ip-port-hash triad(s)");
		return -1;
	}

	if ((MKC_CB_ARGC - 2) % 3)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, "need to specify range map name, hash string, target ip-port-hash triad(s)");
		return -1;
	}

	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(1, MKC_D_BUF);

	for (i = 2; i < MKC_CB_ARGC; i += 3)	{
		MKC_CB_ARG_TYPE_CHECK(i, MKC_D_BUF);
		MKC_CB_ARG_TYPE_CHECK(i + 1, MKC_D_INT);
		MKC_CB_ARG_TYPE_CHECK(i + 2, MKC_D_BUF);
	}

	result = MKC_CB_RESULT;
	result->type = MKC_D_VOID;

	range_map = MKC_CB_ARGV(0);
	hash = MKC_CB_ARGV(1);

	if (range_map->length >= RANGE_MAP_NAME_MAX)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, "range map name too long");
		return -1;
	}

	if (range_map_name_check2(range_map->buffer, range_map->length))	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, "invalid range map name");
		return -1;
	}

	if (hash->length >= HASH_VALUE_LENGTH)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, "hash string too long");
		return -1;
	}

	r = get_hash_string(hash->buffer, hash->length, &hash_start, &hash_len);
	if (-1 == r)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, "invalid hash string");
		return -1;
	}

	if (0 != hash_start || hash->length != (hash_len + 1))	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, "invalid hash string");
		return -1;
	}

	target_cnt = ((MKC_CB_ARGC - 2) / 3);
	malloc_size = sizeof(bucket_sync_session_t) + (sizeof(bucket_sync_target_t) * target_cnt);

	sync_session = malloc(malloc_size);
	if (!sync_session)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, "out of memory");
		return -1;
	}

	bzero(sync_session, malloc_size);
	memcpy(sync_session->range_map_name, range_map->buffer, range_map->length + 1);
	sync_session->range_map_name_length = range_map->length;

	memcpy(sync_session->hash_str, hash->buffer, hash->length + 1);
	sync_session->hash_str_length = hash->length;
	sync_session->target_cnt = target_cnt;
	sync_session->processed = 0;

	for (i = 0; i < target_cnt; i ++)	{
		bucket_sync_target_t * target;
		unsigned long ap;

		target_ip = MKC_CB_ARGV(2 + i * 3);
		target_port = MKC_CB_ARGV(2 + i * 3 + 1);
		target_hash = MKC_CB_ARGV(2 + i * 3 + 2);

		r = get_ipv4_addr(target_ip->buffer, target_ip->length, &ap);
		if (!r)	{
			free(sync_session);
			MKC_CB_ERROR("%s, %d: argument %d, invalid ip", __func__, __LINE__, 2 + i * 3);
			return -1;
		}

		if (PORT_NUMBER_INVALID(target_port->integer))	{
			free(sync_session);
			MKC_CB_ERROR("%s, %d: argument %d, invalid port", __func__, __LINE__, 2 + i * 3 + 1);
			return -1;
		}

		#if 0
		if (target_hash->length >= sizeof(sync_session->targets[0].hash_str))	{
			free(sync_session);
			MKC_CB_ERROR("%s, %d: argument %d, hash string too long", __func__, __LINE__, 2 + i * 3 + 2);
			return -1;
		}
		#endif

		r = get_hash_string(target_hash->buffer, target_hash->length, &hash_start, &hash_len);
		if (-1 == r)	{
			free(sync_session);
			MKC_CB_ERROR("%s, %d: %d invalid hash string", __func__, __LINE__, 2 + i * 3 + 2);
			return -1;
		}

		if (0 != hash_start || target_hash->length != (hash_len + 1))	{
			free(sync_session);
			MKC_CB_ERROR("%s, %d: %d invalid hash string", __func__, __LINE__, 2 + i * 3 + 2);
			return -1;
		}

		target = sync_session->targets + i;

		if (hash_str_2_bin(target->hash.buf, target_hash->buffer, hash_len))	{
			free(sync_session);
			MKC_CB_ERROR("%s, %d: failed to transform hash string into binary", __func__, __LINE__);
			return -1;
		}

		target->hash_length = (hash_len >> 1);
		memcpy(target->hash_str, target_hash->buffer, target_hash->length);
		target->hash_str_length = target_hash->length;
		target->address.sin_family = AF_INET;
		target->address.sin_port = htons(target_port->integer);
		target->address.sin_addr.s_addr = inet_addr(target_ip->buffer);

	}

	/* check duplication */
	for (i = 0; i < target_cnt; i ++)	{

		for (k = i + 1; k < target_cnt; k ++)	{

			if (sync_session->targets[i].address.sin_port != sync_session->targets[k].address.sin_port)	{
				continue;
			}

			if (sync_session->targets[i].address.sin_addr.s_addr != sync_session->targets[k].address.sin_addr.s_addr)	{
				continue;
			}

			if (sync_session->targets[i].hash_length != sync_session->targets[k].hash_length)	{
				continue;
			}

			if (memcmp(sync_session->targets[i].hash.buf, sync_session->targets[k].hash.buf, sync_session->targets[i].hash_length))	{
				continue;
			}

			free(sync_session);
			MKC_CB_ERROR("%s, %d: item %d and %d are duplicated", __func__, __LINE__, i, k);

			return -1;

		}
	}

	dashboard = esq_dashboard_new();
	if (-1 == dashboard)	{
		free(sync_session);
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, "failed to create dashboard item");
		return -1;
	}

	sync_session->dashboard = dashboard;

	/* sort same range map targets */
	bee_create_target_cross_links(sync_session);

	/* set all fd to invalid value */
	for (i = 0; i < target_cnt; i ++)	{
		sync_session->targets[i].socket_fd = -1;
	}

	errs = bee_bucket_start_sync_thread(sync_session);

	if (errs)	{
		esq_dashboard_remove_item(dashboard);
		free(sync_session);
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, errs);
		return -1;
	}
	else	{
		result->type = MKC_D_INT;
		result->integer = dashboard;
	}

	return 0;
}

/* return value: buffer */
STATIC int
esq_mkccb_get_mkc_session_param_by_name(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	mkc_data * name;
	char * namestr;
	int argc;
	int r;

	void * session_param_ptr;
	void * result_buf;

	int value;
	char * buf;
	int buf_len;

	argc = MKC_CB_ARGC;
	if (1 != argc)	{
		MKC_CB_ERROR("%s", "need to provide parameter name");
		return -1;
	}

	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_BUF);

	result = MKC_CB_RESULT;
	name = MKC_CB_ARGV(0);

	if (name->length <= 0)	{
		result->type = MKC_D_VOID;
		return -1;
	}

	namestr = name->buffer;
	if (namestr[name->length])	{
		result->type = MKC_D_VOID;
		return -1;
	}

	session_param_ptr = MKC_CB_USER_PARAM0;

	r = esq_mkc_get_session_param_by_name(session_param_ptr, namestr, name->length, &value, &buf, &buf_len);

	if (MKC_D_BUF == r)	{
		if (buf_len <= 0)	{
			result->type = MKC_D_VOID;
			return -1;
		}
		else	{
			result_buf = mkc_session_malloc(session, buf_len);
		}
		if (result_buf)	{
			memcpy(result_buf, buf, buf_len);
			result->type = MKC_D_BUF;
			result->length = buf_len;
			result->buffer = result_buf;
		}
		else	{
			result->type = MKC_D_VOID;
		}
	}
	else if (MKC_D_ID == r)	{	/* string */
		if (buf_len < 0)	{
			result->type = MKC_D_VOID;
			return -1;
		}
		else	{
			result_buf = mkc_session_malloc(session, buf_len + 1);
		}
		if (result_buf)	{
			memcpy(result_buf, buf, buf_len);
			((char *)result_buf)[buf_len] = '\0';
			result->type = MKC_D_BUF;
			result->length = buf_len;
			result->buffer = result_buf;
		}
		else	{
			result->type = MKC_D_VOID;
		}
	}
	else if(MKC_D_INT == r)	{
		result->integer = value;
		result->type = MKC_D_INT;
	}
	else	{
		result->type = MKC_D_VOID;
	}

	return 0;
}

int
esq_bee_install_mkc_commands(void)	{
	void * mkc;

	mkc = esq_get_mkc();

	ESQ_IF_ERROR_RETURN(bee_bucket_init());
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "StartBeeServer", bee_server_mkccb_start_instance));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "CreateBucketSyncSession", bee_mkccb_create_bucket_sync_session));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "Param", esq_mkccb_get_mkc_session_param_by_name));

	return 0;
}

/* eof */
