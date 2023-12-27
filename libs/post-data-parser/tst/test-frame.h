#ifndef __POST_DATA_PARSER_TEST_FRAMEWORK__
#define __POST_DATA_PARSER_TEST_FRAMEWORK__

#define STATIC static

typedef struct _pdp_test_frames_t	{
	int cnt;
	char * frames[];
} pdp_frames_t;

typedef struct _pdp_item_des_t	{
	int type;
	int length;
} pdp_item_des_t;

typedef struct _pdp_items_t	{
	int cnt;
	pdp_item_des_t items[];
} pdp_items_t;

typedef struct _pdp_io_t	{
	pdp_frames_t * frames;
	int * lengths;
	pdp_items_t * items;
} pdp_io_t;

#endif
