/* vi: set sw=4 ts=4: */

#ifndef __ESQ_SESSION_INTERNAL_HEADER_INCLUDED__
#define __ESQ_SESSION_INTERNAL_HEADER_INCLUDED__

/* pt */
#define ESQ_SESSION_PT_BEGIN	switch(session->lc) { case 0: ;
#define ESQ_SESSION_PT_SET	\
		action = NULL; case __LINE__: session->lc = __LINE__; if (!action) { return; }
#define ESQ_SESSION_PT_RESET	session->lc = 0;
#define ESQ_SESSION_PT_END		default: return; }

typedef enum _esq_session_action_type_e	{
	ACTION_NO,
	ACTION_WIND_UP,
	ACTION_WIND_UP_RR,
	ACTION_PICK_UP,
	ACTION_SET_PARENT,
	ACTION_ADD_CHILD,
	ACTION_ADD_SOCK_FD_MULTI,
	ACTION_ADD_SOCK_FD_SINGLE,
	ACTION_MCO_SERVICE_FINISH,
	ACTION_REMOVE_FD,
	ACTION_NOTIFY_EVENT,
	ACTION_NOTIFY_AIO_REMOVED,
	ACTION_NOTIFY_TIMEOUT,
	ACTION_NOTIFY_EXIT,
	ACTION_EXECUTE_CB,
	ACTION_HASH_LOCK_READ,
	ACTION_HASH_LOCK_WRITE,
} esq_session_action_type_e;

extern const char * esq_get_action_type_name(int request_type);

#define GOTO(__SN__)	PRINTF("DBG: %s() %d: %p <%s> switch to "#__SN__"\n" \
		, __func__, __LINE__ \
		, session, session->name \
		); \
		session->state = #__SN__; \
		goto __SN__;

#define FD_NEED_INSTALL(__FD__)	(__FD__ > 0)

#define ESQ_EVENT_BIT_FD_ALL		\
		(ESQ_EVENT_BIT_FD_CLOSE | ESQ_EVENT_BIT_ERROR | \
		 ESQ_EVENT_BIT_TIMEOUT | ESQ_EVENT_BIT_READ | ESQ_EVENT_BIT_WRITE)

#define CLEAR_FD_EVENTS(__BITS__)	__BITS__ = ((__BITS__) & ESQ_EVENT_BIT_FD_ALL)

typedef struct _esq_session_pt_action_t	{
	esq_session_action_type_e type;
	int fd;
	int exp_event_bits;
	int notify_event_bits;
	int timeouts;
	int timeoutms;
	esq_event_cb_t cb;
	mco_cofuncp_t entry;
	size_t stack_size;
	void * user_param;
	int hash_lock_idx;
	int hash_lock_owner_idx;
} esq_session_pt_action_t;

#define ESQ_SESSION_MAGIC			0x1a1a8e8e

#define ESQ_CHECK_SESSION(__SP__)	\
	if ((!__SP__))	{ \
		printf("ERROR: %s() %d: NULL session pointer\n", __func__, __LINE__); \
		esq_backtrace(); \
		return ESQ_ERROR_PARAM; \
	} \
	else if ((ESQ_SESSION_MAGIC != (__SP__)->magic0) || (ESQ_SESSION_MAGIC != (__SP__)->magic1))	{ \
		printf("ERROR: %s() %d: invalid session magic\n", __func__, __LINE__); \
		esq_backtrace(); \
		return ESQ_ERROR_PARAM; \
	}

#define ESQ_CHECK_SESSION_NULL(__SP__)	\
	if ((!__SP__))	{ \
		printf("ERROR: %s() %d: NULL session pointer\n", __func__, __LINE__); \
		esq_backtrace(); \
		return NULL; \
	} \
	else if ((ESQ_SESSION_MAGIC != (__SP__)->magic0) || (ESQ_SESSION_MAGIC != (__SP__)->magic1))	{ \
		printf("ERROR: %s() %d: invalid session magic\n", __func__, __LINE__); \
		esq_backtrace(); \
		return NULL; \
	}

#define ESQ_CHECK_SESSION_VOID(__SP__)	\
	if ((!__SP__))	{ \
		printf("ERROR: %s() %d: NULL session pointer\n", __func__, __LINE__); \
		esq_backtrace(); \
		return; \
	} \
	else if ((ESQ_SESSION_MAGIC != (__SP__)->magic0) || (ESQ_SESSION_MAGIC != (__SP__)->magic1))	{ \
		printf("ERROR: %s() %d: invalid session magic\n", __func__, __LINE__); \
		esq_backtrace(); \
		return; \
	}

#define ESQ_CHECK_SESSION_0(__SP__)	\
	if ((!__SP__))	{ \
		printf("ERROR: %s() %d: NULL session pointer\n", __func__, __LINE__); \
		esq_backtrace(); \
		return 0; \
	} \
	else if ((ESQ_SESSION_MAGIC != (__SP__)->magic0) || (ESQ_SESSION_MAGIC != (__SP__)->magic1))	{ \
		printf("ERROR: %s() %d: invalid session magic\n", __func__, __LINE__); \
		esq_backtrace(); \
		return 0; \
	}

typedef struct _esq_session_t esq_session_t;

