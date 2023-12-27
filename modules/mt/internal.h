#ifndef __ESQ_MODULE_TEST_INTERNAL_HEADER_INCLUDED__
#define __ESQ_MODULE_TEST_INTERNAL_HEADER_INCLUDED__

extern pthread_spinlock_t esq_mod_test_spinlock;
extern volatile int esq_tst_handled_timedout_events;

extern void esq_tst_event_cb(int fd, void * current_session, void * event_param);
extern int esq_mt_single_shot_socket_pt_handle_timeout_event(int session_cnt, int timeout, int wait_usecond);
extern int esq_mt_single_shot_socket_pt_handle_task_done_event(int parent_cnt, int child_cnt, int wait_for_child);
extern int esq_mt_single_shot_socket_pt_handle_exit_event(int session_cnt, int wait_usecond);
extern int esq_mt_multi_shot_socket_pt_handle_exit_event(int session_cnt, int wait_usecond);
extern int esq_mt_hash_lock_sessions(void);

extern void range_map_test_0(void);

#endif
