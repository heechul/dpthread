#ifndef _DPTHREAD_DEPS_H_
#define _DPTHREAD_DEPS_H_

#include <pthread.h> 
#include <malloc.h>  // valloc 
#include <stdio.h>   // printf 

#define LOCK    ops.__pthread_mutex_lock
#define UNLOCK  ops.__pthread_mutex_unlock
#define CWAIT   ops.__pthread_cond_wait 
#define CSGNL ops.__pthread_cond_broadcast

/* all functions that dpthread runtime internally use */ 
struct dpthread_dep_ops
{
	/* synchronization operations */ 
	int (*__pthread_create)(pthread_t *thread, const pthread_attr_t *attr,
				void *(*start_routine)(void*), void *arg); 
	int (*__pthread_join)(pthread_t thread, void **value_ptr);
	void (*__pthread_exit)(void *value_ptr);

	int (*__pthread_mutex_init)(pthread_mutex_t *mutex, 
				    const pthread_mutexattr_t *attr);
	int (*__pthread_mutex_lock)(pthread_mutex_t *mutex);
	int (*__pthread_mutex_trylock)(pthread_mutex_t *mutex);
	int (*__pthread_mutex_unlock)(pthread_mutex_t *mutex);

	int (*__pthread_barrier_init)(pthread_barrier_t *barrier,
				      const pthread_barrierattr_t *attr, unsigned count); 
	int (*__pthread_barrier_wait)(pthread_barrier_t *barrier);

	int (*__pthread_cond_init)(pthread_cond_t *cond,
				   const pthread_condattr_t *attr);
	int (*__pthread_cond_wait)(pthread_cond_t *cond, pthread_mutex_t *mutex);
	int (*__pthread_cond_signal)(pthread_cond_t *cond);
	int (*__pthread_cond_broadcast)(pthread_cond_t *cond);
        int (*__pthread_cancel)(pthread_t thread);
}; 

int det_init_ops(); 

#endif /* _DPTHREAD_DEPS_H_ */ 
