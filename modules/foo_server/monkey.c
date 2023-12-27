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

#define BURST_SIZE	1000

#include "modules/foo_server/callbacks.h"

void
foo_pony_query_monkey_bsc_case_0(void)	{
	char buffer[16];
	int buflen;
	void * ctx;
	int r;

	ctx = pony_monkey_ctx_new(2);
	if (!ctx)	{
		printf("ERROR: failed to malloc memory for monkey ctx\n");
		return;
	}

	r = pony_monkey_ctx_set_script(ctx, "return DupStr(Param(\"s\"), 2);");
	if (r)	{
		printf("ERROR: failed to set monkey call script\n");
		goto __exx;
	}

	r = pony_monkey_ctx_add_str_param(ctx, "s", "Str0", sizeof("Str0") - 1);
	if (r)	{
		printf("ERROR: failed to set monkey string value\n");
		goto __exx;
	}

	buflen = sizeof(buffer);
	r = pony_monkey("x", ctx, buffer, &buflen);
	if (r)	{
		printf("ERROR: failed to execute monkey script\n");
		printf("       r = %d\n", r);
		goto __exx;
	}

	if (strcmp("Str0Str0", buffer))	{
		printf("ERROR: %s monkey call result is [%s], strlen is %d, supposed to be 'Str0Str0'\n", __func__, buffer, buflen);
		goto __exx;
	}

	pony_monkey_ctx_free(ctx);

	printf("INFO : %s success\n", __func__);

	return;

__exx:

	pony_monkey_ctx_free(ctx);

	return;
}

void
foo_pony_query_monkey_bsc_case_1(void)	{
	char buffer[16];
	int buflen;
	void * ctx;
	int r;

	ctx = pony_monkey_ctx_new(2);
	if (!ctx)	{
		printf("ERROR: failed to malloc memory for monkey ctx\n");
		return;
	}

	r = pony_monkey_ctx_set_script(ctx, "return DupStr(Param(\"s\"), 2);");
	if (r)	{
		printf("ERROR: failed to set monkey call script\n");
		goto __exx;
	}

	r = pony_monkey_ctx_add_str_param(ctx, "ss", "ERR", sizeof("ERR") - 1);
	if (r)	{
		printf("ERROR: failed to set monkey string value\n");
		goto __exx;
	}

	r = pony_monkey_ctx_add_str_param(ctx, "s", "Str0", sizeof("Str0") - 1);
	if (r)	{
		printf("ERROR: failed to set monkey string value\n");
		goto __exx;
	}
	buflen = sizeof(buffer);
	r = pony_monkey("x", ctx, buffer, &buflen);
	if (r)	{
		printf("ERROR: failed to execute monkey script\n");
		printf("       r = %d\n", r);
		goto __exx;
	}

	if (strcmp("Str0Str0", buffer))	{
		printf("ERROR: %s monkey call result is [%s], strlen is %d, supposed to be 'Str0Str0'\n", __func__, buffer, buflen);
		goto __exx;
	}

	pony_monkey_ctx_free(ctx);

	printf("INFO : %s success\n", __func__);

	return;

__exx:

	pony_monkey_ctx_free(ctx);

	return;
}

void
foo_pony_query_monkey_bsc_case_2(void)	{
	char buffer[16];
	int buflen;
	void * ctx;
	int r;

	ctx = pony_monkey_ctx_new(2);
	if (!ctx)	{
		printf("ERROR: failed to malloc memory for monkey ctx\n");
		return;
	}

	r = pony_monkey_ctx_set_script(ctx, "return DupStr(Param(\"s\"), 2);");
	if (r)	{
		printf("ERROR: failed to set monkey call script\n");
		goto __exx;
	}

	r = pony_monkey_ctx_add_buf_param(ctx, "ss", "ERR", sizeof("ERR") - 1);
	if (r)	{
		printf("ERROR: failed to set monkey string value\n");
		goto __exx;
	}

	r = pony_monkey_ctx_add_buf_param(ctx, "s", "Str0", sizeof("Str0") - 1);
	if (r)	{
		printf("ERROR: failed to set monkey string value\n");
		goto __exx;
	}

	buflen = sizeof(buffer);
	r = pony_monkey("x", ctx, buffer, &buflen);
	if (r)	{
		printf("ERROR: failed to execute monkey script\n");
		printf("       r = %d\n", r);
		goto __exx;
	}

	if (strcmp("Str0Str0", buffer))	{
		printf("ERROR: %s monkey call result is [%s], strlen is %d, supposed to be 'Str0Str0'\n", __func__, buffer, buflen);
		goto __exx;
	}

	pony_monkey_ctx_free(ctx);

	printf("INFO : %s success\n", __func__);

	return;

__exx:

	pony_monkey_ctx_free(ctx);

	return;
}

