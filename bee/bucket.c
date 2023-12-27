#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <monkeycall.h>
#include <kclangc.h>

#include <netinet/in.h>
#include <dirent.h>
#include <sys/types.h>	/* stat(), lstat */
#include <sys/stat.h>
#include <sys/time.h>

#include <fcntl.h>	/* open() */
#include "public/esq.h"
#define _ESQ_LIB_SOCKET_FUNCTIONS
#include "public/libesq.h"
#include "public/pony-bee.h"
#include "public/esqll.h"
#include "internal/types.h"
#include "internal/tree.h"
#include "internal/range_map.h"

#include "bee/public.h"
#include "bee/internal.h"

typedef struct _bee_bucket_t	{
	struct _bee_bucket_t * next;
	const char * range_map_name;
	int range_map_name_length;
	const unsigned char * hash_str;
	int hash_str_length;
	char * path;
	int search_id;
	int flags;
	unsigned long ip;			/* keep 0 for local bucket */
	int port;					/* keep 0 for local bucket */
	RB_ENTRY(_bee_bucket_t) rb;
	KCDB * kcdb[2];
	unsigned char buf[];		/* pathname */
} bee_bucket_t;

STATIC int esq_bee_bucket_cmp(bee_bucket_t * a, bee_bucket_t * b)	{
	int r;

	if (a->search_id != b->search_id)	{
		return a->search_id - b->search_id;
	}

	if (a->hash_str_length != b->hash_str_length)	{
		return a->hash_str_length - b->hash_str_length;
	}

	if (a->range_map_name_length != b->range_map_name_length)	{
		return a->range_map_name_length - b->range_map_name_length;
	}

	if (a->ip != b->ip)	{
		return (int)a->ip - (int)b->ip;
	}

	if (a->port != b->port)	{
		return (int)a->port - (int)b->port;
	}

	r = memcmp(a->hash_str, b->hash_str, a->hash_str_length);
	if (r)	{
		return r;
	}

	r = memcmp(a->range_map_name, b->range_map_name, a->range_map_name_length);
	if (r)	{
		return r;
	}

	return 0;
}

RB_HEAD(ESQ_DISK_BUCKET, _bee_bucket_t);
RB_PROTOTYPE_STATIC(ESQ_DISK_BUCKET, _bee_bucket_t, rb, esq_bee_bucket_cmp);
RB_GENERATE_STATIC(ESQ_DISK_BUCKET, _bee_bucket_t, rb, esq_bee_bucket_cmp);

static struct ESQ_DISK_BUCKET esq_bucket_rb;

STATIC void
bee_close_kcdb(KCDB * kcdb)	{
	kcdbclose(kcdb);
	kcdbdel(kcdb);
}

STATIC KCDB *
bee_open_kcdb(const char * filename, int create_new)	{
	KCDB * kcdb;
	int r;

	kcdb = kcdbnew();
	if (!kcdb)	{
		printf("ERROR: %s, %d: %s\n"
				, __func__, __LINE__
				, "no memory for new db object"
				);
		return NULL;
	}

	if (create_new)	{
		r = kcdbopen(kcdb, filename, KCOWRITER | KCOREADER | KCOCREATE);
	}
	else	{
		r = kcdbopen(kcdb, filename, KCOWRITER | KCOREADER);
	}
    if (!r) {
		kcdbdel(kcdb);
		printf("ERROR: %s, %d: %s\n"
				, __func__, __LINE__
        		, "failed to open db file by kyoto cabinet library"
				);
		return NULL;
    }

	return kcdb;
}

STATIC void
bee_close_all_buckets(void)	{
	bee_bucket_t * p;

	range_map_wlock();

	for (p = RB_MIN(ESQ_DISK_BUCKET, &esq_bucket_rb)
			; p
			; p = RB_MIN(ESQ_DISK_BUCKET, &esq_bucket_rb))
	{

		if (p->kcdb[0])	{
			bee_close_kcdb(p->kcdb[0]);
		}

		if (p->kcdb[1])	{
			bee_close_kcdb(p->kcdb[1]);
		}

		ESQ_DISK_BUCKET_RB_REMOVE(&esq_bucket_rb, p);

		free(p);
	}

	range_map_unlock();
	return;
}

STATIC void
bee_bucket_calculate_search_id(bee_bucket_t * p)	{
	int rns;
	int hashs;
	int i;

	rns = 0;
	hashs = 0;

	/*
	const char * range_map_name;
	const char * hash_str;
	int range_map_name_length;
	int hash_str_length;
	*/

	for (i = 0; i < p->range_map_name_length; i ++)	{
		if ((i & 1))	{
			rns += (p->range_map_name[i]);
		}
		else	{
			rns += (p->range_map_name[i] << 8);
		}
	}

	for (i = 0; i < p->hash_str_length; i ++)	{
		if ((i & 1))	{
			hashs += (p->hash_str[i]);
		}
		else	{
			hashs += (p->hash_str[i] << 8);
		}
	}

	p->search_id = rns + (hashs << 16);

	return;
}

void *
esq_bee_find_bucket(const char * range_map, int range_map_len, const unsigned char * hash, int hash_len, unsigned long ip, int port)
{
	bee_bucket_t * p;
	bee_bucket_t n;

	n.range_map_name = range_map;
	n.range_map_name_length = range_map_len;
	n.hash_str = hash;
	n.hash_str_length = hash_len;
	n.ip = ip;
	n.port = port;
	bee_bucket_calculate_search_id(&n);

	p = ESQ_DISK_BUCKET_RB_FIND(&esq_bucket_rb, &n);

	return p;
}

