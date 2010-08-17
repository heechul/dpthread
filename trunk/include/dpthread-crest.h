#ifndef _DPTHREAD_H_
#define _DPTHREAD_H_

#include <sys/types.h>
#include <pthread.h>
#include <stdint.h>
#include <queue.h>

#define USE_DPTHREAD 1 

#define MAX_THR  128

typedef struct {
	int id;
	pthread_mutex_t mutex;
        int64_t released_logical_time; 
	int last_owner; 
	TQueue queue; 
	// TODO: queue 
} det_mutex_t; 


typedef struct {
	int id; 
	TQueue queue; 
	det_mutex_t waiter[MAX_THR]; 
} det_cond_t; 

typedef struct {
	int id; 
	int target_count;
	int wait_count; 
	det_cond_t wait_cond; 
	det_mutex_t wait_mutex; 
} det_barrier_t; 


///////////////////////////////////////////////////////////////////////////////////
// dpthread core API    
///////////////////////////////////////////////////////////////////////////////////
#ifdef __cplusplus
extern "C" {
#endif

// initialize 
int  det_init(int argc, char *argv[]); 

// thread create/join/exit/gettid

int  det_create(pthread_t *thread, const pthread_attr_t *attr,
                void *(*start_routine)(void*), void *arg) __attribute((crest_skip));
int  det_join ( pthread_t threadid, void **thread_return ) __attribute((crest_skip));
void det_exit(void *value_ptr) __attribute((crest_skip));
int  det_get_pid(void) __attribute((crest_skip));

// synchronizations: lock, condition variable, barrier 
int  det_lock_init(det_mutex_t *mutex) __attribute((crest_skip));
int  det_lock(det_mutex_t *mutex) __attribute((crest_skip));
int  det_unlock(det_mutex_t *mutex) __attribute((crest_skip));

int  det_barrier_init(det_barrier_t *barrier, int count) __attribute((crest_skip));
int  det_barrier_wait(det_barrier_t *barrier) __attribute((crest_skip));

int  det_cond_init(det_cond_t *cond) __attribute((crest_skip));
int  det_cond_wait(det_cond_t *cond, det_mutex_t *mutex) __attribute((crest_skip));
int  det_cond_signal(det_cond_t *cond) __attribute((crest_skip));
int  det_cond_broadcast(det_cond_t *cond) __attribute((crest_skip));

// enable/disable determinism manually
void det_enable(void); 
void det_disable(void); 
int  det_is_enabled(); 

// deterministic clock 
int64_t det_get_clock();

// clock enable/disable interface 
int det_enable_logical_clock();
int det_disable_logical_clock();
int64_t det_increase_logical_clock(int64_t incr);
void det_add_logical_clock(int64_t incr); 

// utility functions. 
void det_set_debug(int level);
int  det_dbg(const char *format, ...);
void det_print_stat();

// misc 
unsigned int get_usecs(); 

#include "dpthread-io.h" 

#ifdef __cplusplus
}
#endif


// pid_t gettid(void);
#endif /* _DPTHREAD_H_ */ 
