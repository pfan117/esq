#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <monkeycall.h>
#include <openssl/sha.h>
#include <netinet/in.h>
#include "public/esq.h"
#define _ESQ_LIB_SOCKET_FUNCTIONS
#include "public/libesq.h"

#include "public/esqll.h"
#include "internal/tree.h"
#include "internal/range_map.h"
#ifdef MT_MODE
	#include "internal/monkeycall.h"
#endif
#include "bee/public.h"

typedef struct _range_node_t	{
	int hash_length;	/* the length of the part that supposed to be cared about */
	unsigned char * hash;
	unsigned char hash_buffer[HASH_VALUE_LENGTH + 8];

	unsigned long ip;
	int port;
	int flags;

	RB_ENTRY(_range_node_t) rb;
	struct _range_node_t * next;
	struct _range_node_t * all_node_list_next;
	struct sockaddr_in address;
	void * bucket;
} range_node_t;


#if HASH_VALUE_LENGTH != SHA256_DIGEST_LENGTH
	#error "HASH_VALUE_LENGTH != SHA256_DIGEST_LENGTH"
#endif

void
get_hash_value(void * b, int length, hash_t * hash)	{
	/* SHA256 will create binary format data in buffer (not printable string) */
    SHA256((const unsigned char *)b, length, (unsigned char *)hash->buf);
	return;
}

void
dump_hash_value(hash_t * hash)	{
	char buf[HASH_VALUE_LENGTH * 2 + 1];
	unsigned int c;
	unsigned int k;
	int i;
	int w;

	w = 0;

	for (i = 0; i < HASH_VALUE_LENGTH; i ++)	{
		k = hash->buf[i];

		c = (k & 0xf);
		if (c < 10)	{
			buf[w] = c + '0';
		}
		else	{
			buf[w] = c + 'a' - 10;
		}
		w ++;

		c = (k & 0xf0) >> 4;
		if (c < 10)	{
			buf[w] = c + '0';
		}
		else	{
			buf[w] = c + 'a' - 10;
		}
		w ++;

	}

	buf[w] = '\0';

	printf("--%s--\n", buf);

	return;
}

STATIC int
range_node_compare(range_node_t * a, range_node_t * b)	{
	int al;
	int bl;

	al = a->hash_length;
	bl = b->hash_length;

	if (al != bl)	{
		esq_design_error();
		return al - bl;
	}

	return memcmp(a->hash, b->hash, al);
}

RB_HEAD(RANGE_NODE, _range_node_t);
RB_PROTOTYPE_STATIC(RANGE_NODE, _range_node_t, rb, range_node_compare);
RB_GENERATE_STATIC(RANGE_NODE, _range_node_t, rb, range_node_compare);

typedef struct _range_map_t	{
	char name[RANGE_MAP_NAME_MAX];
	struct RANGE_NODE rb_by_length[HASH_VALUE_LENGTH];
	range_node_t * all_list;
	int longest;
} range_map_t;

#define RANGE_MAP_MAX		10
static range_map_t range_map_list[RANGE_MAP_MAX];
static pthread_rwlock_t range_map_rwlock;

#define RM_RLOCK	pthread_rwlock_rdlock(&range_map_rwlock)
#define RM_WLOCK	pthread_rwlock_wrlock(&range_map_rwlock)
#define RM_UNLOCK	pthread_rwlock_unlock(&range_map_rwlock)

STATIC void
range_map_clean(range_map_t * m)	{
	range_node_t * pp;
	range_node_t * p;
	range_node_t * n;
	int i;

	for (i = 0; i < sizeof(m->rb_by_length) / sizeof(m->rb_by_length[0]); i ++)	{
		RB_INIT(m->rb_by_length + i);
	}

	pp = NULL;
	for (p = m->all_list; p; p = n)	{

		n = p->all_node_list_next;

		if ((RM_NODE_DELETE & p->flags))	{
			if (pp)	{
				pp->all_node_list_next = n;
			}
			else	{
				m->all_list = n;
			}
			free(p);
		}
		else	{
			p->next = NULL;
			pp = p;
		}
	}

	return;
}

