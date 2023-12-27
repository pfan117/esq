#ifndef __ESQ_HEADER_INCLUDED__
#define __ESQ_HEADER_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

#include <mco.h>

/* ESQ - Event Session Queue */

/* return values */
#define ESQ_OK				(0)
#define ESQ_ERROR_PARAM		(-1)
#define ESQ_ERROR_MEMORY	(-2)
#define ESQ_ERROR_LIBCALL	(-3)
#define ESQ_ERROR_NOTFOUND	(-4)
#define ESQ_ERROR_SIZE		(-5)
#define ESQ_ERROR_EXIST		(-6)
#define ESQ_ERROR_CLOSE		(-7)
#define ESQ_ERROR_TIMEOUT	(-8)
#define ESQ_ERROR_BUSY		(-9)
#define ESQ_ERROR_DEAD_LOCK	(-10)
#define ESQ_ERROR_TBD		(-11)

/* module interface */
#define ESQ_MODULE_FILENAME_MAX	(128)
typedef int (*esq_module_init_cb_t)(void);	/* prototype of __esq_mod_init() */
typedef void (*esq_module_detach_cb_t)(void);	/* prototype of __esq_mod_detach() */

/* call back function to handle the listed events */
#define ESQ_EVENT_BIT_FD_CLOSE		0x01
#define ESQ_EVENT_BIT_ERROR			0x02
#define ESQ_EVENT_BIT_TIMEOUT		0x04
#define ESQ_EVENT_BIT_READ			0x08
#define ESQ_EVENT_BIT_WRITE			0x10
#define ESQ_EVENT_BIT_LOCK_OBTAIN	0x20
#define ESQ_EVENT_BIT_TASK_DONE		0x40
#define ESQ_EVENT_BIT_EXIT			0x80

typedef void (*esq_event_cb_t)(void * esq_session_ptr);

extern void * esq_session_new(const char * name);
extern int esq_session_check_magic(void * session_ptr);

extern int esq_session_set_socket_fd(
		void * session, int fd, esq_event_cb_t cb,
		int event_bits,
		int timeouts, int timeoutms,
		void * param
		);
extern int esq_session_set_socket_fd_multi(
		void * session, int fd, esq_event_cb_t cb,
		void * param
		);

extern int esq_session_set_cb(void * session, esq_event_cb_t cb);
extern void esq_session_set_param(void * session_ptr, void * user_param);
extern void * esq_session_get_param(void * session_ptr);
extern int esq_session_get_fd(void * session_ptr);
extern int esq_session_get_event_bits(void * session_ptr);
extern void esq_session_remove_req(void * session);
extern int esq_session_bundle_child(void * parent_session, void * child_session, void * child_finish_cb);
extern void esq_session_wind_up(void * session);
extern void esq_session_wind_up_rr(void * session);
extern void * esq_session_get_parent(void * session);

extern void esq_execute_session_done_cb(void *);

extern void esq_hash_lock_read(void * session, int hash_lock_idx, esq_event_cb_t cb, void * user_param, int timeout_s);
extern void esq_hash_lock_write(void * session, int hash_lock_idx, esq_event_cb_t cb, void * user_param, int timeout_s);

/** esq mco **/
typedef void (*esq_service_mco_entry_t)(void *);

extern int esq_start_mco_session(
		const char * name
		, void * parent_session
		, esq_service_mco_entry_t entry
		, size_t stack_size
		, void * service_mco_param
		);

/* get internal mkc instance handler */
extern void * esq_get_mkc(void);

/* internal test function prototype, function name should be "esq_mt_start" */
typedef void(*esq_mt_start_t)(void);

/* for some unrecoverable failure, or test propose, call this function to cause program detach */
extern void esq_stop_server_on_purpose(void);

/* debug tool functions */
extern int esq_backtrace(void);

/* dashboard */
#define ESQ_DASHBOARD_OP_STOP	9
/* return -1 for error */
/* return >= 0 for success */
extern int esq_dashboard_new(void);
extern int esq_dashboard_get_opcode(int id, int *opcode);
extern int esq_dashboard_set_opcode(int id, int opcode);
extern int esq_dashboard_snprintf(int id, int offset, const char * format, ...);
extern int esq_dashboard_remove_item(int id);

/* logger */
extern int esq_log(const char * fmt, ...) __attribute__((format(printf, 1, 2)));

/* debugging */
extern const char * esq_get_event_bits_string(int bits, char * buf, int length);

#ifdef __cplusplus
}
#endif

#endif /* __ESQ_HEADER_INCLUDED__ */
