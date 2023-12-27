#ifndef __ESQ_CORE_INCLUDED_SIGNALS_POOL__
#define __ESQ_CORE_INCLUDED_SIGNALS_POOL__

extern int esq_get_server_stop_flag(void);
extern int esq_install_master_thread_signal_handlers(void);
extern int esq_master_thread_open_signals(void);
extern int esq_worker_thread_open_signals(void);
extern int esq_install_notify_signal_handler(void);

#endif /* __ESQ_CORE_INCLUDED_SIGNALS_POOL__ */
