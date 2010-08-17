#include <pthread.h> 
#include <stdio.h>
// #include <dpthread-wrapper.h>

#define MAX_THR 2
#define INPUT 20 

static pthread_mutex_t m; 
static int iteration = 20;

volatile int sum = 0; 

unsigned long 
fib(unsigned long n)
{
	if (n == 0)
		return 0;
	if (n == 1)
		return 2;
	return fib(n-1)+fib(n-2);
}

void *my_worker(void *v)
{
	int i; 
	for ( i = 0; i < iteration; i++) { 
		int val; 
		fib(INPUT); 
		fprintf(stdout, "hello\n"); 
		sum++; 
		fib(INPUT); 
	}
	return NULL; 
}

main(int argc, char *argv[])
{
	int i; 
	pthread_t allthr[MAX_THR];
	int max_thr = MAX_THR; 

	pthread_mutex_init(&m, NULL); 

	for ( i = 0; i < max_thr; i++ ) { 
		pthread_create(allthr+i, NULL, my_worker, (void *)(unsigned long)i);
	}

	for (i = 0; i < max_thr; i++) {
		pthread_join(allthr[i], NULL);
	}
}
