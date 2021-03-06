/**
 * Multi-variable atomicity violation example. 
 *
 * This example is taken from Figure 1 of "Finiding Concurrency Bugs with 
 * Context-Aware Communication Graphs", MICRO'09.  
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

static char *g_str = ""; 
static int g_length = 0; 

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
	
	pthread_mutex_lock(&lock);
	DBG(fprintf(stderr, "[1] acquired lock\n"));
	g_str = strdup("test string"); 
	DBG(fprintf(stderr, "[1] release  lock\n"));
	pthread_mutex_unlock(&lock); 

	fib(input); 

	pthread_mutex_lock(&lock);
	DBG(fprintf(stderr, "[1] acquired lock\n"));
	g_length = strlen(g_str); 
	DBG(fprintf(stderr, "[1] release  lock\n"));
	pthread_mutex_unlock(&lock); 

	return NULL; 
}

void *
worker2(void *v)
{
	char *tptr; 
	int tlen; 

	pthread_mutex_lock(&lock);
	DBG(fprintf(stderr, "[2] acquired lock\n"));
	tptr = g_str; 
	DBG(fprintf(stderr, "[2] release  lock\n"));
	pthread_mutex_unlock(&lock); 


	pthread_mutex_lock(&lock);
	DBG(fprintf(stderr, "[2] acquired lock\n"));
	tlen = g_length; 
	DBG(fprintf(stderr, "[2] release  lock\n"));
	pthread_mutex_unlock(&lock); 

	printf("tptr = %s, tlen = %d\n", tptr, tlen); 
	if ( strlen(tptr) != tlen ) { 
		printf("ERROR: mismatch\n"); 
	}
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

