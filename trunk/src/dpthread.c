/**
 * Deterministic threading runtime 
 *
 * Author: Heechul Yun <heechul@illinois.edu> 
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 */ 

#include <sys/time.h>
#include <sys/types.h>
#include <limits.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h> 
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/mman.h>
#include <poll.h>
#include <sched.h>
#include <assert.h>
#include <stdarg.h>
#include <sched.h>
#include <perfmon/pfmlib_perf_event.h>
#include "perf_util.h"

#include <dpthread.h>
#include <dpthread_deps.h> 
#include <dlfcn.h>
#include <atomic.h>

#include "config.h"

////////////////////////////////////////////////////////////////////////////////
// definition 
////////////////////////////////////////////////////////////////////////////////
#define USE_NESTED_LOCK  1  // 
#define USE_PERF_COUNTER 1  // 0 - read_count() always return 0. 
#define USE_TIMING       0  // measure timing 
#define USE_FAKE_DISABLE 0  // not using ioc_enable/disable, but read_count
#define USE_INST_COUNT   0  // use 'inst_retired-intr-pagefault' - not working 

#define PROFILE_KERNEL_EVENTS 0 

static const int64_t MAX_LOGICAL_CLOCK=20000000000000LL;

#define SET_CLOCK(id, clock) {\
	__sync_synchronize(); \
	wa[id].sw_clock = (clock) - wa[id].hw_clock; \
	__sync_synchronize(); }
#define GET_CLOCK(id) (wa[id].sw_clock + wa[id].hw_clock)

////////////////////////////////////////////////////////////////////////////////
// global shared data 
////////////////////////////////////////////////////////////////////////////////
static int __thread myid = 0;

static int num_processors = 0; // set in det_init() 
static int debug_level = 0; 
static char *debug_log_file = NULL; 

struct worker_args {
	// worker function and arg 
	void *(*func)(void*);
	void *arg; 

	// id 
	int  id; 
	pthread_t tid; 

	// performance counter handles 
	perf_event_desc_t *fds;

	// clock (=performance counter)
	volatile int64_t hw_clock; // hw clock reading 
	volatile int64_t sw_clock; // logical clock incremented by runtime (not by hw)
	volatile int hw_clock_enabled; // performance counter enabled 

	// thread control at fork/join 
	det_mutex_t thread_lock; 
	det_cond_t  thread_cond; 

	// thread status 
	volatile int finished; 
	volatile int started; 

	// misc 
	int64_t last_exit_logical_time; 

	// debug 
	FILE *log_file; 
	int nondet_count; // non-deterministic event count 
};

// shared data structure for workers 
static struct worker_args wa[MAX_THR]; 
static volatile uint32_t max_thr = 0; // created thread. 
static volatile uint32_t num_thr = 0; // active threads. 

static volatile int64_t last_sync_logical_time; // update at every sync ops. 
static int64_t __thread my_det_clock; // clock is paused at this 

static int __thread my_det_enabled = 0;   // enabled/disabled 

// TLS for statistics 
static int __thread hw_read;
static int __thread cache_read; 
static int __thread barrier_count; 
static int __thread lock_count; 

// sync counts 
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER; 
static volatile unsigned int g_lock_count = 0; 
static volatile unsigned int g_barr_count = 0; 
static volatile unsigned int g_cond_count = 0; 

// performance
struct perf_mon {
	uint64_t min, max, tot, cnt; /* in CPU cycles. */ 
}; 
static struct perf_mon __thread perf_wait_turn; 
static struct perf_mon __thread perf_logical; 
static struct perf_mon __thread perf_enable; 
static struct perf_mon __thread perf_disable; 

///////////////////////////////////////////////////////////////////////////////////
// utility functions 
///////////////////////////////////////////////////////////////////////////////////
static pthread_mutex_t dbg_mutex = PTHREAD_MUTEX_INITIALIZER; 

static void DBG(int level, char *format, ...)
{
	FILE *out = stderr; 		

	assert( !wa[myid].hw_clock_enabled );

	if ( wa[myid].log_file ) out = wa[myid].log_file;
	if ( level <= debug_level ) {
		va_list ap;
		pthread_mutex_lock(&dbg_mutex); 
#if USE_TIMING
		fprintf(out, "[RT:%08u]", get_usecs()); 
		fprintf(out, "[LT:%08lld](%08lld,%08lld)", 
			GET_CLOCK(myid), 
			wa[myid].hw_clock, wa[myid].sw_clock);
#else 
		fprintf(out, "[LT:%08lld]", GET_CLOCK(myid)); 
#endif 
		fprintf(out, "[%2d]", myid); 
		va_start(ap, format);
		vfprintf(out, format, ap);
		va_end(ap);
		fflush(out);
		pthread_mutex_unlock(&dbg_mutex); 
	}
}

static pid_t gettid(void)
{
	return (pid_t)syscall(__NR_gettid);
}

///////////////////////////////////////////////////////////////////////////////////
// dpthread internal function 
//////////////////////////////////////////////////////////////////////////////////

static unsigned int get_usecs()
{
#if USE_TIMING 
	static struct timeval  base;
	struct timeval         time;
	
	gettimeofday(&time, NULL);
	if (!base.tv_usec) {
		base = time;
	}
	return ((time.tv_sec - base.tv_sec) * 1000000 +
		(time.tv_usec - base.tv_usec));
#else 
	return 0; 
#endif 
}

