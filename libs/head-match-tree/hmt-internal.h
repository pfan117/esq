#ifndef __HMT_INTERNAL_HEADER_INCLUDED__
#define __HMT_INTERNAL_HEADER_INCLUDED__

#define HMT_ITEM_MAX			111	/* using char as index, so... */
#define HMT_PATH_LEN_MAX		(pt->pattern_len_max)
#define HMT_PATH_LEN_MAX_SHIFT	(pt->pattern_len_max_shift)

typedef struct _hmt_item_t	{
	const char * pattern;
	void * userdata;
	void * malloc_item;
} hmt_item_t;

typedef struct _hmt_t	{
	int max;
	int cnt;
	char enter_point;
	char * higher_jump_table;
	char * lower_jump_table;
	char * someone_else_matched_table;
	char * next_level_enter_point_table;
	int * patterns_length;
	int pattern_len_max;
	int pattern_len_max_shift;
	hmt_item_t items[];
} hmt_t;

extern void hmt_free_content(hmt_t * pt);

#endif

/* eof */
