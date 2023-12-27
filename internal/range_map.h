#ifndef __LIB_RANGE_TREE_HEADER_INCLUDED__
#define __LIB_RANGE_TREE_HEADER_INCLUDED__

#include <arpa/inet.h>

/* #define HASH_VALUE_LENGTH	32 */
#define HASH_VALUE_LENGTH	(256 / 8)

#define RANGE_MAP_NAME_MAX	32

/* node flags */
#define RM_NODE_MAIN	(1)	/* this node handle requests first */
#define RM_NODE_DELETE	(2)	/* this node will be removed soon */
#define RM_NODE_ME		(4)	/* this node has local bucket */

extern const char * range_map_name_check(const char * name);
extern const char * range_map_name_check2(const char * name, int l);

/* hash */
typedef struct _hash_t	{
	/* this buffer contains non-printable binary data */
	unsigned char buf[HASH_VALUE_LENGTH];
} hash_t;

extern void get_hash_value(void * b, int length, hash_t * hash);
extern void dump_hash_value(hash_t * hash);

extern int range_map_dump_list_to_buffer(char * buf, int buflen, void * p);
extern int range_map_dump_to_buffer(const char * name, char * buf, int buflen);

/* using these two will cause long read lock between them */
/* no need to unlock if return NULL */
extern void * range_map_find_and_lock(const char * map_name, hash_t * hash);
extern void range_map_rlock();
extern void range_map_wlock();
extern void range_map_unlock();

extern void * range_map_node_get_next(void * node);
extern int range_map_node_get_flags(void * node);
extern void * range_map_node_get_bucket(void * node);

extern int range_map_get_addresses(const char * map_name, hash_t * hash, struct sockaddr_in * addressess, int max_addresses);

extern int range_map_get_main_address(const char * map_name, hash_t * hash, struct sockaddr_in * address);

extern void range_map_get_node_address(void * node, struct sockaddr_in * address);

extern void range_map_notify_bucket_online(void * bucket, const char * range_map_name, int range_map_name_len);
extern void range_map_notify_bucket_offline(void * bucket, const char * range_map_name, int range_map_name_len);
extern void esq_range_map_notify_self_port_setup(void);

extern int esq_range_map_init(void);
extern void esq_range_map_detach(void);

#endif