/**
 * read performance counter data 
 */ 
static uint64_t __read_count(perf_event_desc_t *fds)
{
#if USE_PERF_COUNTER
	int ret; 
	uint64_t values[3]; /* [0] - raw, [1] - time_enabled, [2] - time_running */ 
	ret = read(fds->fd, values, sizeof(values));
	assert(values[1] == values[2]); // owise. incorrect 
	return values[0]; 
#else 
	return 0; 
#endif
}

static uint64_t read_count(perf_event_desc_t *fds)
{
	int64_t count; 
#if USE_INST_COUNT
	count =  __read_count(&fds[0]) - // inst
		__read_count(&fds[1]) - // intr
		__read_count(&fds[2]);  // page fault 
#else /* store count only */ 
	count = __read_count(&fds[0]); 
#endif 
	return count; 
}


static int64_t get_logical_clock(int id)
{
	int64_t ret, hw_clock; 
	if ( myid != id )  __sync_synchronize(); 
	if ( !wa[id].hw_clock_enabled ) {
		hw_clock = wa[id].hw_clock; 
		DBG(4, "clock %d is disabled. cached = %lld\n", id, hw_clock); 
		cache_read ++; 
	} else {
		hw_clock = read_count(wa[id].fds);
		hw_read ++; 
		// DBG(4, "clock %d is enabled. so read from hw = %lld\n", id, hw_clock); 
	}
 
	ret = hw_clock + wa[id].sw_clock; 

	return ret; 
}

static int enable_performance_counter()
{
	if ( wa[myid].fds ) { 
#if USE_INST_COUNT
		ioctl(wa[myid].fds[0].fd, PERF_EVENT_IOC_ENABLE, 0);  	
		ioctl(wa[myid].fds[1].fd, PERF_EVENT_IOC_ENABLE, 0);  	
		ioctl(wa[myid].fds[2].fd, PERF_EVENT_IOC_ENABLE, 0);  	
#else 
		ioctl(wa[myid].fds[0].fd, PERF_EVENT_IOC_ENABLE, 0);  	
#endif 
	}
	return 0; 
}

static int disable_performance_counter()
{
	if ( wa[myid].fds ) { 
#if USE_INST_COUNT
		ioctl(wa[myid].fds[0].fd, PERF_EVENT_IOC_DISABLE, 0);  	
		ioctl(wa[myid].fds[1].fd, PERF_EVENT_IOC_DISABLE, 0);  	
		ioctl(wa[myid].fds[2].fd, PERF_EVENT_IOC_DISABLE, 0);  	
#else 
		ioctl(wa[myid].fds[0].fd, PERF_EVENT_IOC_DISABLE, 0);  	
#endif 
	}
	return 0; 
}


static int enable_logical_clock()
{
#if USE_TIMING
	unsigned start, dur; 
	start = get_usecs(); 
#endif 
	if ( !wa[myid].fds ) return -1; // not initialized 

	__sync_synchronize(); 
	if ( wa[myid].hw_clock_enabled) return -1; // already enabled. 

	DBG(4, "%s: hw_clock = %lld, enabled=%d\n", 
	    __FUNCTION__, wa[myid].hw_clock, wa[myid].hw_clock_enabled); 

#if USE_FAKE_DISABLE
	clock_diff = read_count(wa[myid].fds) - wa[myid].hw_clock; 
	wa[myid].sw_clock -= clock_diff;  // FIXME
	// DBG(1, "enable: diff = %lld\n", clock_diff); 
	wa[myid].hw_clock  += clock_diff; 
	// DBG(1, "enable: curr = %lld\n", GET_CLOCK(myid));
	wa[myid].hw_clock_enabled = 1; 
	__sync_synchronize(); 
#else /* !USE_FAKE_DISABLE */ 
	__sync_synchronize(); 
	wa[myid].hw_clock_enabled = 1; 
	enable_performance_counter();
#endif /* USE_FAKE_DISABLE */ 

#if USE_TIMING 
	dur = get_usecs() - start; 
	perf_enable.tot += dur; 
	perf_enable.min = min(perf_enable.min, dur); 
	perf_enable.max = max(perf_enable.max, dur); 
#endif 
	perf_enable.cnt ++; 

	return 0; 

}

static int disable_logical_clock()
{
#if USE_TIMING 
	unsigned start, dur; 
	start = get_usecs(); 
#endif 
	if ( !wa[myid].fds ) return -1; // not initialized 
	if ( !wa[myid].hw_clock_enabled ) return -1; // already disabled. 

#if USE_FAKE_DISABLE 
	__sync_synchronize(); 
	wa[myid].hw_clock_enabled = 0; 
	__sync_synchronize(); 
	wa[myid].hw_clock = read_count(wa[myid].fds); 
	__sync_synchronize(); 
#else /* !USE_FAKE_DISABLE */ 
	disable_performance_counter(); 
	wa[myid].hw_clock = read_count(wa[myid].fds); 
	__sync_synchronize(); 
	wa[myid].hw_clock_enabled = 0;
	__sync_synchronize(); 
#endif /* USE_FAKE_DISABLE */ 

	DBG(4, "%s: hw_clock = %lld, enabled=%d\n", 
	    __FUNCTION__, wa[myid].hw_clock, wa[myid].hw_clock_enabled); 

#if USE_TIMING
	dur = get_usecs() - start; 
	perf_disable.tot += dur; 
	perf_disable.min = min(perf_disable.min, dur); 
	perf_disable.max = max(perf_disable.max, dur); 
#endif 
	perf_disable.cnt ++; 

	return 0; 
}

