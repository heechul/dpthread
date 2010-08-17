/* 
 */    

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>

#include <dpthread.h>
#include <unistd.h>

#define MY_PRIORITY (49) 
#define MAX_SAFE_STACK (8*1024)
#define MAX_ITER 1000
#define MAX_THR  16

#define DET_CREATE(x...)
#define DET_LOCK(x) if ( det ) det_lock(x); else pthread_mutex_lock(&(x)->mutex)
#define DET_UNLOCK(x) if ( det ) det_unlock(x); else pthread_mutex_unlock(&(x)->mutex)

static det_mutex_t mutexsum;  // hcyun
static int iteration  = MAX_ITER; 
static int max_thr = 1;
static int det = 0; 
static int size = 1024; // unit read size. 
static int sum; 

int g_fd;

void * my_worker1(void *v)
{
	int i; 
	unsigned start, dur; 
	char buf[1024]; 

	g_fd = open("file.dat", O_RDONLY); 
	if ( g_fd < 0 ) {
		perror("open failed"); 
		exit(1);
	}
	start = get_usecs(); 
	for ( i = 0; i < iteration; i++ ) { 
		int cnt; 

		if ( i % 100 == 0) { 
			DET_LOCK(&mutexsum); 
			sum++; 
			DET_UNLOCK(&mutexsum);
		}

		cnt = read(g_fd, buf, size); // average time = x
		if ( cnt <= 0 ) {
			perror("finish"); 
			break; 
		}
	}
	dur = get_usecs() - start; 
	printf("%s:dur = %d, avg = %f\n", __FUNCTION__, dur, (float)dur / iteration ); 

	return NULL; 
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
my_worker2(void *v)
{
	int i; 
	unsigned int start, dur; 
	DET_LOCK(&mutexsum); 
	sum++; 
	DET_UNLOCK(&mutexsum);

	start = get_usecs(); 
	for ( i = 0; i < iteration; i++ ) { 
		if ( i % 100 == 0 ) { 
			DET_LOCK(&mutexsum); 
			sum++; 
			DET_UNLOCK(&mutexsum);
		}

		fib(10);   // average time = x 
	}
	dur = get_usecs() - start; 
	printf("%s:dur = %d, avg = %f\n", __FUNCTION__, dur, (float)dur / iteration ); 
	DET_LOCK(&mutexsum); 
	sum++; 
	DET_UNLOCK(&mutexsum);

	return NULL; 
}

static void
usage(void)
{
	printf("locktest [-n threads] [-i iteration] [-h]\n"
	       "-n thread: number of threads to create (default: 1)\n"
	       "-i : iteration \n"); 
}

void stack_prefault(void) 
{
        unsigned char dummy[MAX_SAFE_STACK];
        memset(&dummy, 0, MAX_SAFE_STACK);
        return;
}


int main(int argc, char *argv[])
{
        struct sched_param param;
	pthread_t allthr[MAX_THR];
	int i, ret; 

	while((i=getopt(argc, argv, "n:i:s:hd")) != EOF) {
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
		case 's':
			size = atoi(optarg); 
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

	det_init(argc, argv);
	det_lock_init(&mutexsum); 
	if ( det ) { 
		det_create(allthr+0, NULL, my_worker1, (void *)(unsigned long)0);
		det_create(allthr+1, NULL, my_worker2, (void *)(unsigned long)1);
		det_enable(); // enable deterministic execution 
	} else { 
		pthread_create(allthr+0, NULL, my_worker1, (void *)(unsigned long)0);
		pthread_create(allthr+1, NULL, my_worker2, (void *)(unsigned long)1); 
	}

	for (i = 0; i < 2; i++) {
		pthread_join(allthr[i], NULL);
	}

	return 0; 
}