STATIC const char *
bee_open_disk_bucket(
		bee_bucket_t * bucket
		, int idx
		, int create_new
		)
{
	bee_bucket_t * p;
	KCDB * kcdb;

	range_map_wlock();

	p = ESQ_DISK_BUCKET_RB_FIND(&esq_bucket_rb, bucket);
	if (p && p->kcdb[idx])	{
		range_map_unlock();
		free(bucket);
        return "bucket for the same map, same range, same index already open";
	}

	kcdb = bee_open_kcdb((char *)bucket->buf, create_new);
	if (!kcdb)	{
		range_map_unlock();
		free(bucket);
		return "failed to open disk db";
	}

	if (p)	{
		p->kcdb[idx] = kcdb;
		free(bucket);
		range_map_unlock();
		return NULL;
	}

	p = bucket;
	p->kcdb[idx] = kcdb;
	ESQ_DISK_BUCKET_RB_INSERT(&esq_bucket_rb, p);
	range_map_notify_bucket_online(p, p->range_map_name, p->range_map_name_length);
	range_map_unlock();

	return NULL;
}

int
bee_bucket_get(void * bucket_ptr, const char * key, int key_len, char * buffer, int buf_len, int * result_length)
{
	bee_bucket_t * bucket = bucket_ptr;
	int r;

	if (!bucket->kcdb[0])	{
		return ESQ_ERROR_NOTFOUND;
	}

	if (bucket->kcdb[1])	{
		r = kcdbgetbuf(bucket->kcdb[1], key, key_len, buffer, buf_len);
		if (r < 0)	{
			r = kcdbgetbuf(bucket->kcdb[0], key, key_len, buffer, buf_len);
		}
	}
	else	{
		r = kcdbgetbuf(bucket->kcdb[0], key, key_len, buffer, buf_len);
	}

	if (r < 0)	{
		return ESQ_ERROR_NOTFOUND;
	}
	else	{
		*result_length = r;
		return ESQ_OK;
	}
}

int
bee_bucket_get2(void * bucket_ptr, const char * key, int key_len, char ** buffer, int * result_length)
{
	bee_bucket_t * bucket = bucket_ptr;
	size_t data_size;
	char * rp;

	if (!bucket->kcdb[0])	{
		return ESQ_ERROR_NOTFOUND;
	}

	if (bucket->kcdb[1])	{
		rp = kcdbget(bucket->kcdb[1], key, key_len, &data_size);
		if (!rp)	{
			rp = kcdbget(bucket->kcdb[0], key, key_len, &data_size);
		}
	}
	else	{
		rp = kcdbget(bucket->kcdb[0], key, key_len, &data_size);
	}

	if (!rp)	{
		return ESQ_ERROR_NOTFOUND;
	}
	else	{
		*buffer = rp;
		*result_length = data_size;
		return ESQ_OK;
	}
}

int
bee_bucket_set(void * bucket_ptr, const char * key, int key_len, const char * value, int value_len)
{
	bee_bucket_t * bucket = bucket_ptr;
	int r;

	if (bucket->kcdb[1])	{
		r = kcdbset(bucket->kcdb[1], key, key_len, value, value_len);
	}
	else if (bucket->kcdb[0])	{
		r = kcdbset(bucket->kcdb[0], key, key_len, value, value_len);
	}
	else	{
		printf("ERROR: %s, %d: %s\n", __FILE__, __LINE__, "no kcdb opened on bucket");
		return ESQ_ERROR_NOTFOUND;
	}

	if (r)	{
		return 0;
	}
	else	{
		printf("ERROR: %s, %d: %s\n", __FILE__, __LINE__, "kcdbset() failed");
		return ESQ_ERROR_LIBCALL;
	}
}

int
bee_bucket_del(void * bucket_ptr, const char * key, int key_len, const char * value, int value_len)
{
	bee_bucket_t * bucket = bucket_ptr;
	int r;

	if (bucket->kcdb[1])	{
		r = kcdbset(bucket->kcdb[1], key, key_len, value, value_len);
	}
	else if (bucket->kcdb[0])	{
		r = kcdbremove(bucket->kcdb[0], key, key_len);
	}
	else	{
		printf("ERROR: %s, %d: %s\n", __FILE__, __LINE__, "no kcdb opened on bucket");
		return ESQ_ERROR_NOTFOUND;
	}

	if (r)	{
		return 0;
	}
	else	{
		printf("ERROR: %s, %d: %s\n", __FILE__, __LINE__, "kcdbset() failed");
		return ESQ_ERROR_LIBCALL;
	}
}

