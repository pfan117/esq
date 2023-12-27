#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "public/esq.h"
#include "public/libesq.h"

/* 512 * 8 = 4096 */
#define BITS_ARRAY_SIZE	512

#define MGC0	0x189a189a
#define MGC1	0x189b189b

typedef struct _path_bloom_filter_t	{
	int magic0;
	unsigned char bitmap0[BITS_ARRAY_SIZE];
	unsigned char bitmap1[BITS_ARRAY_SIZE];
	unsigned char bitmap2[BITS_ARRAY_SIZE];
	int magic1;
}
path_bloom_filter_t;

void * path_bloom_filter_new(void)	{
	path_bloom_filter_t * p;
	p = (path_bloom_filter_t *)malloc(sizeof(path_bloom_filter_t));
	if (!p)	{
		return NULL;
	}

	bzero(p, sizeof(path_bloom_filter_t));

	p->magic0 = MGC0;
	p->magic1 = MGC1;

	return p;
}

static unsigned int
path_bloom_hash0(const char * path, int len)	{
	/* 4096 = 0x1000 */
	int i;
	unsigned int hash = 0;
	unsigned int c;
	unsigned int k;

	for (i = 0; i < len; i ++)	{
#if 1
		c = path[i];

		k = (i & 0x3);

		if (0 == k)	{
			hash += c;
		}
		else if (1 == k)	{
			hash += (c << 3);
		}
		else if (2 == k)	{
			hash += (c << 5);
		}
		else	{
			hash += (c << 8);
		}
#else
		c = path[i];
		hash = (hash << 5) ^ (hash >> 27) ^ c;
#endif
	}

	return hash & 0xfff;
}

static unsigned int 
path_bloom_hash1(const char * path, int len)	{
	/* 4096 = 0x1000 */
	int i;
	unsigned int hash = 0;
	unsigned int c;
	//unsigned int k = 0;

	for (i = 0; i < len; i ++)	{
#if 0
		c = path[i];
		k += i;
		k = (k & 0x7);

		if (0 == k)	{
			hash ^= c;
		}
		else if (1 == k)	{
			hash ^= ~(c << 4);
		}
		else if (2 == k)	{
			hash ^= (c << 3);
		}
		else if (3 == k)	{
			hash ^= (c << 6);
		}
		else if (4 == k)	{
			hash ^= ~(c << 7);
		}
		else if (5 == k)	{
			hash ^= (c << 8);
		}
		else if (6 == k)	{
			hash ^= (c << 2);
		}
		else	{
			hash ^= (c << 1);
	
	}
#else
		c = path[i];
		hash = hash * 131 + c;
#endif
	}

	return hash & 0xfff;
}

static unsigned int 
path_bloom_hash2(const char * path, int len)	{
	/* 4096 = 0x1000 */
	int i;
	unsigned int hash = 0;
	unsigned int c;
	unsigned int k = 0;
	unsigned int last = 68;

	for (i = 0; i < len; i ++)	{

		c = path[i];
		k += i;
		k = (k & 0x7);

		if (0 == k)	{
			hash ^= c;
		}
		else if (1 == k)	{
			hash ^= ~(c << 2);
			last = c;
		}
		else if (2 == k)	{
			hash ^= ((c << 7) ^ last);
		}
		else if (3 == k)	{
			hash ^= ~(c << 5);
			last = c;
		}
		else if (4 == k)	{
			hash ^= (c << 6);
		}
		else if (5 == k)	{
			hash ^= ~(c << 3);
			last = c;
		}
		else if (6 == k)	{
			hash ^= (c << 1);
		}
		else	{
			hash ^= ((c << 4) ^ last);
		}
	}

	return hash & 0xfff;
}

void
path_bloom_filter_free(void * ptr)	{
	path_bloom_filter_t * p = ptr;

	if (p && (MGC0 == p->magic0) && (MGC1 == p->magic1))	{
		;
	}
	else	{
		printf("ERROR: %s() %d: invalid path_bloom_filter\n", __func__, __LINE__);
		return;
	}

	free(p);

	return;
}

static int
__path_bloom_filter_add_path(path_bloom_filter_t * p, const char * path, int path_length)	{
	unsigned int hash0;
	unsigned int hash1;
	unsigned int hash2;

	if ((MGC0 == p->magic0) && (MGC1 == p->magic1))	{
		;
	}
	else	{
		return ESQ_ERROR_PARAM;
	}

	hash0 = path_bloom_hash0(path, path_length);
	hash1 = path_bloom_hash1(path, path_length);
	hash2 = path_bloom_hash2(path, path_length);

	p->bitmap0[hash0 >> 3] |= (1 << (hash0 & 0x3));
	p->bitmap1[hash1 >> 3] |= (1 << (hash1 & 0x3));
	p->bitmap2[hash2 >> 3] |= (1 << (hash2 & 0x3));

	return 0;
}

int
path_bloom_filter_add_path2(void * ptr, const char * path, int path_length)	{
	int r;

	if (ptr && path && (path_length > 0))	{
		;
	}
	else	{
		return ESQ_ERROR_PARAM;
	}

	r = __path_bloom_filter_add_path(ptr, path, path_length);

	return r;
}

