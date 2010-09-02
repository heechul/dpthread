/**
 * A classical bank account example that demonstrate atomicity violation. 
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
#include <assert.h>

// NOTE: uncomment the following line to make app deterministic 
// #include <dpthread-wrapper.h>

// defines 
#define DBG(x) x 

// glocal variables 
static volatile int g_account;  // account 

static pthread_mutex_t lock; 

// worker definitions 
void *
Deposit(int amount)
{
	int balance; // ERROR: balance can be stale

	pthread_mutex_lock(&lock); 
	balance = g_account; 
	pthread_mutex_unlock(&lock); 

	pthread_mutex_lock(&lock);
	g_account = balance + amount; 
	pthread_mutex_unlock(&lock); 

	return NULL; 
}

void *
Withdraw(int amount)
{
	int balance; // ERROR: balance can be stale

	pthread_mutex_lock(&lock);
	balance = g_account; 
	pthread_mutex_unlock(&lock); 

	pthread_mutex_lock(&lock);
	g_account = balance - amount; 
	pthread_mutex_unlock(&lock); 
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

	g_account = 10; 

	// thread create 
	ret = pthread_create(&allthr[0], NULL, Deposit, 1);
	if (ret) err(1, "pthread_create failed");
	Withdraw(2); 

	// wait for workers to finish. 
	pthread_join(allthr[0], NULL);

	assert( g_account == 9 ); 

	return 0; 
}

