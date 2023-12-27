//#include <nghttp2/nghttp2.h>
#include <stdio.h>
#include <sys/time.h>
//#include <string.h>
//#include <monkeycall.h>

//#include "public/esq.h"
//#include "public/esqll.h"
//#include "internal/types.h"
//#include "public/h2esq.h"
#include "modules/foo_server/utils.h"

static struct timeval foo_time;

void
foo_timing_begin(void)	{
	int r;

	r = gettimeofday(&foo_time, NULL);
	if (r)	{
		printf("ERROR: %s() %d: failed to call gettimeofday()\n", __func__, __LINE__);
	}
	else	{
		printf("-- timing begin -- \n");
	}
}

void
foo_timing_end(void)	{
	struct timeval cur_time;
	int r;

	r = gettimeofday(&cur_time, NULL);
	if (r)	{
		printf("ERROR: %s() %d: failed to call gettimeofday()\n", __func__, __LINE__);
	}

	if (cur_time.tv_usec >= foo_time.tv_usec)	{
		printf("-- timing end - %ld.%06ld usec --\n"
				, cur_time.tv_sec - foo_time.tv_sec
				, cur_time.tv_usec - foo_time.tv_usec
				);
	}
	else	{
		printf("-- timing end - %ld.%06ld usec --\n"
				, cur_time.tv_sec - foo_time.tv_sec - 1
				, 1000 * 1000 + cur_time.tv_usec - foo_time.tv_usec
				);
		/* 1000000 */
	}

	return;
}

/* eof */