/**
 * wait until my logical time is global minima. 
 */
static int64_t wait_for_turn()
{
	int i; 
	int64_t my_clock, other_clock; 
	int nthreads; 

#if USE_TIMING
	unsigned start, dur; 
	start = get_usecs();
#endif 
	if ( max_thr == 0 ) return 0; // nothing 

	assert( !wa[myid].hw_clock_enabled); 

	my_clock = get_logical_clock(myid); 

retry:
	nthreads = max_thr; 
	for ( i = 1; i < nthreads; i++ ) { 
		int id = (myid + i) % nthreads; 
		// try with phyiscal clock value
		__sync_synchronize(); 
		other_clock = get_logical_clock(id);

		if ( other_clock < my_clock ||  // i'm not the minimum  
		     ( other_clock == my_clock && myid > id) ) 
			goto retry; 
	}
	
	__sync_synchronize(); 
	if ( max_thr != nthreads ) {
		DBG(3, "thread are changed to %d\n", max_thr); 
		goto retry; 
	}

	DBG(2, "return from wait_for_turn\n");

#if USE_TIMING 
	dur = get_usecs() - start; 
	perf_wait_turn.tot += dur; 
	perf_wait_turn.min = min(perf_wait_turn.min, dur); 
	perf_wait_turn.max = max(perf_wait_turn.max, dur); 
#endif 
	perf_wait_turn.cnt ++; 
	return my_clock; 
}

static int open_pfm_counter( struct worker_args *w ) 
{
	int nevts, i; 
	size_t pgsz;
	pgsz = sysconf(_SC_PAGESIZE);

	DBG(2, "open pfm counter\n"); 
	/* open performance counter */ 
#if USE_INST_COUNT
	nevts = perf_setup_list_events("INST_RETIRED,HW_INT_RCV,PERF_COUNT_SW_PAGE_FAULTS,PERF_COUNT_SW_CONTEXT_SWITCHES", &w->fds); 
#else /* store count */ 
  #if USE_INTEL_CORE2
	nevts = perf_setup_list_events("INST_RETIRED:STORES,PERF_COUNT_SW_CONTEXT_SWITCHES", &w->fds); 
  #elif USE_INTEL_NEHALEM 
	nevts = perf_setup_list_events("INST_RETIRED,PERF_COUNT_SW_CONTEXT_SWITCHES", &w->fds); 
  #else 
	#error "Unsupported Architecture" 
  #endif 
#endif 
	if (nevts < 1)
		errx(1, "cannot monitor event");
	w->fds[0].fd = -1; 
	for ( i = 0; i < nevts; i++ ) { 
		w->fds[i].hw.disabled = 1; /* do not enable now */

		w->fds[i].hw.exclusive = 1; 
		w->fds[i].hw.pinned = 1; 

#if PROFILE_KERNEL_EVENTS
		w->fds[i].hw.exclude_kernel = 0;  /* include kernel event */ 
#endif 
		w->fds[i].hw.read_format = PERF_FORMAT_SCALE; 
		w->fds[i].fd = 
			perf_event_open(&w->fds[i].hw, gettid(), -1, -1, 0);
		if (w->fds[i].fd == -1)	
			err(1, "cannot attach event %s", w->fds[i].name);
		w->fds[i].buf = 
			mmap(NULL, 2* pgsz, PROT_READ|PROT_WRITE, MAP_SHARED, w->fds[i].fd, 0);
		if (w->fds[i].buf == MAP_FAILED) 
			err(1, "cannot mmap buffer");
		w->fds[i].pgmsk = (pgsz) - 1;
	}
	return 0; 
}

static void *worker_thread(void *v)
{
	struct worker_args *w = (struct worker_args *)v; 
	int ret; 
	unsigned start, elapsed; 
	
	/* assign myid (TLS) */ 
	myid = w->id; 
	my_det_enabled = 1; 
	my_det_clock = 0; 

	cpu_set_t cmask;

	assert(num_processors > 0 ); 

	CPU_ZERO(&cmask);
	CPU_SET(myid % num_processors, &cmask);
	sched_setaffinity(0, num_processors, &cmask); 

	/* open counter */ 
	open_pfm_counter(w); 

	start = get_usecs(); 

	enable_logical_clock(); 
	
	// physically enable performance counter 
	enable_performance_counter();

	det_lock(&w->thread_lock); 
	w->started = 1; 
	det_cond_signal(&w->thread_cond); 
	det_unlock(&w->thread_lock); 

	/* execute user supplied function */ 
	w->func(w->arg); 

	/* cleanup and finish */ 
	elapsed = get_usecs() - start; 

	det_exit((void *)&ret); // do not return 

	assert(0); // should not reach here. 
	return NULL; 
}

