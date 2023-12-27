#include <stdio.h>
#include <stdlib.h>
#include "public/libesq.h"
#include "test-functions.h"

tt t[] = {
	test_function_000,
	test_function_001,
	test_function_002,
	test_function_003,
	test_function_004,
	test_function_005,
	test_function_006,
	test_function_007,
	test_function_008,
	test_function_009,
	test_function_010,
	test_function_011,
};

int
main(void)	{

	int i;
	int r;

	for (i = 0; i < (sizeof(t) / sizeof(t[0])); i ++)	{
		r = t[i]();
		if (r)	{
			printf("TC[%d] FAILED\n", i);
			return -1;
		}
		else	{
			printf("TC[%d] OK\n", i);
		}
	}

	printf("FINISHED\n");

	return 0;
}


/* eof */