/* "rangemap0.x.0.kct", "rangemap1.aax.0.kct", "rangemap2.1234x.0.kct" */
STATIC const char *
bee_open_local_bucket_file(char * folder, int folder_len, char * d_name, int * dots)	{
	bee_bucket_t * bucket;
	int hash_str_len;
	int hash_start;
	int malloc_len;
	const char * s;
	int idx;
	int r;

	/* check hash string end */
	if (('x' != d_name[dots[1] - 1]))	{
		return NULL;
	}

	/* check index */
	if ('0' == d_name[dots[2] - 1])	{
		idx = 0;
	}
	else if ('1' == d_name[dots[2] - 1])	{
		idx = 1;
	}
	else	{
		return NULL;
	}

	/* check postfix */
	s = d_name + dots[2];
	if (('k' == s[1]) && ('c' == s[2]) && ('t' == s[3]) && ('\0' == s[4]))	{
		;	/* good */
	}
	else	{
		return NULL;
	}

	/* check range map name */
	if (range_map_name_check2(d_name, dots[0]))	{
		return NULL;
	}

	/* check hash string */
	r = get_hash_string(d_name + dots[0] + 1, dots[1] - dots[0] - 1, &hash_start, &hash_str_len);
	if (-1 == r)	{
		return NULL;
	}

	if ((0 != hash_start) || ((dots[1] - dots[0] - 2) != hash_str_len))	{
		return NULL;
	}

	malloc_len = sizeof(bee_bucket_t)
			+ folder_len
			+ sizeof("/") - 1
			+ dots[2]
			+ 5
			;

	bucket = malloc(malloc_len);
	if (!bucket)	{
		return "no memory for opening local bucket from folder";
	}

	bzero(bucket, malloc_len);

	/* "rangemap0.x.0.kct", "rangemap1.aax.0.kct", "rangemap2.1234x.0.kct" */
	memcpy(bucket->buf, folder, folder_len);
	bucket->buf[folder_len] = '/';
	memcpy(bucket->buf + folder_len + 1, d_name, dots[2] + 5);

	bucket->range_map_name = (char *)(bucket->buf + folder_len + 1);
	bucket->range_map_name_length = dots[0];
	bucket->hash_str = bucket->buf + folder_len + 1 + dots[0] + 1;
	bucket->hash_str_length = hash_str_len;

	bee_bucket_calculate_search_id(bucket);

	#if 0
	printf("bucket->buf = [%s]\n", bucket->buf);
	printf("bucket->range_map_name = [%s]\n", bucket->range_map_name);
	printf("bucket->range_map_name_length = [%d]\n", bucket->range_map_name_length);
	printf("bucket->hash_str = [%s]\n", bucket->hash_str);
	printf("bucket->hash_str_length = [%d]\n", bucket->hash_str_length);
	printf("bucket->search_id = [%d]\n", bucket->search_id);
	#endif

	return bee_open_disk_bucket(bucket, idx, 0);
}

/* "rangemap0.127.0.0.1.2000.x.kct", "rangemap1.127.0.0.1.1000.aax.kct", "rangemap2.192.168.1.1.10000.1234x.kct" */
/*           0   1 2 3 4    5 6  */
STATIC const char *
bee_open_remote_patch_bucket_file(char * folder, int folder_len, char * d_name, int * dots)	{
	bee_bucket_t * bucket;
	unsigned long ap;
	int hash_str_len;
	int hash_start;
	int malloc_len;
	const char * s;
	int port;
	int r;

	/* check hash string end */
	if (('x' != d_name[dots[6] - 1]))	{
		return NULL;
	}

	/* check postfix */
	s = d_name + dots[6];
	if (('k' == s[1]) && ('c' == s[2]) && ('t' == s[3]) && ('\0' == s[4]))	{
		;	/* good */
	}
	else	{
		return NULL;
	}

	/* check range map name */
	if (range_map_name_check2(d_name, dots[0]))	{
		return NULL;
	}

	/* check ip */
	r = get_ipv4_addr(d_name + dots[0] + 1, dots[4] - dots[0] - 1, &ap);
	if (!r)	{
		return NULL;
	}

	/* check port */
	r = get_integer_1000000(d_name + dots[4] + 1, dots[5] - dots[4] - 1, &port);
	if (!r)	{
		return NULL;
	}

	if (PORT_NUMBER_INVALID(port))	{
		return NULL;
	}

	/* check hash string */
	r = get_hash_string(d_name + dots[5] + 1, dots[6] - dots[5] - 1, &hash_start, &hash_str_len);
	if (-1 == r)	{
		return NULL;
	}

	if ((0 != hash_start) || ((dots[6] - dots[5] - 2) != hash_str_len))	{
		return NULL;
	}

	malloc_len = sizeof(bee_bucket_t)
			+ folder_len
			+ sizeof("/") - 1
			+ dots[6]
			+ 4
			;

	bucket = malloc(malloc_len);
	if (!bucket)	{
		return "no memory for opening remote patch bucket from folder";
	}

	bzero(bucket, malloc_len);

	memcpy(bucket->buf, folder, folder_len);
	bucket->buf[folder_len] = '/';
	memcpy(bucket->buf + folder_len + 1, d_name, dots[6] + 5);

	bucket->range_map_name = (char *)(bucket->buf + folder_len + 1);
	bucket->range_map_name_length = dots[0];
	bucket->hash_str = bucket->buf + folder_len + 1 + dots[5] + 1;
	bucket->hash_str_length = hash_str_len;
	bucket->ip = ap;
	bucket->port = port;

	bee_bucket_calculate_search_id(bucket);

	#if 0
	printf("bucket->buf = [%s]\n", bucket->buf);
	printf("bucket->range_map_name = [%s]\n", bucket->range_map_name);
	printf("bucket->range_map_name_length = [%d]\n", bucket->range_map_name_length);
	printf("bucket->hash_str = [%s]\n", bucket->hash_str);
	printf("bucket->hash_str_length = [%d]\n", bucket->hash_str_length);
	printf("bucket->search_id = [%d]\n", bucket->search_id);
	#endif

	return bee_open_disk_bucket(bucket, 1, 0);
}

/* kyoto cabinet tree db in file */
#define DOTSMAX	8
STATIC const char *
bee_open_bucket_file(char * folder, int folder_len, char * d_name)	{
	int dots[DOTSMAX];
	int doti;
	int c;
	int i;

	/* check dots */
	doti = 0;

	for (i = 0;; i ++)	{
		c = d_name[i];
		if ('\0' == c)	{
			break;
		}
		else if ('.' == c)	{
			if (doti < DOTSMAX)	{
				dots[doti] = i;
				doti ++;
			}
			else	{
				return NULL;	/* some other file, ignore */
			}
		}
	}

	if (3 == doti)	{
		return bee_open_local_bucket_file(folder, folder_len, d_name, dots);
	}
	else if (7 == doti)	{
		return bee_open_remote_patch_bucket_file(folder, folder_len, d_name, dots);
	}
	else	{
		return NULL;	/* some other file, ignore */
	}
}

