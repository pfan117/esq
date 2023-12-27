#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <monkeycall.h>

#include "public/esq.h"
#include "public/libesq.h"
#include "internal/types.h"

#include "libs/monkey/internal.h"
#include "libs/monkey/monkey-lib.h"

int
esq_monkey_lib_install(void)	{
	void * mkc;

	mkc = esq_get_mkc();

	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "DupStr", esq_mkccb_duplicate_string));

	return 0;
}

void
esq_monkey_lib_uninstall(void)	{
	void * mkc;

	mkc = esq_get_mkc();
	mkc_cbtree_del(mkc, "DupStr");

	return;
}

/* eof */
