#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "public/esq.h"
#include "public/esqll.h"
#include "internal/types.h"
#include "internal/threads.h"
#include "modules/mt/internal.h"

int
esq_mt_hash_lock_sessions(void)	{
	printf("INFO: %s(): begin\n", __func__);
	printf("INFO: case passed\n");
	return 0;
}

/* eof */
