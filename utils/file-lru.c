#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <nghttp2/nghttp2.h>

#include <monkeycall.h>
#include <public/esq.h>
#include <public/h2esq.h>
#include <public/libesq.h>
#include <public/esqll.h>

#include "internal/file_lru.h"
#include "internal/tree.h"

#define FILE_LRU_MGC0	0xac12ac34
#define FILE_LRU_MGC1	0xab45cd45
#define LRU_PATH_MAX	(256)

#define FILE_LRU_FLAGS_PURGED	0x1

static int lru_size_max = 4096;
static int lru_size_total = 0;
static char lru_root_path[LRU_PATH_MAX] = "./";
static char lru_root_path_length = 2;
static pthread_spinlock_t file_lru_lock;
static void * lru_bloom_filter = NULL;

typedef struct _file_lru_t	{
	int magic0;
	int borrow_cnt;
	int flags;
	RB_ENTRY(_file_lru_t) rb;
	int file_name_hash;
	int file_name_length;
	int file_size;
	const char * file_name;
	const char * file_data;
	struct _file_lru_t * prev;
	struct _file_lru_t * next;
	int magic1;
	char buf[];
} file_lru_t;

int
file_lru_get_data(void * file_lru_ptr, const char ** data_ptr, int * length)	{
	file_lru_t * file_lru = file_lru_ptr;

	if (file_lru_ptr && data_ptr && length)	{
		;
	}
	else	{
		return ESQ_ERROR_PARAM;
	}

	if ((FILE_LRU_MGC0 == file_lru->magic0) && (FILE_LRU_MGC0 == file_lru->magic0))	{
		;
	}
	else	{
		return ESQ_ERROR_PARAM;
	}

	*data_ptr = file_lru->file_data;
	*length = file_lru->file_size;

	return ESQ_OK;
}

int
file_lru_name_hash(const char * s, int len)	{
	unsigned char h0;
	unsigned char h1;
	unsigned char h2;
	unsigned char h3;
	int i;

	h0 = 0;
	h1 = 1;
	h2 = 2;
	h3 = 3;

	for(i = 0; i < len; i ++)	{
		if ((i & 1))	{
			h0 += (unsigned char)s[i];
		}
		if ((i & 3))	{
			h1 += (unsigned char)s[i];
		}
		if ((i & 2))	{
			h2 += (unsigned char)s[i];
		}
		if ((i & 6))	{
			h3 += (unsigned char)s[i];
		}
	}

	return h0 + (h1 << 8) + (h2 << 16) + (h3 << 24);
}

STATIC int
file_lru_name_cmp(file_lru_t * a, file_lru_t * b)	{
	int d;

	d = a->file_name_hash - b->file_name_hash;
	if (d)	{
		return d;
	}

	d = a->file_name_length - b->file_name_length;
	if (d)	{
		return d;
	}

	return strcmp(a->file_name, b->file_name);
}

RB_HEAD(FILE_LRU, _file_lru_t);
RB_PROTOTYPE_STATIC(FILE_LRU, _file_lru_t, rb, file_lru_name_cmp);
RB_GENERATE_STATIC(FILE_LRU, _file_lru_t, rb, file_lru_name_cmp);

static file_lru_t * file_lru_head = NULL;
static file_lru_t * file_lru_tail = NULL;

static file_lru_t * file_lru_purged_head = NULL;
static file_lru_t * file_lru_purged_tail = NULL;

static struct FILE_LRU file_lru_rb = { NULL };

file_lru_t *
file_lru_load_from_disk(const char * file_name, int file_name_len)	{
	char path[LRU_PATH_MAX];
	struct stat buf;
	file_lru_t * p;
	int file_size;
	int total;
	int fd;
	int r;

	r = snprintf(path, sizeof(path), "%s/%s", lru_root_path, file_name);
	if (r <= 0 || r >= sizeof(path))	{
		esq_log("ERROR: %s() %d: invalid path length\n", __func__, __LINE__);
		return NULL;
	}

	//printf("DBG: %s() %d: open file %s and create new file_lru\n", __func__, __LINE__, file_name);

	r = stat(path, &buf);
	if (r)	{
		esq_log("ERROR: %s() %d: failed to stat file\n", __func__, __LINE__);
		return NULL;
	}

	if (S_IFREG != (buf.st_mode & S_IFMT))	{
		esq_log("ERROR: %s() %d: is not a regular file\n", __func__, __LINE__);
		return NULL;
	}

	file_size = buf.st_size;

	if (file_size <= 0)	{
		esq_log("ERROR: %s() %d: invalid file size value\n", __func__, __LINE__);
		return NULL;
	}

	if (lru_size_max > 0 && file_size > lru_size_max)	{
		esq_log("ERROR: %s() %d: file size exceed max limit\n", __func__, __LINE__);
		return NULL;
	}

	total = sizeof(file_lru_t) + file_size + file_name_len + 1;

	p = malloc(total);
	if (p)	{
		;
	}
	else	{
		esq_log("ERROR: %s() %d: oom\n", __func__, __LINE__);
		return NULL;
	}

	bzero(p, sizeof(file_lru_t));

	p->magic0 = FILE_LRU_MGC0;
	p->magic1 = FILE_LRU_MGC1;
	p->file_name_hash = file_lru_name_hash(file_name, file_name_len);
	p->file_name_length = file_name_len;
	memcpy(p->buf, file_name, file_name_len);
	p->buf[file_name_len] = '\0';
	p->file_size = file_size;
	p->file_name = p->buf;
	p->file_data = p->buf + file_name_len + 1;

	fd = open(path, O_RDONLY);
	if (-1 == fd)	{
		free(p);
		esq_log("ERROR: %s() %d: failed to open file for reading\n", __func__, __LINE__);
		return NULL;
	}

	r = read(fd, p->buf + file_name_len + 1, file_size);
	if (r != file_size)	{
		free(p);
		close(fd);
		esq_log("ERROR: %s() %d: failed to read data from file\n", __func__, __LINE__);
		return NULL;
	}

	close(fd);

	return p;
}

