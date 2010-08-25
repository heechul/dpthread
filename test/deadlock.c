/**
 * Deadlock example by acquiring locks in reverse order
 *
 * Author: Heechul Yun <heechul@illinois.edu> 
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 */ 
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>
#include <unistd.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */

// NOTE: uncomment the following line to make app deterministic 
#include <dpthread-wrapper.h>

// defines 
#define DBG(x) x

// glocal variables 
static pthread_mutex_t l1, l2; 
static int val1, val2; 

static int pPid = 0; 
pid_t gettid(void)
{
	return ((pid_t)syscall(__NR_gettid) - pPid);
}

unsigned long fib(unsigned long n)
{
	if (n == 0)
		return 0;
	if (n == 1)
		return 2;
	return fib(n-1)+fib(n-2);
}


void compute(int input)
{
	int i; 
	DBG(printf("[%d] compute - begin %d\n", gettid(), input)); 
	for ( i = 0; i < input; i++ ) { 
		fib(2); 
	}
	DBG(printf("[%d] compute - end %d\n", gettid(), input)); 
}

void *
worker1(void *v)
{
	int input = (int)v; 

	compute(input); 

	// DBG(fprintf(stderr, "[%d] acquire  l1\n", gettid())); 
	pthread_mutex_lock(&l1);
	DBG(fprintf(stderr, "[%d] acquired l1\n", gettid())); 
	val1 = 1; 
	
	// DBG(fprintf(stderr, "[%d] acquire  l2\n", gettid())); 
	pthread_mutex_lock(&l2); 
	DBG(fprintf(stderr, "[%d] acquired l2\n", gettid())); 
	val2 = 1; 

	DBG(fprintf(stderr, "[%d] release  l2\n", gettid())); 
	pthread_mutex_unlock(&l2); 
	// DBG(fprintf(stderr, "[%d] released l2\n", gettid())); 
	
	DBG(fprintf(stderr, "[%d] release  l1\n", gettid())); 
	pthread_mutex_unlock(&l1); 
	// DBG(fprintf(stderr, "[%d] released l1\n", gettid())); 

	return NULL; 
}

void *
worker2(void *v)
{
	int input = (int)v; 

	compute(input); 

	// DBG(fprintf(stderr, "[%d] acquire  l2\n", gettid())); 
	pthread_mutex_lock(&l2);
	DBG(fprintf(stderr, "[%d] acquired l2\n", gettid())); 
	val2 = 2; 

	// DBG(fprintf(stderr, "[%d] acquire  l1\n", gettid())); 
	pthread_mutex_lock(&l1); 
	DBG(fprintf(stderr, "[%d] acquired l1\n", gettid())); 
	val1 = 2; 
	
	DBG(fprintf(stderr, "[%d] release  l1\n", gettid())); 
	pthread_mutex_unlock(&l1); 
	// DBG(fprintf(stderr, "[%d] released l1\n", gettid())); 
	
	DBG(fprintf(stderr, "[%d] release  l2\n", gettid())); 
	pthread_mutex_unlock(&l2); 
	// DBG(fprintf(stderr, "[%d] released l2\n", gettid())); 

	return NULL; 
}

static void
usage(char *argv[])
{
	printf("deadlock -x <x> -y <y>\n"
	       "-x : input for thread 1\n" 
	       "-y : input for thread 2\n", argv[0]); 
}

int main(int argc, char *argv[])
{
	pthread_t allthr[10];
	int i, ret; 
	int x, y; 

	// initialize 
	val1 = val2 = 0; 
	x = 10; y = 0; 

	// parameters 
	while((i=getopt(argc, argv, "x:y:h")) != EOF) {
		switch(i) {
		case 'h':
			usage(argv);
			return 0;
		case 'x':
			x = strtol(optarg, NULL, 0); 
			break; 
		case 'y': 
			y = strtol(optarg, NULL, 0); 
			break; 
		default:
			errx(1, "invalid option");
		}
	}

	// lock initialize 
	pthread_mutex_init(&l1, NULL);
	pthread_mutex_init(&l2, NULL);

	// thread create 
	pPid = gettid(); 
	ret = pthread_create(&allthr[0], NULL, worker1, (void *)(unsigned long)x);
	if (ret) err(1, "pthread_create failed");
	ret = pthread_create(&allthr[1], NULL, worker2, (void *)(unsigned long)y);
	if (ret) err(1, "pthread_create failed");

	// wait for workers to finish. 
	pthread_join(allthr[0], NULL);
	pthread_join(allthr[1], NULL);

	// print the who's got the lock. 
	printf("val1 : %d\n", val1); 
	printf("val2 : %d\n", val2); 

	return 0; 
}

