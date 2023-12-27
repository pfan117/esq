// #define PRINT_DBG_LOG

#include <signal.h>
#include <stdio.h>
#include <strings.h>

#include "public/esq.h"
#include "public/libesq.h"
#include "public/esqll.h"
#include "internal/log_print.h"

#include "internal/signals.h"
#include "internal/threads.h"

static volatile int esq_server_stop_flag = 0;

int
esq_get_server_stop_flag(void)	{
	return esq_server_stop_flag;
}

STATIC void
esq_set_server_stop_flag(int para)	{
	esq_server_stop_flag = 1;
	return;
}

void
esq_stop_server_on_purpose(void)	{
	esq_server_stop_flag = 1;
	esq_wake_up_master_thread();
	return;
}

STATIC void
esq_dump_backtrace(int para)	{
	esq_backtrace();
	return;
}

STATIC void
esq_dummy_signal_handler(int para)	{
	PRINTF("DBG: %s() %d\n", __func__, __LINE__);
	return;
}

int
esq_install_sigsegv_signal_handler(void)	{
#if 1
	struct sigaction s;
	int r;

	bzero(&s, sizeof(s));
	s.sa_handler = esq_dump_backtrace;
	r = sigaction(SIGSEGV, &s, NULL);
	if (r)	{
		printf("ERROR: %s() %d: failed to register signal handler for epoll/kqueue gear change notify\n"
				, __func__, __LINE__
				);
		return -1;
	}
#endif

	return 0;
}

int
esq_install_notify_signal_handler(void)	{
	struct sigaction s;
	int r;

	bzero(&s, sizeof(s));
	s.sa_handler = esq_dummy_signal_handler;
	r = sigaction(SIGUSR1, &s, NULL);
	if (r)	{
		printf("ERROR: %s() %d: failed to register signal handler for epoll/kqueue gear change notify\n"
				, __func__, __LINE__
				);
		return -1;
	}

	return 0;
}

#define MASTER_X_BLOCK_SIGNAL_LIST	SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGSEGV, SIGABRT, SIGPIPE, SIGUSR1

int
esq_install_master_thread_signal_handlers(void)	{
	int sigs[] = {MASTER_X_BLOCK_SIGNAL_LIST};
	struct sigaction s;
	int i;
	int r;

	bzero(&s, sizeof(s));
	s.sa_handler = esq_set_server_stop_flag;

	for (i = 0; i < (sizeof(sigs) / sizeof(sigs[0])); i ++)	{
		r = sigaction(sigs[i], &s, NULL);
		if (r)	{
			printf("ERROR: %s() %d: failed to register signal handler\n", __func__, __LINE__);
			return -1;
		}
	}

	r = esq_install_notify_signal_handler();
	if (r)	{
		return -1;
	}

	r = esq_install_sigsegv_signal_handler();
	if (r)	{
		return -1;
	}

	return 0;
}

int
__esq_mask_signal_list(int * sigs, int cnt)	{
	sigset_t mask;
	int r;
	int i;

	sigemptyset(&mask);

	for (i = 0; i < cnt; i ++)	{
		sigaddset(&mask, sigs[i]);
	}

	r = pthread_sigmask(SIG_BLOCK, &mask, NULL);
	if (r)	{
		printf("ERROR: %s() %d: failed to block signals\n", __func__, __LINE__);
		return -1;
	}

	return 0;
}

int
esq_master_thread_mask_signals(void)	{
	int sigs[] = {MASTER_X_BLOCK_SIGNAL_LIST};
	int r;

	r = __esq_mask_signal_list(sigs, (sizeof(sigs) / sizeof(sigs[0])));

	return r;
}

#define WORKER_X_BLOCK_SIGNAL_LIST	SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGSEGV, SIGABRT, SIGPIPE

int
esq_worker_thread_mask_signals(void)	{
	int sigs[] = {WORKER_X_BLOCK_SIGNAL_LIST};
	int r;

	r = __esq_mask_signal_list(sigs, (sizeof(sigs) / sizeof(sigs[0])));

	return r;
}

int
__esq_thread_open_signals(int *sigs, int cnt)	{
	sigset_t mask;
	int r;
	int i;

	sigemptyset(&mask);

	for (i = 0; i < cnt; i ++)	{
		sigaddset(&mask, sigs[i]);
	}

	r = pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
	if (r)	{
		printf("ERROR: %s() %d: failed to unblock signals\n", __func__, __LINE__);
		return -1;
	}

	return 0;

}

int
esq_master_thread_open_signals(void)	{
	int sigs[] = {MASTER_X_BLOCK_SIGNAL_LIST};
	int r;

	r = __esq_thread_open_signals(sigs, sizeof(sigs) / sizeof(sigs[0]));
	if (r)	{
		printf("ERROR: %s() %d: failed to unblock signals\n", __func__, __LINE__);
		return -1;
	}

	return 0;
}

int
esq_worker_thread_open_signals(void)	{
	int sigs[] = { SIGUSR1 };
	int r;

	r = __esq_thread_open_signals(sigs, sizeof(sigs) / sizeof(sigs[0]));
	if (r)	{
		printf("ERROR: %s() %d: failed to unblock signals\n", __func__, __LINE__);
		return -1;
	}

	return 0;
}

/* eof */