#define MAX_DISK_BUCKET_FOLDER	100
STATIC const char *
bee_open_buckets_from_folder(char * folder, int folder_len)	{
	struct dirent * ent;
	const char * errs;
	DIR * dir;

	if (folder_len > MAX_DISK_BUCKET_FOLDER)	{
		return "folder name too long";
	}
	else if (0 == folder_len)	{
		folder = ".";
		folder_len = 1;
	}

	dir = opendir(folder);
	if (!dir)	{
		return "failed to open folder";
	}

	for (;;)	{
		ent = readdir(dir);
		if (!ent)	{
			break;
		}

		if (DT_REG != ent->d_type)	{
			continue;
		}

		errs = bee_open_bucket_file(folder, folder_len, ent->d_name);
		if (errs)	{
			closedir(dir);
			return errs;
		}
	}

	closedir(dir);

	return NULL;
}

STATIC const char *
bee_add_disk_bucket_by_config(
		char * folder, int folder_len,
		char * range_map, int range_map_length,
		char * ip, int ip_length, int port,
		char * hash, int hash_length,
		int idx
		)
{
	int create_local_bucket;
	bee_bucket_t * bucket;
	const char * errs;
	unsigned long ap;
	int malloc_len;
	int hash_start;
	int hash_len;
	int r;

	if (!ip && !ip_length && (-1 == port))	{
		create_local_bucket = 1;
	}
	else if (ip && ip_length > 0 && (-1 != port))	{
		create_local_bucket = 0;
	}
	else	{
		return "design error";
	}

	if (0 != idx && 1 != idx)	{
		return "value of bucket idx should be 0 or 1";
	}

	if (create_local_bucket)	{
		;
	}
	else	{
		r = get_ipv4_addr(ip, ip_length, &ap);
		if (r != ip_length)	{
			return "invalid IP address string";
		}

		if (PORT_NUMBER_INVALID(port))	{
			return "invalid port number";
		}
	}

	folder_len = strnlen(folder, MAX_DISK_BUCKET_FOLDER + 3);
	if (folder_len > MAX_DISK_BUCKET_FOLDER)	{
		return "folder name too long";
	}
	else if (0 == folder_len)	{
		folder = ".";
		folder_len = 1;
	}

	errs = range_map_name_check2(range_map, range_map_length);
	if (errs)	{
		return errs;
	}

	r = get_hash_string(hash, hash_length, &hash_start, &hash_len);
	if (-1 == r)	{
		return "invalid hash string";
	}

	if (0 != hash_start || hash_length != (hash_len + 1))	{
		return "invalid hash string format";
	}

	if (create_local_bucket)	{
		/* "rangemap0.x.0.kct", "rangemap1.aax.0.kct", "rangemap2.1234x.0.kct" */
		malloc_len = sizeof(bee_bucket_t)
				+ folder_len
				+ 1	/* "/" */
				+ range_map_length
				+ 1 /* "." */
				+ hash_len + 1
				+ sizeof(".0.kct")
				;
		bucket = malloc(malloc_len);
		if (!bucket)	{
			return "no memory for new bucket";
		}

		bzero(bucket, malloc_len);

		/* "rangemap0.x.0.kct", "rangemap1.aax.0.kct", "rangemap2.1234x.0.kct" */
		r = snprintf((char *)bucket->buf, malloc_len - sizeof(bee_bucket_t)
				, "%s/%s.%s.%d.kct"
				, folder
				, range_map
				, hash
				, idx
				);
		if (r >= malloc_len - sizeof(bee_bucket_t) || r <= 0)	{
			free(bucket);
			return "design error";
		}

		bucket->range_map_name = (char *)(bucket->buf + folder_len + 1);
		bucket->range_map_name_length = range_map_length;
		bucket->hash_str = bucket->buf + folder_len + 1 + range_map_length  + 1;
		bucket->hash_str_length = hash_len;
		bee_bucket_calculate_search_id(bucket);

		#if 0
		printf("bucket->buf = [%s]\n", bucket->buf);
		printf("bucket->range_map_name = [%s]\n", bucket->range_map_name);
		printf("bucket->range_map_name_length = [%d]\n", bucket->range_map_name_length);
		printf("bucket->hash_str = [%s]\n", bucket->hash_str);
		printf("bucket->hash_str_length = [%d]\n", bucket->hash_str_length);
		printf("bucket->search_id = [%d]\n", bucket->search_id);
		#endif
	}
	else	{
		/* "rangemap0.127.0.0.1.2000.x.kct", "rangemap1.127.0.0.1.1000.aax.kct", "rangemap2.192.168.1.1.10000.1234x.kct" */
		malloc_len = sizeof(bee_bucket_t)
				+ folder_len
				+ 1	/* "/" */
				+ range_map_length
				+ 1 /* "." */
				+ ip_length
				+ 1
				+ 5	/* port number */
				+ 1
				+ hash_len + 1
				+ sizeof(".kct")
				;
		bucket = malloc(malloc_len);
		if (!bucket)	{
			return "no memory for new bucket";
		}

		bzero(bucket, malloc_len);

		/* "rangemap0.127.0.0.1.2000.x.kct", "rangemap1.127.0.0.1.1000.aax.kct", "rangemap2.192.168.1.1.10000.1234x.kct" */
		r = snprintf((char *)bucket->buf, malloc_len - sizeof(bee_bucket_t)
				, "%s/%s.%s.%d.%s.kct"
				, folder
				, range_map
				, ip
				, port
				, hash
				);
		if (r >= malloc_len - sizeof(bee_bucket_t) || r <= 0)	{
			free(bucket);
			return "design error";
		}

		bucket->range_map_name = (char *)(bucket->buf + folder_len + 1);
		bucket->range_map_name_length = range_map_length;
		bucket->hash_str = bucket->buf + r - sizeof(".kct") - hash_len;
		bucket->hash_str_length = hash_len;
		bucket->ip = ap;
		bucket->port = port;
		bee_bucket_calculate_search_id(bucket);

		#if 0
		printf("bucket->buf = [%s]\n", bucket->buf);
		printf("bucket->range_map_name = [%s]\n", bucket->range_map_name);
		printf("bucket->range_map_name_length = [%d]\n", bucket->range_map_name_length);
		printf("bucket->hash_str = [%s]\n", bucket->hash_str);
		printf("bucket->hash_str_length = [%d]\n", bucket->hash_str_length);
		printf("bucket->search_id = [%d]\n", bucket->search_id);
		#endif
	}

	errs = bee_open_disk_bucket(bucket, idx, 1);

	return errs;
}

