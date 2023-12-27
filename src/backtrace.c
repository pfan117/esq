/* vi: set sw=4 ts=4: */

#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>

#include "internal/ebacktrace.h"

int
esq_pool_backtrace(void)	{
	int i, nptrs;
	char ** strings;
	#define SIZE 100
	void * buffer[SIZE];

	nptrs = backtrace(buffer, SIZE);
	strings = backtrace_symbols(buffer, nptrs);
	if (!strings)	{
		return -1;
	}

	for (i = 0; i < nptrs; i ++)	{
        printf("%s\n", strings[i]);
	}

	free(strings);

	return 0;
}

int
esq_backtrace(void)	{
	int r;

	printf("%s\n", "-- esq backtrace start --");

	r = ebacktrace();

	if (!r)	{
		printf("%s\n", "-- esq backtrace end --");
		return 0;
	}

	esq_pool_backtrace();

	printf("%s\n", "-- esq backtrace end --");

	return 0;
}

/* eof */