STATIC void
range_map_load_node(range_map_t * m, range_node_t * p)	{
	range_node_t * s;
	int length;

	length = p->hash_length;

	s = RANGE_NODE_RB_FIND(m->rb_by_length + length, p);
	if (s)	{
		while (s->next)	{
			s = s->next;
		}
		s->next = p;
	}
	else	{
		RANGE_NODE_RB_INSERT(m->rb_by_length + length, p);
	}
}

STATIC void
range_map_link_short_match(range_map_t * m, int length, range_node_t * n)	{
	range_node_t * p;
	range_node_t * k;
	range_node_t f;
	int l;

	if (length < 0)	{
		return;
	}

	f.hash = n->hash;

	for(l = length; l >= 0; l --)	{
		f.hash_length = l;

		p = RANGE_NODE_RB_FIND(m->rb_by_length + l, &f);
		if (p)	{
			for (k = n; k->next; k = k->next)	{
				;
			}
			k->next = p;
			break;
		}
	}
}

STATIC void
range_map_build_cross_depth_reference(range_map_t * m)	{
	int max;
	int length;
	range_node_t * n;
	int longest = 0;

	max = sizeof(m->rb_by_length) / sizeof(m->rb_by_length[0]);

	for (length = max - 1; length >= 0; length --)	{
		RB_FOREACH(n, RANGE_NODE, m->rb_by_length + length)	{
			if (!longest)	{
				longest = length;
			}
			range_map_link_short_match(m, length - 1, n);
		}
	}

	m->longest = longest;

	return;
}

STATIC void
range_map_load(range_map_t * m)	{
	range_node_t * p;

	for (p = m->all_list; p; p = p->all_node_list_next)	{
		p->next = NULL;
		range_map_load_node(m, p);
	}

	range_map_build_cross_depth_reference(m);

	return;
}

STATIC range_map_t *
range_map_find_by_name(const char * name)	{
	int i;
	int total;

	total = sizeof(range_map_list) / sizeof(range_map_list[0]);

	for (i = 0; i < total; i ++)	{
		if (strcmp(range_map_list[i].name, name))	{
			continue;
		}
		else	{
			return range_map_list + i;
		}
	}

	return NULL;
}

STATIC range_map_t *
range_map_find_by_name2(const char * name, int name_len)	{
	int i;
	int total;

	total = sizeof(range_map_list) / sizeof(range_map_list[0]);

	for (i = 0; i < total; i ++)	{
		if (memcmp(range_map_list[i].name, name, name_len))	{
			continue;
		}

		if (range_map_list[i].name[name_len])	{
			continue;
		}

		return range_map_list + i;
	}

	return NULL;
}

const char *
range_map_name_check2(const char * name, int l)	{
	int i;

	if (l <= 0)	{
		return "range map name invalid";
	}

	if (l >= RANGE_MAP_NAME_MAX)	{
		return "range map name too long";
	}

	for (i = 0; i < l; i ++)	{
		if (name[i] >= 'a' && name[i] <= 'z')	{
			continue;
		}
		if (name[i] >= '0' && name[i] <= '9')	{
			continue;
		}
		return "only number and lower case allowed";
	}

	return NULL;
}

const char *
range_map_name_check(const char * name)	{
	int l;
	l = strnlen(name, RANGE_MAP_NAME_MAX + 2);
	return range_map_name_check2(name, l);
}

const char *
range_map_new(const char * name)	{
	const char * errs;
	int i;

	errs = range_map_name_check(name);
	if (errs)	{
		return errs;
	}

	RM_WLOCK;

	if (range_map_find_by_name(name))	{
		RM_UNLOCK;
		return "range map already exist";
	}

	for (i = 0; i < (sizeof(range_map_list) / sizeof(range_map_list[0])); i ++)	{
		if (range_map_list[i].name[0])	{
			continue;
		}
		else	{
			goto __load;
		}
	}

	RM_UNLOCK;

	return "too many range maps";

__load:

	strcpy(range_map_list[i].name, name);

	RM_UNLOCK;

	return NULL;
}