STATIC const char *
bee_shift_mem_bucket(char * range_map_name, int range_map_name_len, char * hash, int hash_length)	{
	bee_bucket_t * bucket;
	bee_bucket_t * ep;
	bee_bucket_t * p;
	const char * errs;
	int hash_start;
	int hash_len;
	int malloc_len;
	KCDB * kcdb;
	int r;

	errs = range_map_name_check2(range_map_name, range_map_name_len);
	if (errs)	{
		return errs;
	}

	r = get_hash_string(hash, hash_length, &hash_start, &hash_len);
	if (-1 == r)	{
		return "invalid hash string (mem bucket)";
	}

	if (0 != hash_start || hash_length != (hash_len + 1))	{
		return "invalid hash string format (mem bucket)";
	}

	malloc_len = sizeof(bee_bucket_t)
			+ range_map_name_len
			+ sizeof(".") - 1
			+ hash_length
			+ sizeof(".%")
			;

	/* may be a bucket */
	bucket = malloc(malloc_len);
	if (!bucket)	{
		return "no memory for new memory bucket";
	}

	bzero(bucket, malloc_len);

	memcpy(bucket->buf, range_map_name, range_map_name_len);
	bucket->buf[range_map_name_len] = '.';
	memcpy(bucket->buf + range_map_name_len + 1, hash, hash_length);
	strcpy((char *)(bucket->buf + range_map_name_len + 1 + hash_length), ".%");

	bucket->range_map_name = (char *)(bucket->buf);
	bucket->range_map_name_length = range_map_name_len;
	bucket->hash_str = bucket->buf + range_map_name_len + 1;
	bucket->hash_str_length = hash_len;

	bee_bucket_calculate_search_id(bucket);

	kcdb = kcdbnew();
	if (!kcdb)	{
		free(bucket);
		return "no memory for new memory db";
	}

	/* "-" -> kyotocabinet::ProtoHashDB -- on-memory hash database based on std::unordered_map */
	/* "+" -> kyotocabinet::ProtoTreeDB -- on-memory hash database based on std::map */
	/* ":" -> kyotocabinet::StashDB -- economical on-memory hash database for cache. */
	/* "*" -> kyotocabinet::CacheDB -- on-memory hash database for cache with LRU deletion */
	/* "%" -> kyotocabinet::GrassDB -- on-memory tree database for cache in order */
	r = kcdbopen(kcdb, ":", KCOWRITER | KCOREADER | KCOCREATE);
	if (!r)	{
		free(bucket);
		kcdbdel(kcdb);
		return "failed to open memory kcdb";
	}

	#if 0
	printf("bucket->buf = [%s]\n", bucket->buf);
	printf("bucket->range_map_name = [%s]\n", bucket->range_map_name);
	printf("bucket->range_map_name_length = [%d]\n", bucket->range_map_name_length);
	printf("bucket->hash_str = [%s]\n", bucket->hash_str);
	printf("bucket->hash_str_length = [%d]\n", bucket->hash_str_length);
	printf("bucket->search_id = [%d]\n", bucket->search_id);
	#endif

	range_map_wlock();

	ep = ESQ_DISK_BUCKET_RB_INSERT(&esq_bucket_rb, bucket);
	if (ep)	{
		free(bucket);
		p = ep;
	}
	else	{
		p = bucket;
	}

	if (p->kcdb[0])	{
		if (p->kcdb[1])	{
			kcdbclose(p->kcdb[0]);
			kcdbdel(p->kcdb[0]);
			p->kcdb[0] = p->kcdb[1];
			p->kcdb[1] = kcdb;
			range_map_unlock();
			return NULL;
		}
		else	{
			p->kcdb[1] = kcdb;
			range_map_unlock();
			return NULL;
		}
	}
	else	{
		p->kcdb[0] = kcdb;
		range_map_notify_bucket_online(p, p->range_map_name, p->range_map_name_length);
		range_map_unlock();
		return NULL;
	}
}

STATIC int
bee_mkccb_open_buckets_from_folder(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	mkc_data * folder;
	cc * errs;

	MKC_CB_ARGC_CHECK(1);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_BUF);
	result = MKC_CB_RESULT;
	folder = MKC_CB_ARGV(0);
	result->type = MKC_D_VOID;

	errs = bee_open_buckets_from_folder(folder->buffer, folder->length);

	if (errs)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, errs);
		return -1;
	}

	return 0;
}

