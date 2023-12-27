#ifndef __ESQ_THREADS_HEADER_INCLUDED__
#define __ESQ_THREADS_HEADER_INCLUDED__

extern int esq_threads_dump_info(char * buf, int bufsize);

extern int esq_start_worker_threads(void);
extern void esq_main_thread_loop(void);
extern void esq_stop_worker_threads(void);

extern void esq_wake_up_master_thread(void);
extern void esq_worker_thread_sleep(void);
extern int esq_get_worker_list_length(void);

#endif /* __ESQ_THREADS_HEADER_INCLUDED__ */
