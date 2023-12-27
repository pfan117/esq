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

#include "modules/foo_server/utils.h"
#include "modules/foo_server/callbacks.h"

void
foo_mco_basic_test(void * dummy)	{
	printf("mco basic test begin\n");
	printf("mco basic test end\n");
	return;
}

/* eof */
