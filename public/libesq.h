#ifndef __ESQ_COMMON_LIB_HEADER_INCLUDED__
#define __ESQ_COMMON_LIB_HEADER_INCLUDED__

#define ESQ_ARRAY_SIZE(__ar)	(sizeof(__ar) / sizeof(__ar[0]))
#define ESQ_ARRAY_BZERO(__ar)	(bzero((__ar), sizeof(__ar)))
#define esq_malloc(__type)		(__type *)malloc(sizeof(__type))
#define esq_design_error()	\
		printf("ERROR: %s %d: ## design error here ##\n", __func__, __LINE__); \
		esq_backtrace(); \
		printf("quit\n\n"); \
		exit(-1);
#define ESQ_HERE		printf("%s %d: %s(): >> here <<\n", __FILE__, __LINE__, __func__);

#define ESQ_IF_ERROR_RETURN(__op)	do	{	\
	if (__op < 0)	{	\
		return -1;	\
	}	\
}while(0);

#define SAY_NOT_SUPPOSE_TO_BE_HERE printf("ERROR: %s() %d: not suppose to be here\n", __func__, __LINE__)

#define NOT_SUPPOSE_TO_BE_HERE printf("ERROR: %s() %d: not suppose to be here\n", __func__, __LINE__); esq_design_error();

/* return -1 for error */
extern int escape_percent_in_place(char * buf, int l);

extern int find_last_slash(const char *b);
extern int find_last_slash2(const char * b, int len);

/* hmt */
enum	{ HMT_SCALE_16, HMT_SCALE_32, HMT_SCALE_64, HMT_SCALE_128, HMT_SCALE_512 };
extern void * hmt_new(int max);
extern void * hmt_new2(int max, int scale);
extern int hmt_add(void * ppt, const char * pattern, void * userdata);
extern int hmt_add2(void * ppt, const char * pattern, void * userdata);
extern int hmt_compile(void * ppt);
extern void * hmt_match(void * ppt, const char * string, int * matched_length);
extern void hmt_free(void * ppt);
extern void hmt_dump(void * ppt);

extern void hex_dump(const char * b, int l);

#ifdef _ESQ_LIB_SOCKET_FUNCTIONS
extern int os_fast_open_sendto_nb(int fd, struct sockaddr * peer, int addrlen, const char * buf, int len);
extern int os_open_tcp_socket(int blocking, int seconds);
extern int os_ip_address_is_me(struct sockaddr_in * sin);
#endif

extern int os_get_if_list(void);
extern void os_free_if_list(void);
extern int os_socket_set_timeout(int sd, int second);
extern int os_socket_set_non_blocking(int fd);
extern int os_read_nb(int fd, char * buf, int len);
extern int os_write_nb(int fd, const char * buf, int len);
extern int os_write_b(int fd, const char * buf, int len);
extern int os_get_current_core_id_slow(void);

#define SAFE_PATH_MAX	2048
#define SAFE_FILENAME_MAX	255
extern int path_is_very_safe(const char * path);

/* post element types */
enum	{
	POST_ET_VOID = 0,
	POST_ET_ERROR,
	POST_ET_EMPTY_LINE,
	POST_ET_BOUNDARY,
	POST_ET_CONTENT_FORM_DATA,
	POST_ET_FINISH,
	POST_ET_NAME,
	POST_ET_FILENAME,
	POST_ET_VALUE,
	POST_ET_CONTENT_TYPE,	/* e.g. "text/plain", "text/html", "application/octet-stream"
							 * , "application/gzip", "image/jpeg", "image/png" */
};

#define PDP_BOUNDARY_MAX	256
typedef struct _post_data_ctx_t	{
	int last_item_type;
	char boundary[PDP_BOUNDARY_MAX];
	int boundary_len;
} post_data_ctx_t;

typedef int (*post_data_parser_app_cb)(void * app_ctx, int element_type, const char * buf, int len);
extern int post_data_parser(const char * buf, int len, post_data_parser_app_cb cb, post_data_ctx_t * ctx, void * app_ctx);
extern int remove_redundancy_slash(char * b);

extern int get_ipv4_addr(const char * buf, int len, unsigned long * ap);
extern int get_ipv4_addr2(const char * buf, unsigned long * ap);
extern int match_ipv4_addr(const char * buf, int len);
extern int get_integer_1000000(const char * buf, int len, int * v);
extern int get_hash_string(const char * buf, int len, int * hash_start, int * hash_len);
extern int hash_str_2_bin(unsigned char * buf, const char * hash_string, int hash_str_len);

/* return value <= 0 means error */
extern int get_code_from_utf8(const char * buf, unsigned int * codep);

extern int esq_master_thread_mask_signals(void);
extern int esq_worker_thread_mask_signals(void);

/* path bloom filter (failure rate 43 / 1000 -_-b) */
#define ESQ_PATH_BLOOM_MAX	1024
extern void * path_bloom_filter_new(void);
extern void path_bloom_filter_free(void * ptr);
extern int path_bloom_filter_add_path(void * ptr, const char * path);
extern int path_bloom_filter_add_path2(void * ptr, const char * path, int path_length);
/* return 0 for not exist */
/* return 1 for exist */
extern int path_bloom_filter_check_exist(void * ptr, const char * path);
extern int path_bloom_filter_check_exist2(void * ptr, const char * path, int path_length);

/* file enumerators */
#define ESQ_FILE_PATH_MAX	1024
/* return 0 to continue, return 1 for stop */
typedef int (*esq_file_enumerators_cb_t)(const char * path_name, int length, void * param);
extern int esq_file_enumerators(const char * root, esq_file_enumerators_cb_t cb, void * cb_param);

#define PORT_NUMBER_INVALID(__PORT__)	((__PORT__) < 0 || (__PORT__) > 65535)

/* URL parameters spliter */
typedef struct _esq_url_parameter_t	{
	char * name;
	char * value;
	int name_len;
	int value_len;
} esq_url_parameter_t;
/* return -1 for error */
/* return count of parameters that recognized */
/* ?a=xxx?b=yyy?c=zzz */
extern int esq_split_parameters(char * url_ptr, int url_len, esq_url_parameter_t * param, int param_max);
extern void esq_url_params_print(esq_url_parameter_t * param, int param_max);
extern int esq_url_parames_escape_in_place(esq_url_parameter_t * param, int param_max);

/* file LRU */
extern void * file_lru_borrow(const char * file_name);
extern int file_lru_get_data(void * file_lru_ptr, const char ** data_ptr, int * length);
extern void file_lru_return(void * file_lru_ptr);

#endif