void
range_map_notify_bucket_online(void * bucket, const char * range_map_name, int range_map_name_len)	{
	const unsigned char * hash_str;
	int hash_str_length;
	range_map_t * range_map;
	range_node_t * p;
	unsigned long ip;
	int port;

	/* range map lock has been locked by caller */
	range_map = range_map_find_by_name2(range_map_name, range_map_name_len);
	if (!range_map)	{
		return;
	}

	bee_bucket_get_ip_port(bucket, &ip, &port);
	bee_bucket_get_hash_str(bucket, &hash_str, &hash_str_length);

	if (ip || port)	{
		/* remote patch bucket */
		for (p = range_map->all_list; p; p = p->all_node_list_next)	{
			if (p->flags & RM_NODE_ME)	{
				continue;
			}

			if (p->ip != ip)	{
				continue;
			}

			if (p->port != port)	{
				continue;
			}

			if (p->hash_length != hash_str_length)	{
				continue;
			}

			if (memcmp(p->hash_buffer, (char *)hash_str, hash_str_length))	{
				continue;
			}

			p->bucket = bucket;
		}
	}
	else	{
		/* self bucket */
		for (p = range_map->all_list; p; p = p->all_node_list_next)	{

			if (p->flags & RM_NODE_ME)	{
				;
			}
			else	{
				continue;
			}

			if (p->hash_length != hash_str_length)	{
				continue;
			}

			if (memcmp(p->hash_buffer, (char *)hash_str, hash_str_length))	{
				continue;
			}

			p->bucket = bucket;
		}
	}

	return;
}

void
range_map_notify_bucket_offline(void * bucket, const char * range_map_name, int range_map_name_len)	{
	range_map_t * range_map;
	range_node_t * p;

	/* range map lock has been locked by caller */
	range_map = range_map_find_by_name2(range_map_name, range_map_name_len);
	if (!range_map)	{
		return;
	}

	for (p = range_map->all_list; p; p = p->all_node_list_next)	{
		if (p->bucket == bucket)	{
			p->bucket = NULL;
		}
	}

	return;
}

STATIC void
esq_range_map_flush_range_node_bucket_connection(void)	{
	range_node_t * p;
	int map_name_len;
	char * map_name;
	int i;

	for (i = 0; i < ESQ_ARRAY_SIZE(range_map_list); i ++)	{
		if (!range_map_list[i].name[0])	{
			continue;
		}

		for (p = range_map_list[i].all_list; p; p = p->all_node_list_next)	{

			map_name = range_map_list[i].name;
			map_name_len = strlen(map_name);

			if (os_ip_address_is_me(&(p->address)) && bee_server_is_my_port(p->port))	{
				p->flags |= RM_NODE_ME;
				p->bucket = esq_bee_find_bucket(map_name, map_name_len, p->hash, p->hash_length, 0, 0);
			}
			else	{
				p->flags &= (~RM_NODE_ME);
				p->bucket = esq_bee_find_bucket(map_name, map_name_len, p->hash, p->hash_length, p->ip, p->port);
			}
		}
	}

	return;
}

void
esq_range_map_notify_self_port_setup(void)	{
	RM_WLOCK;
	esq_range_map_flush_range_node_bucket_connection();
	RM_UNLOCK;
	return;
}

STATIC range_node_t *
range_map_find_node(range_map_t * range_map, unsigned long ip, int port, unsigned char * hash_buffer, int hash_len)
{
	range_node_t * p;

	for (p = range_map->all_list; p; p = p->all_node_list_next)	{
		if (p->ip != ip)	{
			continue;
		}
		if (p->port != port)	{
			continue;
		}
		if (p->hash_length != hash_len)	{
			continue;
		}
		if (memcmp(p->hash_buffer, hash_buffer, hash_len))	{
			continue;
		}

		return p;
	}

	return NULL;
}

