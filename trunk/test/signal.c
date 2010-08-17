#include <pthread.h> 
#include <stdio.h>
#include <dpthread-wrapper.h>
#include <signal.h>

static sigset_t SignalMask;
static int SIG_HANDLER_QUIT_SIGNAL = SIGUSR1; // signal used to stop SignalHandlerThread

unsigned long 
fib(unsigned long n)
{
	if (n == 0)
		return 0;
	if (n == 1)
		return 2;
	return fib(n-1)+fib(n-2);
}

void* signalHandlerProc(void* arg)
{
	int signalCaught;

	// wait for specified in mask signal
	int ret = sigwait(&SignalMask, &signalCaught);

	fib(15); 
#if 0
	if (ret != 0)
	{
		fprintf(stderr, "\n *signalHandlerProc - sigwait error: %d\n", ret);
	}
	else 
	{
		fprintf(stdout, "\n signal received %d\n", signalCaught); 
	}
#endif 
}

int initSignalMask()
{
	int ret = 0;
	ret = sigemptyset(&SignalMask);

	ret = sigaddset(&SignalMask, SIGINT) | ret;
	ret = sigaddset(&SignalMask, SIGTERM) | ret;
	ret = sigaddset(&SignalMask, SIGABRT) | ret;
	ret = sigaddset(&SignalMask, SIG_HANDLER_QUIT_SIGNAL) | ret;
	#ifndef WIN32
	ret = sigaddset(&SignalMask, SIGHUP) | ret;
	#endif

	if (ret == 0)
	{
		ret = pthread_sigmask(SIG_BLOCK, &SignalMask, NULL);
	}

	return ret;
}


main(int argc, char *argv[])
{
	int i; 
	pthread_t sig_thr;
	int ret; 

	initSignalMask(); 

	pthread_create(&sig_thr, NULL, signalHandlerProc, (void *)(unsigned long)i);
	fib(15); 	
	// detio_printf("sleep 3 sec\n"); 
	// detio_sleep(3); 
	ret = pthread_kill(sig_thr, SIG_HANDLER_QUIT_SIGNAL);
	fib(15); 
	pthread_join(sig_thr, NULL);
}
