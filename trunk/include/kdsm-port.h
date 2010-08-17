#include <dpthread.h>
#include <stdlib.h>

#define DsmInit(x,y) det_init(x,y)
#define DsmAlloc(x) malloc(x)
#define DsmGetPid() gettid()
#define DsmGetUsecs() get_usecs()

#if PORT_PTHREAD
  #define DsmLock(x) pthread_mutex_lock(&x.mutex)
  #define DsmUnlock(x) pthread_mutex_unlock(&x.mutex)
  #define DsmBarrier(x) pthread_barrier_wait(&x.bar)
  #define DsmExit() pthead_exit(NULL)
#else 
  #define DsmLock(x) det_lock(&(x))
  #define DsmUnlock(x) det_unlock(&(x))
  #define DsmBarrier(x) det_barrier_wait(&(x))
  #define DsmExit() det_exit(NULL)
#endif 
int gLockCount;
int gFlagCount;
int gBarrCount;
