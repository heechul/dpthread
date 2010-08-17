
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>

#include <unistd.h>

#define MY_PRIORITY (49) 
#define MAX_SAFE_STACK (8*1024)
#define MAX_ITER 1000
#define MAX_THR  16

#include <dpthread-wrapper.h>

static pthread_mutex_t mutexsum;  // hcyun
static pthread_barrier_t barrier; 

static int iteration  = MAX_ITER; 
static int max_thr = 1;
static int det = 0; 



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

void *
comp_worker(void *v)
{
	int i; 
	for ( i = 0; i < iteration; i++ ) 
		fib(16); 
}

void *
my_worker(void *v)
{
	int i; 
	int64_t clock; 

	// for ( i = 0; i < 1000000; i++) { 
	for ( i = 0; i < iteration; i++) 
	{ 
		clock = det_get_clock(); 
		
		if ( i % ( iteration / 2 ) == 0 ) 
		{ 
			printf("[%d]iter=%d clock=%lld\n", (int)v, i, clock); 
		}
	}
	return NULL; 
}

static void
usage(void)
{
	printf("ubench [-n threads] [-i iteration] [-h]\n"
	       "-n thread: number of threads to create (default: 1)\n"
	       "-i : iteration \n"); 
}

int main(int argc, char *argv[])
{
        struct sched_param param;
	pthread_t allthr[MAX_THR];
	int i, ret; 

	det_set_debug(1); 

	while((i=getopt(argc, argv, "n:i:h")) != EOF) {
		switch(i) {
		case 'h':
			usage();
			return 0;
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

	pthread_mutex_init(&mutexsum, NULL); 		
	pthread_barrier_init(&barrier, NULL, max_thr); 

	for(i=1; i < max_thr; i++) {
		pthread_create(allthr+i, NULL, my_worker, (void *)(unsigned long)i);
	}

	// comp_worker(0); 
	my_worker(0); 

	for (i = 1; i < max_thr; i++) {
		pthread_join(allthr[i], NULL);
	}

	return 0; 
}