///////////////////////////////////////////////////////////////////////////////////
// dpthread core  
///////////////////////////////////////////////////////////////////////////////////

int det_increase_logical_clock(int incr)
{
	if ( !det_is_enabled() ) return -1; 
	wa[myid].sw_clock +=incr; 
	return 0; 
}

int det_enable_logical_clock(int incr)
{
	if (!det_is_enabled() ) return -1; 

	wa[myid].sw_clock +=incr; 
	return enable_logical_clock(); 
}

int det_disable_logical_clock()
{
	if ( !det_is_enabled() ) return -1; 

	return disable_logical_clock(); 
}

int det_exit_logical_clock()
{
	if ( !det_is_enabled() ) return -1; 

	wa[myid].last_exit_logical_time = GET_CLOCK(myid); 
	SET_CLOCK(myid, MAX_LOGICAL_CLOCK); 

	return 0; 
}

/**
 * @brief asynchronously wokeup by external event. 
 * I catch up time progress up to last sync time. 
 * use this function only for waiting for external events. 
 */ 
int det_adjust_logical_clock()
{
	if ( !det_is_enabled() ) return -1; 

	if ( last_sync_logical_time > wa[myid].last_exit_logical_time ) {
		int lret = disable_logical_clock();
		SET_CLOCK(myid, last_sync_logical_time); 
		wa[myid].nondet_count++; 
		DBG(2, "got alarm\n"); 
		if ( lret == 0 ) enable_logical_clock(); 
	} else {
		SET_CLOCK(myid, wa[myid].last_exit_logical_time); 
	}

	return 0; 
}

void det_set_debug(int level)
{
	debug_level = level; 
}

/**
 * initialize dpthread 
 */
int det_init(int argc, char *argv[])
{
	struct worker_args *w; 

	/* 
	   Environment variables: 
	   DPTHREAD_DEBUG <number>     # enable debug. 0 - none, 1 - basic, 2 - verbose, 3 - all.
	*/ 
	char *ptr; 
	cpu_set_t cmask;

	num_processors = sysconf(_SC_NPROCESSORS_CONF); 
	assert(num_processors > 0 ); 

	CPU_ZERO(&cmask);
	sched_setaffinity(0, num_processors, &cmask); 

	if ( (ptr = getenv("DPTHREAD_DEBUG")) ) { 
		debug_level = atoi(ptr); 
	}
	if ( (ptr = getenv("DPTHREAD_LOG_FILE")) ) { 
		debug_log_file = ptr; 
	}

	// if ( (ptr = getenv("LD_PRELOAD")) && strstr(ptr, "libdetio.so") )

	if (pfm_initialize() != PFM_SUCCESS)
		errx(1, "pfm_initialize failed");
	
	// initialize structure
	memset(wa, 0, MAX_THR * sizeof(struct worker_args)); 

	// setup master thread 
	assert(max_thr == 0 ); 

	myid = 0; 
	my_det_enabled = 1; 
	my_det_clock = 0; 
	max_thr = num_thr = 1; 

	w = &wa[0]; 
	w->id = myid; 
	w->func = NULL; 
	w->arg  = NULL; 
	w->sw_clock = 0; 
	w->hw_clock  = 0; 
	w->hw_clock_enabled = 0; 

	if ( debug_log_file ) {
		char name[40]; 
		sprintf(name, "%s.p%d", debug_log_file, myid); 
		w->log_file = fopen(name, "w+"); 
	} else {
		w->log_file = stderr; 
	}

	// open performance counter
	open_pfm_counter(w); 

	// perf related. 
	perf_logical.min = perf_wait_turn.min = INT_MAX;
	perf_enable.min = perf_disable.min = INT_MAX; 

	DBG(1, "INIT: debug_level=%d. event begin \n", debug_level); 


	return 0; 
}

int det_lock_init(det_mutex_t *mutex)
{
	int ret; 
	int lret;

	// if not initialized, initialize. 
	if ( max_thr == 0 ) det_init(0, NULL);
	lret = disable_logical_clock(); 

	InitQ(&mutex->queue, MAX_THR); 

	pthread_mutex_lock(&count_mutex); 
	mutex->id = ++g_lock_count; 
	pthread_mutex_unlock(&count_mutex); 
	mutex->released_logical_time = 0; 

	DBG(1, "mutex_init(%d)\n", mutex->id); 

	ret = pthread_mutex_init(&mutex->mutex, NULL); 
	if ( lret == 0 ) enable_logical_clock(); 

	return 0; 
}

