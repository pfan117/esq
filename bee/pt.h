#ifndef __ESQ_BEE_PROTO_THREAD_HEADER_INCLUDED__
#define __ESQ_BEE_PROTO_THREAD_HEADER_INCLUDED__

#define PT_BEGIN	switch(ctx->lc) { case 0: ;
#define PT_SET		case __LINE__: ctx->lc = __LINE__;
#define PT_END	default: return; }

#define PT_RESET(__PT__)	(__PT__->lc = 0);

#define check_sub_pt_result \
	if (BDB_OK == r)	{ \
		;	/* fine */ \
	} \
	else if (BDB_PT_IO_REQ == r)	{ \
		return r;	/* yield */ \
	} \
	else	{ \
		ctx->pt_return_value = r; \
		PT_SET;	/* quit */ \
		return ctx->pt_return_value; \
	}

#define yield_to_io_and_check_result(__IO_OPT__) \
	req->io_type = __IO_OPT__; \
	PT_SET \
	if (__IO_OPT__ == req->io_type)	{ \
		return BDB_PT_IO_REQ;	/* yield */ \
	} \
	if (BDB_OK == req->io_handler_return_value)	{ \
		;	/* go on */ \
	} \
	else	{ \
		ctx->pt_return_value = req->io_handler_return_value; \
		return ctx->pt_return_value;	/* quit */ \
	}

#endif