STATIC int
bee_mkccb_add_disk_bucket(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	mkc_data * folder;
	mkc_data * range_map;
	mkc_data * hash;
	mkc_data * idx;
	cc * errs;

	MKC_CB_ARGC_CHECK(4);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(1, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(2, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(3, MKC_D_INT);
	result = MKC_CB_RESULT;
	folder = MKC_CB_ARGV(0);
	range_map = MKC_CB_ARGV(1);
	hash = MKC_CB_ARGV(2);
	idx = MKC_CB_ARGV(3);

	result->type = MKC_D_VOID;

	errs = bee_add_disk_bucket_by_config(folder->buffer, folder->length
			, range_map->buffer, range_map->length
			, NULL, 0, -1
			, hash->buffer, hash->length
			, idx->integer
			);

	if (errs)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, errs);
		return -1;
	}

	return 0;
}

STATIC int
bee_mkccb_add_remote_patch_bucket(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	mkc_data * folder;
	mkc_data * range_map;
	mkc_data * ip;
	mkc_data * port;
	mkc_data * hash;
	cc * errs;

	MKC_CB_ARGC_CHECK(5);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(1, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(2, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(3, MKC_D_INT);
	MKC_CB_ARG_TYPE_CHECK(4, MKC_D_BUF);
	result = MKC_CB_RESULT;
	folder = MKC_CB_ARGV(0);
	range_map = MKC_CB_ARGV(1);
	ip = MKC_CB_ARGV(2);
	port = MKC_CB_ARGV(3);
	hash = MKC_CB_ARGV(4);

	result->type = MKC_D_VOID;

	errs = bee_add_disk_bucket_by_config(folder->buffer, folder->length
			, range_map->buffer, range_map->length
			, ip->buffer, ip->length, port->integer
			, hash->buffer, hash->length
			, 1
			);

	if (errs)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, errs);
		return -1;
	}

	return 0;
}

STATIC int
bee_mkccb_shift_mem_bucket(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	mkc_data * range_map;
	mkc_data * hash;
	cc * errs;

	MKC_CB_ARGC_CHECK(2);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(1, MKC_D_BUF);
	result = MKC_CB_RESULT;
	range_map = MKC_CB_ARGV(0);
	hash = MKC_CB_ARGV(1);

	result->type = MKC_D_VOID;

	errs = bee_shift_mem_bucket(range_map->buffer, range_map->length, hash->buffer, hash->length);

	if (errs)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, errs);
		return -1;
	}

	return 0;
}

void
bee_bucket_get_ip_port(void * bucket_ptr, unsigned long * ip, int * port)	{
	bee_bucket_t * bucket = bucket_ptr;
	*ip = bucket->ip;
	*port = bucket->port;
	return;
}

void
bee_bucket_get_hash_str(void * bucket_ptr, const unsigned char ** hash_str, int * hash_str_length)	{
	bee_bucket_t * bucket = bucket_ptr;
	*hash_str = bucket->hash_str;
	*hash_str_length = bucket->hash_str_length;
	return;
}

const char *
bee_bucket_get_db_ptr_state(void * bucket_ptr)	{
	bee_bucket_t * bucket = bucket_ptr;
	if (bucket->kcdb[0])	{
		if (bucket->kcdb[1])	{
			return "BP";
		}
		else	{
			return "B";
		}
	}
	else	{
		if (bucket->kcdb[1])	{
			return "P";
		}
		else	{
			esq_design_error();
			return "";	/* not supposed happend */
		}
	}
}

STATIC const char *
bee_bucket_set_offline(
		char * range_map, int range_map_length,
		const unsigned char * hash, int hash_length,
		int idx
		)
{
	bee_bucket_t * p;
	bee_bucket_t n;
	int hash_start;
	int hash_len;
	int r;

	if (range_map_name_check2(range_map, range_map_length))	{
		return "invalid range map name";
	}

	r = get_hash_string((const char *)hash, hash_length, &hash_start, &hash_len);
	if (-1 == r)	{
		return "invalid hash string";
	}

	if ((0 != idx) && (1 != idx))	{
		return "bucket index should be 0 or 1";
	}

	n.range_map_name = range_map;
	n.range_map_name_length = range_map_length;
	n.hash_str = (unsigned char *)hash;
	n.hash_str_length = hash_length;
	bee_bucket_calculate_search_id(&n);

	#if 0
	printf("n.buf = [%s]\n", n.buf);
	printf("n.range_map_name = [%s]\n", n.range_map_name);
	printf("n.range_map_name_length = [%d]\n", n.range_map_name_length);
	printf("n.hash_str = [%s]\n", n.hash_str);
	printf("n.hash_str_length = [%d]\n", n.hash_str_length);
	printf("n.search_id = [%d]\n", n.search_id);
	#endif

	range_map_wlock();

	p = ESQ_DISK_BUCKET_RB_FIND(&esq_bucket_rb, &n);
	if (p)	{
		if (p->kcdb[idx])	{
			;	/* good */
		}
		else	{
			range_map_unlock();
			return "the range map ok, but the bucket serve that idx is not attached";
		}
	}
	else	{
		range_map_unlock();
		return "no such range map item";
	}

	kcdbclose(p->kcdb[idx]);
	p->kcdb[idx] = NULL;

	if ((!p->kcdb[0]) && (!p->kcdb[1]))	{
		ESQ_DISK_BUCKET_RB_REMOVE(&esq_bucket_rb, p);
		range_map_notify_bucket_offline(p, p->range_map_name, p->range_map_name_length);
		free(p);
	}

	range_map_unlock();

	return NULL;
}

STATIC int
bee_mkccb_bucket_set_offline(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	mkc_data * range_map;
	mkc_data * hash;
	mkc_data * idx;
	cc * errs;

	MKC_CB_ARGC_CHECK(3);
	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(1, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(2, MKC_D_INT);
	result = MKC_CB_RESULT;
	range_map = MKC_CB_ARGV(0);
	hash = MKC_CB_ARGV(1);
	idx = MKC_CB_ARGV(2);

	result->type = MKC_D_VOID;

	errs = bee_bucket_set_offline(range_map->buffer, range_map->length
			, hash->buffer, hash->length
			, idx->integer
			);

	if (errs)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, errs);
		return -1;
	}

	return 0;
}

