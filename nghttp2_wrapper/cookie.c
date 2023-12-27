#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <nghttp2/nghttp2.h>

#include "public/libesq.h"
#include "public/h2esq.h"
#include "internal/tree.h"
#include "public/esqll.h"

#include "nghttp2_wrapper/includes/cookie.h"

typedef struct _h2esq_cookies_rb_roots	{
	void * root_by_name;
	void * root_by_idx;
}
h2esq_cookies_rb_roots;

typedef struct _h2esq_cookie_t	{
	RB_ENTRY(_h2esq_cookie_t) rb_by_name;
	RB_ENTRY(_h2esq_cookie_t) rb_by_idx;
	int idx;
	const char * name;
	int name_len;
	unsigned long name_hash;
	const char * value;
	int value_len;
	char buf[];
} h2esq_cookie_t;

STATIC int
h2esq_rbtree_cmp_name(h2esq_cookie_t * a, h2esq_cookie_t * b)	{
	if (a->name_hash != b->name_hash)	{
		return a->name_hash - b->name_hash;
	}

	if (a->name_len != b->name_len)	{
		return a->name_len - b->name_len;
	}

	return strcmp(a->name, b->name);
}

RB_HEAD(H2ESQ_COOKIE_BY_NAME, _h2esq_cookie_t);
RB_PROTOTYPE_STATIC(H2ESQ_COOKIE_BY_NAME, _h2esq_cookie_t, rb_by_name, h2esq_rbtree_cmp_name);
RB_GENERATE_STATIC(H2ESQ_COOKIE_BY_NAME, _h2esq_cookie_t, rb_by_name, h2esq_rbtree_cmp_name);

STATIC int
h2esq_rbtree_cmp_idx(h2esq_cookie_t * a, h2esq_cookie_t * b)	{
	return a->idx - b->idx;
}

RB_HEAD(H2ESQ_COOKIE_BY_IDX, _h2esq_cookie_t);
RB_PROTOTYPE_STATIC(H2ESQ_COOKIE_BY_IDX, _h2esq_cookie_t, rb_by_idx, h2esq_rbtree_cmp_idx);
RB_GENERATE_STATIC(H2ESQ_COOKIE_BY_IDX, _h2esq_cookie_t, rb_by_idx, h2esq_rbtree_cmp_idx);

/* return 0 for success
 * hash: a loose hash value of cookie name
 * namelen: length of the cookie name, value of strlen()
 */
STATIC int
h2esq_get_cookie_string_features(const char * str, int len, unsigned long * hash, int * namelen)	{
	unsigned char h0;
	unsigned char h1;
	unsigned char h2;
	unsigned char h3;
	int i;

	if (len <=0 || len >= COOKIE_STR_MAX)	{
		return -1;
	}

	h0 = 0;
	h1 = 0;
	h2 = 0;
	h3 = 0;

	for (i = 0; i < len; i ++)	{
		if ('=' == str[i])	{
			if (i)	{
				*hash = h0 + (h1 << 8) + (h2 << 16) + (h3 << 23);
				*namelen = i;
				return 0;
			}
			else	{
				return -1;
			}
		}
		else	{
			if ((i & 1))	{
				h0 += (unsigned char)str[i];
			}
			if ((i & 3))	{
				h1 += (unsigned char)str[i];
			}
			if ((i & 2))	{
				h2 += (unsigned char)str[i];
			}
			if ((i & 6))	{
				h3 += (unsigned char)str[i];
			}
		}
	}

	*hash = h0 + (h1 << 8) + (h2 << 16) + (h3 << 23);
	*namelen = i;

	return -1;
}

/* return 0 for success
 * hash: a loose hash value of cookie name
 * namelen: length of the cookie name, value of strlen()
 */
STATIC int
h2esq_get_cookie_string_hash(const char * str, int len, unsigned long * hash)	{
	unsigned char h0;
	unsigned char h1;
	unsigned char h2;
	unsigned char h3;
	int i;

	h0 = 0;
	h1 = 0;
	h2 = 0;
	h3 = 0;

	for (i = 0; i < len; i ++)	{
		if ((i & 1))	{
			h0 += (unsigned char)str[i];
		}
		if ((i & 3))	{
			h1 += (unsigned char)str[i];
		}
		if ((i & 2))	{
			h2 += (unsigned char)str[i];
		}
		if ((i & 6))	{
			h3 += (unsigned char)str[i];
		}
	}

	*hash = h0 + (h1 << 8) + (h2 << 16) + (h3 << 23);

	return 0;
}

