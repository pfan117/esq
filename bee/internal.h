#ifndef __BEE_SERVER_INTERNAL_HEADER_INCLUDED__
#define __BEE_SERVER_INTERNAL_HEADER_INCLUDED__

#define BEE_SERVER_INSTANCE_SERVICE_MAX		16
#define BEE_SERVER_SERVER_INSTANCE_NAME_MAX	64
#define BEE_MONKEY_MCO_STACK_MAX	(64 * 1024)

#define BEE_SERVER_MODE_NODE	0
#define BEE_SERVER_MODE_AGENT	1

typedef struct _bee_server_instance_t	{
	void * esq_session;
	int port;
	int sd;
} bee_server_instance_t;

typedef struct _bee_service_ctx_t	{
	bee_server_instance_t * instance;
	header_from_pony_to_bee_t header;
	char * recv_buf;
	int payload_size;
	int offset;
	int opcode;
	int lc;
	int fd;
	/* when the reply_ptr is NULL, reply the data in this structure field */
	reply_from_bee_to_pony_t reply;
	reply_from_bee_to_pony_2_t * reply_ptr;
} bee_service_ctx_t;

/* instance */
extern int bee_server_instance_init(void);
extern void bee_server_instance_detach(void);

/* session */
extern void bee_create_new_session(bee_server_instance_t * parent_instance);
extern void bee_session_receive_request(void * current_session);
extern void bee_wait_for_service_complete(int fd, void * esq_session, void * event_param);

/* db */
extern void bee_db_set(int fd, bee_service_ctx_t *, int need_sync);
extern void bee_db_del(int fd, bee_service_ctx_t *, int need_sync);
extern void bee_db_get(int fd, bee_service_ctx_t *);

/* monkey */
extern void bee_monkey(int fd, header_from_pony_to_bee_t * header, char * recv_buf);

/* disk bucket */
extern int bee_bucket_init(void);
extern void bee_bucket_detach(void);
extern int bee_bucket_set(void * bucket, const char * key, int key_len, const char * value, int value_len);
extern int bee_bucket_get(void * bucket_ptr, const char * key, int key_len, char * buffer, int buf_len, int * result_length);
extern int bee_bucket_del(void * bucket_ptr, const char * key, int key_len, const char * value, int value_len);

/* bucket sync */
#define BEE_BUCKET_SYNC_RETRY_MAX	3
typedef struct _bucket_sync_target_t	{
	int socket_fd;
	struct sockaddr_in address;
	hash_t hash;
	int hash_length;
	struct _bucket_sync_target_t * same_hash_next;
	struct _bucket_sync_target_t * next_hash_list;
	const char * err;
	char hash_str[HASH_VALUE_LENGTH + 8];
	int hash_str_length;
} bucket_sync_target_t;

typedef struct _bucket_sync_session_t	{
	char range_map_name[RANGE_MAP_NAME_MAX + 8];
	int range_map_name_length;
	char hash_str[HASH_VALUE_LENGTH + 8];
	int hash_str_length;
	int dashboard;
	int target_cnt;
	void * kcdb;
	void * kccursor;
	const char * session_err;
	int processed;
	bucket_sync_target_t targets[];
} bucket_sync_session_t;

extern const char * bee_bucket_start_sync_thread(bucket_sync_session_t * sync_session);
extern const char * bee_bucket_start_sync_session(bucket_sync_session_t * sync_session);
extern int bee_bucket_sync_session_sprint(bucket_sync_session_t * sync_session);
extern void bee_bucket_end_sync_session(bucket_sync_session_t * sync_session);

typedef struct _bee_bucket_merge_session_t	{
	int fd;
	char range_map_name[RANGE_MAP_NAME_MAX + 8];
	int range_map_name_length;
	char * hash_str;
	int hash_str_length;
	char * key;
	int key_length;
	char * value;
	int value_length;
	char * buffer;
	void * bucket;
} bee_bucket_merge_session_t;

extern void bee_db_start_merge_sprint(int fd, header_from_pony_to_bee_t * header);
extern int esq_bucket_merge_item(bee_bucket_merge_session_t * merge_session);

extern int esq_mkc_get_session_param_by_name(
		void * params_des, char * name, int name_len
		, int * int_value
		, char ** buf, int * buf_len);

extern const char * bee_server_start_instance(int port);
extern void bee_create_target_cross_links(bucket_sync_session_t * sync_session);
extern int esq_bee_install_mkc_commands(void);

#endif
