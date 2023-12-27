#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <strings.h>
#include <string.h>
#include <time.h>
#include <monkeycall.h>

#include <sys/types.h>	/* stat(), lstat */
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>	/* open() */

#include "public/esq.h"
#include "public/libesq.h"

#include "internal/logger.h"

#define LOGBUFCNT	4
#define LOGBUFSIZE	4096

static char esq_logger_buf[LOGBUFCNT][LOGBUFSIZE];
static int esq_logger_buf_offset[LOGBUFCNT];
enum {BUFFER_S_EMPTY, BUFFER_S_USE, BUFFER_S_FULL};
static char esq_logger_buf_state[LOGBUFCNT] = {0};
static volatile int esq_logger_buf_user_idx = 0;
static volatile int esq_logger_buf_worker_idx = 0;

static pthread_spinlock_t esq_logger_spin;
static sem_t esq_logger_user_sem;
static sem_t esq_logger_worker_sem;

static volatile int esq_logger_fd = -1;

static volatile int esq_logger_stop_flag = 0;
static volatile int esq_logger_change_filename_flag = 0;
static pthread_t esq_logger_worker_thread;
static volatile int esq_logger_filename_idx = 0;

#define IOERRORPAUSEGAP	(60 * 5)
static time_t esq_logger_last_io_error_time = 0;

STATIC int
esq_logger_fill_log_filename(char * buf, int buflen)	{
	struct tm * ptminfo;
	time_t rawtime;
	int r;

	time(&rawtime);
	ptminfo = localtime(&rawtime);

	r = snprintf(buf, buflen, "log-%04d-%02d-%02d-%02d-%02d-%02d-%04d.log"
			, ptminfo->tm_year + 1900
			, ptminfo->tm_mon + 1
			, ptminfo->tm_mday + 1
			, ptminfo->tm_hour
			, ptminfo->tm_min
			, ptminfo->tm_sec
			, esq_logger_filename_idx
			);

	esq_logger_filename_idx ++;
	if (esq_logger_filename_idx > 9999)	{
		esq_logger_filename_idx = 0;
	}

	if (r <= 0 || r >= buflen)	{
		esq_design_error();
	}

	return 0;
}

/* return 0 for success */
/* return -1 for io error */
STATIC int
esq_logger_flush_to_disk(int buf_idx, int buf_length)	{
	int r;

	if (-1 == esq_logger_fd)	{
		char fn[128];
		int fd;

		r = esq_logger_fill_log_filename(fn, sizeof(fn));
		if (r)	{
			return -1;
		}

		fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC, (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH));
		if (-1 == fd)	{
			return -1;
		}
		else	{
			esq_logger_fd = fd;
		}
	}

	r = write(esq_logger_fd, esq_logger_buf[buf_idx], buf_length);
	if (r == buf_length)	{
		;
	}
	else	{
		return -1;
	}

	return 0;
}

STATIC void
esq_logger_close_file(void)	{

	if (-1 == esq_logger_fd)	{
		return;
	}

	fsync(esq_logger_fd);
	close(esq_logger_fd);
	esq_logger_fd = -1;

	return;
}

STATIC int
esq_logger_buf_next_idx(int i)	{
	int r;

	r = i + 1;

	if (r >= LOGBUFCNT)	{
		return 0;
	}
	else	{
		return r;
	}
}

