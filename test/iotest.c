/* 
 * microbench 
 *  SCHED_RR, stack_prefault
 * 
 * load,#lock     non-det    det  
 * ------------------------------
 * fib(0) ,200k : 0.030     1.144
 * fib(5) ,200k : 0.060     1.185
 * fib(10),200k : 0.225     1.279
 * fib(12),200k : 0.516     1.581
 * fib(14),200k : 1.275     2.351 
 * fib(15),100k : 1.078     1.593
 * fib(16),100k : 1.900     2.229
 * fib(17),100k : 2.689     3.354
 * fib(18),100k : 4.442     5.050
 * fib(19),100k : 7.261     7.884
 * fib(20),100k :11.763    11.964 
 * fib(25), 10k : 13.160   12.899
 */    

#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>

#include <dpthread.h>
#include <unistd.h>

#include <dpthread-wrapper.h>

#define MY_PRIORITY (49) 
#define MAX_SAFE_STACK (8*1024)
#define MAX_ITER 1000
#define MAX_THR  16

#define DET_CREATE(x...)

static det_mutex_t mutexsum;  // hcyun
static int iteration  = MAX_ITER; 
static int max_thr = 1;
static int det = 0; 
volatile int sum = 0; 

void stack_prefault(void) 
{
        unsigned char dummy[MAX_SAFE_STACK];
        memset(&dummy, 0, MAX_SAFE_STACK);
        return;
}

unsigned long 
fib(unsigned long n)
{
	if (n == 0)
		return 0;
	if (n == 1)
		return 2;
	return fib(n-1)+fib(n-2);
}

#define DET_LOCK(x) if ( det ) det_lock(x); else pthread_mutex_lock(&(x)->mutex)
#define DET_UNLOCK(x) if ( det ) det_unlock(x); else pthread_mutex_unlock(&(x)->mutex)

void *
my_worker1(void *v)
{
	int i; 

	detio_sleep(3); 
	DET_LOCK(&mutexsum);
	sum++; 
	DET_UNLOCK(&mutexsum);

	return NULL; 
}

void *
my_worker2(void *v)
{
	int i; 

	fib(6); 
	DET_LOCK(&mutexsum); 
	sum++; 
	DET_UNLOCK(&mutexsum);
	detio_sleep(3); 
	return NULL; 
}


static void
usage(void)
{
	printf("locktest [-n threads] [-i iteration] [-h]\n"
	       "-n thread: number of threads to create (default: 1)\n"
	       "-i : iteration \n"); 
}

int main(int argc, char *argv[])
{
        struct sched_param param;
	pthread_t allthr[MAX_THR];
	int i, ret; 

	while((i=getopt(argc, argv, "n:i:hd")) != EOF) {
		switch(i) {
		case 'h':
			usage();
			return 0;
		case 'd':
			det = 1; 
			break; 
		case 'n':
			max_thr = atoi(optarg);
			if (max_thr >= MAX_THR)
				errx(1, "no more than %d threads", MAX_THR);
			break;
		case 'i': 
			iteration = atoi(optarg); 
			break; 
		default:
			errx(1, "invalid option");
		}
	}

        param.sched_priority = MY_PRIORITY;
        if(sched_setscheduler(0, SCHED_RR, &param) == -1) {
                perror("sched_setscheduler failed");
                // exit(-1);
        }

        if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
                perror("mlockall failed");
                // exit(-2);
        }

        /* Pre-fault our stack */
        stack_prefault();

	// pthread_mutex_init(&(mutexsum.mutex), NULL);

	if ( det ) { 
		det_init(argc, argv);
		det_lock_init(&mutexsum); 
		det_create(allthr+0, NULL, my_worker1, (void *)(unsigned long)0);
		det_create(allthr+1, NULL, my_worker2, (void *)(unsigned long)1);
		det_enable(); // enable deterministic execution 
	} else { 
		pthread_mutex_init(&(mutexsum.mutex), NULL); 
		pthread_create(allthr+0, NULL, my_worker1, (void *)(unsigned long)0);
		pthread_create(allthr+1, NULL, my_worker2, (void *)(unsigned long)1);
	}

	
	for (i = 0; i < 2; i++) {
		pthread_join(allthr[i], NULL);
	}

	printf("sum : %d\n", sum); 

	return 0; 
}