int det_trylock(det_mutex_t *mutex)
{
	int ret = 0; 
	int lret; 
	int64_t clock, incr; 

	assert(mutex->id > 0 ); 

	// if det is disabled simply same as pthread. 
	if ( !det_is_enabled() ) 
		return pthread_mutex_lock(&mutex->mutex);
	// disable count       
	lret = disable_logical_clock(); 

	DBG(1, "trylock(%d)\n", mutex->id);

	clock = wait_for_turn(); 
	AddQ(&mutex->queue, (void *)myid);

	if ( (int)GetHeadQ(&mutex->queue) == myid && 
	     (ret = pthread_mutex_trylock(&mutex->mutex)) == 0 ) 
	{ // success. 
		DBG(3, "--trylock");
		if ( mutex->released_logical_time >= clock ) 
		{ // physically ok but logically not. 
			DBG(3, "--case2: released at %lld\n",
			    mutex->released_logical_time); 
			
			incr = mutex->released_logical_time - clock; 
			pthread_mutex_unlock(&mutex->mutex); 

			// deterministic fast forward. 
			wa[myid].sw_clock += incr; 

			ret = EBUSY; 
		}
		else 
		{ // logically and physically ok. 
			mutex->last_owner = myid; 
		}
	} 
	assert(GET_CLOCK(myid) < MAX_LOGICAL_CLOCK); 

	// update last sync logical time 
	last_sync_logical_time = GET_CLOCK(myid); 
	

	if ( ret == 0 ) {
		DBG(1, "trylock acq(%d)\n", mutex->id);
		// statistic 
		lock_count ++; 
	} else {
		DBG(1, "trylock fail(%d)\n", mutex->id); 
	}

	// remove from the queue. 
	DelQ(&mutex->queue); 

	// increase logical clock 
	wa[myid].sw_clock++; 

	// resume logical clock 
	if ( lret == 0 ) enable_logical_clock(); 

	return ret; 
}

int det_lock(det_mutex_t *mutex)
{
	int ret = 0; 
	int lret; 
	int64_t clock, incr; 

	if ( mutex->id < 0 ) { 
		det_lock_init(mutex); 
	}

	assert(mutex->id > 0 ); 

	// if det is disabled simply same as pthread. 
	if ( !det_is_enabled() ) 
		return pthread_mutex_lock(&mutex->mutex);

	// disable count       
	lret = disable_logical_clock(); 

#if !USE_NESTED_LOCK
	DBG(1, "acq(%d) enter\n", mutex->id);

	// wait for turn 
	clock = wait_for_turn();

	ret = pthread_mutex_lock(&mutex->mutex);
	DBG(1, "acq(%d)\n", mutex->id); 

#else // USE_NESTED_LOCK

	DBG(2, "acq(%d) - enter\n", mutex->id);

	clock = wait_for_turn(); 
	AddQ(&mutex->queue, (void *)myid);

	while ( 1 ) {
		if ( (int)GetHeadQ(&mutex->queue) == myid && 
			 pthread_mutex_trylock(&mutex->mutex) == 0 ) 
		{ // success. 
			DBG(3, "--trylock");
			if ( mutex->released_logical_time >= clock ) 
			{ // physically ok but logically not. 
				DBG(3, "--case2: released at %lld\n",
				    mutex->released_logical_time); 
				
				incr = mutex->released_logical_time - clock; 
				pthread_mutex_unlock(&mutex->mutex); 
				// deterministic fast forward. 
				wa[myid].sw_clock += incr; 
			}
			else 
			{ // logically and physically ok. 
				mutex->last_owner = myid; 
				break; // quit the loop. 
			}
		} 
		assert(GET_CLOCK(myid) < MAX_LOGICAL_CLOCK); 
		DBG(2, "--spinning");

		// increase clock 
		wa[myid].sw_clock ++; 

		// wait for turn 
		clock = wait_for_turn();
	}	

#endif // USE_NESTED_LOCK
	assert(GET_CLOCK(myid) < MAX_LOGICAL_CLOCK); 
	DBG(1, "acq(%d)\n", mutex->id);

	// update last sync logical time 
	last_sync_logical_time = GET_CLOCK(myid); 
	
	// remove from the queue. 
	DelQ(&mutex->queue); 

	// increase logical clock 
	wa[myid].sw_clock++; 

	// resume logical clock 
	if ( lret == 0 ) enable_logical_clock(); 

	// statistic 
	lock_count ++; 
	return ret; 
}

int det_unlock(det_mutex_t *mutex)
{
	int ret = 0; 
	int lret; 

	assert(mutex->id > 0 ); 

	// if det is disabled simply same as pthread. 
	if ( !det_is_enabled() ) 
		return pthread_mutex_unlock(&mutex->mutex);

	// must be initialized first. 
	assert( max_thr > 0 ); 

	// disable count 	
	lret = disable_logical_clock(); 

	mutex->released_logical_time = get_logical_clock(myid) ;  
	DBG(1, "rel(%d)\n", mutex->id); 

	// update last sync logical time 
	last_sync_logical_time = GET_CLOCK(myid); 

	ret = pthread_mutex_unlock(&mutex->mutex); 

	// increase logical clock 
	wa[myid].sw_clock ++; 
	// resume logical clock 
	if ( lret == 0 ) enable_logical_clock(); 

	return ret; 
}

int  det_cond_init(det_cond_t *cond)
{
	int lret; 
	int i; 

	// if not initialized, initialize. 
	if ( max_thr == 0 ) det_init(0, NULL);

	lret = disable_logical_clock(); 
	
	InitQ(&cond->queue, MAX_THR); 

	for ( i = 0; i < MAX_THR; i++ ) { 
		cond->waiter[i].id = i; 
		pthread_mutex_init(&cond->waiter[i].mutex, NULL); 
		pthread_mutex_lock(&cond->waiter[i].mutex); 
	}

	pthread_mutex_lock(&count_mutex); 
	cond->id = ++g_cond_count; 
	pthread_mutex_unlock(&count_mutex); 

	DBG(1, "cond_init(%d)\n", cond->id); 
	if ( lret == 0 ) enable_logical_clock(); 
	return 0; 
}

