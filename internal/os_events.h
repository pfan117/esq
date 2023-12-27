#ifndef __ESQ_CORE_OS_EVENT_HEADER_INCLUDED__
#define __ESQ_CORE_OS_EVENT_HEADER_INCLUDED__

/* for service code */
extern int esq_os_events_add_socket_fd(int fd, void * user_data, int exp_event_bits);
extern int esq_os_events_add_socket_fd_multishot(int , void * user_data, int exp_event_bits);
extern int esq_os_events_remove_socket_fd(void * user_data);

/* miscs */
extern const char * esq_get_io_uring_bits_str(int bits);

#endif /* __ESQ_CORE_OS_EVENT_HEADER_INCLUDED__ */
