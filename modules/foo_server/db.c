#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "public/esq.h"
#include "public/pony-bee.h"
#include "public/libesq.h"

#include "public/esqll.h"
#include "internal/types.h"
#include "internal/range_map.h"

#define BURST_SIZE	10000

#include "modules/foo_server/utils.h"
#include "modules/foo_server/callbacks.h"

char foo_pq_db_recv_buf[1024];
#define FOO_DB_KEY_0	"echo_key_0"
#define FOO_DB_DATA_0	"echo_data_0"
#define FOO_DB_REPLY	""

void
foo_db_bsc_test(const char * rmm)	{
	char result_buf[128];
	int result_len;
	int r;

	/* -- */
	result_len = sizeof(result_buf);

	r = pony(rmm
		, FOO_DB_KEY_0, sizeof(FOO_DB_KEY_0) - 1	/* send without '\0' */
		, FOO_DB_DATA_0, sizeof(FOO_DB_DATA_0) - 1	/* send without '\0' */
		, BEE_OP_DB_SET
		, result_buf, &result_len
		);
	if (r)	{
		printf("foo db set failed: pony return %d\n", r);
		return;
	}
	else	{
		printf("foo db set success\n");
	}

	/* -- */
	result_len = sizeof(result_buf);

	r = pony(rmm
		, FOO_DB_KEY_0, sizeof(FOO_DB_KEY_0) - 1	/* send without '\0' */
		, NULL, 0
		, BEE_OP_DB_GET
		, result_buf, &result_len
		);
	if (r)	{
		printf("foo db get failed: pony return %d\n", r);
		return;
	}

	if (result_len != (sizeof(FOO_DB_DATA_0) - 1))	{
		printf("foo db get failed: pony return error size data %d, should be %lu\n"
				, result_len, (sizeof(FOO_DB_DATA_0) - 1)
				);
		return;
	}

	if (memcmp(result_buf, FOO_DB_DATA_0, sizeof(FOO_DB_DATA_0) - 1))	{
		printf("foo db get failed: pony return error data missmatched\n");
		return;
	}
	else	{
		printf("foo db get success\n");
	}

	/* -- */
	result_len = sizeof(result_buf);

	r = pony(rmm
		, FOO_DB_KEY_0, sizeof(FOO_DB_KEY_0) - 1	/* send without '\0' */
		, NULL, 0
		, BEE_OP_DB_DEL
		, result_buf, &result_len
		);
	if (r)	{
		printf("foo db del failed: pony return %d\n", r);
		return;
	}
	else	{
		printf("foo db del success\n");
	}

	/* -- */
	result_len = sizeof(result_buf);

	r = pony(rmm
		, FOO_DB_KEY_0, sizeof(FOO_DB_KEY_0) - 1	/* send without '\0' */
		, NULL, 0
		, BEE_OP_DB_GET
		, result_buf, &result_len
		);
	if (ESQ_ERROR_NOTFOUND == r)	{
		printf("foo db get removed item success\n");
	}
	else	{
		printf("foo db get removed item failed: pony return %d\n", r);
		return;
	}

	return;
}

void
foo_db_burst_set_test(const char * rmm)	{
	char result_buf[128];
	char kb[128];
	char vb[128];
	int result_len;
	int r;
	int i;

	foo_timing_begin();

	for (i = 0; i < BURST_SIZE; i ++)	{

		snprintf(kb, sizeof(kb), "key-%05d", i);
		snprintf(vb, sizeof(vb), "value-%05d", i);
		result_len = sizeof(result_buf);

		r = pony(rmm
				, kb, sizeof("key-00000") - 1	/* send without '\0' */
				, vb, sizeof("value-00000") - 1	/* send without '\0' */
				, BEE_OP_DB_SET
				, result_buf, &result_len
				);

		if (r)	{
			printf("foo db set failed: idx = %d, pony return %d\n", i, r);
			return;
		}
		else	{
		}
	}

	foo_timing_end();

	printf("foo db set success, i = %d\n", i);
	printf("\n");

	return;
}

void
foo_db_burst_del_test(const char * rmm)	{
	char result_buf[128];
	char kb[128];
	int result_len;
	int r;
	int i;

	foo_timing_begin();

	for (i = 0; i < BURST_SIZE; i ++)	{

		snprintf(kb, sizeof(kb), "key-%05d", i);
		result_len = sizeof(result_buf);

		r = pony(rmm
			, kb, sizeof("key-00000") - 1	/* send without '\0' */
			, NULL, 0
			, BEE_OP_DB_DEL
			, result_buf, &result_len
			);
		if (r)	{
			printf("foo db delete failed: pony return %d\n", r);
			return;
		}
	}

	foo_timing_end();

	printf("foo db delete success, i = %d\n", i);
	printf("\n");

	return;
}

void
foo_db_burst_get_test(const char * rmm)	{
	char result_buf[128];
	char kb[128];
	char vb[128];
	int result_len;
	int r;
	int i;

	foo_timing_begin();

	for (i = 0; i < BURST_SIZE; i ++)	{

		snprintf(kb, sizeof(kb), "key-%05d", i);
		snprintf(vb, sizeof(vb), "value-%05d", i);
		result_len = sizeof(result_buf);

		r = pony(rmm
			, kb, sizeof("key-00000") - 1	/* send without '\0' */
			, NULL, 0
			, BEE_OP_DB_GET
			, result_buf, &result_len
			);
		if (r)	{
			printf("foo db get failed: pony return %d\n", r);
			return;
		}

		if (result_len != (sizeof("value-00000") - 1))	{
			printf("kcdb get failed: pony return error size data %d, should be %lu\n"
					, result_len
					, sizeof("value-000") - 1
					);
			return;
		}

		if (memcmp(result_buf, vb, sizeof("value-00000") - 1))	{
			printf("foo db get failed: pony return error data missmatched, i = %d\n", i);
			return;
		}
		else	{
			;
		}
	}

	foo_timing_end();

	printf("foo db check success, i = %d\n", i);
	printf("\n");

	return;
}

void
foo_disk_db_bsc_test(void * dummy)	{
	printf("Basic test for \"kcdb\"\n");
	foo_db_bsc_test("kcdb");
}

void
foo_disk_db_burst_set_test(void * dummy)	{
	printf("Set data into \"kcdb\"\n");
	foo_db_burst_set_test("kcdb");
}

void
foo_disk_db_burst_del_test(void * dummy)	{
	printf("Delete data from \"kcdb\"\n");
	foo_db_burst_del_test("kcdb");
}

void
foo_disk_db_burst_get_test(void * dummy)	{
	printf("Get data from \"kcdb\"\n");
	foo_db_burst_get_test("kcdb");
}

void
foo_mem_db_bsc_test(void * dummy)	{
	printf("Basic test for \"mm\"\n");
	foo_db_bsc_test("mm");
}

void
foo_mem_db_burst_set_test(void * dummy)	{
	printf("Set data into \"mm\"\n");
	foo_db_burst_set_test("mm");
}

void
foo_mem_db_burst_del_test(void * dummy)	{
	printf("Delete data from \"mm\"\n");
	foo_db_burst_del_test("mm");
}

void
foo_mem_db_burst_get_test(void * dummy)	{
	printf("Get data from \"mm\"\n");
	foo_db_burst_get_test("mm");
}

/* eof */