void
foo_pony_query_monkey_bsc_case_3(void)	{
	char buffer[16];
	int buflen;
	void * ctx;
	int r;

	ctx = pony_monkey_ctx_new(4);
	if (!ctx)	{
		printf("ERROR: failed to malloc memory for monkey ctx\n");
		return;
	}

	r = pony_monkey_ctx_set_script(ctx, "return DupStr(Param(\"s\"), Param(\"t\"));");
	if (r)	{
		printf("ERROR: failed to set monkey call script\n");
		goto __exx;
	}

	r = pony_monkey_ctx_add_buf_param(ctx, "ss", "ERR", sizeof("ERR") - 1);
	if (r)	{
		printf("ERROR: failed to set monkey string value\n");
		goto __exx;
	}

	r = pony_monkey_ctx_add_buf_param(ctx, "s", "Str0", sizeof("Str0") - 1);
	if (r)	{
		printf("ERROR: failed to set monkey string value\n");
		goto __exx;
	}

	r = pony_monkey_ctx_add_int_param(ctx, "t", 3);
	if (r)	{
		printf("ERROR: failed to set monkey string value\n");
		goto __exx;
	}

	r = pony_monkey_ctx_add_int_param(ctx, "tt", 4);
	if (r)	{
		printf("ERROR: failed to set monkey string value\n");
		goto __exx;
	}

	buflen = sizeof(buffer);
	r = pony_monkey("x", ctx, buffer, &buflen);
	if (r)	{
		printf("ERROR: failed to execute monkey script\n");
		printf("       r = %d\n", r);
		goto __exx;
	}

	if (strcmp("Str0Str0Str0", buffer))	{
		printf("ERROR: %s monkey call result is [%s], strlen is %d, supposed to be 'Str0Str0Str0'\n", __func__, buffer, buflen);
		goto __exx;
	}

	pony_monkey_ctx_free(ctx);

	printf("INFO : %s success\n", __func__);

	return;

__exx:

	pony_monkey_ctx_free(ctx);

	return;
}

void
foo_pony_query_monkey_bsc_case_4(void)	{
	char buffer[16];
	int buflen;
	void * ctx;
	int t;
	int r;

	ctx = pony_monkey_ctx_new(2);
	if (!ctx)	{
		printf("ERROR: failed to malloc memory for monkey ctx\n");
		return;
	}

	r = pony_monkey_ctx_set_script(ctx, "return Param(\"int0\");");
	if (r)	{
		printf("ERROR: failed to set monkey call script\n");
		goto __exx;
	}

	t = 0x12345678;

	r = pony_monkey_ctx_add_int_param(ctx, "int0", 0x12345678);
	if (r)	{
		printf("ERROR: failed to set monkey string value\n");
		goto __exx;
	}

	r = pony_monkey_ctx_add_int_param(ctx, "int1", 0x87654321);
	if (r)	{
		printf("ERROR: failed to set monkey string value\n");
		goto __exx;
	}

	buflen = sizeof(buffer);
	r = pony_monkey("x", ctx, buffer, &buflen);
	if (r)	{
		printf("ERROR: failed to execute monkey script\n");
		printf("       r = %d\n", r);
		goto __exx;
	}

	r = *(int *)buffer;

	if (t != r)	{
		printf("ERROR: remote monkey call result is 0x%x supposed to be 0x%x\n", r, t);
		goto __exx;
	}

	pony_monkey_ctx_free(ctx);

	printf("INFO : %s success\n", __func__);

	return;

__exx:

	pony_monkey_ctx_free(ctx);

	return;
}

void
foo_pony_query_monkey_bsc(void * dummy)	{
	foo_pony_query_monkey_bsc_case_0();
	foo_pony_query_monkey_bsc_case_1();
	foo_pony_query_monkey_bsc_case_2();
	foo_pony_query_monkey_bsc_case_3();
	foo_pony_query_monkey_bsc_case_4();
}

/* eof */