typedef struct _esq_hash_lock_ctx_t	{
	#define ESQ_SESSION_HASH_LOCK_READ	0
	#define ESQ_SESSION_HASH_LOCK_WRITE	1
	short state;
	short idx;
	esq_session_t * session;
	void * hash_lock;
	struct _esq_hash_lock_ctx_t * next;
	struct _esq_hash_lock_ctx_t * next_in_esq_session;
	struct _esq_hash_lock_ctx_t * next_in_local_wake_up_list;
	void * worker_ctx;	/* esq_worker_ctx_t */
} esq_hash_lock_ctx_t;

#define ESQ_SESSION_HASH_LOCK_OWNER_MAX	3

typedef struct _esq_session_t	{

	const char * name;
	const char * state;

	int magic0;

	/* session type */
	#define ESQ_SESSION_TYPE_NEW				0
	#define ESQ_SESSION_TYPE_SOCKET_CB 			1
	#define ESQ_SESSION_TYPE_MULTI_SOCKET_CB	2
	#define ESQ_SESSION_TYPE_MCO				3
	#define ESQ_SESSION_TYPE_NO_FD				4
	#define ESQ_SESSION_TYPE_WAIT_FOR_HASH_LOCK	5
	unsigned char session_type;

	/* post op bits */
	#define POP_NO_OP					0
	#define POP_FREE_SESSION			0x01
	#define POP_SWITCH_SESSION_TYPE		0x02
	#define POP_RETURN_VALUE			0x04
	#define POP_JOIN_GLOBAL_NEW_LIST	0x08
	#define POP_JOIN_LOCAL_NEW_LIST		0x10
	unsigned char post_op;

	short hash_lock_state;
	short hash_lock_idx;
	esq_hash_lock_ctx_t hash_lock_ctx[ESQ_SESSION_HASH_LOCK_OWNER_MAX];
	esq_hash_lock_ctx_t * hash_lock_ctx_list;

	unsigned int have_parent_session:1;

	unsigned short exp_event_bits;
	unsigned short event_bits;
	unsigned short event_bits2;
	int timeouts;
	int timeoutms;
	int fd;
	int lc;
	int wind_up_s;
	int wind_up_ms;
	int magic1;
	int r;
	esq_event_cb_t cb;
	void * user_param;
	void * mco;

	struct _esq_session_t * next;
	struct _esq_session_t * prev;

	struct _esq_session_t * parent_session;
	struct _esq_session_t * next_child;
	struct _esq_session_t * child_session_list;
	struct _esq_session_t * finished_child_session_list;
	esq_event_cb_t session_done_cb;
} esq_session_t;

extern void esq_update_polling_interval(void);
extern void esq_wake_up_worker_thread_rr(void);

#define SHOW_ACTION_TYPE_NAME	PRINTF("DBG: %s() %d: %p <%s> %s (%d)\n" \
		, __func__, __LINE__, session, session->name \
		, esq_get_action_type_name(action->type) \
		, action->type \
		)

#define INVALID_ACTION_TYPE	printf("ERROR: %s() %d: session %p <%s>, invalid action %s (%d)\n" \
		, __func__, __LINE__, session, session->name \
		, esq_get_action_type_name(action->type), action->type \
		); \
		esq_backtrace(); \
		exit(1);

#if defined PRINT_DBG_LOG
#define SHOW_SESSION_EVENT_BITS(__EVT__)	do	{ \
		char __event_bits0[64]; \
		char __event_bits1[64]; \
		PRINTF("DBG: %s() %d: %p <%s> add event bit 0x%x <%s> into 0x%x <%s>\n" \
				, __func__, __LINE__, session, session->name \
				, __EVT__ \
				, esq_get_event_bits_string(__EVT__, __event_bits0, sizeof(__event_bits0)) \
				, session->event_bits \
				, esq_get_event_bits_string(session->event_bits, __event_bits1, sizeof(__event_bits1)) \
				); \
		} while(0);
#else
#define SHOW_SESSION_EVENT_BITS(__EVT__)
#endif

extern void esq_pt_of_no_fd(esq_session_t *, esq_session_pt_action_t *);
extern void esq_pt_of_single_shot_socket(esq_session_t *, esq_session_pt_action_t *);
extern void esq_pt_of_multi_shot_socket(esq_session_t *, esq_session_pt_action_t *);
extern void esq_pt_of_mco(esq_session_t *, esq_session_pt_action_t *);
extern void esq_pt_of_hash_lock(esq_session_t * session, esq_session_pt_action_t * action);

extern int __esq_session_take_action(esq_session_t *, esq_session_pt_action_t *);
extern void esq_session_pick_up(void * session);
extern int esq_session_check_for_executing(void * session);

extern void esq_session_join_timing_list(esq_session_t * session);
extern void esq_session_left_timing_list(esq_session_t * session);
extern void esq_session_change_to_no_timing_list(esq_session_t * session);

extern int esq_try_fetch_hash_lock(esq_session_t * session);
extern int esq_hash_lock_release(esq_session_t * session, int hash_lock_idx);

extern esq_hash_lock_ctx_t * esq_alloc_hash_lock_ctx(esq_session_t * session);
extern void esq_free_hash_lock_ctx(esq_hash_lock_ctx_t * ctx);

/* session */
extern void esq_join_local_new_list(esq_session_t * session);
extern void esq_join_global_new_list(esq_session_t * session);
extern void __esq_notify_timeout_event(esq_session_t * session);
extern void __esq_notify_exit(esq_session_t * session);
extern void esq_session_join_active_list(esq_session_t * session);
extern void esq_session_left_active_list(esq_session_t * session);

#endif
