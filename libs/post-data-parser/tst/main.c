#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include "public/libesq.h"
#include "test-frame.h"

#include "frames/chrome-empty-form0.h"
#include "frames/edge-empty-form0.h"
#include "frames/ff-empty-form0.h"

#include "frames/chrome-no-file-form0.h"
#include "frames/edge-no-file-form0.h"
#include "frames/ff-no-file-form0.h"

#include "frames/chrome-small-file-form0.h"
#include "frames/edge-small-file-form0.h"
#include "frames/ff-small-file-form0.h"

#include "frames/chrome-big-file-form0.h"
#include "frames/edge-big-file-form0.h"
#include "frames/ff-big-file-form0.h"

#include "frames/ff-radio-button-form1.h"

#include "frames/ff-checkbox-select-nothing-form2.h"

#include "frames/ff-checkbox-select-all-form2.h"

#include "frames/chrome-dll-upload.h"

pdp_io_t test_pattens[] = {
	/* form0 with no value */
	{ &chrome_empty_form0_frames, NULL, &chrome_empty_form0_items, },
	{ &frame_20191025_155425_00, NULL, &frame_20191025_155425_00_items, },
	{ &frame_20191025_155123_00, NULL, &frame_20191025_155123_00_items, },

	/* form0 with no file */
	{ &frame_20191025_155525_01, NULL, &frame_20191025_155525_01_items, },
	{ &frame_20191025_155527_00, NULL, &frame_20191025_155527_00_items, },
	{ &frame_20191025_155526_00, NULL, &frame_20191025_155526_00_items, },

	/* form0 with small file */
	{ &frame_20191025_1555528_00_frames, NULL, &frame_20191025_1555528_00_items, },
	{ &frame_20191025_155825_00_frames, NULL, &frame_20191025_155825_00_items, },
	{ &frame_20191025_155700_00_frames, NULL, &frame_20191025_155700_00_items, },

	/* form0 with big file */
	{ &frame_20191025_160243_00_frames, NULL, &frame_20191025_160243_00_items, },
	{ &frame_20191025_160449_00_frames, NULL, &frame_20191025_160449_00_items, },
	{ &frame_20191025_160355_00_frames, NULL, &frame_20191025_160355_00_items, },

	/* form1 with radio button */
	{ &frame_20191103_061945_00_frames, NULL, &frame_20191103_061945_00_items, },

	/* form2 with no checkbox selected */
	{ &frame_20191103_checkbox_nothing_frames, NULL, &frame_20191103_checkbox_nothing_items, },

	/* form2 with all checkboxs selected */
	{ &frame_20191103_all_checkbox_frames, NULL, &frame_20191103_all_checkbox_items, },

	/* post a dll file from chrome */
	{ &frame_20191108_160355_00_frames, frame_20191108_160355_00_length, &frame_20191108_160355_00_items, },
};

typedef struct _tc_ctx_t	{
	int idx;
	pdp_items_t * items;
} tc_ctx_t;

tc_ctx_t app_ctx;

int
test_cb(void * ctx_ptr, int element_type, const char * buf, int len)	{
	tc_ctx_t * ctx = ctx_ptr;
	pdp_item_des_t * item;

	if (ctx->idx >= ctx->items->cnt)	{
		printf("ERROR: too many elements, item index = %d\n", ctx->idx);
		goto __exx;
	}

	item = ctx->items->items + ctx->idx;

	if (item->type != element_type)	{
		printf("ERROR: idx = %d, element_type = %d, expected %d\n", ctx->idx, element_type, item->type);
		goto __exx;
	}

	#if 0
	if (POST_ET_NAME == element_type)	{
		hex_dump(buf, len);
	}
	#endif

	#if 0
	if (POST_ET_FILENAME == element_type)	{
		hex_dump(buf, len);
	}
	#endif

	#if 0
	if (POST_ET_CONTENT_TYPE == element_type)	{
		hex_dump(buf, len);
	}
	#endif

	if (item->length != len)	{
		printf("ERROR: idx = %d, len = %d, expected %d\n", ctx->idx, len, item->length);
		goto __exx;
	}

	ctx->idx ++;

	return 0;

__exx:

	if (buf && len)	{
		printf("get buf x len = \n");
		hex_dump(buf, len);
	}

	return -1;
}

int
tc_enum(pdp_io_t * p)	{
	int r;
	int i;
	pdp_frames_t * frames;
	post_data_ctx_t ctx;

	app_ctx.idx = 0;
	app_ctx.items = p->items;
	frames = p->frames;
	bzero(&ctx, sizeof(ctx));

	for (i = 0; i < frames->cnt; i ++)	{
		if (p->lengths)	{
			r = post_data_parser(
					frames->frames[i]
					, p->lengths[i]
					, test_cb
					, &ctx
					, &app_ctx
					);
		}
		else	{
			r = post_data_parser(
					frames->frames[i]
					, strlen(frames->frames[i])
					, test_cb
					, &ctx
					, &app_ctx
					);
		}
		if (r)	{
			printf("ERROR: parser return -1, syntax error\n");
			return -1;
		}
	}

	if (app_ctx.idx != app_ctx.items->cnt)	{
		printf("ERROR: some expected items are missed\n");
		return -r;
	}

	return 0;
}

int
main(void)	{

	int r;
	int i;

	for (i = 0; i < (sizeof(test_pattens) / sizeof(test_pattens[0])); i ++)	{
		printf("TC %d:\n", i);
		r = tc_enum(test_pattens + i);
		if (r)	{
			printf("FAILED\n");
			return -1;
		}
	}

	printf("OK\n");

	return 0;
}


/* eof */
