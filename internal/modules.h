#ifndef __ESQ_MODULES_HEADER_INCLEDED__
#define __ESQ_MODULES_HEADER_INCLEDED__

extern void esq_trigger_mt(void);
extern int esq_modules_init(void);
extern void esq_modules_detach(void);
extern int esq_modules_load(const char * filename);

#endif /* __ESQ_MODULES_HEADER_INCLEDED__ */