void
file_lru_purge_all(void)	{

	file_lru_t * p;

	pthread_spin_lock(&file_lru_lock);

	for (p = file_lru_head; p; p = file_lru_head)	{

		DOUBLE_LL_LEFT(file_lru_head, file_lru_tail, prev, next, p);
		FILE_LRU_RB_REMOVE(&file_lru_rb, p);

		if (p->borrow_cnt)	{
			p->flags |= FILE_LRU_FLAGS_PURGED;
			DOUBLE_LL_JOIN_HEAD(file_lru_purged_head, file_lru_purged_tail, prev, next, p);
		}
		else	{
			pthread_spin_unlock(&file_lru_lock);
			free(p);
			pthread_spin_lock(&file_lru_lock);
		}
	}

	pthread_spin_unlock(&file_lru_lock);

	return;
}

STATIC void
__file_lru_purge(int file_size)	{
	file_lru_t * p;

	while(lru_size_max - lru_size_total < file_size)	{

		if (!file_lru_tail)	{
			break;
		}

		p = file_lru_tail;

		DOUBLE_LL_LEFT(file_lru_head, file_lru_tail, prev, next, p);
		FILE_LRU_RB_REMOVE(&file_lru_rb, p);

		lru_size_total -= p->file_size;

		if (p->borrow_cnt)	{
			p->flags |= FILE_LRU_FLAGS_PURGED;
			DOUBLE_LL_JOIN_HEAD(file_lru_purged_head, file_lru_purged_tail, prev, next, p);
		}
		else	{
			free(p);
		}
	}

	return;
}

void *
file_lru_borrow(const char * file_name)	{

	file_lru_t * p;
	file_lru_t * e;
	file_lru_t f;
	int hash;
	int file_name_len;
	int new_total;

	file_name_len = strnlen(file_name, LRU_PATH_MAX);
	if (file_name_len <= 0 || file_name_len >= LRU_PATH_MAX)	{
		esq_log("ERROR: %s() %d: invalid file name length\n", __func__, __LINE__);
		return NULL;
	}

	hash = file_lru_name_hash(file_name, file_name_len);
	f.file_name = file_name;
	f.file_name_length = file_name_len;
	f.file_name_hash = hash;

	pthread_spin_lock(&file_lru_lock);

	if (path_bloom_filter_check_exist2(lru_bloom_filter, file_name, file_name_len))	{
		;
	}
	else	{
		pthread_spin_unlock(&file_lru_lock);
		esq_log("WARN: %s() %d: bloom filter blocked a file access to %s\n", __func__, __LINE__, file_name);
		return NULL;
	}

	p = FILE_LRU_RB_FIND(&file_lru_rb, &f);
	if (p)	{
		p->borrow_cnt ++;
		pthread_spin_unlock(&file_lru_lock);
		return p;
	}
	else	{
		pthread_spin_unlock(&file_lru_lock);
	}

	p = file_lru_load_from_disk(file_name, file_name_len);
	if (!p)	{
		return NULL;
	}

	pthread_spin_lock(&file_lru_lock);

	e = FILE_LRU_RB_FIND(&file_lru_rb, p);
	if (e)	{
		e->borrow_cnt ++;
		DOUBLE_LL_LEFT(file_lru_head, file_lru_tail, prev, next, e);
		DOUBLE_LL_JOIN_HEAD(file_lru_head, file_lru_tail, prev, next, e);
		pthread_spin_unlock(&file_lru_lock);
		free(p);
		return e;
	}

	if (lru_size_max <= 0)	{
		goto __insert;	/* no lru size limit */
	}

	new_total = lru_size_total + p->file_size;

	if (new_total < lru_size_total)	{
		pthread_spin_unlock(&file_lru_lock);
		free(p);
		esq_log("ERROR: %s() %d: unexpected size value\n", __func__, __LINE__);
		return NULL;
	}

	if (new_total > lru_size_max)	{
		__file_lru_purge(p->file_size);
	}

__insert:

	p->borrow_cnt ++;
	lru_size_total += p->file_size;
	FILE_LRU_RB_INSERT(&file_lru_rb, p);
	DOUBLE_LL_JOIN_HEAD(file_lru_head, file_lru_tail, prev, next, p);
	pthread_spin_unlock(&file_lru_lock);
	esq_log("INFO: %s() %d: new file lru created for %s\n", __func__, __LINE__, file_name);

	return p;
}

