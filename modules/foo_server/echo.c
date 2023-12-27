#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "public/esq.h"
#include "public/esqll.h"
#include "internal/types.h"
#include "modules/foo_server/callbacks.h"

#if 0

char foo_pq_echo_recv_buf[1024];
#define ECHO_QUERY_0	"echo_key_0"
#define ECHO_DATA_0		"echo_data_0"
#define ECHO_REPLY		ECHO_QUERY_0 ECHO_DATA_0

STATIC int
foo_pq_echo_on_fragment_receive(void * pony_query_session_ptr)	{
	char * buf;
	int len;

	buf = pony_query_get_recv_buf(pony_query_session_ptr);
	len = pony_query_get_recv_len(pony_query_session_ptr);

	if (len != (sizeof(ECHO_REPLY) - 1))	{
		printf("ERROR: reply length supposed to be %lu not %d\n", (sizeof(ECHO_REPLY) - 1), len);
		return 0;
	}

	if (memcmp(buf, ECHO_REPLY, len))	{
		printf("ERROR: reply content error\n");
		return 0;
	}

	printf("OK\n");

	return 0;
}

STATIC int
foo_pq_echo_on_reply_finish(void * pony_query_session_ptr)	{
ESQ_HERE;
	return 0;
}

STATIC int
foo_pq_echo_on_error(void * pony_query_session_ptr)	{
ESQ_HERE;
	return 0;
}

STATIC pony_query_app_cbs_t foo_pq_echo_cbs = {
	foo_pq_echo_on_fragment_receive,
	foo_pq_echo_on_reply_finish,
	foo_pq_echo_on_error,
};

void
foo_pony_query_echo_test(void)	{
	struct sockaddr_in address;
	void * esq_session;
	void * session;

	esq_session = esq_session_new(NULL);
	if (!esq_session)	{
		return;
	}

	bzero(&address, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(12121);
	address.sin_addr.s_addr = inet_addr("127.0.0.2");

	session = pony_query_session_new(
			esq_session
			, &foo_pq_echo_cbs
			, &address
			, BEE_SERVICE_ECHO, 0
			, ECHO_QUERY_0, sizeof(ECHO_QUERY_0) - 1
			, ECHO_DATA_0, sizeof(ECHO_DATA_0) - 1
			, foo_pq_echo_recv_buf, sizeof(foo_pq_echo_recv_buf)
			);
	if (!session)	{
		ESQ_HERE;
	}

	esq_session_wind_up(esq_session);

	return;
}

#endif

/* eof */
