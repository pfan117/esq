#ifndef __ESQ_BEE_HEADER_FILE_INCLUDED__
#define __ESQ_BEE_HEADER_FILE_INCLUDED__

/* message headers */
typedef struct _header_from_pony_to_bee_t	{
	int opcode;
	int range_map_name_len;
	int key_len;
	int data_len;
	int result_buf_size;
} header_from_pony_to_bee_t;

typedef struct _reply_from_bee_to_pony_t	{
	int return_code;
	int result_len;
} reply_from_bee_to_pony_t;

typedef struct _monkey_reply_from_bee_to_pony_t	{
	int return_code;
	int result_len;
	int type;
} monkey_reply_from_bee_to_pony_t;

typedef struct _reply_from_bee_to_pony_2_t	{
	int return_code;
	int result_len;
	char result[];
} reply_from_bee_to_pony_2_t;

/* interface */
extern int esq_bee_init(void);
extern void esq_bee_detach(void);
extern int bee_server_is_my_port(int port);

extern void bee_bucket_get_ip_port(void * bucket_ptr, unsigned long * ip, int * port);
extern void bee_bucket_get_hash_str(void * bucket_ptr, const unsigned char ** hash_str, int * hash_str_length);
extern const char * bee_bucket_get_db_ptr_state(void * bucket_ptr);

/* should be called between range map lock and unlock */
extern void * esq_bee_find_bucket(const char * range_map, int range_map_len, const unsigned char * hash, int hash_len, unsigned long ip, int port);

#endif