/**
 */ 
int  det_cond_wait(det_cond_t *cond, det_mutex_t *mutex)
{
	int lret = disable_logical_clock(); 
	det_mutex_t *lock;
	DBG(1, "cond(%d) wait enter\n", cond->id); 

	// add to waiting list. mutex is still held. 
	lock = &cond->waiter[myid]; 
	AddQ(&cond->queue, (void *)lock);

	// release condition lock
	det_unlock(mutex); 

	// equal to exit()
	SET_CLOCK(myid, MAX_LOGICAL_CLOCK); 

	// waiter->P()
	pthread_mutex_lock(&lock->mutex);  

	// signaler must set this already. 
	__sync_synchronize();
	assert(GET_CLOCK(myid) < MAX_LOGICAL_CLOCK); 

	DBG(1, "cond(%d) wait leave\n", cond->id); 
	
	if ( lret == 0 ) enable_logical_clock();
	
	// acquite the lock 
	det_lock(mutex); 

	return 0;
}

int  det_cond_signal(det_cond_t *cond)
{ 
	int64_t clock; 
	det_mutex_t *lock; 

	int lret = disable_logical_clock(); 

	clock = get_logical_clock(myid); 
	assert( clock < MAX_LOGICAL_CLOCK ); // FIXME: sometimes it aborts. 

	/* condition lock is already held */ 

	if ( !IsEmptyQ(&cond->queue) ) { 
		lock = (det_mutex_t*)DelQ(&cond->queue); 
		SET_CLOCK(lock->id, clock + 1); 
		pthread_mutex_unlock(&lock->mutex); 
		DBG(1, "cond(%d) signal\n", cond->id); 
	}

	// increase logical clock 
	wa[myid].sw_clock ++; 

	if ( lret == 0 ) enable_logical_clock(); 
	return 0; 
}

int  det_cond_broadcast(det_cond_t *cond) 
{
	int lret = disable_logical_clock(); 	

	while ( !IsEmptyQ(&cond->queue) ) { 
		det_cond_signal(cond); 
	}

	if ( lret == 0 ) enable_logical_clock(); 
	return 0; 
}

int det_barrier_init(det_barrier_t *barrier, int count)
{
	// if not initialized, initialize. 
	if ( max_thr == 0 ) det_init(0, NULL);

	barrier->target_count = count; 
	barrier->wait_count   = 0; 

	pthread_mutex_lock(&count_mutex); 
	barrier->id = ++g_barr_count; 
	pthread_mutex_unlock(&count_mutex); 

	det_lock_init(&barrier->wait_mutex); 
	det_cond_init(&barrier->wait_cond); 

	DBG(0, "lock %d is for barrier\n", g_lock_count); 
	DBG(0, "cond %d is for barrier\n", g_cond_count); 
	return 0; 
}

int det_barrier_wait(det_barrier_t *barrier)
{
	int ret = 0; 

	// disable counting
	int lret = disable_logical_clock(); 

	DBG(1, "barrier(%d) enter\n", barrier->id); 

	det_lock(&barrier->wait_mutex); 

	// must be initialized before (barrier init)
	assert( max_thr > 0 );

	barrier->wait_count ++; 
	if ( barrier->wait_count == barrier->target_count ) {
		barrier->wait_count = 0; 
		det_cond_broadcast(&barrier->wait_cond); 
	} else { 
		det_cond_wait(&barrier->wait_cond, &barrier->wait_mutex); 
	}
	det_unlock(&barrier->wait_mutex); 

	DBG(1, "barrier(%d) leave\n\n", barrier->id); 
	// enable counting 

	if ( lret == 0 ) enable_logical_clock(); 

	barrier_count ++; 
	return ret; 
}

int det_create( pthread_t *thread, const pthread_attr_t *attr,
		void *(*start_routine)(void*), void *arg)
{
	int id; 
	int ret; 
	int lret; 

	// if not initialized, initialize. 
	if ( max_thr == 0 ) 
		det_init(0, NULL);

	if ( num_thr <= 1 ) {
		// physically enable performance counter 
		enable_logical_clock(); 
#if USE_FAKE_DISABLE 
		enable_performance_counter();
#endif 
	}

	// disable count 	
	lret = disable_logical_clock(); 

	DBG(1, "create thread %d from a child thread %d\n", max_thr, myid); 

	wait_for_turn(); 

	id = max_thr; 

	max_thr ++; 
	num_thr ++; 

	wa[id].id   = id; 
	wa[id].func = start_routine; 
	wa[id].arg  = arg; 
 	wa[id].sw_clock = get_logical_clock(myid) + 1; // assign initial 
	wa[id].hw_clock = 0; 
	wa[id].last_exit_logical_time = 0; 
	
	wa[id].finished = 0; 
	wa[id].started = 0; 

	det_lock_init(&wa[id].thread_lock); 
	det_cond_init(&wa[id].thread_cond); 

	wa[id].nondet_count = 0; 

	DBG(1, "lock %d is thread %d internal\n", g_lock_count, id); 
	DBG(1, "cond %d is thread %d internal\n", g_cond_count, id); 

	DBG(2, "Thread %d initial clock = %ld, hw = %d\n", 
	    id, wa[id].sw_clock, wa[id].hw_clock); 

	if ( debug_log_file ) {
		char name[40]; 
		sprintf(name, "%s.p%d", debug_log_file, id); 
		wa[id].log_file = fopen(name, "w+"); 
	} else {
		wa[id].log_file = stderr; 
	}

	ret = pthread_create(thread, attr, worker_thread, &wa[id]); 

	// pthread_t 
	wa[id].tid = *thread; 

	det_lock(&wa[id].thread_lock);
	if ( !wa[id].started ) {
		det_cond_wait(&wa[id].thread_cond, &wa[id].thread_lock); 
	}
	det_unlock(&wa[id].thread_lock); 
	// resume logical clock 
	if ( lret == 0 ) enable_logical_clock(); 

	return 0; 
}