#define TRANSLATE_MKC_PARAMS	do	{\
	l = strnlen(ip_str, 20);\
	if (l < 7 || l > 15)	{\
		return "invalid IP address length";\
	}\
\
	r = get_ipv4_addr(ip_str, l, &ip);\
	if (!r)	{\
		return "invalid IP address";\
	}\
\
	l = strnlen(hash_string, HASH_VALUE_LENGTH + 3);\
	if (l < 1 || l > HASH_VALUE_LENGTH + 1)	{\
		return "invalid hash string pameter length";\
	}\
\
	r = get_hash_string(hash_string, l, &hash_start, &hash_str_len);\
	if (-1 == r)	{\
		return "invalid hash string";\
	}\
\
	if ((0 != hash_start) || (hash_str_len + 1 != l) || (hash_str_len > HASH_VALUE_LENGTH * 2))	{\
		return "invalid hash string length";\
	}\
\
	hash_len = (hash_str_len >> 1);\
	if (hash_str_2_bin(hash_buffer, hash_string + hash_start, hash_str_len))	{\
		return "failed to transform hash string into binary";\
	}\
\
} while(0);

const char *
range_map_add_node(
		const char * map_name
		, const char * ip_str, int port, int flags
		, const char * hash_string)
{
	unsigned char hash_buffer[HASH_VALUE_LENGTH];
	range_map_t * range_map;
	void * bucket;
	unsigned long ip;
	range_node_t * n;
	range_node_t * p;
	int r;
	int l;
	int hash_start;
	int hash_str_len;
	int hash_len;

	TRANSLATE_MKC_PARAMS

	RM_WLOCK;

	range_map = range_map_find_by_name(map_name);
	if (!range_map)	{
		RM_UNLOCK;
		return "range map not exist";
	}

	p = range_map_find_node(range_map, ip, port, hash_buffer, hash_len);
	if (p)	{
		RM_UNLOCK;
		return "range map node already exist";
	}

	n = malloc(sizeof(range_node_t));
	if (!n)	{
		RM_UNLOCK;
		return "out of memory";
	}

	bzero(n, sizeof(range_node_t));

	n->ip = ip;
	n->port = port;
	n->hash_length = hash_len;
	memcpy(n->hash_buffer, hash_buffer, hash_len);
	n->flags = flags;
	n->hash = n->hash_buffer;
	n->address.sin_family = AF_INET;
	n->address.sin_port = htons(port);
	n->address.sin_addr.s_addr = inet_addr(ip_str);
	/* bzero(&(n->address.sin_zero), sizeof(n->address.sin_zero)); */
	if (os_ip_address_is_me(&(n->address)) && bee_server_is_my_port(port))	{
		n->flags |= RM_NODE_ME;
		bucket = esq_bee_find_bucket(map_name, strlen(map_name), (const unsigned char *)hash_string, hash_str_len + 1, 0, 0);
	}
	else	{
		bucket = esq_bee_find_bucket(map_name, strlen(map_name), (const unsigned char *)hash_string, hash_str_len + 1, ip, port);
	}

	n->bucket = bucket;
	n->all_node_list_next = range_map->all_list;
	range_map->all_list = n;

	RM_UNLOCK;

	return NULL;
}

const char *
range_map_del_node(const char * map_name, const char * ip_str, int port, const char * hash_string)	{
	unsigned char hash_buffer[HASH_VALUE_LENGTH];
	range_map_t * range_map;
	unsigned long ip;
	range_node_t * p;
	int r;
	int l;
	int hash_start;
	int hash_str_len;
	int hash_len;

	TRANSLATE_MKC_PARAMS

	RM_WLOCK;

	range_map = range_map_find_by_name(map_name);
	if (!range_map)	{
		RM_UNLOCK;
		return "range map not exist";
	}

	p = range_map_find_node(range_map, ip, port, hash_buffer, hash_len);
	if (p)	{
		p->flags |= RM_NODE_DELETE;
	}
	else	{
		RM_UNLOCK;
		return "range node not exist";
	}

	range_map_clean(range_map);
	range_map_load(range_map);

	RM_UNLOCK;

	return NULL;
}