int
path_bloom_filter_add_path(void * ptr, const char * path)	{
	int path_length;
	int r;

	if (ptr && path)	{
		;
	}
	else	{
		return ESQ_ERROR_PARAM;
	}

	path_length = strnlen(path, ESQ_PATH_BLOOM_MAX);
	if (path_length <= 0 || path_length >= ESQ_PATH_BLOOM_MAX)	{
		return ESQ_ERROR_PARAM;
	}

	r = __path_bloom_filter_add_path(ptr, path, path_length);

	return r;
}

static int
__path_bloom_filter_check_exist(path_bloom_filter_t * p, const char * path, int path_length)	{
	unsigned int hash0;
	unsigned int hash1;
	unsigned int hash2;

	if ((MGC0 == p->magic0) && (MGC1 == p->magic1))	{
		;
	}
	else	{
		return 0;
	}

	hash0 = path_bloom_hash0(path, path_length);
	hash1 = path_bloom_hash1(path, path_length);
	hash2 = path_bloom_hash2(path, path_length);

	if (!(p->bitmap0[hash0 >> 3] & (1 << (hash0 & 0x3))))	{
		return 0;
	}

	if (!(p->bitmap1[hash1 >> 3] & (1 << (hash1 & 0x3))))	{
		return 0;
	}

	if (!(p->bitmap2[hash2 >> 3] & (1 << (hash2 & 0x3))))	{
		return 0;
	}

	return 1;
}

/* return 0 for not exist */
/* return 1 for exist */
int
path_bloom_filter_check_exist2(void * ptr, const char * path, int path_length)	{
	int r;

	if (ptr && path && (path_length > 0))	{
		;
	}
	else	{
		return 0;
	}

	r = __path_bloom_filter_check_exist(ptr, path, path_length);

	return r;
}

/* return 0 for not exist */
/* return 1 for exist */
int
path_bloom_filter_check_exist(void * ptr, const char * path)	{
	int path_length;
	int r;

	if (ptr && path)	{
		;
	}
	else	{
		return 0;
	}

	path_length = strnlen(path, ESQ_PATH_BLOOM_MAX);
	if (path_length <= 0 || path_length >= ESQ_PATH_BLOOM_MAX)	{
		return 0;
	}

	r = __path_bloom_filter_check_exist(ptr, path, path_length);

	return r;
}

#if defined PATH_BLOOM_FILTER_TEST

#include "tmp_etc_find_list.inc"
#include "tmp_usr_include_find_list.inc"

static int
bit_count(unsigned char k)	{
	int c = 0;

	if (k & 0x1) c ++;
	if (k & 0x2) c ++;
	if (k & 0x4) c ++;
	if (k & 0x8) c ++;
	if (k & 0x10) c ++;
	if (k & 0x20) c ++;
	if (k & 0x40) c ++;
	if (k & 0x80) c ++;

	return c;
}

int
main(void)	{
	int i;
	void * pbf;
	path_bloom_filter_t * p;

	int l;
	int c;

	int total;
	int exist;
	int count;

	pbf = path_bloom_filter_new();
	if (!pbf)	{
		printf("ERROR: %s() %d: oom\n", __func__, __LINE__);
		return -1;
	}

	for (i = 0; i < (sizeof(etc_list) / sizeof(etc_list[0])); i ++)	{
		//printf("DBG: %s() %d: etc_list[%d] = %s\n", __func__, __LINE__, i, etc_list[i]);
		path_bloom_filter_add_path(pbf, etc_list[i]);
	}

	p = pbf;

	printf("bitmap0:\n");

	for (l = 0; l < (BITS_ARRAY_SIZE / 32); l ++)	{
		for (c = 0; c < 32; c ++)	{
			count = bit_count(p->bitmap0[l * 32 + c]);
			if (count)	{
				printf("%d", count);
			}
			else	{
				printf("-");
			}
		}
		printf("\n");
	}
	
	printf("bitmap1:\n");

	for (l = 0; l < (BITS_ARRAY_SIZE / 32); l ++)	{
		for (c = 0; c < 32; c ++)	{
			count = bit_count(p->bitmap1[l * 32 + c]);
			if (count)	{
				printf("%d", count);
			}
			else	{
				printf("-");
			}

		}
		printf("\n");
	}

	printf("bitmap2:\n");

	for (l = 0; l < (BITS_ARRAY_SIZE / 32); l ++)	{
		for (c = 0; c < 32; c ++)	{
			count = bit_count(p->bitmap2[l * 32 + c]);
			if (count)	{
				printf("%d", count);
			}
			else	{
				printf("-");
			}

		}
		printf("\n");
	}

	total = 0;
	exist = 0;

	for (i = 0; i < (sizeof(usr_include_list) / sizeof(usr_include_list[0])); i ++)	{
		//printf("DBG: %s() %d: etc_list[%d] = %s\n", __func__, __LINE__, i, usr_include_list[i]);
		total ++;
		if (path_bloom_filter_check_exist(pbf, usr_include_list[i]))	{
			exist ++;
		}
	}

	printf("result:\n");

	printf("error %d\n", exist);
	printf("total %d\n", total);
	printf("failure rate %d / 1000\n", exist * 1000 / total);
	printf("\n");

	path_bloom_filter_free(pbf);

	return 0;
}

#endif

/* eof */