void
esq_logger_flush_one_buffer(void)	{

	int offset;
	time_t t;
	int idx;
	int r;
	int i;

	if (esq_logger_last_io_error_time)	{
		time(&t);
		if ((esq_logger_last_io_error_time + IOERRORPAUSEGAP) < t)	{
			goto __drop_all;
		}
	}

	pthread_spin_lock(&esq_logger_spin);

	idx = esq_logger_buf_worker_idx;

	if (idx == esq_logger_buf_user_idx)	{
		idx = esq_logger_buf_next_idx(idx);
	}

	for (i = 0; i < LOGBUFCNT; i ++)	{
		if (BUFFER_S_EMPTY == esq_logger_buf_state[idx])	{
			idx = esq_logger_buf_next_idx(idx);
			continue;
		}
		else	{
			esq_logger_buf_worker_idx = idx;
			goto __work;
		}
	}

	/* nothing to write back for this time */
	pthread_spin_unlock(&esq_logger_spin);
	sem_post(&esq_logger_user_sem);
	return;

__work:

	esq_logger_buf_state[esq_logger_buf_worker_idx] = BUFFER_S_FULL;
	offset = esq_logger_buf_offset[esq_logger_buf_worker_idx];

	if (!offset)	{
		pthread_spin_unlock(&esq_logger_spin);
		sem_post(&esq_logger_user_sem);
		return;
	}

	pthread_spin_unlock(&esq_logger_spin);

	/* some data need to flush back */

	r = esq_logger_flush_to_disk(esq_logger_buf_worker_idx, offset);
	if (r)	{
		goto __io_error;
	}
	else	{
		pthread_spin_lock(&esq_logger_spin);
		esq_logger_buf_offset[esq_logger_buf_worker_idx] = 0;
		esq_logger_buf_state[esq_logger_buf_worker_idx] = BUFFER_S_EMPTY;
		pthread_spin_unlock(&esq_logger_spin);
		sem_post(&esq_logger_user_sem);
		return;
	}

	return;

__io_error:

	time(&t);
	esq_logger_last_io_error_time = t;

__drop_all:

	pthread_spin_lock(&esq_logger_spin);
	bzero(esq_logger_buf_offset, sizeof(esq_logger_buf_offset));
	bzero(esq_logger_buf_state, sizeof(esq_logger_buf_state));
	esq_logger_buf_user_idx = 0;
	esq_logger_buf_worker_idx = 0;
	pthread_spin_unlock(&esq_logger_spin);
	sem_post(&esq_logger_user_sem);

	return;
}

STATIC void *
esq_logger_worker(void * dummy)	{

	int i;

	for (;;)	{

		sem_wait(&esq_logger_worker_sem);

		pthread_spin_lock(&esq_logger_spin);
		if (esq_logger_stop_flag)	{
			pthread_spin_unlock(&esq_logger_spin);
			break;
		}
		pthread_spin_unlock(&esq_logger_spin);

		esq_logger_flush_one_buffer();

		pthread_spin_lock(&esq_logger_spin);
		if (esq_logger_change_filename_flag)	{
			esq_logger_change_filename_flag = 0;
			pthread_spin_unlock(&esq_logger_spin);
			esq_logger_close_file();
		}
		else	{
			pthread_spin_unlock(&esq_logger_spin);
		}
	}

	for (i = 0; i < LOGBUFCNT; i ++)	{
		esq_logger_flush_one_buffer();
	}

	esq_logger_close_file();

	return NULL;
}

int esq_log(const char * fmt, ...) __attribute__((format(printf, 1, 2)));

int esq_log(const char * fmt, ...)	{
	va_list arg_ptr;
	int next;
	int r;

	for (;;)	{

		pthread_spin_lock(&esq_logger_spin);

		if (esq_logger_stop_flag)	{
			pthread_spin_unlock(&esq_logger_spin);
			return ESQ_ERROR_CLOSE;
		}

		if (BUFFER_S_FULL == esq_logger_buf_state[esq_logger_buf_user_idx])	{
			next = esq_logger_buf_next_idx(esq_logger_buf_user_idx);
			if (BUFFER_S_EMPTY == esq_logger_buf_state[next])	{
				esq_logger_buf_user_idx = next;
				esq_logger_buf_state[esq_logger_buf_user_idx] = BUFFER_S_USE;
				esq_logger_buf_offset[esq_logger_buf_user_idx] = 0;
			}
			else	{
				pthread_spin_unlock(&esq_logger_spin);
				sem_post(&esq_logger_worker_sem);
				sem_wait(&esq_logger_user_sem);
				continue;
			}
		}
		else	{
			esq_logger_buf_state[esq_logger_buf_user_idx] = BUFFER_S_USE;
		}

		va_start(arg_ptr, fmt);
		r = vsnprintf(
				esq_logger_buf[esq_logger_buf_user_idx] + esq_logger_buf_offset[esq_logger_buf_user_idx]
				, LOGBUFSIZE - esq_logger_buf_offset[esq_logger_buf_user_idx]
				, fmt, arg_ptr
				);
		va_end(arg_ptr);

		if (r <= 0)	{
			pthread_spin_unlock(&esq_logger_spin);
			return ESQ_ERROR_PARAM;
		}
		else if (r >= LOGBUFSIZE - esq_logger_buf_offset[esq_logger_buf_user_idx])	{
			if (r >= LOGBUFSIZE)	{
				pthread_spin_unlock(&esq_logger_spin);
				return ESQ_ERROR_SIZE;
			}
			else	{
				esq_logger_buf_state[esq_logger_buf_user_idx] = BUFFER_S_FULL;
				pthread_spin_unlock(&esq_logger_spin);
				continue;	/* will switch to the next buffer, maybe start using a empty one */
			}
		}
		else	{
			esq_logger_buf_offset[esq_logger_buf_user_idx] += r;
			pthread_spin_unlock(&esq_logger_spin);
			sem_post(&esq_logger_worker_sem);
			sem_post(&esq_logger_user_sem);
			return ESQ_OK;
		}
	}

	esq_design_error();

	return ESQ_ERROR_TIMEOUT;	/* never be here */
}