int det_join ( pthread_t threadid, void **thread_return )
{
	int ret; 
	int i; 
	struct worker_args *w = NULL; 

	// disable count       
	int lret = disable_logical_clock();

	// somewhat ugly. but doesn't matter. 
	for ( i = 0; i < MAX_THR; i++ ) { 
		w = &wa[i]; 
		if ( w->tid == threadid) break; 
	}
	assert( i < MAX_THR ); 

	DBG(1, "JOIN(%d):enter \n", i); 

	det_lock(&w->thread_lock); 
	if ( !w->finished ) {
		det_cond_wait(&w->thread_cond, &w->thread_lock); 
	}
	det_unlock(&w->thread_lock); 
	
	ret = pthread_join( threadid, thread_return); 

	DBG(1, "JOIN(%d):exit \n", i);

	num_thr --; 

	if ( lret == 0 ) enable_logical_clock(); 

	if ( num_thr <= 1 ) {
		// physically enable performance counter 
		disable_logical_clock(); 
#if USE_FAKE_DISABLE 
		disable_performance_counter();
#endif 
	}

	return ret; 
}

void det_exit(void *value_ptr)
{
	struct worker_args *w = &wa[myid]; 

	enable_logical_clock(); 
	det_lock(&w->thread_lock); 
	w->finished = 1; 
	det_cond_signal(&w->thread_cond); 
	det_unlock(&w->thread_lock); 

	/* disable event */ 
	disable_logical_clock(); 
	disable_performance_counter();

	free(w->fds); 

	DBG(1, "EXIT: (hw_evt:%lld, sw_evt:%lld) ndet_evt:%d, %d locks and %d barriers.\n", 
	    wa[myid].hw_clock, wa[myid].sw_clock, wa[myid].nondet_count, 
	    lock_count, 
	    barrier_count); 	

	SET_CLOCK(myid, MAX_LOGICAL_CLOCK * 2 ); 

	pthread_exit(value_ptr);
}

int det_cancel(pthread_t threadid)
{
	int i; 
	int lret; 
	struct worker_args *w = NULL;

	// somewhat ugly. but doesn't matter. 
	for ( i = 0; i < MAX_THR; i++ ) { 
		w = &wa[i]; 
		if ( w->tid == threadid) break; 
	}

	assert( w ); 
	det_lock(&w->thread_lock); 
	w->finished = 1; 
	det_cond_signal(&w->thread_cond); 
	det_unlock(&w->thread_lock); 

	pthread_cancel(threadid); 

	lret = disable_logical_clock(); 	

	SET_CLOCK(i, MAX_LOGICAL_CLOCK * 2); 
	DBG(1, "EXIT: Thread %d: (hw_evt:%lld, sw_evt:%lld) ndet_evt:%d\n", 
	    w->id, 
	    w->hw_clock, w->sw_clock, w->nondet_count); 

	if ( lret == 0 ) enable_logical_clock(); 
	return 0; 
}

void  det_enable(void)
{
	assert(max_thr > 0 ); // must be initialized first. 
	assert(!my_det_enabled); // must be disabled 
	DBG(1, "%s: \n", __FUNCTION__); 

	SET_CLOCK(myid, my_det_clock); 
	my_det_enabled = 1; 
	enable_logical_clock(); 
}

void  det_disable(void)
{
	// if not initialized, initialize. 
	if ( max_thr == 0 ) det_init(0, NULL);

	assert(my_det_enabled); // must be enabled 
	disable_logical_clock();
	my_det_clock = get_logical_clock(myid); 
	SET_CLOCK(myid, MAX_LOGICAL_CLOCK); // quit if somebody is waiting. 
	my_det_enabled = 0; 
	DBG(1, "%s: \n", __FUNCTION__); 
}

int  det_is_enabled(void)
{
	if ( num_thr <= 1 ) 
		return 0; 
	else 
		return my_det_enabled; 
}

int  det_get_pid(void)
{
	return myid; 
}

int64_t det_get_clock() 
{
	int64_t ret; 
	int lret; 

	// if not initialized, initialize. 
	if ( max_thr == 0 ) det_init(0, NULL);

	lret = disable_logical_clock(); 

	ret = get_logical_clock(myid); 

	if ( lret == 0 ) enable_logical_clock(); 
	return ret; 
}
///////////////////////////////////////////////////////////////////////////////////
// dpthread I/O   
///////////////////////////////////////////////////////////////////////////////////