int
h2esq_save_cookie(void * cookies_rb_roots_ptr, const char * str, int len)	{

	h2esq_cookies_rb_roots * cookies_rb_roots;
	h2esq_cookie_t * p;
	h2esq_cookie_t * max;
	void * root_by_name;
	void * root_by_idx;
	int r;

	unsigned long hash;
	int name_len;
	int value_len;

	cookies_rb_roots = cookies_rb_roots_ptr;
	root_by_name = &cookies_rb_roots->root_by_name;
	root_by_idx = &cookies_rb_roots->root_by_idx;

	r = h2esq_get_cookie_string_features(str, len, &hash, &name_len);
	if (r)	{
		return -1;
	}

	value_len = len - name_len - 1;

	p = malloc(name_len + value_len + 2 + sizeof(h2esq_cookie_t));
	if (!p)	{
		return -1;
	}

	p->idx = 0;
	p->name = p->buf;
	p->name_len = name_len;
	p->name_hash = hash;
	p->value = p->buf + name_len + 1;
	p->value_len = value_len;

	memcpy(p->buf, str, name_len);
	memcpy(p->buf + name_len + 1, str + name_len + 1, value_len);
	p->buf[name_len] = '\0';
	p->buf[name_len + 1 + value_len] = '\0';

	#if 0
	printf("DBG: %s() %d: name_len = %d, value_len = %d, name = %s, value = %s\n",
			__func__, __LINE__,
			p->name_len,
			p->value_len,
			p->name,
			p->value
			);
	#endif

	if (H2ESQ_COOKIE_BY_NAME_RB_INSERT(root_by_name, p))	{
		//printf("DBG: %s() %d: duplicated cookies, drap\n", __func__, __LINE__);
		free(p);
		return 0;
	}

	max = RB_MAX(H2ESQ_COOKIE_BY_NAME, root_by_idx);
	if (max)	{
		p->idx = max->idx + 1;
		H2ESQ_COOKIE_BY_IDX_RB_INSERT(root_by_idx, p);
		//printf("DBG: %s() %d: max->idx = %d, insert as %d\n", __func__, __LINE__, max->idx, p->idx);
	}
	else	{
		p->idx = 0;
		H2ESQ_COOKIE_BY_IDX_RB_INSERT(root_by_idx, p);
		//printf("DBG: %s() %d: there is no max node, insert as %d\n", __func__, __LINE__, p->idx);
	}

	return 0;
}

const char *
h2esq_get_cookie_value_by_name(void * cookies_rb_roots_ptr, const char * name)	{
	h2esq_cookies_rb_roots * cookies_rb_roots;
	void * root_by_name;
	h2esq_cookie_t f;
	h2esq_cookie_t * p;
	int name_len;
	unsigned long name_hash;

	cookies_rb_roots = cookies_rb_roots_ptr;
	root_by_name = &cookies_rb_roots->root_by_name;

	name_len = strnlen(name, COOKIE_NAME_MAX);
	if (name_len <= 0 || name_len >= COOKIE_NAME_MAX)	{
		return NULL;
	}

	h2esq_get_cookie_string_hash(name, name_len, &name_hash);

	f.name = name;
	f.name_len = name_len;
	f.name_hash = name_hash;

	p = H2ESQ_COOKIE_BY_NAME_RB_FIND(root_by_name, &f);
	if (p)	{
		return p->value;
	}

	return NULL;
}

void
h2esq_get_cookie_value_by_idx(void * cookies_rb_roots_ptr, int idx, const char ** name, const char ** value)	{
	h2esq_cookies_rb_roots * cookies_rb_roots;
	void * root_by_idx;
	h2esq_cookie_t f;
	h2esq_cookie_t * p;

	cookies_rb_roots = cookies_rb_roots_ptr;
	root_by_idx = &cookies_rb_roots->root_by_idx;

	f.idx = idx;

	p = H2ESQ_COOKIE_BY_IDX_RB_FIND(root_by_idx, &f);
	if (p)	{
		if (name)	{
			*name = p->name;
		}
		if (value)	{
			*value = p->value;
		}
	}
	else	{
		if (name)	{
			*name = NULL;
		}
		if (value)	{
			*value = NULL;
		}
	}
}

void
h2esq_free_cookies(void * cookies_root)	{
	h2esq_cookie_t * p;

	for (p = RB_MIN(H2ESQ_COOKIE_BY_NAME, cookies_root)
			; p
			; p = RB_MIN(H2ESQ_COOKIE_BY_NAME, cookies_root)
			)
	{
		H2ESQ_COOKIE_BY_NAME_RB_REMOVE(cookies_root, p);
		free(p);
	}

	return;
}

/* eof */