const char * range_map_del_all_node(const char * map_name)	{
	range_node_t * p;
	range_map_t * m;

	RM_WLOCK;

	m = range_map_find_by_name(map_name);
	if (!m)	{
		RM_UNLOCK;
		return "range map not exist";
	}

	for (p = m->all_list; p; p = p->all_node_list_next)	{
		p->flags |= RM_NODE_DELETE;
	}

	range_map_clean(m);
	range_map_load(m);

	RM_UNLOCK;

	return NULL;
}

const char *
range_map_flush(const char * name)	{
	range_map_t * m;

	RM_WLOCK;
	
	m = range_map_find_by_name(name);
	if (!m)	{
		RM_UNLOCK;
		return "range map not found";
	}

	range_map_clean(m);
	range_map_load(m);

	RM_UNLOCK;

	return NULL;
}

void *
range_map_find_and_lock(const char * map_name, hash_t * hash)	{
	range_node_t * p;
	range_node_t f;
	range_map_t * m;
	int l;

	f.hash = hash->buf;

	RM_RLOCK;

	m = range_map_find_by_name(map_name);
	if (!m)	{
		RM_UNLOCK;
		return NULL;
	}

	for (l = m->longest; l >= 0; l --)	{
		f.hash_length = l;
		p = RANGE_NODE_RB_FIND(m->rb_by_length + l, &f);
		if (p)	{
			return p;
		}
	}

	RM_UNLOCK;

	return NULL;
}

void
range_map_rlock()	{
	RM_RLOCK;
}

void
range_map_wlock()	{
	RM_WLOCK;
}

/* return addresses count */
void
range_map_unlock()	{
	RM_UNLOCK;
}

/* return addresses count */
/* return -1 for error */
int
range_map_get_addresses(const char * map_name
		, hash_t * hash
		, struct sockaddr_in * addresses, int max_addresses)
{
	range_node_t * n;
	int i;

	n = range_map_find_and_lock(map_name, hash);
	if (!n)	{
		return 0;
	}

	i = 0;

	while(n)	{

		if ((RM_NODE_DELETE & n->flags))	{
			n = n->next;
		}

		if (i >= max_addresses)	{
			range_map_unlock();
			return -1;
		}
		memcpy(addresses + i, &(n->address), sizeof(n->address));
		i ++;
		n = n->next;
	}

	range_map_unlock();

	return i;
}

/* return 0 for success */
/* return -1 for error */
int
range_map_get_main_address(const char * map_name
		, hash_t * hash
		, struct sockaddr_in * address)
{
	range_node_t * n;

	n = range_map_find_and_lock(map_name, hash);
	if (!n)	{
		return 0;
	}

	while(n)	{
		if ((n->flags & RM_NODE_MAIN))	{
			memcpy(address, &(n->address), sizeof(n->address));
			range_map_unlock();
			return 0;
		}
		else	{
			n = n->next;
			continue;
		}
	}

	range_map_unlock();

	return -1;
}

void
range_map_get_node_address(void * node, struct sockaddr_in * address)	{
	range_node_t * n = node;
	memcpy(address, &(n->address), sizeof(n->address));
	return;
}