const char *
bee_bucket_start_sync_session(bucket_sync_session_t * sync_session)	{
	bee_bucket_t * p;
	bee_bucket_t n;
	KCCUR * cursor;

	n.range_map_name = sync_session->range_map_name;
	n.range_map_name_length = sync_session->range_map_name_length;
	n.hash_str = (unsigned char *)sync_session->hash_str;
	n.hash_str_length = sync_session->hash_str_length - 1;	/* remove the 'x' */
	bee_bucket_calculate_search_id(&n);

	#if 0
	printf("bucket->range_map_name = [%s]\n", n.range_map_name);
	printf("bucket->range_map_name_length = [%d]\n", n.range_map_name_length);
	printf("bucket->hash_str = [%s]\n", n.hash_str);
	printf("bucket->hash_str_length = [%d]\n", n.hash_str_length);
	printf("bucket->search_id = [%d]\n", n.search_id);
	#endif

	range_map_rlock();

	p = ESQ_DISK_BUCKET_RB_FIND(&esq_bucket_rb, &n);
	if (!p)	{
		range_map_unlock();
		return "source bucket do not exist";
	}

	if (!p->kcdb[0])	{
		range_map_unlock();
        return "source bucket do not have main db";
	}

	sync_session->kcdb = p->kcdb[0];

	cursor = kcdbcursor(p->kcdb[0]);
	if (!cursor)	{
		range_map_unlock();
		return "failed to create new kcdbcursor";
	}

	kccurjump(cursor);

	sync_session->kccursor = cursor;

	range_map_unlock();

	return NULL;
}

STATIC int
bee_bucket_sync_data_to_target(
		bucket_sync_session_t * sync_session
		, char * kbuf, int ksiz, const char * cvbuf, int vsiz
		, bucket_sync_target_t * p
		)
{
	header_from_pony_to_bee_t header;
	int fd;
	int r;

	if (-1 == p->socket_fd)	{
		header.opcode = BEE_OP_SYNC;
		header.range_map_name_len = sync_session->range_map_name_length;
		header.key_len = ksiz;
		header.data_len = vsiz;
		header.result_buf_size = p->hash_str_length;

		fd = os_open_tcp_socket(1, 10);
		if (-1 == fd)	{
			p->err = "failed to open socket for data sync";
			return -1;
		}

		p->socket_fd = fd;

		/* header */
		r = os_fast_open_sendto_nb(fd, (struct sockaddr *)&(p->address), sizeof(p->address), (void *)&header, sizeof(header));
		if (sizeof(header) !=  r)	{
			p->err = "failed to send initial header";
			return -1;
		}

		/* range map name */
		r = write(fd, sync_session->range_map_name, sync_session->range_map_name_length);
		if (r != sync_session->range_map_name_length)	{
			p->err = "failed to send range map name";
			return -1;
		}
	}
	else	{
		header.opcode = BEE_OP_SYNC;
		header.range_map_name_len = 0;
		header.key_len = ksiz;
		header.data_len = vsiz;
		header.result_buf_size = p->hash_str_length;

		fd = p->socket_fd;

		/* header */
		r = write(fd, &header, sizeof(header));
		if (sizeof(header) != r)	{
			p->err = "failed to send sync header";
			return -1;
		}
	}

	/* hash */
	r = write(fd, p->hash_str, p->hash_str_length);
	if (r != ksiz)	{
		p->err = "failed to send key";
		return -1;
	}

	/* key */
	r = write(fd, kbuf, ksiz);
	if (r != ksiz)	{
		p->err = "failed to send key";
		return -1;
	}

	/* value */
	r = write(fd, cvbuf, vsiz);
	if (r != vsiz)	{
		p->err = "failed to send value";
		return -1;
	}

	return 0;
}

STATIC int
bee_bucket_sync_data_to_target_list(
		bucket_sync_session_t * sync_session
		, char * kbuf, int ksiz, const char * cvbuf, int vsiz
		, bucket_sync_target_t * list
		)
{
	bucket_sync_target_t * p;
	int r;

	for (p = list; p; p = p->same_hash_next)	{
		r = bee_bucket_sync_data_to_target(sync_session, kbuf, ksiz, cvbuf, vsiz, p);
		if (r)	{
			return r;
		}
	}

	return 0;
}

STATIC int
bee_bucket_sync_db_item(bucket_sync_session_t * sync_session, char * kbuf, int ksiz, const char * cvbuf, int vsiz)
{
	bucket_sync_target_t * p;
	hash_t hash;
	int r;

	get_hash_value(kbuf, ksiz, &hash);

	for (p = sync_session->targets; p; p = p->next_hash_list)	{

		if (p->hash_length)	{
			if (memcmp(p->hash.buf, hash.buf, p->hash_length))	{
				continue;	/* different */
			}
			else	{
				;	/* match */
			}
		}
		else	{
			;	/* match */
		}
		
		r = bee_bucket_sync_data_to_target_list(sync_session, kbuf, ksiz, cvbuf, vsiz, p);
		if (r)	{
			return r;
		}
	}

	return 0;
}

#define BUCKET_SYNC_SPRINT_SIZE	20

