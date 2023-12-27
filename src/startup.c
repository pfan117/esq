#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef USE_GRPOF
	#include <sys/types.h>
	#include <unistd.h>
	#include <gperftools/profiler.h>
#endif

#include "public/esq.h"
#include "public/libesq.h"
#include "internal/types.h"
#include "internal/tree.h"
#include "public/esqll.h"
#include "internal/range_map.h"
#include "internal/os_events.h"
#include "internal/session.h"
#include "internal/threads.h"
#include "internal/modules.h"
#include "internal/lock.h"
#include "internal/monkeycall.h"
#include "internal/dashboard.h"
#include "internal/logger.h"
#include "internal/ebacktrace.h"
#include "internal/file_lru.h"
#include "nghttp2_wrapper/nghttp2_wrapper.h"
#include "bee/public.h"
#include "internal/host_control.h"
#include "internal/config.h"

#include "libs/monkey/monkey-lib.h"

#define IF_ERROR_RETURN(__OP__)	do	{	\
	if ((__OP__))	{	\
		return -1;	\
	}	\
} while(0);

int
esq_process_arguments(int argc, char ** argv)	{
	int i;
	int l;
	int r;

	for (i = 1; i < argc; i ++)	{

		l = strnlen(argv[i], ESQ_MODULE_FILENAME_MAX);
		if (l >= ESQ_MODULE_FILENAME_MAX || l <= 0)	{
			printf("ERROR: %s, %d, %s(): invalid argument\n", __FILE__, __LINE__, __func__);
			return ESQ_ERROR_PARAM;
		}

		if ((l > 3) && ('.' == argv[i][l - 3]) && ('s' == argv[i][l - 2]) && ('o' == argv[i][l - 1]))	{
			r = esq_modules_load(argv[i]);
		}
		else	{
			r = esq_load_configuration_from_file(argv[i]);
		}

		if (r)	{
			return r;
		}
	}

	return ESQ_OK;
}

int
main(int argc, char ** argv)	{
	int r;

	#ifdef USE_GRPOF
	pid_t pid;
	char prof_fn[128];

	pid = getpid();
	r = snprintf(prof_fn, sizeof(prof_fn), "gprof.%d.prof", pid);
	if (r >= sizeof(prof_fn) || r <= 0)	{
		printf("ERROR: failed to prepare gprof file name\n");
		return -1;
	}

	ProfilerStart(prof_fn);
	#endif

	ebacktrace_init(argv[0]);

	IF_ERROR_RETURN(os_get_if_list());
	IF_ERROR_RETURN(esq_mkc_init());
	IF_ERROR_RETURN(esq_monkey_lib_install());
	IF_ERROR_RETURN(esq_logger_init());
	IF_ERROR_RETURN(file_lru_init());
	IF_ERROR_RETURN(esq_dashboard_init());
	IF_ERROR_RETURN(esq_range_map_init());
	IF_ERROR_RETURN(esq_global_lock_init());
	IF_ERROR_RETURN(esq_hash_lock_init());
	IF_ERROR_RETURN(esq_modules_init());
	IF_ERROR_RETURN(h2esq_init());
	IF_ERROR_RETURN(esq_bee_init());	/* 'bee' uses the lock from 'range map' */
	IF_ERROR_RETURN(esq_host_control_init());
	IF_ERROR_RETURN(esq_start_worker_threads());

	r = esq_process_arguments(argc, argv);
	if (r)	{
		esq_stop_worker_threads();
	}
	else	{
		esq_main_thread_loop();
		esq_stop_worker_threads();
	}

	esq_dashboard_stop();
	esq_host_control_detach();
	esq_bee_detach();
	h2esq_detach();
	esq_modules_detach();
	esq_hash_lock_detach();
	esq_global_lock_detach();
	esq_range_map_detach();
	esq_dashboard_detach();
	esq_monkey_lib_uninstall();
	esq_mkc_detach();
	os_free_if_list();
	file_lru_detach();
	esq_logger_detach();

	#ifdef USE_GRPOF
	ProfilerStop();
	#endif

	return r;
}

/* eof */
