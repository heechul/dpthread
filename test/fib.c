#include <dpthread.h>

unsigned long 
fib(unsigned long n)
{
	if (n == 0)
		return 0;
	if (n == 1)
		return 2;
	return fib(n-1)+fib(n-2);
}

void myworker(void *arg)
{
	fib(30); 
}


int main(int argc, char *argv[])
{
	pthread_t allthr[10]; 
	fib(30); 

        det_init(argc, argv);
	det_create(allthr, NULL, myworker, 0); 
	det_enable(); 
	
	pthread_join(allthr[0], NULL); 

	return 0; 
}