void
file_lru_return(void * file_lru_ptr)	{
	file_lru_t * file_lru = file_lru_ptr;

	if (!file_lru)	{
		esq_log("ERROR: %s() %d: invalid parameter\n", __func__, __LINE__);
		return;
	}

	if ((FILE_LRU_MGC0 == file_lru->magic0) && (FILE_LRU_MGC1 == file_lru->magic1))	{
		;
	}
	else	{
		esq_log("ERROR: %s() %d: invalid file_lru\n", __func__, __LINE__);
		return;
	}

	pthread_spin_lock(&file_lru_lock);

	file_lru->borrow_cnt --;

	if (file_lru->borrow_cnt < 0)	{
		pthread_spin_unlock(&file_lru_lock);
		esq_log("ERROR: %s() %d: file_lru borrow_cnt less than zero!\n", __func__, __LINE__);
		return;
	}

	if ((file_lru->flags & FILE_LRU_FLAGS_PURGED))	{
		;	/* fall through */
	}
	else	{
		pthread_spin_unlock(&file_lru_lock);
		return;
	}

	if (file_lru->borrow_cnt > 0)	{
		pthread_spin_unlock(&file_lru_lock);
	}
	else	{
		DOUBLE_LL_LEFT(file_lru_purged_head, file_lru_purged_tail, prev, next, file_lru);
		pthread_spin_unlock(&file_lru_lock);
		free(file_lru);
	}

	return;
}

const char *
file_lru_setup(const char * root_path, int root_path_length, int lru_size_max_in_byte)	{
	char path[LRU_PATH_MAX];
	struct stat buf;
	int r;

	if (!root_path || root_path_length <= 0 || root_path_length >= LRU_PATH_MAX)	{
		return "invalid root_path string";
	}

	memcpy(path, root_path, root_path_length);
	path[root_path_length] = '\0';

	//printf("DBG: %s() %d: specify root path %s\n", __func__, __LINE__, path);

	r = stat(path, &buf);
	if (r)	{
		return "failed to get target stat";
	}

	if (S_IFDIR & buf.st_mode)	{
		//printf("DBG: %s() %d: sure, it is a folder\n", __func__, __LINE__);
	}
	else	{
		return "not a good folder target";
	}

	pthread_spin_lock(&file_lru_lock);

	memcpy(lru_root_path, root_path, root_path_length);
	lru_root_path[root_path_length] = '\0';
	lru_root_path_length = root_path_length;
	lru_size_max = lru_size_max_in_byte;

	pthread_spin_unlock(&file_lru_lock);

	#if 0
	printf("DBG: %s() %d: file LRU root folder set to %s, max size set to %d bytes\n",
			__func__, __LINE__,
			lru_root_path, lru_size_max
			);
	#endif

	return NULL;
}

STATIC int
sbc_mkccb_file_lru_setup(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	mkc_data * root_folder;
	mkc_data * lru_size_max_in_byte;
	int argc;
	const char * errs;

	argc = MKC_CB_ARGC;
	if (2 != argc)	{
		MKC_CB_ERROR("%s", "need one parameters");
		return -1;
	}

	MKC_CB_ARG_TYPE_CHECK(0, MKC_D_BUF);
	MKC_CB_ARG_TYPE_CHECK(1, MKC_D_INT);

	root_folder = MKC_CB_ARGV(0);
	lru_size_max_in_byte = MKC_CB_ARGV(1);
	result = MKC_CB_RESULT;
	result->type = MKC_D_VOID;

	errs = file_lru_setup(root_folder->buffer, root_folder->length, lru_size_max_in_byte->integer);
	if (errs)	{
		MKC_CB_ERROR("%s, %d: %s", __func__, __LINE__, errs);
		return -1;
	}

	return 0;
}

