#include <stdio.h>
#include <pthread.h>

#include "public/esq.h"
#include "internal/lock.h"

static pthread_spinlock_t esq_spinlock;

void
esq_global_lock(void)	{
	pthread_spin_lock(&esq_spinlock);
	//printf("spin locked\n");
	//esq_backtrace();
}

void
esq_global_unlock(void)	{
	pthread_spin_unlock(&esq_spinlock);
	//printf("spin unlocked\n");
}

int
esq_global_lock_init(void)	{
	int r;

	r = pthread_spin_init(&esq_spinlock, 0);
	if (r)	{
		fprintf(stderr, "error: %s, %d, %s(): %s\n", __FILE__, __LINE__, __func__, "failed to initialize spinlock");
		return ESQ_ERROR_LIBCALL;
	}
	else	{
		return ESQ_OK;
	}
}

void
esq_global_lock_detach(void)	{
	pthread_spin_destroy(&esq_spinlock);
}

/* eof */