STATIC int
range_map_dump_node_to_buffer(char * buf, int buflen, range_node_t * p)	{
	int offset;
	int hashi;
	int r;

	offset = 0;

	r = snprintf(buf + offset, buflen - offset, "%lu.%lu.%lu.%lu:%d:"
			, ((p->ip & 0xff000000) >> 24)
			, ((p->ip & 0xff0000) >> 16)
			, ((p->ip & 0xff00) >> 8)
			, ((p->ip & 0xff))
			, p->port
			);
	if (r <= 0 || r >= buflen - offset)	{
		return r;
	}
	else	{
		offset += r;
	}

	for (hashi = 0; hashi < p->hash_length; hashi ++)	{
		r = snprintf(buf + offset, buflen - offset, "%02x", p->hash_buffer[hashi]);
		if (r <= 0 || r >= buflen - offset)	{
			return r;
		}
		else	{
			offset += r;
		}
	}

	if (buflen - offset > 3)	{
		buf[offset] = 'x';
		buf[offset + 1] = '\0';
		offset ++;
	}
	else	{
		return -1;
	}

	if (p->bucket)	{
		const char * s;

		s = bee_bucket_get_db_ptr_state(p->bucket);

		r = snprintf(buf + offset, buflen - offset, ":%s", s);
		if (r <= 0 || r >= buflen - offset)	{
			return -1;
		}

		offset += r;
	}

	return offset;
}

const char *
range_map_dump_node(
		const char * map_name
		, const char * ip_str, int port
		, const char * hash_string
		, char * buf, int * buflen
		)
{
	unsigned char hash_buffer[HASH_VALUE_LENGTH];
	range_map_t * range_map;
	unsigned long ip;
	range_node_t * p;
	int r;
	int l;
	int hash_start;
	int hash_str_len;
	int hash_len;

	TRANSLATE_MKC_PARAMS

	RM_RLOCK;

	range_map = range_map_find_by_name(map_name);
	if (!range_map)	{
		RM_UNLOCK;
		return "range map not exist";
	}

	p = range_map_find_node(range_map, ip, port, hash_buffer, hash_len);
	if (!p)	{
		RM_UNLOCK;
		return "range map node not exist";
	}

	r = range_map_dump_node_to_buffer(buf, *buflen, p);

	RM_UNLOCK;

	if (r <= 0)	{
		return "node info buffer too small";
	}

	*buflen = r;

	return NULL;
}

int
range_map_dump_list_to_buffer(char * buf, int buflen, void * node_ptr)	{
	range_node_t * p;
	int offset;
	int r;

	p = node_ptr;
	offset = 0;

	r = range_map_dump_node_to_buffer(buf + offset, buflen - offset, p);
	if (r <= 0 || r >= buflen - offset)	{
		return -1;
	}
	else	{
		offset += r;
	}

	for (p = p->next; p; p = p->next)	{

		if (buflen - offset > 3)	{	/* keep away from the edge */
			buf[offset] = ' ';
			offset ++;
		}
		else	{
			return -1;
		}

		r = range_map_dump_node_to_buffer(buf + offset, buflen - offset, p);
		if (r <= 0 || r >= buflen - offset)	{
			return -1;
		}
		else	{
			offset += r;
		}
	}

	return offset;
}

void *
range_map_node_get_next(void * node)	{
	range_node_t * p = node;
	return p->next;
}

int
range_map_node_get_flags(void * node)	{
	range_node_t * p = node;
	return p->flags;
}

void *
range_map_node_get_bucket(void * node)	{
	range_node_t * p = node;
	return p->bucket;
}

/* return length for success */
/* return <= 0 for error */
int
range_map_dump_to_buffer(const char * name, char * buf, int buflen)	{
	range_node_t * p;
	range_map_t * m;
	int first_list;
	int offset;
	int l;
	int r;

	RM_RLOCK;

	buf[0] = '\0';

	m = range_map_find_by_name(name);
	if (!m)	{
		RM_UNLOCK;
		r = snprintf(buf, buflen, "range not found\n");
		return r;
	}

	offset = 0;
	first_list = 1;

	for (l = 0; l < sizeof(m->rb_by_length) / sizeof(m->rb_by_length[0]); l ++)	{

		RB_FOREACH(p, RANGE_NODE, m->rb_by_length + l)	{
			if (first_list)	{
				first_list = 0;
				r = snprintf(buf + offset, buflen - offset, "%d: ", l);
			}
			else	{
				r = snprintf(buf + offset, buflen - offset, ", %d: ", l);
			}

			if (r <= 0 || r >= buflen - offset)	{
				goto __exx;
			}
			else	{
				offset += r;
			}

			r = range_map_dump_list_to_buffer(buf + offset, buflen - offset, p);
			if (r >= 0)	{
				offset += r;
			}
			else	{
				goto __exx;
			}
		}

	}

	RM_UNLOCK;

	return offset;

__exx:

	RM_UNLOCK;

	return -1;
}

