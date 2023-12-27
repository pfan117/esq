#ifndef __ESQ_MKC_HEADER_INCLUDED__
#define __ESQ_MKC_HEADER_INCLUDED__

extern int esq_mkc_execute(const char * code, int codelen, char * buf, int * buflen);
extern int esq_mkc_init(void);
extern void esq_mkc_detach(void);

#endif /* __ESQ_MKC_HEADER_INCLUDED__ */
