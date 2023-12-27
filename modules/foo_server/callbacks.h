#ifndef __H2ESQ_MOD_FOO_CALLBACKS_HEADER_INCLUDED__
#define __H2ESQ_MOD_FOO_CALLBACKS_HEADER_INCLUDED__

extern void foo_pony_query_echo_test(void *);
extern void foo_pony_query_monkey_bsc(void *);

extern void foo_mco_basic_test(void *);

extern void foo_disk_db_bsc_test(void *);
extern void foo_disk_db_burst_set_test(void *);
extern void foo_disk_db_burst_del_test(void *);
extern void foo_disk_db_burst_get_test(void *);

extern void foo_mem_db_bsc_test(void *);
extern void foo_mem_db_burst_set_test(void *);
extern void foo_mem_db_burst_del_test(void *);
extern void foo_mem_db_burst_get_test(void *);

extern void foo_db_bsc_test(const char * rmm);
extern void foo_db_burst_set_test(const char * rmm);
extern void foo_db_burst_del_test(const char * rmm);
extern void foo_db_burst_get_test(const char * rmm);

#endif
