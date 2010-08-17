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

int g_fd;

void * my_worker1(void *v)
{
	int i; 
	uint64_t start, dur; 
	char *buf = malloc(size); 

	g_fd = open("file.dat", O_RDONLY); 
	if ( g_fd < 0 ) {
		perror("open failed"); 
		exit(1);
	}

	for ( i = 0; i < iteration; i++ ) { 
	// while (1) { 
		int cnt; 
		start = det_get_clock(det_get_pid()); 
		cnt = read(g_fd, buf, size);
		if ( cnt <= 0 ) {
			perror("finish"); 
			break; 
		}
		dur = det_get_clock(det_get_pid()) - start; 
		printf("%lld\n", dur); 
	}
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

	if ( det ) { 
		det_init(argc, argv);
		det_lock_init(&mutexsum); 
		det_create(allthr+0, NULL, my_worker1, (void *)(unsigned long)0);
		det_enable(); // enable deterministic execution 
	} else { 
		pthread_mutex_init(&(mutexsum.mutex), NULL); 
		pthread_create(allthr+0, NULL, my_worker1, (void *)(unsigned long)0);
	}

	
	for (i = 0; i < 1; i++) {
		pthread_join(allthr[i], NULL);
	}

	return 0; 
}

