#ifndef __ESQ_CORE_LOCK_HEADER_INCLUDED__
#define __ESQ_CORE_LOCK_HEADER_INCLUDED__

extern void esq_global_lock(void);
extern void esq_global_unlock(void);
extern int esq_global_lock_init(void);
extern void esq_global_lock_detach(void);

#endif /* __ESQ_CORE_LOCK_HEADER_INCLUDED__ */
