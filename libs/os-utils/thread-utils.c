#define _GNU_SOURCE	/* for thread-CPU binding */

#include <sys/sysinfo.h>	/* get cpu count */

#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

/* return -1 for error */
int
os_get_current_core_id_slow(void)	{
	cpu_set_t get;
	int cpus;
	int i;
	int r;

	cpus = sysconf(_SC_NPROCESSORS_CONF);
	CPU_ZERO(&get);
	r = pthread_getaffinity_np(pthread_self(), sizeof(get), &get);
	if (r)	{
		printf("ERROR: %s() %d: pthread_getaffinity_np() return error\n", __func__, __LINE__);
		return -1;
	}
	else	{
		for (i = 0; i < cpus; i ++)	{
			if (CPU_ISSET(i, &get))	{
				/* printf("INFO: %s() %d: running on processor %d\n", __func__, __LINE__, i); */
				return i;
			}
		}

		printf("ERROR: %s() %d: failed to get current core id\n", __func__, __LINE__);

		return -1;
	}
}

/* eof */
