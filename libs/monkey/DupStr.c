#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <monkeycall.h>

#include "public/esq.h"
#include "public/libesq.h"
#include "internal/types.h"

#include "libs/monkey/internal.h"

int
esq_mkccb_duplicate_string(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	mkc_data * s;
	mkc_data * t;
	char * buf;
	int offset;
	int ssize;
	int size;
	int i;

	MKC_CB_ARGC_CHECK(2);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(1, MKC_D_INT);
	result = MKC_CB_RESULT;
	s = MKC_CB_ARGV(0);
	t = MKC_CB_ARGV(1);

	if (t->integer <= 0)	{
		goto __exx;
	}

	ssize = s->length;
	size = ssize * t->integer;

	if (size <= 0 || (size < s->length) || (size < t->integer))	{
		goto __exx;
	}

	buf = mkc_session_malloc(session, size + 1);
	if (!buf)	{
		goto __exx;
	}

	offset = 0;

	for (i = 0; i < t->integer; i ++)	{
		memcpy(buf + offset, s->buffer, ssize);
		offset += ssize;
	}

	buf[size] = '\0';

	result->type = MKC_D_BUF;
	result->buffer = buf;
	result->length = size;

	return 0;

__exx:

	result->type = MKC_D_VOID;

	return 0;
}

/* eof */
