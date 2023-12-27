#ifndef __H2ESQ_INSTANCE_HEADER_INCLDUED__
#define __H2ESQ_INSTANCE_HEADER_INCLDUED__

#define H2ESQ_PATH_HANDLER_MAX	100
#define H2ESQ_SERVER_INSTANCE_NAME_MAX	64
#define H2ESQ_INSTANCE_VAR_MAX	100
#define H2ESQ_INSTANCE_SERVICE_MAX	16

typedef struct _h2esq_instance_t	{
	char name[H2ESQ_SERVER_INSTANCE_NAME_MAX];
	void * esq_session;
	void * ssl_ctx;
	//int flags;
	int port;
	int sd;
	void * path_hmt;
	void * var_hmt;
	h2esq_service_t service[H2ESQ_INSTANCE_SERVICE_MAX];
} h2esq_instance_t;
extern h2esq_service_t * h2esq_instance_find_service(h2esq_instance_t * instance, const char * path, int * matched_length);
extern int h2esq_instance_init(void);
extern void h2esq_instance_detach(void);
extern void * h2esq_instance_get_ssl_ctx(void * instance);

#endif
