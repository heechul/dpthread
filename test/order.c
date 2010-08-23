/**
 * Order violation example 
 *
 * This example is taken from Figure 2 of "Learning from Mistakes -- A 
 * Comprehensive Study on Real World Concurrency Bug Characteristics", 
 * ASPLOS'08.  
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

// NOTE: uncomment the following line to make app deterministic 
#include <dpthread-wrapper.h>

// defines 
#define DBG(x) x 

// glocal variables 
static pthread_mutex_t lock; 

typedef struct { 
	int tid; 
	int state; 
} my_thread_t; 

static my_thread_t *my_thread_ptr; 

// local functions 
unsigned long fib(unsigned long n)
{
	if (n == 0)
		return 0;
	if (n == 1)
		return 2;
	return fib(n-1)+fib(n-2);
}

// worker definitions 
void *
worker1(void *v)
{
	int input = (int)v; 
	
	fib(input);

	DBG(fprintf(stderr, "[1] acquire  lock\n"));
	pthread_mutex_lock(&lock);
	DBG(fprintf(stderr, "[1] acquired lock\n"));

	my_thread_ptr = (my_thread_t *)malloc(sizeof(my_thread_t)); 

	my_thread_ptr->tid = 1; 
	my_thread_ptr->state = 1; 

	DBG(fprintf(stderr, "[1] release lock\n"));
	pthread_mutex_unlock(&lock); 

	return NULL; 
}

void *
worker2(void *v)
{
	int input = (int)v; 
	
	int m_tid = 0; 
	int m_state = 0; 

	fib(input);

	DBG(fprintf(stderr, "[2] acquire  lock\n"));
	pthread_mutex_lock(&lock);
	DBG(fprintf(stderr, "[2] acquired lock\n"));

	m_tid = my_thread_ptr->tid; 
	m_state = my_thread_ptr->state; 

	DBG(fprintf(stderr, "[2] release lock\n"));
	pthread_mutex_unlock(&lock); 

	printf("m_tid = %d, m_state = %d\n", m_tid, m_state); 

	return NULL; 
}


// main 
static void
usage(char *argv[])
{
	printf("%s -x <x> -y <y>\n"
	       "-x : input for thread 1\n" 
	       "-y : input for thread 2\n", argv[0]); 
}

int main(int argc, char *argv[])
{
	pthread_t allthr[10];
	int i, ret; 
	int x, y; 

	// initialize 
	x = 13; 
	y = 0; 

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
	pthread_mutex_init(&lock, NULL);

	// thread create 
	ret = pthread_create(&allthr[0], NULL, worker1, (void *)(unsigned long)x);
	if (ret) err(1, "pthread_create failed");
	ret = pthread_create(&allthr[1], NULL, worker2, (void *)(unsigned long)y);
	if (ret) err(1, "pthread_create failed");

	// wait for workers to finish. 
	pthread_join(allthr[0], NULL);
	pthread_join(allthr[1], NULL);

	return 0; 
}