STATIC int
esq_logger_worker_thread_start(void)	{
	pthread_attr_t attr;
	int r;

	pthread_attr_init(&attr);

	r = pthread_create(&esq_logger_worker_thread, &attr, esq_logger_worker, NULL);
	if (r)	{
		pthread_attr_destroy(&attr);
		fprintf(stderr, "error: failed to start log thread\n");
		return -1;
	}

	pthread_attr_destroy(&attr);

	return 0;
}

STATIC int
esq_mkc_change_log_file(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	esq_logger_change_filename_flag = 1;
	sem_post(&esq_logger_worker_sem);
	result = MKC_CB_RESULT;
	result->type = MKC_D_VOID;
	return 0;
}

STATIC int
esq_mkc_fill_logs(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	mkc_data * cntv;
	int cnt;
	int i;
	int r;

	MKC_CB_ARGC_CHECK(1);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_INT);
	result = MKC_CB_RESULT;
	cntv = MKC_CB_ARGV(0);
	result->type = MKC_D_VOID;

	cnt = cntv->integer;

	for (i = 0; i < cnt; i ++)	{
		r = esq_log("ESQ_LOGGER debug log information, item %d\n", i);
		if (ESQ_OK != r)	{
			return -1;
		}
	}

	return 0;
}

int esq_log_print_enabled = 1;

STATIC int
esq_mkc_enable_log_print(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	MKC_CB_ARGC_CHECK(0);
	esq_log_print_enabled = 1;
	return 0;
}

STATIC int
esq_mkc_disable_log_print(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	MKC_CB_ARGC_CHECK(0);
	esq_log_print_enabled = 0;
	return 0;
}

int
esq_logger_init(void)	{
	void * mkc;
	int r;

	mkc = esq_get_mkc();
	if (!mkc)	{
		fprintf(stderr, "error: %s, %d, %s(): %s\n", __FILE__, __LINE__, __func__, "failed to get mkc instance");
		return ESQ_ERROR_PARAM;
	}

	bzero(esq_logger_buf_offset, sizeof(esq_logger_buf_offset));

	r = pthread_spin_init(&esq_logger_spin, 0);
	if (r)	{
		fprintf(stderr, "error: %s, %d: failed to initial spinlock\n", __func__, __LINE__);
		return -1;
	}

	r = sem_init(&esq_logger_user_sem, 0, 0); 
	if (r)	{
		fprintf(stderr, "error: %s, %d: failed to initial log user wait object\n", __func__, __LINE__);
		return -1;
	}

	r = sem_init(&esq_logger_worker_sem, 0, 0);
	if (r)	{
		fprintf(stderr, "error: %s, %d: failed to initial log worker wait object\n", __func__, __LINE__);
		return -1;
	}

	r = esq_logger_worker_thread_start();
	if (r)	{
		return -1;
	}

	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "ChangeLogFile", esq_mkc_change_log_file));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "FillLogs", esq_mkc_fill_logs));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "LogPrintEnable", esq_mkc_enable_log_print));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "LogPrintDisable", esq_mkc_disable_log_print));

	return ESQ_OK;
}

void
esq_logger_detach(void)	{

	pthread_spin_lock(&esq_logger_spin);
	esq_logger_stop_flag = 1;
	pthread_spin_unlock(&esq_logger_spin);
	sem_post(&esq_logger_user_sem);
	sem_post(&esq_logger_worker_sem);
	pthread_join(esq_logger_worker_thread, NULL);

	sem_destroy(&esq_logger_worker_sem);
	sem_destroy(&esq_logger_user_sem);
	pthread_spin_destroy(&esq_logger_spin);

	return;
}

/* eof */
