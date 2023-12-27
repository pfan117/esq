#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#define _ESQ_LIB_SOCKET_FUNCTIONS
#include "public/esq.h"
#include "public/libesq.h"
#include "public/pony-bee.h"
#include "public/esqll.h"
#include "internal/types.h"
#include "internal/range_map.h"

#include "bee/public.h"
#include "bee/internal.h"

typedef struct _bee_db_op_session_t	{
	void * range_nodes;
	header_from_pony_to_bee_t * header;
	const char * range_map_name;
	int range_map_name_len;
	const char * key;
	int key_len;
	const char * value;
	int value_len;
	char * recv_buf;
	int recv_buf_size;
} bee_db_op_session_t;

STATIC int
bee_db_get_local(void * range_nodes, const char * key, int key_len, char * buffer, int buf_len, int * result_length)
{
	int r;
	void * p;
	int flags;
	void * bucket;

	for (p = range_nodes; p; p = range_map_node_get_next(p))	{

		flags = range_map_node_get_flags(p);

		if (!(RM_NODE_ME & flags))	{
			continue;
		}

		bucket = range_map_node_get_bucket(p);
		if (!bucket)	{
			continue;
		}

		r = bee_bucket_get(bucket, key, key_len, buffer, buf_len, result_length);
		if (!r)	{
			return ESQ_OK;
		}
	}

	return ESQ_ERROR_NOTFOUND;
}

STATIC int
bee_db_set_local(bee_db_op_session_t * session)	{
	int r;
	void * p;
	void * bucket;

	r = ESQ_ERROR_NOTFOUND;

	for (p = session->range_nodes; p; p = range_map_node_get_next(p))	{

		bucket = range_map_node_get_bucket(p);
		if (!bucket)	{
			continue;
		}

		r = bee_bucket_set(bucket, session->key, session->key_len, session->value, session->value_len);
		if (r)	{
			return r;
		}
	}

	return r;
}

STATIC int
bee_db_del_local(bee_db_op_session_t * session)	{
	int r;
	void * p;
	void * bucket;

	for (p = session->range_nodes; p; p = range_map_node_get_next(p))	{

		bucket = range_map_node_get_bucket(p);

		if (!bucket)	{
			continue;
		}

		r = bee_bucket_del(bucket, session->key, session->key_len, session->value, session->value_len);
		if (r)	{
			return r;
		}
	}

	return ESQ_OK;
}

STATIC int
bee_db_forward_to_node(void * range_node, int opcode, bee_db_op_session_t * session)	{
	struct sockaddr_in address;
	int offset;
	int fd;
	int r;

	session->header->opcode = opcode;

	range_map_get_node_address(range_node, &address);

	fd = os_open_tcp_socket(1, 10);	/* blocking, 10 seconds */
	if (-1 == fd)	{
		return ESQ_ERROR_LIBCALL;
	}

	r = sendto(fd
			, session->header, sizeof(header_from_pony_to_bee_t)
			, MSG_FASTOPEN
			, (struct sockaddr *)&address, sizeof(address)
			);
	if (sizeof(header_from_pony_to_bee_t) != r)	{
		close(fd);
		return ESQ_ERROR_LIBCALL;
	}

	offset = 0;

	while(offset < session->recv_buf_size)	{
		#if 0
	    r = sendto(fd
				, session->recv_buf + offset, session->recv_buf_size - offset
				, MSG_FASTOPEN
				, (struct sockaddr *)&address, sizeof(address)
				);
		#else
		r = write (fd, session->recv_buf + offset, session->recv_buf_size - offset);
		#endif
		if (r <= 0)	{
			close(fd);
			return ESQ_ERROR_LIBCALL;
		}
		else	{
			offset += r;
		}
	}

	close(fd);

	return ESQ_OK;
}

STATIC int
bee_db_sync_to_remote(int op, bee_db_op_session_t * session)	{
	void * bucket;
	int flags;
	void * p;
	int r;

	for (p = session->range_nodes; p; p = range_map_node_get_next(p))	{

		flags = range_map_node_get_flags(p);

		if ((RM_NODE_ME & flags))	{
			continue;
		}

		bucket = range_map_node_get_bucket(p);

		if (bucket)	{
			r = ESQ_OK;	/* handle by local set/del function */
		}
		else	{
			r = bee_db_forward_to_node(p, op, session);
		}

		if (r)	{
			return r;
		}
	}

	return ESQ_OK;
}

