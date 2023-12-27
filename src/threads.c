#ifdef __linux__
#define _GNU_SOURCE	/* for thread-CPU binding */
#endif

// #define PRINT_DBG_LOG

#include <sys/sysinfo.h>	/* get cpu count */

#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <signal.h>
#include <strings.h>
#include <stdlib.h>
#include <semaphore.h>
#include <monkeycall.h>
#include <mco.h>
#include <time.h>

#include "public/esq.h"
#include "public/libesq.h"
#include "public/esqll.h"
#include "internal/threads.h"
#include "internal/log_print.h"

#include "internal/threads.h"
#include "internal/session.h"
#include "internal/modules.h"
#include "internal/signals.h"
#include "internal/lock.h"

static pthread_t esq_master_thread;
static int esq_cpu_core_number = -1;

void
esq_wake_up_master_thread(void)	{
	pthread_kill(esq_master_thread, SIGUSR1);
	return;
}

STATIC int
esq_thread_bind_2_cpu(pthread_t t, unsigned int cpu)	{
	int r;
	cpu_set_t mask;

	CPU_ZERO(&mask);
	CPU_SET(cpu, &mask);
	r = pthread_setaffinity_np(t, sizeof(mask), &mask);
	if (r)	{
		printf("ERROR: %s, %d: %s\n", __func__, __LINE__, "failed to bind thread to CPU");
		return -1;
	}

	return 0;
}

static volatile int esq_terminate_all_session_flag = 0;

int esq_get_terminate_all_session_flag(void)	{
	return esq_terminate_all_session_flag;
}

int
esq_start_worker_threads(void)	{
	int cpus;
	int r;
	int i;
	pthread_t thread;
	pthread_attr_t attr;

	pthread_attr_init(&attr);

	esq_master_thread = pthread_self();

	cpus = get_nprocs();
	if (cpus <= 0)	{
		printf("ERROR: %s(), %d: invalid cpu count - %d\n", __func__, __LINE__, cpus);
		return -1;
	}

	esq_cpu_core_number = cpus;

	r = esq_master_thread_mask_signals();
	if (r)	{
		return -1;
	}

	for (i = 0; i < cpus; i ++)	{
		r = pthread_create(&thread, &attr, esq_worker_loop, NULL);
		if (r)	{
			pthread_attr_destroy(&attr);
			printf("ERROR: %s, %d: %s %d\n", __func__, __LINE__, "failed to create worker thread", i);
			return -1;
		}

		r = esq_thread_bind_2_cpu(thread, i);
		if (r)	{
			pthread_attr_destroy(&attr);
			return -1;
		}
	}

	pthread_attr_destroy(&attr);

	r = esq_install_master_thread_signal_handlers();
	if (r)	{
		return -1;
	}

	r = esq_master_thread_open_signals();
	if (r)	{
		return -1;
	}

	for (i = 0; i < cpus * 2; i ++)	{
		sleep(8);
		if (esq_get_worker_list_length() == cpus)	{
			return ESQ_OK;
		}
	}

	printf("ERROR: %s() %d: threads not started in time\n", __func__, __LINE__);

	return ESQ_ERROR_LIBCALL;
}

void
esq_main_thread_loop(void)	{
	#ifdef MT_MODE
	esq_trigger_mt();
	#endif

	/* working */
	while(1)	{

		sleep(100);
		if (esq_get_server_stop_flag())	{
			break;
		}
		else	{
			continue;
		}

	}
}

void
esq_stop_worker_threads(void)	{

	for (;;)	{

		esq_wake_up_all_worker_thread();
		esq_hash_lock_release_all();

		if (!esq_get_worker_list_length())	{
			PRINTF("INFO: %s() %d: all worker thread quit\n", __func__, __LINE__);
			return;
		}

		sleep(1);
	}

	return;
}

int
esq_threads_dump_info(char * buf, int bufsize)	{
	int r;

	r = snprintf(buf, bufsize
			, "cpu_core_number %d\n"
			, esq_cpu_core_number
			);

	return r;
}

/* eof */