STATIC void
range_map_free(range_map_t * m)	{
	range_node_t * p;
	range_node_t * n;

	for (p = m->all_list; p; p = n)	{
		n = p->all_node_list_next;
		free(p);
	}

	return;
}

int
range_map_init(void)	{
	int r;

	r = pthread_rwlock_init(&(range_map_rwlock), NULL);
	if (r)	{
		fprintf(stderr, "error: %s, %d: failed to initial rwlock\n", __func__, __LINE__);
		return -1;
	}

	bzero(range_map_list, sizeof(range_map_list));

	return 0;
}

STATIC void
range_map_destroy(void)	{
	int i;

	pthread_rwlock_destroy(&(range_map_rwlock));

	for (i = 0; i < (sizeof(range_map_list) / sizeof(range_map_list[0])); i ++)	{
		if (range_map_list[i].name[0])	{
			range_map_free(range_map_list + i);
			range_map_list[i].name[0] = '\0';
		}
	}

	return;
}

#define IF_ERROR_RETURN(__OP__)	do	{	\
	r = __OP__();	\
	if (r)	{	\
		return r;	\
	}	\
} while(0);

STATIC int
range_map_mkccb_create(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * map_name;
	const char * errs;
	mkc_data * result;

	MKC_CB_ARGC_CHECK(1);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_BUF);
	result = MKC_CB_RESULT;
	map_name = MKC_CB_ARGV(0);
	result->type = MKC_D_VOID;

	errs = range_map_new(map_name->buffer);

	if (errs)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, errs);
		return -1;
	}
	else	{
		return 0;
	}
}

STATIC int
range_map_mkccb_add_node(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * map_name;
	mkc_data * ip;
	mkc_data * port;
	mkc_data * flags;
	mkc_data * hash;
	mkc_data * result;
	const char * errs;

	/* AppendRangeMap("kcdb", "127.0.0.1", 20000, 0, "*"); */
	/* need flush for new node be presented */
	MKC_CB_ARGC_CHECK(5);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(1, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(2, MKC_D_INT);
	MKC_CB_ARG_TYPE_CHECK(3, MKC_D_INT);
	MKC_CB_ARG_TYPE_CHECK(4, MKC_D_BUF);

	result = MKC_CB_RESULT;
	result->type = MKC_D_VOID;

	map_name = MKC_CB_ARGV(0);
	ip = MKC_CB_ARGV(1);
	port = MKC_CB_ARGV(2);
	flags = MKC_CB_ARGV(3);
	hash = MKC_CB_ARGV(4);

	errs = range_map_add_node(
			map_name->buffer
			, ip->buffer
			, port->integer
			, flags->integer
			, hash->buffer
			);

	if (errs)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, errs);
		return -1;
	}
	else	{
		return 0;
	}
}

#define RANGE_MAP_INFO_BUF_MAX	1024 * 1024
STATIC int
range_map_mkccb_dump(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * map_name;
	mkc_data * result;
	int buflen;
	char * buf;

	MKC_CB_ARGC_CHECK(1);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_BUF);

	result = MKC_CB_RESULT;
	result->type = MKC_D_VOID;

	buflen = RANGE_MAP_INFO_BUF_MAX;
	buf = mkc_session_malloc(session, buflen);
	if (!buf)	{
		return -1;
	}

	map_name = MKC_CB_ARGV(0);

	buflen = range_map_dump_to_buffer(map_name->buffer, buf, buflen);

	if (buflen > 0)	{
		result->type = MKC_D_BUF;
		result->buffer = buf;
		result->length = buflen;
	}
	else	{
		result->type = MKC_D_VOID;
		mkc_session_free(session, buf);
		return -1;
	}

	return 0;
}