void
bee_db_get(int fd, bee_service_ctx_t * ctx)	{

	void * range_nodes;
	int result_size;
	hash_t hash;
	int r;

	header_from_pony_to_bee_t * header;
	char * recv_buf;

	header = &ctx->header;
	recv_buf = ctx->recv_buf;

	ctx->reply_ptr = NULL;

	if (header->result_buf_size <= 0)	{
		r = ESQ_ERROR_PARAM;
		goto __exx;
	}

	ctx->reply_ptr = malloc(sizeof(reply_from_bee_to_pony_t) + header->result_buf_size + sizeof(struct timeval));
	if (!ctx->reply_ptr)	{
		r = ESQ_ERROR_MEMORY;
		goto __exx;
	}

	get_hash_value(recv_buf + header->range_map_name_len + 1, header->key_len, &hash);

	range_nodes = range_map_find_and_lock(recv_buf, &hash);

	if (!range_nodes)	{
		r = ESQ_ERROR_NOTFOUND;
		goto __exx;
	}

	r = bee_db_get_local(range_nodes
			, recv_buf + header->range_map_name_len + 1, header->key_len
			, ctx->reply_ptr->result, header->result_buf_size
			, &result_size
			);
	range_map_unlock();

	if (r)	{
		goto __exx;		
	}

	if (result_size <= sizeof(struct timeval))	{
		r = ESQ_ERROR_NOTFOUND;
		goto __exx;
	}

	ctx->reply_ptr->return_code = ESQ_OK;
	ctx->reply_ptr->result_len = result_size - sizeof(struct timeval);

	return;

__exx:

	ctx->reply.return_code = r;
	ctx->reply.result_len = 0;

	return;
}

void
bee_db_set(int fd, bee_service_ctx_t * ctx, int need_sync)	{

	bee_db_op_session_t session;
	void * range_nodes;
	hash_t hash;
	int r;

	header_from_pony_to_bee_t * header;
	char * recv_buf;
	int buf_size;

	header = &ctx->header;
	recv_buf = ctx->recv_buf;
	buf_size = ctx->payload_size;

	get_hash_value(recv_buf + header->range_map_name_len + 1, header->key_len, &hash);

	range_nodes = range_map_find_and_lock(recv_buf, &hash);

	if (!range_nodes)	{
		r = ESQ_ERROR_NOTFOUND;
		goto __exx;
	}

	session.range_nodes = range_nodes;
	session.header = header;
	session.range_map_name = recv_buf;
	session.range_map_name_len = header->range_map_name_len;
	session.key = recv_buf + header->range_map_name_len + 1;
	session.key_len = header->key_len;
	session.value = recv_buf + header->range_map_name_len + 1 + header->key_len;
	session.value_len = sizeof(struct timeval) + header->data_len;
	session.recv_buf = recv_buf;
	session.recv_buf_size = buf_size;

	r = bee_db_set_local(&session);
	if (r)	{
		range_map_unlock();
		goto __exx;		
	}

	if (need_sync)	{
		r = bee_db_sync_to_remote(BEE_OP_INTERNAL_SET, &session);
		range_map_unlock();
		if (r)	{
			goto __exx;		
		}
	}
	else	{
		range_map_unlock();
	}

	ctx->reply.return_code = ESQ_OK;
	ctx->reply.result_len = 0;

	return;

__exx:

	ctx->reply.return_code = r;
	ctx->reply.result_len = 0;

	return;
}

void
bee_db_del(int fd, bee_service_ctx_t * ctx, int need_sync)	{

	bee_db_op_session_t session;
	void * range_nodes;
	hash_t hash;
	int r;

	header_from_pony_to_bee_t * header;
	char * recv_buf;
	int buf_size;

	header = &ctx->header;
	recv_buf = ctx->recv_buf;
	buf_size = ctx->payload_size;

	get_hash_value(recv_buf + header->range_map_name_len + 1, header->key_len, &hash);

	range_nodes = range_map_find_and_lock(recv_buf, &hash);

	if (!range_nodes)	{
		r = ESQ_ERROR_NOTFOUND;
		goto __exx;
	}

	session.range_nodes = range_nodes;
	session.header = header;
	session.range_map_name = recv_buf;
	session.range_map_name_len = header->range_map_name_len;
	session.key = recv_buf + header->range_map_name_len + 1;
	session.key_len = header->key_len;
	session.value = recv_buf + header->range_map_name_len + 1 + header->key_len;
	session.value_len = sizeof(struct timeval);
	session.recv_buf = recv_buf;
	session.recv_buf_size = buf_size;

	r = bee_db_del_local(&session);
	if (r)	{
		range_map_unlock();
		goto __exx;		
	}

	if (need_sync)	{
		r = bee_db_sync_to_remote(BEE_OP_INTERNAL_DEL, &session);
		range_map_unlock();
		if (r)	{
			goto __exx;		
		}
	}
	else	{
		range_map_unlock();
	}

	ctx->reply.return_code = ESQ_OK;
	ctx->reply.result_len = 0;

	return;

__exx:

	ctx->reply.return_code = r;
	ctx->reply.result_len = 0;

	return;
}