STATIC int
sbc_mkccb_file_lru_purge(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	int argc;

	argc = MKC_CB_ARGC;
	if (0 != argc)	{
		MKC_CB_ERROR("%s", "need no parameters");
		return -1;
	}

	file_lru_purge_all();

	result = MKC_CB_RESULT;
	result->type = MKC_D_VOID;

	return 0;
}

void
file_lru_bloom_filter_free(void)	{
	void * obf;

	pthread_spin_lock(&file_lru_lock);
	obf = lru_bloom_filter;
	lru_bloom_filter = NULL;
	pthread_spin_unlock(&file_lru_lock);

	if (obf)	{
		path_bloom_filter_free(obf);
	}

	return;
}

static int
file_lru_file_enumerators_cb(const char * path_name, int length, void * param)	{
	int r;

	//printf("DBG: %s() %d: path_name = [%s], length = %d\n", __func__, __LINE__, path_name, length);
	#if 0
	printf("DBG: %s() %d: effect_path_name = [%s], length = %d\n", __func__, __LINE__,
			path_name + lru_root_path_length,
			length - lru_root_path_length
			);
	#endif

	if (length <= lru_root_path_length)	{
		esq_log("WARN: %s() %d: %s is too long to add into bloom filter, ignore\n", __func__, __LINE__, path_name);
		return 0;
	}

	r = path_bloom_filter_add_path2(param, path_name + lru_root_path_length, length - lru_root_path_length);
	if (r)	{
		esq_log("ERROR: %s() %d: failed to add path %s path bloom filter\n", __func__, __LINE__, path_name);
	}
	else	{
		esq_log("INFO: %s() %d: path %s added into path bloom filter\n", __func__, __LINE__, path_name);
	}

	return 0;
}

STATIC void
file_lru_update_bloom_filter(void)	{
	
	void * bf;
	void * obf;

	bf = path_bloom_filter_new();

	if (!bf)	{
		esq_log("ERROR: %s() %d: oom\n", __func__, __LINE__);
		return;
	}

	esq_log("INFO: %s() %d: create file LRU bloom filter for path %s\n", __func__, __LINE__, lru_root_path);

	esq_file_enumerators(lru_root_path, file_lru_file_enumerators_cb, bf);

	pthread_spin_lock(&file_lru_lock);
	obf = lru_bloom_filter;
	lru_bloom_filter = bf;
	pthread_spin_unlock(&file_lru_lock);

	if (obf)	{
		path_bloom_filter_free(obf);
	}

	esq_log("INFO: %s() %d: new file lru bloom filter created\n", __func__, __LINE__);

	return;
}

STATIC void
file_lru_remove_bloom_filter(void)	{
	void * p;
	
	pthread_spin_lock(&file_lru_lock);
	p = lru_bloom_filter;
	lru_bloom_filter = NULL;
	pthread_spin_unlock(&file_lru_lock);

	if (p)	{
		path_bloom_filter_free(p);
		esq_log("INFO: %s() %d: remove and free file lru path bloom filter\n", __func__, __LINE__);
	}
	else	{
		esq_log("INFO: %s() %d: file lru path bloom filter has been removed before\n", __func__, __LINE__);
	}
}

/* "FileLRUUpdateBloomFilter" */
STATIC int
sbc_mkccb_file_lru_update_bloom_filter(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	int argc;

	argc = MKC_CB_ARGC;
	if (0 != argc)	{
		MKC_CB_ERROR("%s", "need no parameters");
		return -1;
	}

	file_lru_update_bloom_filter();

	result = MKC_CB_RESULT;
	result->type = MKC_D_VOID;

	return 0;
}

/* "FileLRURemoveBloomFilter" */
STATIC int
sbc_mkccb_file_lru_remove_bloom_filter(mkc_session * session, mkc_cb_stack_frame * mkc_cb_stack)	{
	mkc_data * result;
	int argc;

	argc = MKC_CB_ARGC;
	if (0 != argc)	{
		MKC_CB_ERROR("%s", "need no parameters");
		return -1;
	}

	file_lru_remove_bloom_filter();

	result = MKC_CB_RESULT;
	result->type = MKC_D_VOID;

	return 0;
}

int
file_lru_init(void)	{

	void * server;

	pthread_spin_init(&file_lru_lock, 0);

	server = esq_get_mkc();

	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(server, "FileLRUSetup", sbc_mkccb_file_lru_setup));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(server, "FileLRUPurge", sbc_mkccb_file_lru_purge));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(server, "FileLRUUpdateBloomFilter", sbc_mkccb_file_lru_update_bloom_filter));
	ESQ_IF_ERROR_RETURN(mkc_cbtree_add(server, "FileLRURemoveBloomFilter", sbc_mkccb_file_lru_remove_bloom_filter));

	return 0;
}

void
file_lru_detach(void)	{
	file_lru_purge_all();
	file_lru_bloom_filter_free();
	pthread_spin_destroy(&file_lru_lock);
}

/* eof */