int det_dbg(const char *format, ...) 
{
	int ret; 
	va_list ap; 
	FILE *out = stderr; 		
	int lret; 

	// if not initialized, initialize. 
	if ( max_thr == 0 ) det_init(0, NULL);

	// disable count       
	lret = disable_logical_clock();

	if ( wa[myid].log_file ) out = wa[myid].log_file;

	pthread_mutex_lock(&dbg_mutex); 
	fprintf(out, "[%2d] %lld:", myid, get_logical_clock(myid)); 
	va_start(ap, format); 
	ret = vfprintf(out, format, ap); 
	va_end(ap); 
	fflush(out);

	pthread_mutex_unlock(&dbg_mutex); 

	// increase logical clock 
	wa[myid].sw_clock ++; 

	// resume logical clock 
	if ( lret == 0 ) enable_logical_clock();

	return ret; 
}

void det_print_stat()
{
	int lret = disable_logical_clock();

	DBG(0, "EXIT: (hw_evt:%lld, sw_evt:%lld) ndet_evt:%d, %d locks and %d barriers.\n", 
	    wa[myid].hw_clock, wa[myid].sw_clock, wa[myid].nondet_count, 
	    lock_count, 
	    barrier_count); 	

#if USE_TIMING 
	DBG(0, "Thread %d : wait_turn: min(%lld),max(%lld),tot(%lld),cnt(%lld),avg(%lld)\n", 
	    myid, 
	    perf_wait_turn.min, perf_wait_turn.max, perf_wait_turn.tot, 
	    perf_wait_turn.cnt, 
	    (perf_wait_turn.cnt >0 ) ? perf_wait_turn.tot / perf_wait_turn.cnt : 0);

	DBG(0, "Thread %d : enable: min(%lld),max(%lld),tot(%lld),cnt(%lld),avg(%lld)\n", 
	    myid, 
	    perf_enable.min, perf_enable.max, perf_enable.tot, 
	    perf_enable.cnt, 
	    (perf_enable.cnt >0 ) ? perf_enable.tot / perf_enable.cnt : 0);
	DBG(0, "Thread %d : disable: min(%lld),max(%lld),tot(%lld),cnt(%lld),avg(%lld)\n", 
	    myid, 
	    perf_disable.min, perf_disable.max, perf_disable.tot, 
	    perf_disable.cnt, 
	    (perf_disable.cnt >0 ) ? perf_disable.tot / perf_disable.cnt : 0);
#endif 

	if ( lret == 0 ) enable_logical_clock(); 
}

#if SELF_TEST
#define SELF_TEST_LOOP 10000000
static void self_test()
{
	int i,j; 
	unsigned start, dur; 
	int64_t clock; 
	int inner_loops = max_thr; // max_thr 
	
	int64_t old, cur, diff;

	for ( i = 0; i < SELF_TEST_LOOP; i++ ) { 
		int64_t tmp; 
		old = read_count(wa[myid].fds); 
		cur = read_count(wa[myid].fds); 
		tmp = cur - old; 
		if ( i == 0 ) 
			diff = tmp; 
		else if ( tmp != diff ) {
			det_dbg("ERROR: tmp=%lld, diff=%lld\n", tmp, diff); 
			det_dbg("ERROR: old=%lld, cur=%lld\n", old, cur); 
		}
	}
	
	if ( i == SELF_TEST_LOOP) {
		det_dbg("PASS: read_count deterministically took %lld events\n", diff); 
		return ;
	}

	start = get_usecs(); 
	for ( i = 0; i < SELF_TEST_LOOP; i++ ) { 
		wa[myid].hw_clock = i; 
		for ( j = 0; j < inner_loops; j++ ) { 
			clock += wa[(myid + j)%max_thr].hw_clock;
		}
	}
	dur = get_usecs() - start; 
	DBG(0, "wa[].hw_clock finished in %u (usec) avg is %u (usec)\n", 
	    dur, dur / SELF_TEST_LOOP ); 


	start = get_usecs(); 
	for ( i = 0; i < SELF_TEST_LOOP; i++ ) { 
		for ( j = 0; j < inner_loops; j++ ) { 
			clock += get_logical_clock((myid + j)%max_thr); 
		}
	}
	dur = get_usecs() - start; 
	DBG(0, "read_count test finished in %u (usec) avg is %u (usec)\n", 
	    dur, dur / SELF_TEST_LOOP ); 

	start = get_usecs(); 
	for ( i = 0; i < SELF_TEST_LOOP; i++ ) { 
		disable_logical_clock(); 
		enable_logical_clock();
	}
	dur = get_usecs() - start; 
	DBG(0, "dis/enable ioctl test finished in %u (usec) avg is %u (usec)\n", 
	    dur, dur / SELF_TEST_LOOP ); 


	start = get_usecs(); 
	for ( i = 0; i < SELF_TEST_LOOP; i++ ) { 
		disable_logical_clock(); 
		wait_for_turn(); 
		enable_logical_clock();
	}

	dur = get_usecs() - start; 
	DBG(0, "wait_for_turn test finished in %u (usec) avg is %u (usec)\n", 
	    dur, dur / SELF_TEST_LOOP ); 
}
#endif 