#define NODE_INFO_BUF_MAX	1024
STATIC int
range_map_mkccb_dump_node(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * map_name;
	mkc_data * ip;
	mkc_data * port;
	mkc_data * hash;
	mkc_data * result;
	const char * errs;
	int buflen;
	char * buf;

	MKC_CB_ARGC_CHECK(4);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(1, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(2, MKC_D_INT);
	MKC_CB_ARG_TYPE_CHECK(3, MKC_D_BUF);

	result = MKC_CB_RESULT;
	result->type = MKC_D_VOID;

	buflen = NODE_INFO_BUF_MAX;
	buf = mkc_session_malloc(session, NODE_INFO_BUF_MAX);
	if (!buf)	{
		return -1;
	}

	map_name = MKC_CB_ARGV(0);
	ip = MKC_CB_ARGV(1);
	port = MKC_CB_ARGV(2);
	hash = MKC_CB_ARGV(3);

	errs = range_map_dump_node(
			map_name->buffer
			, ip->buffer
			, port->integer
			, hash->buffer
			, buf, &buflen
			);

	if (errs)	{
		mkc_session_free(session, buf);
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, errs);
		return -1;
	}

	result->type = MKC_D_BUF;
	result->buffer = buf;
	result->length = buflen;

	return 0;
}

STATIC int
range_map_mkccb_del_node(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * map_name;
	mkc_data * ip;
	mkc_data * port;
	mkc_data * hash_string;
	mkc_data * result;
	const char * errs;

	/* AppendRangeMap("kcdb", "127.0.0.1", 20000, 0, "*"); */
	MKC_CB_ARGC_CHECK(4);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(1, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(2, MKC_D_INT);
	MKC_CB_ARG_TYPE_CHECK(3, MKC_D_BUF);

	result = MKC_CB_RESULT;
	result->type = MKC_D_VOID;

	map_name = MKC_CB_ARGV(0);
	ip = MKC_CB_ARGV(1);
	port = MKC_CB_ARGV(2);
	hash_string = MKC_CB_ARGV(3);

	errs = range_map_del_node(
			map_name->buffer
			, ip->buffer
			, port->integer
			, hash_string->buffer
			);

	if (errs)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, errs);
		return -1;
	}
	else	{
		return 0;
	}
}

STATIC int
range_map_mkccb_del_all_node(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * map_name;
	mkc_data * result;
	const char * errs;

	MKC_CB_ARGC_CHECK(1);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_BUF);

	result = MKC_CB_RESULT;
	result->type = MKC_D_VOID;

	map_name = MKC_CB_ARGV(0);

	errs = range_map_del_all_node(map_name->buffer);

	if (errs)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, errs);
		return -1;
	}
	else	{
		return 0;
	}
}

STATIC int
range_map_mkccb_flush(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * map_name;
	mkc_data * result;
	const char * errs;

	MKC_CB_ARGC_CHECK(1);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_BUF);
	result = MKC_CB_RESULT;
	map_name = MKC_CB_ARGV(0);
	result->type = MKC_D_VOID;

	errs = range_map_flush(map_name->buffer);
	if (errs)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, errs);
		return -1;
	}
	else	{
		return 0;
	}

	return -1;
}

int
esq_range_map_init(void)	{
	void * server;

	ESQ_IF_ERROR_RETURN(range_map_init());

	server = esq_get_mkc();

	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(server, "RangeMapNew", range_map_mkccb_create));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(server, "RangeMapAddNode", range_map_mkccb_add_node));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(server, "RangeMapDump", range_map_mkccb_dump));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(server, "RangeMapDumpNode", range_map_mkccb_dump_node));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(server, "RangeMapDelNode", range_map_mkccb_del_node));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(server, "RangeMapDelAllNode", range_map_mkccb_del_all_node));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(server, "RangeMapFlush", range_map_mkccb_flush));

	return ESQ_OK;
}

void
esq_range_map_detach(void)	{
	range_map_destroy();
	return;
}

/* eof */
