/* 
 * microbench for lock operation 
 * 
 * REV-309. configuration 
 *   USE_NESTED_LOCK 1
 *   USE_FAKE_DISABLE 1  
 * 
 * load,#lock     pthread   dpthread 
 * ------------------------------
 * fib(0) ,100k : 0.007  :  0.403
 * fib(5) ,100k : 0.016  :  0.427
 * fib(10),100k : 0.101  :  0.501
 * fib(12),100k : 0.247  :  0.646
 * fib(14),100k : 0.622  :  1.031 
 * fib(15),100k : 1.042  :  1.410
 * fib(16),100k : 1.688  :  2.031
 * fib(17),100k : 2.689  :  3.031
 * fib(19),100k : 6.917  :  7.152
 */    

#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>
#include <sched.h>
#include <pthread.h>
#include <unistd.h>

#define MY_PRIORITY (49) 
#define MAX_SAFE_STACK (8*1024)
#define MAX_ITER 1000

#include <dpthread-wrapper.h>

static pthread_mutex_t mutexsum;  // hcyun
static pthread_barrier_t barrier; 

static int iteration  = MAX_ITER; 
static int max_thr = 1;
static int comp_A = 15; 
static int comp_B = 15; 

volatile int sum = 0; 
volatile int finished = 0; 


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
my_compa(void *v)
{
	int i; 

	int compTime = (int)v; 

	for ( i = 0; i < iteration; i++) { 
		int val; 

		fib(compTime); 

		pthread_mutex_lock(&mutexsum); 
		val = sum ++; 
		pthread_mutex_unlock(&mutexsum); 
	}

	finished = 1; 
	return NULL; 
}

void *
my_compb(void *v)
{
	int compTime = (int)v; 

	while ( !finished ) { 
		int val; 

		fib(compTime); 

		pthread_mutex_lock(&mutexsum); 
		val = sum ++; 
		pthread_mutex_unlock(&mutexsum); 
	}
	return NULL; 
}

static void
usage(void)
{
	printf("locktest [-n threads] [-i iteration] [-a comp_A] [-b comp_B] [-h]\n"
	       "-n thread: number of threads to create (default: 1)\n"
	       "-i : iteration \n"); 
	printf("ex) % ./locktest -n 2 -i 100000 -a 0 -b 0 \n"); 
	exit(1); 
}

int main(int argc, char *argv[])
{
        struct sched_param param;
	pthread_t allthr[MAX_THR];
	int i; 


	while((i=getopt(argc, argv, "a:b:n:i:h")) != EOF) {
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
		case 'a': 
			comp_A = atoi(optarg); 
			break; 
		case 'b':
			comp_B = atoi(optarg); 
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

	for(i=0; i < max_thr - 1; i++) {
		pthread_create(allthr+i, NULL, my_compb, (void *)comp_B);
	}

	my_compa((void *)comp_A); 
	
	for (i = 0; i < max_thr - 1; i++) {
		pthread_join(allthr[i], NULL);
	}

	printf("sum : %d\n", sum); 

#if USE_DPTHREAD
	det_print_stat(); 
#endif 
	return 0; 
}