STATIC void *
esq_bucket_sync_worker(void * sync_session_ptr)	{
	bucket_sync_session_t * sync_session = sync_session_ptr;
	int r;

	do	{
		r = bee_bucket_sync_session_sprint(sync_session);
	}
	while(!r);

	bee_bucket_end_sync_session(sync_session);

	return NULL;	/* no one care about this value */
}

const char *
bee_bucket_start_sync_thread(bucket_sync_session_t * sync_session)	{
	pthread_attr_t attr;
	const char * errs;
	pthread_t thread;
	int r;

	pthread_attr_init(&attr);

	errs = bee_bucket_start_sync_session(sync_session);
	if (errs)	{
		pthread_attr_destroy(&attr);
		return errs;
	}

	r = pthread_create(&thread, &attr, esq_bucket_sync_worker, sync_session);
	if (r)	{
		bee_bucket_end_sync_session(sync_session);
		pthread_attr_destroy(&attr);
		return "failed to create bucket sync worker thread";
	}

	pthread_attr_destroy(&attr);

	return NULL;
}

/* the merge sprint */

void
bee_db_start_merge_sprint0(int fd, header_from_pony_to_bee_t * header0)	{
	bee_bucket_merge_session_t merge_session;
	header_from_pony_to_bee_t header;
	int hash_str_length;
	int malloc_size;
	char * buffer;
	int r;

	r = os_socket_set_timeout(fd, 20);
	if (r)	{
		close(fd);
		return;
	}

	merge_session.fd = fd;
	merge_session.buffer = NULL;
	merge_session.range_map_name_length = 0;
	merge_session.bucket = NULL;

	header = *header0;

	for (;;)	{

		hash_str_length = header.result_buf_size;

		/* check header sanity */
		if (BEE_OP_SYNC != header.opcode)	{
			break;
		}

		if (header.range_map_name_len < 0 || header.key_len <= 0 || header.data_len < 0)	{
			break;
		}

		if (header.range_map_name_len >= RANGE_MAP_NAME_MAX)	{
			break;
		}

		if (hash_str_length < 0)	{
			break;
		}

		if (hash_str_length > (HASH_VALUE_LENGTH * 2))	{
			break;
		}

		if (sizeof(struct timeval) > header.data_len)	{
			break;
		}

		malloc_size = header.range_map_name_len + header.key_len + header.data_len + hash_str_length;

		#if 0
		int range_map_name_len;
		int key_len;
		int data_len;
		int result_buf_size;
		#endif

		buffer = malloc(malloc_size);
		if (!buffer)	{
			break;
		}

		if (merge_session.buffer)	{
			free(merge_session.buffer);
		}

		merge_session.buffer = buffer;

		r = read(fd, buffer, malloc_size);
		if (r != malloc_size)	{
			break;
		}

		if (header.range_map_name_len)	{
			memcpy(merge_session.range_map_name, buffer, header.range_map_name_len);
			merge_session.range_map_name_length = header.range_map_name_len;;
		}

		#if 0
		char * hash_str;
		int hash_str_length;
		char * key;
		int key_length;
		char * value;
		int value_length;
		#endif

		/* range map name, hash, key, value */
		merge_session.hash_str = buffer + header.range_map_name_len;
		merge_session.key = buffer + header.range_map_name_len + hash_str_length;
		merge_session.value = buffer + header.range_map_name_len + hash_str_length + header.key_len;
		merge_session.hash_str_length = hash_str_length;
		merge_session.key_length = header.key_len;
		merge_session.value_length = header.data_len;

		r = esq_bucket_merge_item(&merge_session);
		if (r)	{
			break;
		}

		r = read(fd, &header, sizeof(header));
		if (sizeof(header) != r)	{
			break;
		}
	}

	if (merge_session.buffer)	{
		free(merge_session.buffer);
	}

	close(fd);

	return;
}

void
bee_db_start_merge_sprint(int fd, header_from_pony_to_bee_t * header0)	{
	range_map_rlock();
	bee_db_start_merge_sprint0(fd, header0);
	range_map_unlock();
}

/* of */
