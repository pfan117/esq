#ifndef __PONY_BEE_HEAD_BLOCK_DEFINES_INCLUDED__
#define __PONY_BEE_HEAD_BLOCK_DEFINES_INCLUDED__

/* opcode */
#define BEE_OP_TYPE_MASK		0xF
#define BEE_OP_DB_SET			0x1
#define BEE_OP_DB_GET			0x2
#define BEE_OP_DB_DEL			0x3
#define BEE_OP_FILE_WRITE		0x4
#define BEE_OP_FILE_READ		0x5
#define BEE_OP_FILE_DEL			0x6
#define BEE_OP_SCRIPT			0x7
#define BEE_OP_INTERNAL_SET		0x8
#define BEE_OP_INTERNAL_DEL		0x9
#define BEE_OP_SYNC				0xa
#define BEE_OP_MONKEY			0xb

#define BEE_OP_MOD_MASK             0xF0
#define BEE_OP_MOD_WAIT_FINISH      0x10
#define BEE_OP_MOD_WAIT_SYNC_FINISH 0x20

extern int pony(
		const char * range_map_name
		, char * key, int key_len
		, char * value, int value_len
		, int opcode
		, char * result_buf
		, int * result_buf_size
		);

extern void * pony_monkey_ctx_new(int values_max);
extern void pony_monkey_ctx_free(void * ctx);
extern int pony_monkey_ctx_set_script2(void * ctx, char * script, int length);
extern int pony_monkey_ctx_set_script(void * ctx, char * script);
extern int pony_monkey_ctx_add_int_param(void * ctx, char * name, int value);
extern int pony_monkey_ctx_add_buf_param(void * ctx, char * name, void * buf, int buf_len);
/* the str needs a terminating zero */
extern int pony_monkey_ctx_add_str_param(void * ctx, char * name, char * str, int str_len);
extern int pony_monkey(const char * range_map_name, void * ctx, char * result_buf, int * result_buf_size);

#endif
