#ifndef __ESQ_CORE_SESSION_HEADER_INCLUDED__
#define __ESQ_CORE_SESSION_HEADER_INCLUDED__

/* for master thread */
extern int esq_event_check_magic(void * event);
extern void esq_wake_up_all_worker_thread(void);
extern void esq_hash_lock_release_all(void);

/* for worker thread */
extern void * esq_worker_loop(void * dummy);
extern void * esq_worker_get_io_uring(void);

/* for mco */
extern void esq_session_init_mco_params(void * session_ptr, void * mco);

/* for os-event */
extern void esq_notify_event_to_session(void * session_ptr, int event_bits);
extern void esq_notify_aio_removed(void * session);

/* for mt */
extern void esq_session_notify_exit_to_all_sessions(void);

extern int esq_hash_lock_init(void);
extern void esq_hash_lock_detach(void);

#endif /* __ESQ_CORE_SESSION_HEADER_INCLUDED__ */
