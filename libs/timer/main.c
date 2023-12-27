#ifdef TIMER_CHECK

#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>

int
main(void)	{
	struct timeval a, b;
	int r;

	gettimeofday(&a, NULL);
	usleep(1);
	gettimeofday(&b, NULL);

	r = timercmp(&a, &a, >);
	printf("r = %d, should be 0\n", r);

	r = timercmp(&b, &b, >);
	printf("r = %d, should be 0\n", r);

	r = timercmp(&a, &b, >);
	printf("r = %d, should be 0\n", r);

	r = timercmp(&b, &a, >);
	printf("r = %d, should be 1\n", r);

	return 0;
}

#endif
