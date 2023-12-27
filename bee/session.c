//#define PRINT_DBG_LOG

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#include "public/esq.h"
#include "public/esqll.h"
#include "internal/types.h"
#include "internal/range_map.h"
#include "internal/log_print.h"
#include "public/libesq.h"
#include "public/pony-bee.h"

#include "bee/public.h"
#include "bee/internal.h"
#include "bee/pt.h"

void
bee_session_receive_request(void * esq_session)	{

	bee_service_ctx_t * ctx;
	int event_bits;
	int fd;
	int r;

	event_bits = esq_session_get_event_bits(esq_session);
	fd = esq_session_get_fd(esq_session);
	ctx = esq_session_get_param(esq_session);

	if (fd <= 0)	{
		esq_design_error();
	}

	if ((ESQ_EVENT_BIT_FD_CLOSE & event_bits))	{
		PRINTF("DBG: %s() %d: fd close event received\n", __func__, __LINE__);
        goto __exx;
	}

	if ((ESQ_EVENT_BIT_ERROR & event_bits))	{
		PRINTF("DBG: %s() %d: error event received\n", __func__, __LINE__);
        goto __exx;
	}

	if ((ESQ_EVENT_BIT_TIMEOUT & event_bits))	{
		PRINTF("DBG: %s() %d: time out event received\n", __func__, __LINE__);
		goto __exx;
	}

	if ((ESQ_EVENT_BIT_READ & event_bits))	{
		;
	}

	if ((ESQ_EVENT_BIT_WRITE & event_bits))	{
		;
	}

	if ((ESQ_EVENT_BIT_EXIT & event_bits))	{
		PRINTF("DBG: %s() %d: esq quit event received\n", __func__, __LINE__);
		goto __exx;
	}

	SHOW_EVENT_BITS(event_bits);

	PT_BEGIN

	ctx->offset = 0;

	PT_SET

	r = os_read_nb(fd, (char *)&ctx->header + ctx->offset, sizeof(ctx->header) - ctx->offset);
	PRINTF("DBG: %s() %d: read() return %d\n", __func__, __LINE__, r);
	if (r < 0)	{
		if (EAGAIN == errno || EWOULDBLOCK == errno)	{
			PRINTF("DBG: %s() %d: again\n", __func__, __LINE__);
			goto __read_again;
		}
		else	{
			PRINTF("DBG: %s() %d: error, errno = %d\n", __func__, __LINE__, errno);
			goto __exx;
		}
	}
	else if (0 == r)	{
		PRINTF("DBG: %s() %d: client side closed\n", __func__, __LINE__);
		goto __exx;
	}
	else if (r == (sizeof(ctx->header) - ctx->offset))	{
		PRINTF("DBG: %s() %d: complete\n", __func__, __LINE__);
	}
	else if (r > (sizeof(ctx->header) - ctx->offset))	{
		PRINTF("DBG: %s() %d: unknown error\n", __func__, __LINE__);
		goto __exx;
	}
	else	{
		ctx->offset += r;
		PRINTF("DBG: %s() %d: again\n", __func__, __LINE__);
		goto __read_again;
	}

	if (ctx->header.range_map_name_len <= 0)	{
		PRINTF("DBG: %s() %d: header error, invalid range_map_name length\n", __func__, __LINE__);
		goto __exx;
	}

	if (ctx->header.key_len <= 0)	{
		PRINTF("DBG: %s() %d: header error, invalid key_len\n", __func__, __LINE__);
		goto __exx;
	}

	if (ctx->header.data_len < 0)	{
		PRINTF("DBG: %s() %d: header error, invalid data_len\n", __func__, __LINE__);
		goto __exx;
	}

	if (ctx->header.range_map_name_len >= RANGE_MAP_NAME_MAX)	{
		PRINTF("DBG: %s() %d: header error, range_map_name too long\n", __func__, __LINE__);
		goto __exx;
	}

	ctx->opcode = (ctx->header.opcode & BEE_OP_TYPE_MASK);
	if (BEE_OP_SYNC == ctx->opcode)	{
		bee_db_start_merge_sprint(fd, &ctx->header);
		goto __exx;
	}

	ctx->payload_size = ctx->header.range_map_name_len + 1 + ctx->header.key_len
			+ sizeof(struct timeval) + ctx->header.data_len;
	/* Range_map_name_len is the value of strlen(). But in buffer, there is a '\0' after range map name. */
	ctx->recv_buf = malloc(ctx->payload_size);
	if (!ctx->recv_buf)	{
		PRINTF("DBG: %s() %d: malloc() error\n", __func__, __LINE__);
		goto __exx;
	}

	ctx->offset = 0;

	PT_SET

    if ((ESQ_EVENT_BIT_READ & event_bits))	{
		r = read(fd, ctx->recv_buf + ctx->offset, ctx->payload_size - ctx->offset);
		if (r < 0)	{
			if (EAGAIN == errno || EWOULDBLOCK == errno)	{
				PRINTF("DBG: %s() %d: again\n", __func__, __LINE__);
				goto __read_again;
			}
			else	{
				PRINTF("DBG: %s() %d: again\n", __func__, __LINE__);
				goto __exx;
			}
		}
		else if (0 == r)	{
			PRINTF("DBG: %s() %d: client closed\n", __func__, __LINE__);
			goto __exx;
		}
		if (r == (ctx->payload_size - ctx->offset))	{
			PRINTF("DBG: %s() %d: complete\n", __func__, __LINE__);
		}
		else if (r > (ctx->payload_size - ctx->offset))	{
			PRINTF("DBG: %s() %d: unknown error\n", __func__, __LINE__);
			goto __exx;
		}
		else	{
			ctx->offset += r;
			PRINTF("DBG: %s() %d: again\n", __func__, __LINE__);
			goto __read_again;
		}
	}

	/* check eos of range map name string */
	if ('\0' != ctx->recv_buf[ctx->header.range_map_name_len])	{
		goto __exx;
	}

	if (BEE_OP_DB_SET == ctx->opcode)	{
		PT_SET
		bee_db_set(fd, ctx, 1);
	}
	else if(BEE_OP_DB_GET == ctx->opcode)	{
		PT_SET
		bee_db_get(fd, ctx);
	}
	else if (BEE_OP_DB_DEL == ctx->opcode)	{
		PT_SET
		bee_db_del(fd, ctx, 1);
	}
	else if (BEE_OP_FILE_WRITE == ctx->opcode)	{
		PT_SET
	}
	else if (BEE_OP_FILE_READ == ctx->opcode)	{
		PT_SET
	}
	else if (BEE_OP_FILE_DEL == ctx->opcode)	{
		PT_SET
	}
	else if (BEE_OP_SCRIPT == ctx->opcode)	{
		PT_SET
	}
	else if (BEE_OP_INTERNAL_SET == ctx->opcode)	{
		PT_SET
		bee_db_set(fd, ctx, 0);
	}
	else if (BEE_OP_INTERNAL_DEL == ctx->opcode)	{
		PT_SET
		bee_db_del(fd, ctx, 0);
	}
	else if (BEE_OP_MONKEY == ctx->opcode)	{
		PT_SET
		bee_monkey(fd, &ctx->header, ctx->recv_buf);
#if 0
/* TODO, using mco for monkey call */
int
esq_start_mco_session(
		const char * name
		, void * parent_session
		, esq_service_mco_entry_t entry
		, size_t stack_size
		, void * service_mco_param
		)
#endif
	}
	else	{
		PRINTF("ERROR: %s() %d: invalid opcode\n", __func__, __LINE__);
		ctx->reply.return_code = ESQ_ERROR_PARAM;
		ctx->reply.result_len = 0;
	}

	if (BEE_OP_MONKEY == ctx->opcode)	{
		/* TODO */
		printf("DBG: %s() %d: TODO\n", __func__, __LINE__);
	}
	else	{
		if (ctx->reply_ptr)	{
			write(fd, ctx->reply_ptr, ctx->reply_ptr->result_len + sizeof(struct timeval) + sizeof(reply_from_bee_to_pony_t));
			free(ctx->reply_ptr);
			ctx->reply_ptr = NULL;
			write(fd, &ctx->reply, sizeof(ctx->reply));
		}
		else	{
			write(fd, &ctx->reply, sizeof(ctx->reply));
		}
	}

__exx:

	close(fd);
	if (ctx->recv_buf)	{
		free(ctx->recv_buf);
		ctx->recv_buf = NULL;
	}
	free(ctx);

	PT_END

	return;

__read_again:

	r = esq_session_set_socket_fd(esq_session, fd, bee_session_receive_request
			, ESQ_EVENT_BIT_READ, 2, 0, esq_session_get_param(esq_session));
	if (r)	{
		printf("ERROR: %s() %d: failed to add bee client socket into session\n"
				, __func__, __LINE__);
		close(fd);
		if (ctx->recv_buf)	{
			free(ctx->recv_buf);
			ctx->recv_buf = NULL;
		}
		free(ctx);
		return;
	}

	return;
}

/* of */
