#ifndef __H2ESQ_HEADER_INCLUDED__
#define __H2ESQ_HEADER_INCLUDED__

typedef int (*h2esq_service_handler_t)(
		nghttp2_session * session
		, void * h2esq_session
		, void * h2esq_stream
		, void * service_ctx
		);

typedef void (*h2esq_service_on_stream_close_cb_t)(
		void * h2esq_stream
		);

typedef int (*h2esq_on_parse_post_data_cb_t)(void * h2esq_stream, const char * buf, int len);

typedef struct _h2esq_service_class_t	{
	const char * name;
	void (*detach)(void * service_ctx);
	h2esq_service_handler_t handler;
	h2esq_service_on_stream_close_cb_t on_stream_close;
	h2esq_on_parse_post_data_cb_t on_parse_post_data;
} h2esq_service_class_t;

typedef struct _h2esq_service_t	{
	h2esq_service_class_t * service_class;
	void * ctx;
} h2esq_service_t;

extern int h2esq_stream_get_id(void * h2esq_stream_ptr);
extern char * h2esq_stream_get_header(void * h2esq_stream, int header_idx, int * header_len);
extern void * h2esq_stream_get_service_app_ctx(void * h2esq_stream_ptr);

extern int h2esq_stream_set_app_ctx(void * h2esq_stream_ptr, void * app_stream_ctx_ptr);
extern void * h2esq_stream_get_app_ctx(void * h2esq_stream_ptr);
extern void h2esq_stream_clear_app_ctx(void * h2esq_stream_ptr);

extern int h2esq_stream_get_path_matched_length(void * h2esq_stream_ptr);

enum	{
	H2ESQ_HEADER_METHOD = 0,
	H2ESQ_HEADER_PATH,
	H2ESQ_HEADER_AUTHORITY,
	H2ESQ_HEADER_USER_AGENT,
	H2ESQ_HEADER_ACCEPT,
	H2ESQ_HEADER_REFERER,
	H2ESQ_HEADER_ACCEPT_ENCODING,
	H2ESQ_HEADER_ACCEPT_LANGUAGE,
	H2ESQ_HEADER_CACHE_CONTROL,
	H2ESQ_HEADER_HOST,
	H2ESQ_HEADER_MAX
};

extern int h2esq_instance_bind_service(const char * instance_name, const char * path, h2esq_service_class_t * service_class, void * service_ctx);

extern void * h2esq_stream_get_cookie_root(void * h2esq_stream_ptr);
extern const char * h2esq_get_cookie_value_by_name(void * cookies_rb_roots_ptr, const char * name);
extern void h2esq_get_cookie_value_by_idx(void * cookies_rb_roots_ptr, int idx, const char ** name, const char ** value);

#endif
