#ifndef __H2ESQ_COOKIE_HEADER_INCLDUED__
#define __H2ESQ_COOKIE_HEADER_INCLDUED__

#define COOKIE_STR_MAX	4095
#define COOKIE_NAME_MAX	1024

extern int h2esq_save_cookie(void * cookie_root, const char * str, int len);
extern void h2esq_free_cookies(void * cookies_root);

#endif