/* return 0 for continue */
STATIC int
bee_bucket_sync_session_sprint_0(bucket_sync_session_t * sync_session)	{
	bee_bucket_t * p;
	bee_bucket_t n;
	KCCUR * cursor;
	KCDB * kcdb;

	char * kbuf;
	size_t ksiz;
	size_t vsiz;
	const char * cvbuf;

	int i;
	int r;

	n.range_map_name = sync_session->range_map_name;
	n.range_map_name_length = sync_session->range_map_name_length;
	n.hash_str = (unsigned char *)sync_session->hash_str;
	n.hash_str_length = sync_session->hash_str_length - 1;
	bee_bucket_calculate_search_id(&n);

	#if 0
	printf("bucket->range_map_name = [%s]\n", n.range_map_name);
	printf("bucket->range_map_name_length = [%d]\n", n.range_map_name_length);
	printf("bucket->hash_str = [%s]\n", n.hash_str);
	printf("bucket->hash_str_length = [%d]\n", n.hash_str_length);
	printf("bucket->search_id = [%d]\n", n.search_id);
	#endif

	p = ESQ_DISK_BUCKET_RB_FIND(&esq_bucket_rb, &n);
	if (!p)	{
		sync_session->session_err = "source bucket removed, bucket sync thread can't continue";
		return -1;
	}

	kcdb = p->kcdb[0];
	if (!kcdb)	{
        sync_session->session_err = "source bucket db removed, bucket sync thread can't continue";
		return -1;
	}

	if (sync_session->kcdb != kcdb)	{
        sync_session->session_err = "local kcdb changed between sync sprints, please restart sync thread";
		return -1;
	}

	cursor = sync_session->kccursor;

	i = 0;

	while ((kbuf = kccurget(cursor, &ksiz, &cvbuf, &vsiz, 1))) {

		r = bee_bucket_sync_db_item(sync_session, kbuf, ksiz, cvbuf, vsiz);

		kcfree(kbuf);

		if (r)	{
			return -1;
		}
		else	{
			sync_session->processed ++;
		}

		i ++;

		if (i >= BUCKET_SYNC_SPRINT_SIZE)	{
			break;
		}
 	}

	/* todo kccurecode	(	KCCUR * 	cur	) */

	return 0;
}

STATIC void
bee_bucket_sync_session_write_dashboard(bucket_sync_session_t * sync_session)	{
	bucket_sync_target_t * p;
	int offset;
	int i;
	int r;

	offset = 0;

	if (sync_session->session_err)	{
		r = esq_dashboard_snprintf(sync_session->dashboard, offset, "sync session reports: %s\n", sync_session->session_err);
		if (r <= 0)	{
			return;
		}
		offset += r;
	}

	for (i = 0; i < sync_session->target_cnt; i ++)	{
		p = sync_session->targets + i;
		if (p->err)	{
			r = esq_dashboard_snprintf(sync_session->dashboard, offset, "sync target reports: %s\n", p->err);
			if (r <= 0)	{
				return;
			}
			offset += r;
		}
	}

	r = esq_dashboard_snprintf(sync_session->dashboard, offset, "sync session proceed items: %d\n", sync_session->processed);
	if (r <= 0)	{
		return;
	}
	offset += r;

	return;
}

int
bee_bucket_sync_session_sprint(bucket_sync_session_t * sync_session)	{
	int r;

	range_map_rlock();

	r = bee_bucket_sync_session_sprint_0(sync_session);

	range_map_unlock();

	bee_bucket_sync_session_write_dashboard(sync_session);

	return r;
}

void
bee_bucket_end_sync_session(bucket_sync_session_t * sync_session)	{
	int i;
	int fd;

	if (sync_session->kccursor)	{
		kccurdel(sync_session->kccursor);
		sync_session->kccursor = NULL;
	}

	for (i = 0; i < sync_session->target_cnt; i ++)	{
		fd = sync_session->targets[i].socket_fd;
		if (-1 != fd)	{
			close(fd);
		}
	}
}

int
esq_bucket_merge_item(bee_bucket_merge_session_t * session)	{
#if 0
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
	void * kcdb;
} bee_bucket_merge_session_t;
#endif
	bee_bucket_t * b;
	char * data_ptr;
	int data_len;
	int r;

	if (session->bucket)	{
		b = session->bucket;
	}
	else	{

		if (!session->range_map_name_length)	{
			printf("ERROR: %s, %d: range map name supposed to be provided in the first header\n", __FILE__, __LINE__);
			return -1;
		}

		b = esq_bee_find_bucket(
				session->range_map_name, session->range_map_name_length
				, (const unsigned char *)session->hash_str, session->hash_str_length
				, 0, 0
				);

		if (b)	{
			session->bucket = b;
		}
		else	{
			printf("ERROR: %s, %d: failed to find the local bucket for data merge\n", __FILE__, __LINE__);
			return -1;
		}
	}

	r = bee_bucket_get2(b, session->key, session->key_length, &data_ptr, &data_len);
	if (r)	{
		return 0;
	}

	if (data_len < sizeof(struct timeval))	{
		r = bee_bucket_set(b, session->key, session->key_length, data_ptr, data_len);
		goto __exx;
	}


	if (timercmp((struct timeval *)(session->value), (struct timeval *)(data_ptr), >))	{
		r = bee_bucket_set(b, session->key, session->key_length, data_ptr, data_len);
		goto __exx;
	}

__exx:

	kcfree(data_ptr);

	return 0;
}

int
bee_bucket_init(void)	{
	void * mkc;

	RB_INIT(&esq_bucket_rb);
	mkc = esq_get_mkc();
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "OpenDiskBucketFromFolder", bee_mkccb_open_buckets_from_folder));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "AddDiskBucket", bee_mkccb_add_disk_bucket));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "AddRemotePatchBucket", bee_mkccb_add_remote_patch_bucket));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "ShiftMemBucket", bee_mkccb_shift_mem_bucket));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(mkc, "BucketSetOffline", bee_mkccb_bucket_set_offline));

	return 0;
}

void
bee_bucket_detach(void)	{
	bee_close_all_buckets();
	return;
}

/* of */
